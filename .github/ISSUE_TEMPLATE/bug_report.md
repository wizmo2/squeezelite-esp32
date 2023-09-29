---
name: Bug report
about: How to Submit an Issue for the squeezelite-esp32 Project
title: ''
labels: ''
assignees: ''

---

To help us resolve your issue as quickly as possible, please follow these guidelines when submitting an issue. Providing all the necessary information will save both your time and ours.

### Describe the bug
A clear and concise description of what the bug is.

### Preliminary Information

1. **Firmware Version**: Specify the version of the firmware you are using.
2. **Plugin Version**: Mention the version of the plugin installed on your LMS (Logitech Media Server).

### Hardware Details

Please describe your hardware setup:

- **ESP32 Module**: For example, ESP32 WROVER, ESP32-S3, etc.
- **Board Type**: If applicable, e.g., ESP32 audio kit, etc.
- **DAC Chip**: Specify the DAC chip you are using.
- **Additional Hardware**: Include details about any other hardware like rotary controls, buttons, screens (SPI, I2C), Ethernet, IO expansion, etc.

### NVS Settings

Follow these steps to share your NVS settings:

1. Open the web UI of your device.
2. Click on the "Credit" tab.
3. Enable the "Show NVS Editor" checkbox. This allows you to view or change the NVS configuration even when not in recovery mode.
4. Navigate to the "NVS Editor" tab.
5. Scroll to the bottom and click "Download Config".
6. Share the downloaded content here.

<details>
<pre><code>
  Your log content here
</code></pre>
</details>


### Logs

To share logs:

1. Connect your player to a computer using a USB cable. Use a built-in Serial-USB adapter if your player has one, or an external USB adapter otherwise.
2. Go to the [web installer](https://sle118.github.io/squeezelite-esp32-installer/).
3. Click "Connect to Device".
4. Select the appropriate serial port.
5. Click "Logs And Console".
6. Download the logs and share them here. Please remove any sensitive information like Wi-Fi passwords or MAC addresses.
- **If the problem occurs soon after booting**: Share the full log until the issue occurs.
- **If the problem occurs later during playback**: Trim the logs to include information just before and after the problem occurs.

#### Example Log

Here's an example log for reference. Make sure to obfuscate sensitive information like Wi-Fi passwords, MAC addresses, and change IP addresses to something more generic.

<details>
<pre><code>
=== START OF LOG ===
Example of log from the console 
rst:0x1 (POWERON_RESET),boot:0x17 (SPI_FAST_FLASH_BOOT)
configsip: 0, SPIWP:0xee
clk_drv:0x00,q_drv:0x00,d_drv:0x00,cs0_drv:0x00,hd_drv:0x00,wp_drv:0x00
mode:DIO, clock div:1
...
I (1041) cpu_start: Application information:
I (1044) cpu_start: Project name:     Squeezelite-ESP32
I (1050) cpu_start: App version:      I2S-4MFlash-1336
I (1055) cpu_start: Compile time:     Aug 12 2023 01:20:18
I (1062) cpu_start: ELF file SHA256:  34241d6e99fd1d6b...
I (1068) cpu_start: ESP-IDF:          v4.3.5-dirty
...
I (1133) heap_init: At 40094A8C len 0000B574 (45 KiB): IRAM
I (1139) spiram: Adding pool of 4066K of external SPI memory to heap allocator
=== END OF LOG ===   
</code></pre>
</details>

### Issue Description

1. **Observed Behavior**: Describe what you think is wrong.
2. **Expected Behavior**: Describe what you expect should happen.
3. **Steps to Reproduce**: Provide a step-by-step guide on how to replicate the issue.
