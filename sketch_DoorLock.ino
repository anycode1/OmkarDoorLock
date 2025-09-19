#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <EEPROM.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_Fingerprint.h>
#include <WiFi.h>
#include <WebServer.h>

#define EEPROM_SIZE 64
#define RFID_TIMEOUT 10000

// WiFi credentials
const char* ssid = "iPad"; //set to real name of wifi
const char* password = "12345678"; //set to real password of wifi
WebServer server(80);

// LCD (SDA = 21, SCL = 22 for ESP32 default I2C)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {32, 33, 25, 26};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String storedPassword = "1111"; // Default password
String inputPassword = "";
String newPassword = "";
bool changeMode = false;
bool layeredSecurity = false;
bool passwordVerified = false;
bool fingerprintVerified = false;
bool enrollMode = false;
int enrollStep = 0;
int enrollId = 1;
String lastRFIDTag = "";
unsigned long passwordEnteredTime = 0;
unsigned long fingerprintEnteredTime = 0;

// Relay pin
#define RELAY_PIN 4

// RFID
#define RST_PIN 17
#define SDA_PIN 5
MFRC522 mfrc522(SDA_PIN, RST_PIN);

// Fingerprint sensor
#define FINGERPRINT_RX_PIN 16
#define FINGERPRINT_TX_PIN 15
HardwareSerial fingerSerial(1);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&fingerSerial);

//Define Functions

void grantAccess() {
  lcd.clear();
  lcd.print("Access Granted");
  digitalWrite(RELAY_PIN, HIGH);
  delay(2000);
  digitalWrite(RELAY_PIN, LOW);
  passwordVerified = false;
  fingerprintVerified = false;
  lastRFIDTag = "";
  ESP.restart();
  showEnterPassword();
}

void checkPassword() {
  Serial.print("Checking password: ");
  Serial.println(inputPassword);
  Serial.print("Stored password: ");
  Serial.println(storedPassword);
  
  if (inputPassword == storedPassword) {
    grantAccess();
  }
  else {
    lcd.clear();
    lcd.print("Wrong Password");
    delay(2002);
    showEnterPassword();
  }
  inputPassword = "";
}

void showEnterPassword() {
  changeMode = false;
  enrollMode = false;
  inputPassword = "";
  lcd.clear();
  lcd.setCursor(0, 0);

  if (layeredSecurity) {
    lcd.print("Layered Mode ON");
  } else {
    lcd.print("Enter Password:");
  }
}

void showChangePassword() {
  newPassword = "";
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("New Password:");
}

void showEnrollMenu() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enroll Fingerprint");
  lcd.setCursor(0, 1);
  lcd.print("ID:");
  lcd.print(enrollId);
  lcd.print(" Press #");
}

void saveNewPassword() {
  if (newPassword.length() > 0) {
    storedPassword = newPassword;
    savePassword(storedPassword);
    lcd.clear();
    lcd.print("PW Changed");
    delay(2000);
    changeMode = false;
    showEnterPassword();
  } else {
    lcd.clear();
    lcd.print("No Change");
    delay(2000);
    changeMode = false;
    showEnterPassword();
  }
}

void loadPassword() {
  // Check if EEPROM has been initialized with a password
  bool eepromInitialized = false;
  for (int i = 0; i < 4; i++) {
    if (EEPROM.read(i) != 0 && EEPROM.read(i) != 255) {
      eepromInitialized = true;
      break;
    }
  }
  
  if (eepromInitialized) {
    // Load password from EEPROM
    String storedPass = "";
    for (int i = 0; i < 16; i++) {
      char c = EEPROM.read(i);
      if (c == 0 || c == 255) break;
      storedPass += c;
    }
    storedPassword = storedPass;
  } else {
    // Initialize EEPROM with default password
    savePassword("1111");
  }
  
  Serial.print("Loaded password: ");
  Serial.println(storedPassword);
}

void savePassword(String pass) {
  // Clear the password section of EEPROM
  for (int i = 0; i < 16; i++) {
    EEPROM.write(i, 0);
  }
  
  // Write the new password
  for (int i = 0; i < pass.length(); i++) {
    EEPROM.write(i, pass[i]);
  }
  EEPROM.commit();
  
  // Update the password variable
  storedPassword = pass;
}

void toggleLayeredSecurity() {
  layeredSecurity = !layeredSecurity;
  saveLayeredMode();
  lcd.clear();

  if (layeredSecurity) {
    lcd.print("Layered Mode ON");
  } else {
    lcd.print("Layered Mode OFF");
  }
  delay(2000);
}

void saveLayeredMode() {
  EEPROM.write(32, layeredSecurity ? 1 : 0);
  EEPROM.commit();
}

void loadLayeredMode() {
  byte mode = EEPROM.read(32);
  // If EEPROM value is invalid, default to false
  layeredSecurity = (mode == 1);
}

// Returns -1 if failed, otherwise returns ID #
int getFingerprintIDez() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -1;
  
  // found a match!
  Serial.print("Found ID #"); Serial.print(finger.fingerID);
  Serial.print(" with confidence of "); Serial.println(finger.confidence);
  return finger.fingerID;
}

uint8_t getFingerprintEnroll() {
  int p = -1;
  lcd.clear();
  lcd.print("Place finger");

  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        lcd.clear();
        lcd.print("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        lcd.clear();
        lcd.print("Comm error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        lcd.clear();
        lcd.print("Imaging error");
        break;
      default:
        lcd.clear();
        lcd.print("Unknown error");
        break;
    }
  }

  // OK success!
  p = finger.image2Tz(1);
  switch (p) {
    case FINGERPRINT_OK:
      lcd.clear();
      lcd.print("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      lcd.clear();
      lcd.print("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      lcd.clear();
      lcd.print("Comm error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      lcd.clear();
      lcd.print("Could not find features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      lcd.clear();
      lcd.print("Could not find features");
      return p;
    default:
      lcd.clear();
      lcd.print("Unknown error");
      return p;
  }
  
  lcd.clear();
  lcd.print("Remove finger");
  delay(2000);

  p = 0;
  while (p != FINGERPRINT_NOFINGER) {
    p = finger.getImage();
  }

  p = -1;
  lcd.clear();
  lcd.print("Place same finger again");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    switch (p) {
      case FINGERPRINT_OK:
        lcd.clear();
        lcd.print("Image taken");
        break;
      case FINGERPRINT_NOFINGER:
        break;
      case FINGERPRINT_PACKETRECIEVEERR:
        lcd.clear();
        lcd.print("Comm error");
        break;
      case FINGERPRINT_IMAGEFAIL:
        lcd.clear();
        lcd.print("Imaging error");
        break;
      default:
        lcd.clear();
        lcd.print("Unknown error");
        break;
    }
  }

  // OK success!
  p = finger.image2Tz(2);
  switch (p) {
    case FINGERPRINT_OK:
      lcd.clear();
      lcd.print("Image converted");
      break;
    case FINGERPRINT_IMAGEMESS:
      lcd.clear();
      lcd.print("Image too messy");
      return p;
    case FINGERPRINT_PACKETRECIEVEERR:
      lcd.clear();
      lcd.print("Comm error");
      return p;
    case FINGERPRINT_FEATUREFAIL:
      lcd.clear();
      lcd.print("Could not find features");
      return p;
    case FINGERPRINT_INVALIDIMAGE:
      lcd.clear();
      lcd.print("Could not find features");
      return p;
    default:
      lcd.clear();
      lcd.print("Unknown error");
      return p;
  }
  
  // OK converted!
  p = finger.createModel();
  if (p == FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Prints matched!");
  }
   else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    lcd.clear();
    lcd.print("Comm error");
    return p;
  } else if (p == FINGERPRINT_ENROLLMISMATCH) {
    lcd.clear();
    lcd.print("Fingerprints did not match");
    return p;
  } else {
    lcd.clear();
    lcd.print("Unknown error");
    return p;
  }
  
  p = finger.storeModel(enrollId);
  if (p == FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Stored!");
    delay(2000);
  } else if (p == FINGERPRINT_PACKETRECIEVEERR) {
    lcd.clear();
    lcd.print("Comm error");
    return p;
  } else if (p == FINGERPRINT_BADLOCATION) {
    lcd.clear();
    lcd.print("Could not store in that location");
    return p;
  } else if (p == FINGERPRINT_FLASHERR) {
    lcd.clear();
    lcd.print("Error writing to flash");
    return p;
  } else {
    lcd.clear();
    lcd.print("Unknown error");
    return p;
  }
  
  return true;
}

void enrollFingerprint() {
  lcd.clear();
  lcd.print("Starting enroll");
  lcd.setCursor(0, 1);
  lcd.print("ID: ");
  lcd.print(enrollId);
  delay(2000);
  
  uint8_t result = getFingerprintEnroll();
  if (result == FINGERPRINT_OK) {
    lcd.clear();
    lcd.print("Enroll Success!");
    delay(2000);
  } else {
    lcd.clear();
    lcd.print("Enroll Failed");
    delay(2000);
  }
  
  enrollMode = false;
  showEnterPassword();
}

// Web server functions
String getHTMLPage() {
  String html = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>ESP32 Lock Control</title>
  <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.4.0/css/all.min.css">
  <style>
    body {
      font-family: Arial, sans-serif;
      background: linear-gradient(135deg, #1a2a6c, #b21f1f, #fdbb2d);
      color: white;
      text-align: center;
      padding: 40px;
    }
    h1 {
      font-size: 2.5rem;
      margin-bottom: 20px;
    }
    .btn {
      display: inline-block;
      margin: 20px;
      padding: 20px 40px;
      font-size: 1.5rem;
      font-weight: bold;
      border: none;
      border-radius: 15px;
      cursor: pointer;
      color: white;
      transition: 0.3s;
      box-shadow: 0 6px 12px rgba(0,0,0,0.3);
    }
    .btn:hover {
      transform: translateY(-5px);
      box-shadow: 0 10px 18px rgba(0,0,0,0.4);
    }
    .btn-success {
      background: rgba(46, 204, 113, 0.8);
    }
    .btn-danger {
      background: rgba(231, 76, 60, 0.8);
    }
    .time-input {
      margin-top: 20px;
      font-size: 1.2rem;
      padding: 10px;
      border-radius: 10px;
      border: none;
      text-align: center;
      width: 150px;
    }
  </style>
</head>
<body>
  <h1><i class="fas fa-lock"></i> ESP32 Lock Control</h1>

  <button class="btn btn-success" onclick="sendCommand('unlock')">
    <i class="fas fa-unlock"></i> Unlock
  </button>
  <script>
    function sendCommand(cmd) {
      fetch('/' + cmd)
        .then(res => res.text())
        .then(data => console.log('Response:', data));
    }
  </script>
</body>
</html>
)=====";
  return html;
}

void setup() {
  Serial.begin(115200);

  EEPROM.begin(EEPROM_SIZE);
  loadPassword();
  loadLayeredMode();

  lcd.init();
  lcd.backlight();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  SPI.begin();
  mfrc522.PCD_Init();

  // Initialize fingerprint sensor
  fingerSerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX_PIN, FINGERPRINT_TX_PIN);
  delay(5);
  
  // Only show error if fingerprint sensor is not found
  if (!finger.verifyPassword()) {
    Serial.println("Did not find fingerprint sensor :(");
  }

  // Connect to WiFi
  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.print("Connecting 2 WF");
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.setCursor(0, 1);
    lcd.print(".");
    dots++;
    if (dots > 3) {
      lcd.setCursor(0, 1);
      lcd.print("    ");
      dots = 0;
    }
  }
  lcd.clear();
  lcd.print("WiFi Connected!");
  lcd.setCursor(0, 1);
  lcd.print("IP: ");
  lcd.print(WiFi.localIP());
  delay(2000);

  server.begin();
  
  Serial.println("HTTP server started");

  showEnterPassword();
}

void loop() {
  server.handleClient();
  
  // Check for timeout in layered mode
if (layeredSecurity && passwordVerified && 
    (millis() - passwordEnteredTime > RFID_TIMEOUT)) {
  passwordVerified = false;  // Reset if timeout
}

if (layeredSecurity && fingerprintVerified && 
    (millis() - fingerprintEnteredTime > RFID_TIMEOUT)) {
  fingerprintVerified = false;  // Reset if timeout
}

// In layered mode, require multiple authentication methods
if (layeredSecurity) {
  if (inputPassword == storedPassword) {
    passwordVerified = true;
    passwordEnteredTime = millis();
    // Wait for RFID or fingerprint
  }
  
  // Grant access only when two factors are verified
  if ((passwordVerified && fingerprintVerified) ||
      (passwordVerified && lastRFIDTag != "") ||
      (fingerprintVerified && lastRFIDTag != "")) {
    grantAccess();
  }
}

  char key = keypad.getKey();
  if (key) {
    Serial.print("Key pressed: ");
    Serial.println(key);
    
    if (key == '*') {
      if (changeMode) {
        newPassword = "";
      } else if (enrollMode) {
        enrollMode = false;
        enrollStep = 0;
        showEnterPassword();
      } else {
        inputPassword = "";
        passwordVerified = false;
      }
      lcd.clear();
      lcd.print("Cleared");
      delay(500);

      if (changeMode) {
        showChangePassword();
      } else if (enrollMode) {
        showEnrollMenu();
      } else {
        showEnterPassword();
      }
    }
    else if (key == '#') {
      if (changeMode) {
        saveNewPassword();
      } else if (enrollMode) {
        // Start fingerprint enrollment process
        enrollFingerprint();
      } else {
        Serial.print("Input password: ");
        Serial.println(inputPassword);
        
        if (inputPassword == "3DCD") {
          toggleLayeredSecurity();
          inputPassword = "";
          showEnterPassword();
        } 
        else if (inputPassword == "C7C9") {
          // Enter fingerprint enrollment mode
          enrollMode = true;
          enrollStep = 0;
          inputPassword = "";
          showEnrollMenu();
        }
        else if (!layeredSecurity) {
          checkPassword();
        } 
        else {
          if (inputPassword == storedPassword) {
            passwordVerified = true;
            passwordEnteredTime = millis();
            lcd.clear();
            lcd.print("Password OK");
            lcd.setCursor(0, 1);
            lcd.print("Scan RFID/Finger");
            inputPassword = "";
            delay(5000);
            
            // Check if we already have another authentication
            if (fingerprintVerified || lastRFIDTag != "") {
              grantAccess();
            }
          } else {
            lcd.clear();
            lcd.print("Wrong Password");
            delay(2000);
            showEnterPassword();
            passwordVerified = false;
          }
        }
      }
    }
    else if (key == 'A') {
      if (!changeMode && !layeredSecurity && !enrollMode) {
        if (inputPassword == storedPassword) {
          changeMode = true;
          inputPassword = "";
          showChangePassword();
        } else {
          lcd.clear();
          lcd.print("Wrong Password");
          delay(2000);
          showEnterPassword();
        }
      }
    }
    else if (key == 'B' && enrollMode) {
      // Decrement fingerprint ID
      if (enrollId > 1) enrollId--;
      showEnrollMenu();
    }
    else if (key == 'C' && enrollMode) {
      // Increment fingerprint ID
      if (enrollId < 127) enrollId++;
      showEnrollMenu();
    }
    else if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'D')) {
      if (changeMode) {
        newPassword += key;
        lcd.setCursor(0, 1);
        for (int i = 0; i < newPassword.length(); i++) lcd.print('*');
      } else if (enrollMode) {
        // Handle numeric input for fingerprint ID
        if (key >= '0' && key <= '9') {
          enrollId = enrollId * 10 + (key - '0');
          if (enrollId > 127) enrollId = 127;
          showEnrollMenu();
        }
      } else {
        inputPassword += key;
        lcd.setCursor(0, 1);
        for (int i = 0; i < inputPassword.length(); i++) lcd.print('*');
      }
    }
  }

  // RFID Handling
  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String tagID = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
      tagID += String(mfrc522.uid.uidByte[i], HEX);
    }
    tagID.toUpperCase();
    Serial.println("Scanned RFID: " + tagID);
    lastRFIDTag = tagID;

    if (layeredSecurity) {
      // Check if we already have another authentication
      if (passwordVerified || fingerprintVerified) {
        grantAccess();
      } else {
        lcd.clear();
        lcd.print("RFID Scanned");
        lcd.setCursor(0, 1);
        lcd.print("Need Pass/Finger");
        delay(6000);
        showEnterPassword();
      }
    } else {
      grantAccess();
    }

    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
  }
  
  // Fingerprint Handling (only if not in enroll mode)
  if (!enrollMode) {
    int fingerprintID = getFingerprintIDez();
    if (fingerprintID >= 0) {
      Serial.print("Fingerprint ID: ");
      Serial.println(fingerprintID);
      
      if (layeredSecurity) {
        fingerprintVerified = true;
        fingerprintEnteredTime = millis();
        lcd.clear();
        lcd.print("Fingerprint OK");
        lcd.setCursor(0, 1);
        lcd.print("Need Pass/RFID");
        delay(6000);
        
        // Check if we already have another authentication
        if (passwordVerified || lastRFIDTag != "") {
          grantAccess();
        } else {
          showEnterPassword();
        }
      } else {
        grantAccess();
      }
    }
  }
}