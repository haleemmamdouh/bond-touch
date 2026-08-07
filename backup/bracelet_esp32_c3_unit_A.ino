/*
  ====================================================================================
  BOND TOUCH — ESP32-C3 SUPERMINI (BRACELET UNIT A - AHMED)
  ====================================================================================
  EXACT PIN WIRING:
  - GPIO 1  --> Single-Color LED Light (+) [through 1kΩ resistor]
  - GPIO 3  --> Buzzer / Speaker (+) [PURE SQUARE-WAVE DRIVER]
  - GPIO 4  --> Touch Sensor SIG (Touch OUT)
  - GPIO 5  --> Vibrator Motor (+)
  - 3V3     --> Touch Sensor VCC
  - GND     --> Shared Ground
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEAdvertising.h>

const int PIN_LED      = 1;
const int PIN_BUZZER   = 3;
const int PIN_TOUCH    = 4;
const int PIN_VIBRATOR = 5;
const int PIN_LED_BUILTIN = 8;

#define DEVICE_BLE_NAME        "BondTouch_ESP32_A"
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "a3c87500-8ed3-4bdf-8a39-a01bebede295"

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastTouchTime = 0;
const unsigned long DEBOUNCE_DELAY = 500;

int curBrightness = 0;

void setLED(int brightness) {
  curBrightness = constrain(brightness, 0, 255);
  analogWrite(PIN_LED, curBrightness);
}

void setVibrator(int strength) {
  if (strength <= 0) {
    digitalWrite(PIN_VIBRATOR, LOW);
    analogWrite(PIN_VIBRATOR, 0);
  } else {
    digitalWrite(PIN_VIBRATOR, HIGH);
  }
}

// PURE SOFTWARE SQUARE-WAVE BUZZER DRIVER: WORKS ON EVERY BUZZER & NO LEDC CONFLICTS!
void makeBuzzerSound(int durationMs, int periodUs = 400) {
  unsigned long start = millis();
  while (millis() - start < (unsigned long)durationMs) {
    digitalWrite(PIN_BUZZER, HIGH);
    delayMicroseconds(periodUs);
    digitalWrite(PIN_BUZZER, LOW);
    delayMicroseconds(periodUs);
  }
  digitalWrite(PIN_BUZZER, LOW);
}

void noBuzz() {
  digitalWrite(PIN_BUZZER, LOW);
}

void fadeLED(int targetBrightness, int steps, int stepDelayMs) {
  int startBrightness = curBrightness;
  for (int i = 0; i <= steps; i++) {
    int v = startBrightness + ((targetBrightness - startBrightness) * i / steps);
    setLED(v);
    delay(stepDelayMs);
  }
}

// ── HEARTBEAT PULSE PATTERN ON INCOMING SIGNAL ──────────────────────────────────
void playHeartbeat() {
  setVibrator(255);
  setLED(255);
  makeBuzzerSound(120, 350); // Loud 1.4kHz tone
  setVibrator(0);

  fadeLED(60, 8, 8);
  delay(60);

  setVibrator(255);
  setLED(255);
  makeBuzzerSound(180, 300); // Higher 1.6kHz tone
  setVibrator(0);

  fadeLED(0, 15, 8);
  noBuzz();
}

// ── WAVEFORM PLAYER ────────────────────────────────────────────────────────────
void parseAndPlayWaveform(String dataStr) {
  int c1 = dataStr.indexOf(':');
  if (c1 == -1) return;

  int durationSec = constrain(dataStr.substring(0, c1).toInt(), 1, 10);
  int c2 = dataStr.indexOf(':', c1 + 1);

  String arrayStr = (c2 != -1) ? dataStr.substring(c2 + 1) : dataStr.substring(c1 + 1);

  int barMs     = (durationSec * 1000) / 16;
  int stepDelay = max(1, barMs / 40);

  int idx = 0, startPos = 0;

  while (idx < 16 && startPos < arrayStr.length()) {
    int commaPos = arrayStr.indexOf(',', startPos);
    if (commaPos == -1) commaPos = arrayStr.length();

    int intensity = constrain(arrayStr.substring(startPos, commaPos).toInt(), 0, 255);

    fadeLED(intensity, 5, stepDelay / 5);
    setVibrator(intensity > 80 ? 255 : 0);

    if (intensity > 100) {
      makeBuzzerSound(30, 400 - (intensity));
    } else {
      noBuzz();
    }

    delay(max(1, barMs - stepDelay * 4));

    idx++;
    startPos = commaPos + 1;
  }

  fadeLED(0, 15, 8);
  setVibrator(0);
  noBuzz();
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

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      digitalWrite(PIN_LED_BUILTIN, LOW);
      setLED(255); setVibrator(255); makeBuzzerSound(150, 300);
      setLED(0);   setVibrator(0);   noBuzz();
    }
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      digitalWrite(PIN_LED_BUILTIN, HIGH);
      setLED(0); setVibrator(0); noBuzz();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      processIncomingCommand(pCharacteristic->getValue().c_str());
    }
};

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_VIBRATOR, OUTPUT);
  pinMode(PIN_TOUCH, INPUT);
  pinMode(PIN_LED_BUILTIN, OUTPUT);

  // ENSURE EVERYTHING IS 100% OFF IMMEDIATELY AT STARTUP
  digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_VIBRATOR, LOW);
  digitalWrite(PIN_LED_BUILTIN, HIGH);
  setLED(0);

  Serial.println("⚡ BOND TOUCH UNIT A BOOTING...");

  // Quick 0.15s boot self-test (LED + Vibrator + Loud Buzzer)
  setLED(255); setVibrator(255); makeBuzzerSound(150, 350);
  setLED(0);   setVibrator(0);   noBuzz();

  BLEDevice::init(DEVICE_BLE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE
  );
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->start();

  Serial.println("⚡ BOND TOUCH UNIT A READY!");
}

void loop() {
  if (digitalRead(PIN_TOUCH) == HIGH) {
    if (millis() - lastTouchTime > DEBOUNCE_DELAY) {
      lastTouchTime = millis();

      setLED(255); setVibrator(255); makeBuzzerSound(120, 350);
      setLED(0);   setVibrator(0);   noBuzz();

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
