/*
  ====================================================================================
  BOND TOUCH — ESP32-C3 SUPERMINI (PURE BLE BLUETOOTH FIRMWARE)
  كود البلوتوث المباشر للـ ESP32-C3
  ====================================================================================
  
  WIRING FOR ESP32-C3 SUPERMINI:
  - RED Pin (R / +)     --> GPIO 0 (through 1000Ω / 1k resistor)
  - GREEN Pin (G / +)   --> GPIO 1 (through 1000Ω / 1k resistor)
  - GND Pin (-)         --> GND
  - TOUCH SIG           --> GPIO 4 (3-Pin Touch Sensor OUT)
  - TOUCH VCC           --> 3V3
  - TOUCH GND           --> GND
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEAdvertising.h>

const int PIN_RED   = 0; // Red LED (GPIO 0)
const int PIN_GREEN = 1; // Green LED (GPIO 1)
const int PIN_TOUCH = 4; // Touch Sensor Signal (GPIO 4)
const int PIN_LED_BUILTIN = 8; // Built-in status LED

#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "a3c87500-8ed3-4bdf-8a39-a01bebede295"

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastTouchTime = 0;
const unsigned long DEBOUNCE_DELAY = 500;

int curR = 0, curG = 0;

void setLED(int r, int g) {
  curR = constrain(r, 0, 255);
  curG = constrain(g, 0, 255);

  analogWrite(PIN_RED, curR);
  analogWrite(PIN_GREEN, curG);
}

void fadeLED(int targetR, int targetG, int steps, int stepDelayMs) {
  int startR = curR, startG = curG;

  for (int i = 0; i <= steps; i++) {
    int r = startR + ((targetR - startR) * i / steps);
    int g = startG + ((targetG - startG) * i / steps);
    setLED(r, g);
    delay(stepDelayMs);
  }
}

void processIncomingCommand(String input) {
  input.trim();
  if (input.length() == 0) return;

  if (input.startsWith("WAVE:")) {
    parseAndPlayWaveform(input.substring(5));
  } else if (input == "TOUCH" || input == "V") {
    playHeartbeat();
  }
}

void playHeartbeat() {
  fadeLED(255, 0, 15, 10);
  delay(100);
  fadeLED(50, 0, 15, 10);
  delay(80);
  fadeLED(255, 0, 15, 10);
  delay(150);
  fadeLED(0, 0, 20, 10);
}

void parseAndPlayWaveform(String dataStr) {
  int c1 = dataStr.indexOf(':');
  if (c1 == -1) return;

  int durationSec = constrain(dataStr.substring(0, c1).toInt(), 1, 10);

  int c2 = dataStr.indexOf(':', c1 + 1);
  int baseR = 255, baseG = 65;

  String arrayStr = "";

  if (c2 != -1) {
    String colorStr = dataStr.substring(c1 + 1, c2);
    arrayStr = dataStr.substring(c2 + 1);

    int comma1 = colorStr.indexOf(',');
    int comma2 = colorStr.indexOf(',', comma1 + 1);
    if (comma1 != -1) {
      baseR = colorStr.substring(0, comma1).toInt();
      if (comma2 != -1) {
        baseG = colorStr.substring(comma1 + 1, comma2).toInt();
      } else {
        baseG = colorStr.substring(comma1 + 1, comma2).toInt();
      }
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

    int intensity = arrayStr.substring(startPos, commaPos).toInt();
    intensity = constrain(intensity, 0, 255);

    int targetR = (baseR * intensity) / 255;
    int targetG = (baseG * intensity) / 255;

    fadeLED(targetR, targetG, 10, stepDelay);

    idx++;
    startPos = commaPos + 1;
  }

  fadeLED(0, 0, 15, 8);
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      digitalWrite(PIN_LED_BUILTIN, LOW);
      setLED(255, 255); delay(200); // Yellow flash
      setLED(0, 0);
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      digitalWrite(PIN_LED_BUILTIN, HIGH);
      setLED(0, 0);
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String input = pCharacteristic->getValue().c_str();
      processIncomingCommand(input);
    }
};

void setup() {
  Serial.begin(115200);

  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_TOUCH, INPUT);
  pinMode(PIN_LED_BUILTIN, OUTPUT);
  digitalWrite(PIN_LED_BUILTIN, HIGH);

  // Self-test LED sweep on boot: Red -> Green -> Yellow
  setLED(255, 0);   delay(300); // 🔴
  setLED(0, 255);   delay(300); // 🟢
  setLED(255, 255); delay(300); // 🟡
  setLED(0, 0);

  // Initialize BLE with name broadcast
  BLEDevice::init("BondTouch_ESP32");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_TX,
                        BLECharacteristic::PROPERTY_NOTIFY
                      );
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
                                           CHARACTERISTIC_UUID_RX,
                                           BLECharacteristic::PROPERTY_WRITE
                                         );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();

  // Full BLE Advertising setup for mobile phones & Windows Bluetooth
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  pAdvertising->start();

  Serial.println("⚡ BOND TOUCH PURE BLE READY!");
}

void loop() {
  // Read Physical Touch Sensor
  if (digitalRead(PIN_TOUCH) == HIGH) {
    if (millis() - lastTouchTime > DEBOUNCE_DELAY) {
      lastTouchTime = millis();

      setLED(255, 100);
      delay(120);
      setLED(0, 0);

      if (deviceConnected && pTxCharacteristic != NULL) {
        pTxCharacteristic->setValue("TOUCH");
        pTxCharacteristic->notify();
      }
    }
  }

  if (!deviceConnected && oldDeviceConnected) {
    delay(500);
    pServer->startAdvertising();
    oldDeviceConnected = deviceConnected;
  }
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  delay(10);
}
