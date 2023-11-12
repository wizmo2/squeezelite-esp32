# Using Squeezelite-ESP32 as a Voice Assistant Satellite

The ADC sink service can be used with [openWakeWord](https://github.com/dscripka/openWakeWord) (OWW) as a [Rhasspy Satelite](https://rhasspy.readthedocs.io/en/latest/).

Enable the "ADC Sink" service and set the host and port in "Hardware-ADC_Options"  for the OWW compatibility, the rate must be 16000Hz (default).  In WAV mode, An encoder converts the stream to 16bit mono. _NOTE: use the [UDP test Server script]{../components/adc/test/udp_test_server.py} to verify functionality._

_NOTE The OWW and Rhasspy implementation is based heaviy on the existing [Docker Image](https://github.com/dalehumby/openWakeWord-rhasspy) by @dalehumpy_  

The following python script and configuration file can be used to receive the UDP stream to a remote OWW server.  The script monitors the stream for enabled Wake-Words.  When detected, it streams the audio data to Rhasspy via  an external Hermes-MQTT server.  Monitoring of all saltellites is disabled when any sattelite is Streaming.  The script can forward status beeps to the Rhasspy base unit if required. _NOTE: If using Home-Assistant as an Intent Handler, you can use a mqtt sensor and automations to send audio (play_media) or visual indicators (led_vu / dmx) to the satellites_ 

[detect.py](../components/adc/test/detect.py) [config.yaml](../components/adc/test/config.yaml)

# Rhasspy Configuration
The following notes can be used to configure your base Rhasspy settings.
- set the Rhasspy siteId to 'base'. 
- In MQTT, set to 'External'. _NOTE: This is need if you run Rhasspy in a docker.  You will need an exsiting remote MQTT server (for example, the HASS Mosquitto add-on)_
- In Audio Recording, set the 'UDP Audio (Output) to the OWW server ip address and base port
- In Wake Word, set to 'Hermes MQTT'
- In Speech to Text, set the Satellite siteIds for the names in the config.yaml
- In Intent Recognition, set the Satellite siteIds for the names in the config.yaml
- In Text to Speech, set the Satellite siteIds for the names in the config.yaml
- In Dialogue Management, set the Satellite siteIds for the names in the config.yaml
- In Intent Handling, set the Satellite siteIds for the names in the config.yaml


## Docker operation
You can use dalehumpy's docker image, but will need to replace the default detect.py to have OWW forward the stream for dialogue detection.  _NOTE: The following script uses a virtual mount to access the startup script in the docker host.  The folder must be created manually_

```
CID=oww
sudo docker run -it \
 --restart=unless-stopped \
 --name $CID \
 -p 12202:12202/udp \
 -p 12203:12203/udp \
 -p 12204:12204/udp \
 -v /docker/$CID:/config \
 -v /docker/$CID/detect.py:/app/detect.py \
  dalehumby/openwakeword-rhasspy

```

# ADC configuration reference
**THIS IS AN EARLY RELEASE OF ADC LINE-IN FUNCTIONALITY FOR SQUEEZELITE-ESP32. The initial release only supports the T-Embed-S3 development board. There are likely breaking changes to come!**

ADC Sink can function with a independant I2S chip.

When using a dedicated I2C chip
- Include the model and pin configuration for the second I2C in `adc_config`

`model=es7210,bck=47,ws=21,di=14,mck=48,i2c=64,sda=18,scl=8`

When sharing the DAC chip - **!DO NOT USE! It is highly likely that shared mode will never be released, and if it does will not support audio streaming**
- Only Generic I2S, and AC101 codecs currently support ADC input (need help for extra support).  set source=1 for 'mic' input, source=2 for 'linein' input
- If Generic I2C is used, the `dac_control_set` must be modified to initilaize the ADC input and 'micon' / 'lineinon' / 'lineinoff' can be added to set mixer controls for local playback.  
- In some case, the ADC and DAC must run at the same sample rate.  In this case, you will have to set the rate option in the Squeezelite Audio settings to 16kHz (-Z 16000) for OWW compatibility.

`source=(0|1|2)`
 where source(DAC sharing only) 0=adc_loopback,1=line-in bypass,2=microphone bypass.

The stream configuration is stored in `adc_stream`

`port=<port_num>,host=<destinatiopn_ip>[,rate=<sample_rate][,ch=(1|2)]`
 where ch 1=mono, 2=stereo.

sample_rate frequencies are limited to those supported by the chip.  channel and source selection requires supported chips, and/or custom dac_controlset confgiuration.

# Reommended LMS Plugins

Add `LineIn' to allow LMS control local playback.

Add 'WaveInput for Linux` to allow LMS access to the audio stream. (in development)