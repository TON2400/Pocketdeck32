# PocketDeck — ESP32 Mini Cyberdeck

PocketDeck is a self-contained, offline-capable Mini Cyberdeck firmware for the **ESP32 DevKit V1 (ESP32-WROOM-32)** featuring a **0.96" SSD1306 I2C OLED (128x64)**, a **4x4 Matrix Keypad**, a **NEC Infrared Remote Control**, and a **Physical Input Mode Selector Button**.

---

## 1. Hardware Pin Mapping

| Component | ESP32 GPIO | Notes |
|---|---|---|
| **OLED SDA** | `GPIO21` | Hardware I2C SDA (3.3V, 400kHz) |
| **OLED SCL** | `GPIO22` | Hardware I2C SCL |
| **IR Receiver OUT** | `GPIO23` | 38kHz NEC IR Receiver output |
| **Mode Selector Button** | `GPIO4` | Momentary button to `GND` (internal `INPUT_PULLUP`) |
| **Keypad Row 1** | `GPIO16` | Direct matrix row 1 |
| **Keypad Row 2** | `GPIO17` | Direct matrix row 2 |
| **Keypad Row 3** | `GPIO18` | Direct matrix row 3 |
| **Keypad Row 4** | `GPIO19` | Direct matrix row 4 |
| **Keypad Column 1** | `GPIO25` | Direct matrix column 1 (`INPUT_PULLUP`) |
| **Keypad Column 2** | `GPIO26` | Direct matrix column 2 (`INPUT_PULLUP`) |
| **Keypad Column 3** | `GPIO27` | Direct matrix column 3 (`INPUT_PULLUP`) |
| **Keypad Column 4** | `GPIO32` | Direct matrix column 4 (`INPUT_PULLUP`) |

All modules share a common `GND` and `3.3V` rail from the ESP32 DevKit V1.

---

## 2. Input Architecture & Mode Switching

- **Mode Selector (`GPIO4`)**: Toggles between **Keypad Mode (`[KP]`)** and **IR Remote Mode (`[IR]`)** at any time with hardware debouncing (45 ms), falling-edge detection, and a 250 ms cooldown guard.
- **Strict Device Isolation**: Input from the inactive source is ignored and flushed so inactive hardware never causes unintended actions or OLED redraws.

### 4x4 Matrix Keypad Mapping

| Key | Navigation / System | Multi-Tap Text Entry | Calculator |
|---|---|---|---|
| `1` | Key 1 | `. , ? ! 1 - _ / :` | `1` |
| `2` | Up | `A B C 2` | `2` |
| `3` | Key 3 | `D E F 3` | `3` |
| `4` | Left | `G H I 4` | `4` |
| `5` | Select / Confirm | `J K L 5` | `5` |
| `6` | Right | `M N O 6` | `6` |
| `7` | Key 7 | `P Q R S 7` | `7` |
| `8` | Down | `T U V 8` | `8` |
| `9` | Key 9 | `W X Y Z 9` | `9` |
| `0` | Key 0 | `Space 0` | `0` |
| `*` | Back / Cancel | Backspace (Long press: Clear line) | Backspace / Clear / Exit |
| `#` | Select / Confirm | Confirm / Save | Evaluate (`=`) |
| `A` | Up | Cycle Case (`abc` / `ABC` / `123`) | Addition (`+`) |
| `B` | Down | Scroll Terminal Output | Subtraction (`-`) |
| `C` | Back (Long press: Home) | Cancel / Exit Editor | Multiply / Divide (`*` / `/`) |
| `D` | Context Menu / Rescan | Confirm / Save | Decimal Point (`.`) |

### NEC Car MP3 IR Remote Mapping

| Button | Hex Code | Action |
|---|---|---|
| `CH-` | `0x00FFA25D` | Back / Cancel |
| `CH` | `0x00FF629D` | Home (Main Menu) |
| `CH+` | `0x00FFE21D` | Select / Enter |
| `PREV` | `0x00FF22DD` | Left / Previous |
| `NEXT` | `0x00FF02FD` | Right / Next |
| `PLAY/PAUSE` | `0x00FFC23D` | Confirm / Pause |
| `VOL-` | `0x00FFE01F` | Down |
| `VOL+` | `0x00FFA857` | Up |
| `EQ` | `0x00FF906F` | Context Menu / Rescan / Info |
| `0`–`9` | `0x00FF6897` etc. | Numeric Input / App Shortcuts (`1`–`9`) |
| `100+` | `0x00FF9867` | `*` (Backspace / Decimal) |
| `200+` | `0x00FFB04F` | `#` (Confirm / Evaluate) |

---

## 3. Applications & Games

1. **Terminal**: Local shell with multi-tap input and scrollable output (`help`, `clear`, `info`, `uptime`, `version`, `wifi`, `ble`, `notes`, `games`, `ls`, `reboot`, `exit`).
2. **Wi-Fi Scanner**: Asynchronous 2.4 GHz scanner showing SSID, RSSI, Channel, and Encryption status with a detail view.
3. **BLE Explorer**: On-demand 2-second burst BLE scanner showing Device Name, MAC Address, and RSSI.
4. **Notes**: Persistent note creation, multi-tap editing, viewing, and deletion backed by LittleFS (`/notes`).
5. **File Manager**: Browse LittleFS directories (`/` and `/notes`), view text files, create new files, delete files, and inspect file/filesystem stats.
6. **Calculator**: Floating-point arithmetic (`+`, `-`, `*`, `/`), decimals, backspace/clear, and division-by-zero / overflow protection.
7. **Game Center**: Modular game launcher featuring **Snake**, **Pong**, and **Cyber Reflex (Reaction Game)**.
8. **System Monitor**: Live dual-page telemetry (Uptime, Free/Min Heap, Input Mode, Wi-Fi/BLE status, CPU frequency, LittleFS usage, SDK version).
9. **Settings**: Persistent NVS settings (Mode Indicator badge, Boot Input Mode, OLED Contrast/Brightness, Display Inversion, Screen Timeout, System Info, Factory Reset, Reboot).

---

## 4. Building & Flashing

### Using Arduino CLI

```powershell
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=huge_app .
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32:PartitionScheme=huge_app .
```

### Using PlatformIO

```powershell
pio run
pio run --target upload
```
