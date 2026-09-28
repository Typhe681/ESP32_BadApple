# ESP32_BadApple
Bad Apple video by Touhou on ESP32 with SSD1306 OLED, uses the Heatshrink compression library to decompress the RLE encoded video data.
First version, no sound yet, video only.

Fork of the original [ESP32_BadApple by Hackerspace-FFM](https://github.com/Hackerspace-FFM/ESP32_BadApple), ported to Adafruit_SSD1306 and LittleFS.

![Bad Apple on ESP32](ESP32_BadApple.jpg)

## Hardware Requirements
Runs on any ESP32 board with a 128x64 SSD1306 I2C OLED display. Default wiring is SDA = GPIO21, SCL = GPIO22, I2C address 0x3C. Boards with built-in displays (Heltec, TTGO, etc.) probably work too, but you will need to adjust the pins in the sketch.

## Software Requirements
* Arduino IDE (1.8.x or 2.x)
* ESP32 Arduino core from https://github.com/espressif/arduino-esp32
* Adafruit SSD1306 and Adafruit GFX libraries (install via the Arduino Library Manager)
* Heatshrink for Arduino from https://github.com/p-v-o-s/Arduino-HScompression
* A LittleFS upload tool, for example https://github.com/earlephilhower/arduino-littlefs-upload (Arduino IDE 2.x) or `pio run -t uploadfs` on PlatformIO

# Usage
* Adapt the `Wire.begin()` pins and I2C address in the main sketch if necessary
* Pick a partition scheme with a large enough LittleFS/data partition
* Upload sketch
* Upload the `data` folder (containing `video.hs`) to LittleFS using your upload tool

Enjoy your apple. Pressing PRG button (GPIO0) for max display speed (mainly limited by I2C transfer), otherwise limited to 30 fps.

# How does it work
Video have been separated into >6500 single pictures, resized to 128x64 pixels using VLC. 
Python script used to run-length encode the 8-bit-packed data using 0x55 and 0xAA as escape marker and putting all into one file.
RLE file has been further compressed using heatshrink compression for easy storage into LittleFS. 
Heatshrink for Arduino uses ZIP-like algorithm and is available also as a library under https://github.com/p-v-o-s/Arduino-HScompression and 
original documentation is here: https://spin.atomicobject.com/2013/03/14/heatshrink-embedded-data-compression/

# Credits
Original project by Hackerspace-FFM.de (2018), MIT License. See the LICENSE file.
