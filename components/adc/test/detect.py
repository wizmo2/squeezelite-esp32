"""
Listen on UDP for audio from Rhasspy, detect wake words using Open Wake Word,
and publish on MQTT when wake word is detected to trigger Rhasspy speech-to-text.

Support multiple Satellite instances and forwards audio to Rhasspy for dialog detection.
Allows for tone forwarding to base Rhasspy instant
"""

import argparse
import io
import socket
import threading
import time
import wave
from collections import deque
import json 
import paho.mqtt.client
import yaml
import numpy as np
from openwakeword.model import Model
import logging 

_LOGGER = logging.getLogger(__name__)
logging.basicConfig(level=logging.INFO, format='[%(asctime)s] %(message)s')

RHASSPY_BYTES = 2092
RHASSPY_FRAMES = 1024
OWW_FRAMES = 1280  # 80 ms window @ 16 kHz = 1280 frames

AUDIO_MODES = { 0:'OFF',1:'MONITOR',2:'STREAM',3:'DETECT' }

parser = argparse.ArgumentParser(description="Open Wake Word detection for Rhasspy")
parser.add_argument(
    "-c",
    "--config",
    default="config.yaml",
    help="Configuration yaml file, defaults to `config.yaml`",
    dest="config_file",
)
args = parser.parse_args()


def load_config(config_file):
    """Use config.yaml to override the default configuration."""
    try:
        with open(config_file, "r") as f:
            config_override = yaml.safe_load(f)
    except FileNotFoundError:
        config_override = {}

    default_config = {
        "mqtt": {
            "broker": "192.168.4.3",
            "port": 1883,
            "username": "username",
            "password": "password",
        },
        "oww": {
            # standard OWW parameters 
            "vad_threshold": 0,
            "enable_speex_noise_suppression": False,
            # custom hermes control
            "base_name" : "base",
            "enable_stream": True,
            # custom filter
            "enabled_models": ["alexa", "hey_mycroft", "hey_jarvis"],
            "activation_threshold": 0.7,
            "deactivation_threshold": 0.2,
            "activation_samples": 3,
        },
        "udp_ports": {"base": 12202},
    }

    config = {**default_config, **config_override}
    if not config["udp_ports"]:
        _LOGGER.warning(
            "CONFIG: No UDP ports configured. Configure UDP ports to receive audio for wakeword detection."
        )
        exit()
    return config


class RhasspyUdpAudio(threading.Thread):
    """Get audio from UDP stream and add to wake word detection queue."""

    def __init__(self, roomname, port):
        threading.Thread.__init__(self)
        self.roomname = roomname

        self.port = port
        self.buffer = []
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("", port))
        self.audio_mode = 1 # Monitor mode
        self.base_name = config["oww"]["base_name"] # used only for enabling tone forwarding  

        self.oww = Model(
            #wakeword_model_names=["alexa"], 
            vad_threshold=config["oww"]["vad_threshold"],
            enable_speex_noise_suppression=config["oww"][
                "enable_speex_noise_suppression"
            ],
        )
        self.filters = {}


        self.mqtt = paho.mqtt.client.Client()
        self.mqtt.username_pw_set(
            config["mqtt"]["username"], config["mqtt"]["password"]
        )
        self.mqtt.connect(config["mqtt"]["broker"], config["mqtt"]["port"], 60)
        self.mqtt.on_connect = self.__on_connect
        self.mqtt.on_message = self.__on_message
        self.mqtt.loop_start()
        _LOGGER.debug("RhasspyUdpAudio: Connected to broker")

    def __on_connect(self, client, userdata, flags, rc):
        """Subscribe to Hermes MQTT when connected."""
        client.subscribe("hermes/hotword/#")  # monitor dialogue control messages
        if self.base_name and self.roomname != self.base_name: 
            # monitor tone messages
            client.subscribe(f"hermes/audioServer/{self.roomname}/playBytes/#")

    def __on_message(self, client, userdata, msg):
        """Sets audio mode based on Hermes MQTT responses."""
        if 'playBytes' in msg.topic: # forward tones to base
            self.mqtt.publish(msg.topic.replace(self.roomname, self.base_name), msg.payload) 
            return
        payload = json.loads(msg.payload.decode("utf-8"))
        siteId = payload.get("siteId")
        reason = payload.get("reason")
        mode_was = self.audio_mode
        if not siteId or not reason or 'dialogue' not in reason:
            return 
        if msg.topic.endswith("toggleOff"):
            self.audio_mode = 2 if siteId == self.roomname and config["oww"]["enable_stream"] else 0
            if mode_was != self.audio_mode: _LOGGER.info(f"{self.roomname} mode: {AUDIO_MODES[self.audio_mode]} (siteId:{siteId}) ")
        elif msg.topic.endswith("toggleOn"):
            self.audio_mode = 1
            if mode_was != self.audio_mode: _LOGGER.info(f"{self.roomname} mode: {AUDIO_MODES[self.audio_mode]} ")
        else:
            _LOGGER.debug(f"RhasspyUdpAudio: {self.roomname} topic: {msg.topic} payload:{msg.payload}")
    
    def _monitor(self, data):
        """Sends incomming frames to queue for Wake-Word monitoring."""
        audio = wave.open(io.BytesIO(data))
        frames = audio.readframes(RHASSPY_FRAMES)
        self.buffer.extend(np.frombuffer(frames, dtype=np.int16))
        if len(self.buffer) > OWW_FRAMES:
            self.__predict(np.asarray(self.buffer[:OWW_FRAMES], dtype=np.int16))
            self.buffer = self.buffer[OWW_FRAMES:]

    def _stream(self, data):
        """Sends raw data to Hermes MQTT for dialogue processing."""
        self.mqtt.publish(f"hermes/audioServer/{self.roomname}/audioFrame", data)

    def __predict(self, audio):
        """Run Wake-Word detection and filter responses."""
        prediction = self.oww.predict(audio)
        for wakeword in prediction.keys():
            confidence = prediction[wakeword]
            if ( 
                wakeword in config["oww"]["enabled_models"]  and 
                self.__filter(wakeword, confidence)
            ):
                self.__publish(wakeword, self.roomname)
                #self.audio_mode = 3
                _LOGGER.info(f"{self.roomname} mode: {AUDIO_MODES[self.audio_mode]} (wakeword:{wakeword},conf:{confidence:.2f})) ")
               
            #elif wakeword in config["oww"]["enabled_models"] and confidence > 0:
            #    _LOGGER.debug(f"RhasspyUdpAudio: processing {self.roomname} data - {wakeword} = {confidence}")

    def __publish(self, wakeword, roomname):
        """Publish Wake-Word message to Hermes MQTT."""
        payload = {
            "modelId": wakeword,
            "modelVersion": "",
            "modelType": "universal",
            "currentSensitivity": config["oww"]["activation_threshold"],
            "siteId": roomname,
            "sessionId": None,
            "sendAudioCaptured": None,
            "lang": None,
            "customEntities": None,
        }
        self.mqtt.publish(f"hermes/hotword/{wakeword}/detected", json.dumps(payload))

    def __filter(self, wakeword, confidence):
        """OWW response filter.
        
        When simple moving average (of length `activation_samples`) crosses the `activation_threshold`
        for the first time, then trigger Rhasspy. Only "re-arm" the wakeword when the moving average
        drops below the `deactivation_threshold`."""
        try:
            self.filters[wakeword]["samples"].append(confidence)
        except KeyError:
            self.filters[wakeword] = {
                "samples": deque(
                    [confidence], maxlen=config["oww"]["activation_samples"]
                ),
                "active": False,
            }
        moving_average = np.average(self.filters[wakeword]["samples"])
        activated = False
        if (
            not self.filters[wakeword]["active"]
            and moving_average >= config["oww"]["activation_threshold"]
        ):
            self.filters[wakeword]["active"] = True
            activated = True
        elif (
            self.filters[wakeword]["active"]
            and moving_average < config["oww"]["deactivation_threshold"]
        ):
            self.filters[wakeword]["active"] = False
        if moving_average > 0.1:
            _LOGGER.debug(f"{wakeword:<16} {activated!s:<8} {self.filters[wakeword]}")
        return activated

    def run(self):
        """Thread to receive and process UDP audio streams."""
        _LOGGER.info(f"RhasspyUdpAudio: Listening for {self.roomname} audio on UDP port {self.port}")
        while True:
            data, addr = self.sock.recvfrom(RHASSPY_BYTES)
            if self.audio_mode == 1: 
                self._monitor(data)
            elif self.audio_mode == 2:
                self._stream(data)



class Monitor(threading.Thread):
    """ Monitor mqtt states for custom functionality"""

    def __init__(self, threads):
        threading.Thread.__init__(self)
        self.mqtt = paho.mqtt.client.Client()
        self.mqtt.username_pw_set(
            config["mqtt"]["username"], config["mqtt"]["password"]
        )
        self.mqtt.connect(config["mqtt"]["broker"], config["mqtt"]["port"], 60)
        self.mqtt.on_connect = self.__on_connect
        self.mqtt.on_message = self.__on_message
        self.mqtt.loop_start()
        _LOGGER.debug("Monitor: Connected to broker")

    def __on_connect(self, client, userdata, flags, rc):
        """Subscribe to Hermes MQTT when connected."""
        client.subscribe("hermes/asr/textCaptured")  # monitor dialogue results

    def __on_message(self, client, userdata, msg):
        """Gets subscribed messages."""
        payload = json.loads(msg.payload.decode("utf-8"))
        if payload: _LOGGER.info(f"""Monitor: {payload.get("siteId") or "unknown"} text:'{payload.get("text") or "<null>"}' (conf:{payload.get("likelihood"):0.2f})""")

    def run(self):
        """Thread to monitor functionality."""
        while True:
            time.sleep(10)
        

if __name__ == "__main__":
    config = load_config(args.config_file)
    threads = []
    for name, port in config["udp_ports"].items():
        t = RhasspyUdpAudio(name, port)
        t.daemon = True
        t.start()
        threads.append(t)
    t = Monitor(threads)
    t.start()
    threads.append(t)
    _LOGGER.debug(f"MAIN: Threads: {threads}")
