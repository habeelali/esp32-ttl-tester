# ESP32-Based Multi-Pin IC Tester

This repository contains the source code and documentation for an ESP32-based embedded system designed to test and identify 14 and 16-pin DIP integrated circuits (ICs). The system uses an SD card to store IC pinout and truth table definitions, allowing dynamic testing without firmware updates. It features a user-friendly interface with a 128x64 OLED display, tactile buttons for navigation, and WiFi-based configuration for editing the IC database.

## Project Overview

The ESP32-Based Multi-Pin IC Tester is a portable, cost-effective tool for engineers and hobbyists to verify the functionality of various DIP ICs, such as logic gates (e.g., 7400 NAND, 7432 OR), counters, and timers. The system dynamically configures GPIO mappings based on IC pin counts, applies test stimuli, and compares outputs against predefined truth tables stored on an SD card. It uses I2C GPIO expanders (PCF8574) with a level shifter for safe 5V interfacing.

## Features
- **Multi-Pin Support**: Tests 14 and 16-pin DIP ICs with dynamic GPIO configuration.
- **SD Card Integration**: Reads IC pinout and truth table definitions from `chips.dat` on an SD card, enabling updates without reflashing firmware.
- **WiFi Configuration**: Provides a web interface for editing `chips.dat` via an ESP32-hosted access point.
- **User Interface**: 128x64 OLED display with a button-driven menu for manual testing, auto-detection, and system information.
- **Hardware Interfacing**: Uses two PCF8574 I2C GPIO expanders and a 4-channel level shifter for robust 5V interfacing.
- **Auto-Detection**: Identifies ICs by running test vectors and matching outputs to the database.

## Hardware Components
- **Microcontroller**: ESP32 (controls GPIO, I2C, SPI, and OLED)
- **Display**: 128x64 OLED (I2C, SH1106)
- **I/O Expanders**: 2x PCF8574
- **Level Shifter**: 4-channel bi-directional
- **Sockets**: 8-pin, 14-pin, 16-pin DIP sockets
- **Storage**: SD card module (SPI)
- **Input Controls**: Tactile push buttons (Back, Enter, Scroll)
- **Test ICs**: Supports various logic gates, counters, timers, etc. (see `chips.dat`)

## Software Tools
- **Programming Language**: C/C++
- **IDE**: PlatformIO
- **Libraries**:
  - `SPI.h`, `SD.h` for SD card interfacing
  - `Wire.h` for I2C communication
  - `Adafruit_GFX.h`, `Adafruit_SH110X.h` for OLED display
  - `WiFi.h`, `WebServer.h` for WiFi and web server functionality
  - `esp_system.h`, `esp_spi_flash.h` for ESP32 system functions

## Repository Structure
```
├── chips.dat           # IC pinout and truth table definitions
├── main.cpp            # Main source code for ESP32
├── README.md           # This file
```

## Getting Started

### Prerequisites
- **Hardware**: Assemble the circuit with the listed components, ensuring proper connections for I2C (SDA: Pin 21, SCL: Pin 22), SPI (CS: Pin 2, MOSI: Pin 23, MISO: Pin 19, SCK: Pin 18), and buttons (Back: Pin 16, Enter: Pin 17, Scroll: Pin 5).
- **Software**:
  - Install [PlatformIO](https://platformio.org/) for building and uploading the code.
  - Ensure an SD card is formatted (FAT32) with `chips.dat` in the root directory.

### Installation
1. Clone the repository:
   ```bash
   git clone https://github.com/your-repo/esp32-ic-tester.git
   ```
2. Open the project in PlatformIO.
3. Connect the ESP32 to your computer via USB.
4. Build and upload the code using PlatformIO.
5. Insert the SD card with `chips.dat` into the SD card module.

### Usage
1. **Power On**: The system initializes, displays "IC Tester" on the OLED, and loads the main menu.
2. **Navigation**:
   - **Scroll Button**: Cycles through menu options (Manual Mode, Auto Mode, System Info) or ICs in Manual Mode.
   - **Enter Button**: Selects the highlighted option or runs a test in Manual Mode.
   - **Back Button**: Returns to the main menu from submenus.
3. **Modes**:
   - **Manual Mode**: Select an IC from the database and run its test vectors.
   - **Auto Mode**: Automatically detects the inserted IC by testing against all database entries.
   - **System Info**: Displays CPU frequency, free RAM, and flash usage.
4. **WiFi Configuration**:
   - Connect to the ESP32's WiFi access point (`esp32-ttl`, password: `12345678`).
   - Access the web interface at `http://13.37.4.20` to edit `chips.dat`.
   - Save changes to update the SD card.

### chips.dat Format
The `chips.dat` file defines ICs with the following structure:
```
[IC]
Name=<IC Name>
Pins=<8, 14, or 16>
Inputs=<Comma-separated input pins>
Outputs=<Comma-separated output pins>
Tests=<Test vectors in format input:expected_output;...>
```
Example:
```
[IC]
Name=7400 NAND
Pins=14
Inputs=1,2,4,5,9,10,12,13
Outputs=3,6,8,11
Tests=00000000:1111;01010101:1111;10101010:1111;11111111:0000
```

## Testing
- **Manual Testing**: Insert an IC into the appropriate socket, select it in Manual Mode, and run tests. The OLED displays "Passed" or "Failed" with details.
- **Auto Detection**: Insert an IC and select Auto Mode. The system identifies the IC or reports "Not Found" if unrecognized.
- **Error Handling**: The system checks for I2C communication issues, invalid test vectors, and SD card errors, displaying appropriate messages.

## Contributing
Contributions are welcome! Please fork the repository, make changes, and submit a pull request. Ensure code follows the existing style and includes comments for clarity.

## License
This project is licensed under the MIT License. See the `LICENSE` file for details.

## Acknowledgments
- Thanks to the open-source community for libraries and tools used in this project.
