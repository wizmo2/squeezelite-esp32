# Using Squeezelite-ESP32 as a Streaming source for WaveInput

**WARNING: A dedicated I2S ADC hardware is required for Line-In streaming.** For higher stream rates, a dedicated device (no output DAC) with a LAN connection is recommended.   

The ADC sink service can act as a stream source for for "Waveinput" plugin on Squeezebox Server.

Enable the "ADC Sink" service and set the host and port in "Hardware-ADC_Options". See [README](/README.md#Line-in-Microphone) for details on configuring the ADC hardware and output stream.  For Waveinput compatibility use RAW format.  _NOTE: start with a low quality stream to verify functionality.  for example, 16000Hz 16bit stereo (2 channel)._

# Waveinput for Linux configuration
- Install the 'WaveInput for Linux' plugin form the Server Plugin Settings.
- Verify your server OS installtion has netcat and sox installed.  (for docker installations, run `sudo docker exec -it <container_name> /bin/bash`.  Check `nc` and `sox` to verify presence.  Use `apt update && apt install netcat sox` if necessary)
- Edit the default custom-convert.conf and replace the "wavein pcm * *" entry with
```
wavin pcm * * 
	# R:{OSAMPLERATE=%d}
	[nc] -l -u -w1 $FILE$ | [sox] -t raw -r 16000 -b 16 -c 2 -L -e signed-integer - -t raw -c $OCHANNELS$ -r $OSAMPLERATE$ - 
```
and set the first '-r' '-b' and '-c' to match your ADC stream settings

_NOTE:  If your ADC and network supports CD quality streams (44100Hz 2 channels 16bit), you can obmit the "sox" resample_
```
wavin pcm * * 
	# R:{OSAMPLERATE=%d}
	[nc] -l -u -w1 $FILE$
```

- Create a new favorite entry with `wavin:<port>` where 'port' is your ADC stream value
- Restart the Squeezebox Server
- Play the new favorite entry.

_NOTE It is only possible to play the stream to one primary player.  Use Player syncrohnization to play across multiple devices._

