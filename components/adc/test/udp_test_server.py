import socket
import io
import wave
import pyaudio
import numpy as np


localIP     = "0.0.0.0"
localPort   = 12203
MAX_BYTES = 4096 # 2092
RHASSPY_FRAMES = 1024
OWW_FRAMES = 1280  # 80 ms window @ 16 kHz = 1280 frames
BAR_SIZE = 50
BAR_MAX = 0xffff / 2

# Create a datagram socket
UDPServerSocket = socket.socket(family=socket.AF_INET, type=socket.SOCK_DGRAM)
print("created socket ", UDPServerSocket)

# Bind to address and ip
UDPServerSocket.bind((localIP, localPort))
print("bind ", localIP, localPort)

player = pyaudio.PyAudio()
address = ""
rate = 16000
channels = 1
i = 0
stream = player.open(format = 8, channels = channels, rate = rate, output = True)

print("UDP server up and listening")
# Listen for incoming datagrams

while(True):

    message, addr = UDPServerSocket.recvfrom(MAX_BYTES)
    audio = wave.open(io.BytesIO(message))
    
    bytesReceived = len(message)
    flow = "\x1b[A"
    substr = message[:4]
    if substr == b'RIFF':
       
        audio = wave.open(io.BytesIO(message))
       
        wav_fmt =  player.get_format_from_width(audio.getsampwidth())
        wav_ch = audio.getnchannels();
        wav_rate = audio.getframerate()

        if (address != addr or wav_ch != channels or wav_rate != rate):
            address = addr
            print ("Client IP Address:{}".format(address))
            print
            
            if stream.is_active:
                stream.close()
            rate = wav_rate
            channels = wav_ch
            stream = player.open(format = wav_fmt, channels = channels, rate = rate , output = True)
    
        # Read message in chunks
        data = audio.readframes(RHASSPY_FRAMES)
        
        int_data = np.frombuffer(data, dtype=np.int16)
        volume = np.average(np.abs(int_data))
        percent = min(BAR_SIZE, int(BAR_SIZE * volume / BAR_MAX))
        sampleData = "[{}{}]".format('#' * percent, '_' * (BAR_SIZE-percent))
        #sampleData =  ":".join("{:02x}".format(c) for c in data[:40])
        #sampleData =  ":".join("{:06d}".format(c) for c in int_data[:20])
        wav_fmt_str = "type:{},channels:{},rate:{},frames{}".format(wav_fmt,audio.getnchannels(),audio.getframerate(), audio.getnframes())
        print(flow + "Frame {},{} ({},{},{})".format(wav_fmt_str, sampleData, bytesReceived, i, int(volume)))

        # Play the sound by writing the audio message to the stream
        while data != b'':
            stream.write(data)
            i+=1
            data = audio.readframes(RHASSPY_FRAMES)
            
    else:
        sampleData =  ":".join("{:02x}".format(c) for c in message[:40])
        print(flow + "{}-({}) {}".format(i, bytesReceived, sampleData))

# Close and terminate the stream
stream.close()
player.terminate()