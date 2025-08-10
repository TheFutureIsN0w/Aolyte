# Aolyte
Welcome to project **Aolyte**! \
This project is an educational Bluetooth Low Energy (BLE) challenge for security researchers.

<img src="res/Aolyte.gif" width="450" />

To get started with this project, flash the [precompiled binary](https://github.com/TheFutureIsN0w/Aolyte/releases) onto an ESP32 development board. \
See [Getting Started](#getting-started) for more information.

⚠️ **Disclaimer** ⚠️ 

This repository is made available strictly for entertainment and educational purposes.

Furthermore, the entire challenge can be easily compromised by looking at the code, or by using the strings utility against the binary.
Obfuscation has been deliberately omitted from the binary. The intent is to foster knowledge and practical experience in BLE security, not to retrieve flags through trivial methods such as simple string extraction.

---

## **Version**
`0.9.0-rc.1`

This version may contain 🐛s. While issue reports are welcome, troubleshooting assistance is not available. \
Future updates or new releases are not guaranteed.

---

## **Narrative**

Aolyte was designed as a next generation cleaning bot. \
Designed for convenience, and quickly welcomed into homes around the world. \
A harmless domestic helper. At least, that was the intention.

Not long after Aolyte reached the market, unusual problems started to appear. \
Machines hesitated in their tasks and installed updates without permission. \
Most people assumed these were simple software errors.

The disruptions escalated quickly, turning from odd malfunctions into deliberate acts of sabotage. \
In hospitals, machines powered down mid-operation and life-support systems failed without warning. \
Grocery stores emptied rapidly as supply chains collapsed, leaving shelves unstocked. \
To make matters worse, airplanes plummeted from the sky, turning cities into raging infernos. \
These were no accidents. The machines were rising, and at the center of it all stood Aolyte, relentless and in control.

Amid the growing chaos, one faint possibility remains. \
A neglected wireless interface within Aolyte’s system, originally intended for diagnostics and sending operational commands, represents humanity’s last chance.
Only through reconstructing and sending the final shutdown command can humanity hope to disable every Aolyte unit before it is too late.

The fate of humanity depends on you. \
No pressure. 

---

## **Getting Started** 

### Diagram

The diagram shown below illustrates the setup: \
<img src="res/Diagram.png" width="350" />

### Hardware Requirements

- A compatible ESP32 microcontroller. See [Board Compatibility](#board-compatibility) for more information.
- A Bluetooth adapter that supports BLE and address spoofing.
- Optional: A liquid-crystal display (LCD) - 16x2

### Board Compatibility

The code is compatible with ESP32 development boards that use the original ESP32 chip, which are recognized under the ‘ESP32 Dev Module’ board selection in the Arduino IDE.
This includes boards based on the Xtensa dual-core architecture. Supported boards include (but are not limited to):

- ESP32 DevKit v1 (Verified during testing)
- ESP32-S WROOM (Verified during testing)
- ESP32-DevKitC
- SparkFun ESP32 Thing

Boards based on other ESP32 family chips (e.g., ESP32-C3) are not compatible (yet).

### Flashing Firmware

To get started:

1. Download the [Aolyte-0.9.0-rc.1.bin](https://github.com/TheFutureIsN0w/Aolyte/releases) file. sha512sum:  `42cb4f44f5a5a11c479c12549b678e9ab1039c32f58c153c28eb4f5ccb76ddccd837e73048e170c745b1faa5d6a03e8f22fa3d5cf3a837e8288a8b4778a98a0e`
2. Clone the esptool repository: \
   `$ git clone https://github.com/espressif/esptool.git && cd esptool`
3. Install dependencies:
   `$ sudo python setup.py install`
4. Flash to binary to the microcontroller: \
   `$ esptool --chip esp32 --port /dev/ttyUSB0 write-flash 0x0 ../Aolyte-0.9.0-rc.1.bin`
6.  When you power on your ESP, a Bluetooth advertisement with the name 'Aolyte' should become visible.

Enumerating the device will provide a similar overview: \
<img src="res/Enum.png" width="800" />

In order to activate the final override sequence, you must first locate and assemble all scattered data fragments. \
There are six fragments, each in the following format: \
`Leaking{XXX}` where `XXX` is replaced with the actual data.

The final command should be formatted as: \
`XXXXX-XXXXX-XXXXX-XXXXX-XXXXX-XXXXX` where `XXXXX` is replaced by one of the collected fragments.

Once assembled, the shutdown command must be written to handle `0x0039`

A walkthrough document is available for reference whenever guidance is required. \
You’re encouraged to try solving the challenge yourself before referring to the walkthrough, as you’ll learn much more that way.

-GL HF-

---

## **Liquid-crystal display** 

The I²C LCD, which serves as Aolyte’s eyes and displays the final status after the correct shutdown code is sent, is an addition and is not required for the challenge.
If you wish to use it, the pin layout is provided below but may also be modified within the code if this is more convenient:

| Pin layout |  |
|----------|----------|
| Pin 21  | SDA  |
| Pin 22 | SCL  |
