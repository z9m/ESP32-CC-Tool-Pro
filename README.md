# ⚡ ESP32 CC-Tool Pro (Flasher & Debugger)

![License](https://img.shields.io/badge/license-GPLv3-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-green.svg)
![Status](https://img.shields.io/badge/status-stable-success.svg)

**A fully web-based Flasher and Debugger for Texas Instruments CCxxxx Microcontrollers (8051 core), powered by an ESP32.**

This tool allows you to flash, verify, dump, and debug TI CC-series chips (like **CC2530, CC2531, CC1110, CC1111**) wirelessly over WiFi. No drivers, no dongles, and no software installation required on your PC/Mac/Linux machine – it runs entirely in your browser.

> **Note:** This project is a heavily modified and expanded fork of the [ESP_CC_Flasher](https://github.com/atc1441/ESP_CC_Flasher) by [atc1441](https://github.com/atc1441).

![Web Interface Screenshot](image_343d3a.png)

## ✨ Features

### 🔥 Flasher
* **Read/Dump Flash:** Backup existing firmware to a `.bin` file directly to your device.
* **Write Firmware:** Wireless upload and flashing of `.bin` files.
* **Verify:** Ensure data integrity by comparing flash content with the uploaded file.
* **Chip Erase:** Unlock read-protected chips (mass erase).
* **Lock Chip:** Set lock bits to prevent firmware readout.
* **Smart Detection:** Auto-detects Chip Model and Flash size (supports banking up to 256KB).

### 🐛 Debugger (Real-time)
* **Execution Control:** Halt, Resume, and Single Step through instructions.
* **Live Registers:** Real-time view of **PC, ACC, SP, PSW, DPTR**, and **R0-R7** (with Register Bank support).
* **Memory Editor:** View and modify **XDATA/RAM** live via the Hex-Editor.
* **Disassembler:** Integrated 8051 disassembler converts hex codes to Assembly (ASM) for easy tracing.
* **Breakpoints:** Support for hardware breakpoints.

### 🌐 Web Interface
* **Modern UI:** Dark theme, responsive design, fast updates.
* **Multi-Language:** Fully translated into **EN, DE, ES, FR, IT, PL, CS, JA, ZH**.
* **Cross-Platform:** Works in Chrome, Firefox, Safari, Edge (Desktop & Mobile).

## 🔌 Hardware Setup

You need a standard ESP32 development board. Connect the TI CC-Chip (Target) as follows:

| ESP32 Pin | Target Pin (CC Debugger) | Function |
| :--- | :--- | :--- |
| **GPIO 4** | **DC** | Debug Clock |
| **GPIO 5** | **DD** | Debug Data |
| **GPIO 6** | **RESET** | RESET_N |
| GND | GND | Ground |
| 3.3V | VDD | Power (if target is not self-powered) |

> **Note:** The Pinout can be customized in `main.cpp` under the `// --- HARDWARE CONFIG ---` section. The Web GUI pinout diagram updates automatically based on your firmware settings.

## 🚀 Installation

1.  **Clone this repository:**
    ```bash
    git clone [https://github.com/DEIN_GITHUB_NAME/ESP32-CC-Tool-Pro.git](https://github.com/DEIN_GITHUB_NAME/ESP32-CC-Tool-Pro.git)
    ```
2.  **Open Project:** Open the folder in VS Code with the **PlatformIO** extension installed.
3.  **Flash ESP32:** Connect your ESP32 via USB and click the "Upload" arrow in PlatformIO.
    * *Note: The web interface files are embedded in the firmware PROGMEM, so no separate filesystem upload is required.*

## 📖 Usage

1.  **Power up** the ESP32.
2.  **Initial Setup (AP Mode):**
    * Connect to the WiFi Hotspot: `ESP32-CC-Flasher`
    * Password: `12345678`
3.  **Connect to Home WiFi (Station Mode):**
    * Open the Web GUI at `http://192.168.4.1` (default AP IP).
    * A popup or the settings logic will allow you to enter your local WiFi credentials.
    * Enter your SSID and Password and click **Save**. The ESP32 will reboot and connect to your local network.
4.  **Open the Tool:**
    * Navigate to `http://cc-tool.local` or use the IP address shown in your router or Serial Monitor.

## 🛠 Supported Chips

The tool implements the standard Texas Instruments Debug Interface (2-wire) and works with most 8051-based CC-chips.

* **CC253x Family:** CC2530, CC2531 (common in ZigBee sticks)
* **CC111x Family:** CC1110, CC1111 (Sub-1GHz)
* **CC254x Family:** (Likely compatible, untested)

## 📄 License

This project is licensed under the **GNU General Public License v3.0 (GPLv3)** - see the [LICENSE](LICENSE) file for details.

Based on work by [atc1441](https://github.com/atc1441/ESP_CC_Flasher), licensed under GPLv3.

---
*Disclaimer: This tool is intended for educational and development purposes. Use with care on production devices.*
