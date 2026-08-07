/*
  ====================================================================================
  BOND TOUCH — ESP32 WIRELESS BLE FIRMWARE (RGB LIGHT + TOUCH SENSOR)
  كود الـ ESP32 اللاسلكي: استقبال الإشارة عبر البلوتوث وتشعيل الـ RGB LED واللمس
  ====================================================================================

  WIRING FOR ESP32 DEV BOARD:
  ------------------------------------------------------------------------------------
  RGB LED (3 Pins + Common Ground):
    - RED Pin    --> GPIO 25  (through 220Ω resistor)
    - GREEN Pin  --> GPIO 26  (through 220Ω resistor)
    - BLUE Pin   --> GPIO 27  (through 220Ω resistor)
    - GND Pin    --> GND      (Common Cathode Ground)

  TOUCH BUTTON MODULE (3 Pins: VCC, GND, SIG):
    - VCC Pin    --> 3.3V  (or VIN)
    - GND Pin    --> GND
    - SIG Pin    --> GPIO 4   (Touch Signal Input)

  POWER:
    - Micro-USB cable plugged into phone charger or laptop / power bank
  ====================================================================================
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// PIN DEFINITIONS FOR ESP32
const int PIN_RED   = 25;
const int PIN_GREEN = 26;
const int PIN_BLUE  = 27;
const int PIN_TOUCH = 4;
const int PIN_LED_BUILTIN = 2; // Built-in blue LED on ESP32

// BLE UUIDs
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "a3c87500-8ed3-4bdf-8a39-a01bebede295"

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// Touch sensor debouncing
unsigned long lastTouchTime = 0;
const unsigned long DEBOUNCE_DELAY = 500; // ms

// Smooth RGB color state
int curR = 0, curG = 0, curB = 0;

// Set RGB LED Color (0 to 255 for each channel)
void setRGB(int r, int g, int b) {
  curR = constrain(r, 0, 255);
  curG = constrain(g, 0, 255);
  curB = constrain(b, 0, 255);

  analogWrite(PIN_RED, curR);
  analogWrite(PIN_GREEN, curG);
  analogWrite(PIN_BLUE, curB);
}

// Fade smoothly to a target RGB color
void fadeRGB(int targetR, int targetG, int targetB, int steps, int stepDelayMs) {
  int startR = curR, startG = curG, startB = curB;

  for (int i = 0; i <= steps; i++) {
    int r = startR + ((targetR - startR) * i / steps);
    int g = startG + ((targetG - startG) * i / steps);
    int b = startB + ((targetB - startB) * i / steps);
    setRGB(r, g, b);
    delay(stepDelayMs);
  }
}

// BLE Server Callbacks
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      digitalWrite(PIN_LED_BUILTIN, HIGH); // Light built-in LED when Bluetooth connected
      // Connection chime / color flash (Cyan)
      setRGB(0, 255, 255);
      delay(200);
      setRGB(0, 0, 0);
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      digitalWrite(PIN_LED_BUILTIN, LOW);
      setRGB(0, 0, 0);
    }
};

// BLE RX Callback (Receives Waveforms & Touches from Mobile Phone over Bluetooth)
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        String input = String(rxValue.c_str());
        input.trim();

        // Check if waveform received: WAVE:dur:r,g,b:lightCSV:soundCSV
        if (input.startsWith("WAVE:")) {
          parseAndPlayWaveform(input.substring(5));
        } else if (input == "TOUCH" || input == "V") {
          // Standard Heartbeat Pulse (Red Glow)
          playHeartbeat();
        }
      }
    }

    void playHeartbeat() {
      fadeRGB(255, 0, 80, 15, 10);
      delay(100);
      fadeRGB(40, 0, 15, 15, 10);
      delay(80);
      fadeRGB(255, 0, 80, 15, 10);
      delay(150);
      fadeRGB(0, 0, 0, 20, 10);
    }

    void parseAndPlayWaveform(String dataStr) {
      // Format: durationSec:r,g,b:lightCSV
      int c1 = dataStr.indexOf(':');
      if (c1 == -1) return;

      int durationSec = constrain(dataStr.substring(0, c1).toInt(), 1, 10);

      int c2 = dataStr.indexOf(':', c1 + 1);
      int baseR = 255, baseG = 65, baseB = 108; // Default Pinkish-Red

      String arrayStr = "";

      if (c2 != -1) {
        // Has RGB color prefix: r,g,b
        String colorStr = dataStr.substring(c1 + 1, c2);
        arrayStr = dataStr.substring(c2 + 1);

        int comma1 = colorStr.indexOf(',');
        int comma2 = colorStr.indexOf(',', comma1 + 1);
        if (comma1 != -1 && comma2 != -1) {
          baseR = colorStr.substring(0, comma1).toInt();
          baseG = colorStr.substring(comma1 + 1, comma2).toInt();
          baseB = colorStr.substring(comma2 + 1).toInt();
        }
      } else {
        arrayStr = dataStr.substring(c1 + 1);
      }

      int barMs = (durationSec * 1000) / 16;
      int stepDelay = max(1, barMs / 40);

      int idx = 0;
      int startPos = 0;

      while (idx < 16 && startPos < arrayStr.length()) {
        int commaPos = arrayStr.indexOf(',', startPos);
        if (commaPos == -1) commaPos = arrayStr.length();

        int intensity = arrayStr.substring(startPos, commaPos).toInt(); // 0 to 255
        intensity = constrain(intensity, 0, 255);

        // Scale RGB color by intensity
        int targetR = (baseR * intensity) / 255;
        int targetG = (baseG * intensity) / 255;
        int targetB = (baseB * intensity) / 255;

        // Smooth fade to this bar's color & brightness
        fadeRGB(targetR, targetG, targetB, 10, stepDelay);

        idx++;
        startPos = commaPos + 1;
      }

      // Smooth fade out
      fadeRGB(0, 0, 0, 15, 8);
    }
};

void setup() {
  Serial.begin(115200);

  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);
  pinMode(PIN_TOUCH, INPUT);
  pinMode(PIN_LED_BUILTIN, OUTPUT);

  // Power-on self-test RGB rainbow sweep
  setRGB(255, 0, 0); delay(200);   // Red
  setRGB(0, 255, 0); delay(200);   // Green
  setRGB(0, 0, 255); delay(200);   // Blue
  setRGB(255, 255, 255); delay(200); // White
  setRGB(0, 0, 0);                 // Off

  // Create BLE Device
  BLEDevice::init("BondTouch_ESP32");

  // Create BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create TX Characteristic (ESP32 -> Phone notification for Touch Sensor)
  pTxCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
  pTxCharacteristic->addDescriptor(new BLE2902());

  // Create RX Characteristic (Phone -> ESP32 incoming waveforms)
  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                                           CHARACTERISTIC_UUID_RX,
                                           BLECharacteristic::PROPERTY_WRITE
                                         );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start BLE Service & Advertising
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // Functions that help with iPhone connections
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("==================================================");
  Serial.println("⚡ BOND TOUCH ESP32 BLE FIRMWARE INITIALIZED!");
  Serial.println("📡 Advertising as: BondTouch_ESP32");
  Serial.println("==================================================");
}

void loop() {
  // 1. Read Physical 3-Pin Touch Sensor (GPIO 4)
  if (digitalRead(PIN_TOUCH) == HIGH) { // TTP223 outputs HIGH on touch
    if (millis() - lastTouchTime > DEBOUNCE_DELAY) {
      lastTouchTime = millis();

      Serial.println("👉 Touch Sensor Pressed on ESP32!");

      // Brief local visual touch feedback (Soft Pink Pulse)
      setRGB(255, 50, 120);
      delay(120);
      setRGB(0, 0, 0);

      // Send BLE Notification to Phone!
      if (deviceConnected && pTxCharacteristic != NULL) {
        pTxCharacteristic->setValue("TOUCH");
        pTxCharacteristic->notify();
        Serial.println("📡 Sent TOUCH notification over BLE to phone!");
      }
    }
  }

  // Handle re-advertising on disconnect
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("📡 Restarted BLE Advertising...");
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  delay(10);
}
