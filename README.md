# ESP32 Multi-Layered Smart Lock System  

This project implements a **multi-layered security system** using an ESP32 with support for **password authentication, RFID, and fingerprint recognition**. It also includes a **web-based lock control panel** for remote access via Wi-Fi.  

## ✨ Features  
- **Password Authentication** using a 4x4 Keypad  
- **RFID Authentication** with MFRC522 module  
- **Fingerprint Authentication** using Adafruit Fingerprint Sensor  
- **Layered Security Mode** (requires multiple authentication factors)  
- **Password Change Option** (via keypad)  
- **Fingerprint Enrollment** (add new fingerprints securely)  
- **EEPROM Storage** for password and settings persistence  
- **16x2 I2C LCD Display** for real-time feedback  
- **Relay Control** for lock mechanism  
- **Web Control Panel** accessible from browser on same Wi-Fi network  

---

## 🛠 Hardware Requirements  
- **ESP32** development board  
- **16x2 I2C LCD Display** (SDA: GPIO21, SCL: GPIO22)  
- **4x4 Keypad**  
- **MFRC522 RFID Module** (SDA: GPIO5, RST: GPIO17)  
- **Adafruit Fingerprint Sensor** (RX: GPIO16, TX: GPIO15)  
- **Relay Module** (connected to GPIO4)  
- Power supply and jumper wires  

---

## 📦 Libraries Used  
Make sure to install the following Arduino libraries:  
- `Wire.h`  
- `LiquidCrystal_I2C.h`  
- `Keypad.h`  
- `EEPROM.h`  
- `SPI.h`  
- `MFRC522.h`  
- `Adafruit_Fingerprint.h`  
- `WiFi.h`  
- `WebServer.h`  

---

## ⚙️ Setup Instructions  

1. **Clone/Download** this project and open in Arduino IDE (or PlatformIO).  
2. **Install required libraries** via Library Manager.  
3. **Connect hardware** according to the pin mappings below:  

| Component          | Pin (ESP32)     |  
|--------------------|-----------------|  
| LCD I2C (SDA, SCL) | 21, 22          |  
| Keypad Rows        | 13, 12, 14, 27  |  
| Keypad Cols        | 32, 33, 25, 26  |  
| RFID SDA, RST      | 5, 17           |  
| Fingerprint RX, TX | 16, 15          |  
| Relay              | 4               |  

4. **Set Wi-Fi credentials** inside the code:  
   ```cpp
   const char* ssid = "YourWiFiSSID";
   const char* password = "YourWiFiPassword";
   ```  

5. **Upload the code** to your ESP32.  
6. **Open Serial Monitor** at `115200 baud` for debugging.  
7. Once ESP32 connects to Wi-Fi, the **IP Address** will be displayed on the LCD.  
8. Access the **web control panel** by entering the ESP32’s IP address in a browser.  

---

## 🔑 Usage Guide  

- **Default Password:** `1111` (can be changed via keypad).  
- **Enter Password:** Use keypad, press `#` to confirm.  
- **Change Password:** Enter current password → Press `A` → Enter new password → Press `#`.  
- **Enable/Disable Layered Security:** Enter password `3DCD` → press `#`.  
- **Enroll Fingerprint:** Enter password `C7C9` → press `#` → follow LCD instructions.  
- **Unlock via Web:** Go to ESP32’s IP in browser → Click **Unlock** button.  

---

## 🔒 Security Modes  

- **Normal Mode** → Only one authentication method (Password, RFID, or Fingerprint) required.  
- **Layered Mode** → Requires two authentication methods (e.g., Password + RFID, Password + Fingerprint, or RFID + Fingerprint).  

---

## 🌐 Web Control Panel  

- Simple interface with **Unlock Button**.  
- Styled with CSS for modern look.  
- Uses JavaScript `fetch()` API to send commands to ESP32.  

---

## 📸 Future Improvements  

- Add mobile-friendly control panel with Lock/Unlock + Status display.  
- Implement **logging system** for failed/successful attempts.  
- Add **MQTT integration** for smart home systems.  
- Use **encrypted communication** for enhanced security.  

---

## 👨‍💻 Author  
Developed for **ESP32 Smart Security System** integrating multiple authentication methods.
Made by The Boys
