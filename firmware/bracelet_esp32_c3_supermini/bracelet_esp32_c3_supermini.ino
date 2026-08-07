/*
  ====================================================================================
  BOND TOUCH — ESP32-C3 SUPERMINI BLE FIRMWARE
  كود الـ ESP32-C3 SuperMini الصغير جداً: تحكم بالـ RGB واللمس والبلوتوث
  ====================================================================================

  WIRING FOR ESP32-C3 SUPERMINI:
  ------------------------------------------------------------------------------------
  RGB LED (Common Cathode - 4 Pins):
    - RED Pin (R)    --> GPIO 0  (through 220Ω resistor)
    - GREEN Pin (G)  --> GPIO 1  (through 220Ω resistor)
    - BLUE Pin (B)   --> GPIO 3  (through 220Ω resistor)
    - GND Pin (-)    --> GND

  TOUCH BUTTON MODULE (3 Pins: VCC, GND, SIG):
    - VCC Pin        --> 3V3
    - GND Pin        --> GND
    - SIG Pin        --> GPIO 4

  ONBOARD LED:
    - Built-in LED   --> GPIO 8
  ====================================================================================
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// PIN DEFINITIONS FOR ESP32-C3 SUPERMINI
const int PIN_RED   = 0;
const int PIN_GREEN = 1;
const int PIN_BLUE  = 3;
const int PIN_TOUCH = 4;
const int PIN_LED_BUILTIN = 8; // Built-in LED on ESP32-C3 SuperMini

#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "a3c87500-8ed3-4bdf-8a39-a01bebede295"

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastTouchTime = 0;
const unsigned long DEBOUNCE_DELAY = 500;

int curR = 0, curG = 0, curB = 0;

void setRGB(int r, int g, int b) {
  curR = constrain(r, 0, 255);
  curG = constrain(g, 0, 255);
  curB = constrain(b, 0, 255);

  analogWrite(PIN_RED, curR);
  analogWrite(PIN_GREEN, curG);
  analogWrite(PIN_BLUE, curB);
}

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

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      digitalWrite(PIN_LED_BUILTIN, LOW); // Onboard LED active LOW on SuperMini
      setRGB(0, 255, 255); // Cyan Connection Flash
      delay(200);
      setRGB(0, 0, 0);
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      digitalWrite(PIN_LED_BUILTIN, HIGH);
      setRGB(0, 0, 0);
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        String input = String(rxValue.c_str());
        input.trim();

        if (input.startsWith("WAVE:")) {
          parseAndPlayWaveform(input.substring(5));
        } else if (input == "TOUCH" || input == "V") {
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
      int c1 = dataStr.indexOf(':');
      if (c1 == -1) return;

      int durationSec = constrain(dataStr.substring(0, c1).toInt(), 1, 10);

      int c2 = dataStr.indexOf(':', c1 + 1);
      int baseR = 255, baseG = 65, baseB = 108;

      String arrayStr = "";

      if (c2 != -1) {
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

        int intensity = arrayStr.substring(startPos, commaPos).toInt();
        intensity = constrain(intensity, 0, 255);

        int targetR = (baseR * intensity) / 255;
        int targetG = (baseG * intensity) / 255;
        int targetB = (baseB * intensity) / 255;

        fadeRGB(targetR, targetG, targetB, 10, stepDelay);

        idx++;
        startPos = commaPos + 1;
      }

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
  digitalWrite(PIN_LED_BUILTIN, HIGH); // Default off

  // Power-on RGB self-test sweep
  setRGB(255, 0, 0); delay(200);   // Red
  setRGB(0, 255, 0); delay(200);   // Green
  setRGB(0, 0, 255); delay(200);   // Blue
  setRGB(0, 0, 0);

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
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("⚡ BOND TOUCH ESP32-C3 SUPERMINI BLE READY!");
}

void loop() {
  if (digitalRead(PIN_TOUCH) == HIGH) {
    if (millis() - lastTouchTime > DEBOUNCE_DELAY) {
      lastTouchTime = millis();

      setRGB(255, 50, 120);
      delay(120);
      setRGB(0, 0, 0);

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
