/*
  ====================================================================================
  BOND TOUCH — ESP32-C3 SUPERMINI (UNIT A - DIRECT SEQUENCE & RGB DRIVER)
  ====================================================================================
  EXACT PIN WIRING:
  - GPIO 0  --> 4-Pin RGB LED: RED Pin (R) [through 1kΩ resistor]
  - GPIO 1  --> 4-Pin RGB LED: GREEN Pin (G) [through 1kΩ resistor]
  - GPIO 2  --> 4-Pin RGB LED: BLUE Pin (B) [through 1kΩ resistor]
  - GND     --> 4-Pin RGB LED: Longest Pin (Common Cathode GND)
  - GPIO 3  --> Buzzer / Speaker (+) [PURE SOFTWARE SQUARE-WAVE]
  - GPIO 4  --> Touch Sensor SIG (Touch OUT)
  - GPIO 21 --> Vibrator Motor (+) [ONLY VIBRATES ON INCOMING TOUCH]
  - 3V3     --> Touch Sensor VCC
  - GND     --> Shared Ground
  ====================================================================================
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEAdvertising.h>

const int PIN_LED_R       = 0;
const int PIN_LED_G       = 1;
const int PIN_LED_B       = 2;
const int PIN_BUZZER      = 3;
const int PIN_TOUCH       = 4;
const int PIN_VIBRATOR    = 21;
const int PIN_LED_BUILTIN = 8;

#define DEVICE_BLE_NAME        "BondTouch_ESP32_A"
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "a3c87500-8ed3-4bdf-8a39-a01bebede295"

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
bool deviceConnected    = false;
bool oldDeviceConnected = false;
unsigned long lastTouchTime   = 0;
const unsigned long DEBOUNCE_DELAY = 800;

int curR = 0, curG = 0, curB = 0;

void setRGB(int r, int g, int b) {
  curR = constrain(r, 0, 255);
  curG = constrain(g, 0, 255);
  curB = constrain(b, 0, 255);
  analogWrite(PIN_LED_R, curR);
  analogWrite(PIN_LED_G, curG);
  analogWrite(PIN_LED_B, curB);
}

void setVibrator(bool on) {
  digitalWrite(PIN_VIBRATOR, on ? HIGH : LOW);
}

void makeBuzzerSound(int durationMs, int pitchHz = 1200) {
  if (pitchHz <= 0) pitchHz = 1200;
  int periodUs = 1000000 / (pitchHz * 2);
  unsigned long start = millis();
  while ((int)(millis() - start) < durationMs) {
    digitalWrite(PIN_BUZZER, HIGH); delayMicroseconds(periodUs);
    digitalWrite(PIN_BUZZER, LOW);  delayMicroseconds(periodUs);
  }
  digitalWrite(PIN_BUZZER, LOW);
}

void noBuzz() { digitalWrite(PIN_BUZZER, LOW); }

void fadeRGB(int tR, int tG, int tB, int steps, int stepMs) {
  int sR = curR, sG = curG, sB = curB;
  for (int i = 0; i <= steps; i++) {
    setRGB(sR + (tR-sR)*i/steps, sG + (tG-sG)*i/steps, sB + (tB-sB)*i/steps);
    delay(stepMs);
  }
}

// ── DIRECT ZERO-DELAY MULTI-STEP SEQUENCE PARSER ─────────────────────────────
// Format: "SEQ:RGB:255,0,0:200|BEEP:1200:100|DELAY:100|VIB:150|RGB:0,0,255:200"
void parseAndPlaySequence(String seqData) {
  int pos = 0;
  while (pos < seqData.length()) {
    int pipe = seqData.indexOf('|', pos);
    if (pipe == -1) pipe = seqData.length();

    String item = seqData.substring(pos, pipe);
    item.trim();

    if (item.startsWith("RGB:")) {
      int c1 = item.indexOf(':', 4);
      if (c1 != -1) {
        String colStr = item.substring(4, c1);
        int durMs = item.substring(c1 + 1).toInt();
        int cm1 = colStr.indexOf(',');
        int cm2 = colStr.indexOf(',', cm1 + 1);
        if (cm1 != -1 && cm2 != -1) {
          int r = colStr.substring(0, cm1).toInt();
          int g = colStr.substring(cm1 + 1, cm2).toInt();
          int b = colStr.substring(cm2 + 1).toInt();
          setRGB(r, g, b);
          delay(durMs);
          setRGB(0, 0, 0); // INSTANT OFF
        }
      }
    }
    else if (item.startsWith("BEEP:")) {
      int c1 = item.indexOf(':', 5);
      if (c1 != -1) {
        int pitch = item.substring(5, c1).toInt();
        int durMs = item.substring(c1 + 1).toInt();
        makeBuzzerSound(durMs, pitch);
      }
    }
    else if (item.startsWith("DELAY:")) {
      int durMs = item.substring(6).toInt();
      delay(durMs);
    }
    else if (item.startsWith("VIB:")) {
      int durMs = item.substring(4).toInt();
      setVibrator(true);
      delay(durMs);
      setVibrator(false);
    }

    pos = pipe + 1;
  }
  setRGB(0, 0, 0);
  setVibrator(false);
  noBuzz();
}

void playHeartbeat() {
  setVibrator(true); setRGB(255,0,0);
  makeBuzzerSound(80, 1400);
  setVibrator(false); fadeRGB(40,0,0, 6, 6); delay(60);
  setVibrator(true); setRGB(255,0,0);
  makeBuzzerSound(100, 1200);
  setVibrator(false); fadeRGB(0,0,0, 12, 7);
  setVibrator(false); noBuzz();
}

void playLocalTouch() {
  setVibrator(true); setRGB(255,255,255);
  makeBuzzerSound(40, 1500);
  setVibrator(false); fadeRGB(0,0,0, 8, 5);
  setVibrator(false); noBuzz();
}

void playConnectedBlueFlicker() {
  setVibrator(true);
  for (int i = 0; i < 3; i++) {
    setRGB(0,0,255); makeBuzzerSound(40, 1200); delay(40);
    setRGB(0,0,0);   delay(40);
  }
  setRGB(0,0,255); delay(120); fadeRGB(0,0,0, 10, 6);
  setVibrator(false); noBuzz();
}

void playDisconnectedBlueStrobe() {
  setVibrator(false); noBuzz();
  unsigned long start = millis();
  while (millis() - start < 5000) {
    setRGB(0,0,255); delay(100);
    setRGB(0,0,0);   delay(100);
  }
  setRGB(0,0,0);
}

void playPartnerConnectedGreenFlash() {
  setVibrator(true); setRGB(0,255,0);
  makeBuzzerSound(80, 1600); delay(50); makeBuzzerSound(100, 1800);
  fadeRGB(0,0,0, 15, 8);
  setVibrator(false); noBuzz();
}

void playDeliveryFail() {
  for (int i = 0; i < 3; i++) {
    setVibrator(true); setRGB(255, 0, 0);
    makeBuzzerSound(80, 600);
    setVibrator(false); setRGB(0, 0, 0);
    delay(120);
  }
  setRGB(0, 0, 0);
}

void playBootChime() {
  setVibrator(true);
  setRGB(255,0,0); delay(40);
  setRGB(0,255,0); delay(40);
  setRGB(0,0,255); delay(40);
  makeBuzzerSound(60, 1000); makeBuzzerSound(60, 1500);
  setVibrator(false); setRGB(0,0,0); noBuzz();
}

void parseAndPlayWaveform(String dataStr) {
  int c1 = dataStr.indexOf(':'); if (c1 == -1) return;
  int durationSec = constrain(dataStr.substring(0, c1).toInt(), 1, 10);
  int c2 = dataStr.indexOf(':', c1+1); if (c2 == -1) return;
  String colorStr = dataStr.substring(c1+1, c2);
  int c3 = dataStr.indexOf(':', c2+1);
  String arrayStr = (c3 != -1) ? dataStr.substring(c3+1) : dataStr.substring(c2+1);

  int baseR = 255, baseG = 0, baseB = 0;
  int cm1 = colorStr.indexOf(',');
  if (cm1 != -1) {
    baseR = colorStr.substring(0, cm1).toInt();
    int cm2 = colorStr.indexOf(',', cm1+1);
    if (cm2 != -1) { baseG = colorStr.substring(cm1+1, cm2).toInt(); baseB = colorStr.substring(cm2+1).toInt(); }
    else            { baseG = colorStr.substring(cm1+1).toInt(); baseB = 0; }
  }

  int barMs = (durationSec*1000)/16;
  int holdMs = max(1, barMs-10);
  int idx = 0, pos = 0;
  while (idx < 16 && pos < arrayStr.length()) {
    int cp = arrayStr.indexOf(',', pos); if (cp == -1) cp = arrayStr.length();
    int intensity = constrain(arrayStr.substring(pos, cp).toInt(), 0, 255);
    float f = intensity / 255.0f;
    fadeRGB(baseR*f, baseG*f, baseB*f, 4, 3);
    setVibrator(intensity > 90);
    if (intensity > 110) { int p = constrain(420-intensity, 240, 420); makeBuzzerSound(20, p); }
    else { noBuzz(); }
    delay(holdMs); idx++; pos = cp+1;
  }
  fadeRGB(0,0,0, 10, 6); setVibrator(false); noBuzz();
}

void processIncomingCommand(String input) {
  input.trim();
  if (input.length() == 0) return;
  Serial.print("RX: "); Serial.println(input);

  if (input.startsWith("SEQ:")) {
    parseAndPlaySequence(input.substring(4));
  }
  else if (input.startsWith("WAVE:")) {
    parseAndPlayWaveform(input.substring(5));
  }
  else if (input == "TOUCH" || input == "V" || input == "A") playHeartbeat();
  else if (input == "PARTNER_BLE_ON" || input == "GREEN") playPartnerConnectedGreenFlash();
  else if (input == "FAIL") playDeliveryFail();
}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true; 
    digitalWrite(PIN_LED_BUILTIN, LOW);
    // Explicitly stop advertising so NO OTHER PHONE can scan or connect
    pServer->getAdvertising()->stop();
    playConnectedBlueFlicker();
    delay(200);
    if (pTxCharacteristic) { pTxCharacteristic->setValue("BLE_ON"); pTxCharacteristic->notify(); }
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false; 
    digitalWrite(PIN_LED_BUILTIN, HIGH);
    playDisconnectedBlueStrobe();
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* p) { processIncomingCommand(p->getValue().c_str()); }
};

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_R, OUTPUT); pinMode(PIN_LED_G, OUTPUT); pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT); pinMode(PIN_VIBRATOR, OUTPUT);
  pinMode(PIN_TOUCH, INPUT);  pinMode(PIN_LED_BUILTIN, OUTPUT);
  digitalWrite(PIN_VIBRATOR, LOW); digitalWrite(PIN_BUZZER, LOW);
  digitalWrite(PIN_LED_BUILTIN, HIGH); setRGB(0,0,0);

  Serial.println("BOND TOUCH UNIT A BOOT");
  playBootChime();

  BLEDevice::init(DEVICE_BLE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService* pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic* pRx = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRx->setCallbacks(new MyCallbacks());

  pService->start();
  BLEAdvertising* pAdv = BLEDevice::getAdvertising();
  pAdv->addServiceUUID(SERVICE_UUID); pAdv->setScanResponse(true); pAdv->start();
  Serial.println("BOND TOUCH UNIT A READY");
}

void loop() {
  // Check physical touch sensor (GPIO 4)
  if (digitalRead(PIN_TOUCH) == HIGH) {
    unsigned long now = millis();
    if (now - lastTouchTime > DEBOUNCE_DELAY) {
      lastTouchTime = now;
      if (deviceConnected && pTxCharacteristic) {
        playLocalTouch();
        pTxCharacteristic->setValue("TOUCH"); 
        pTxCharacteristic->notify();
      } else {
        // Bluetooth disconnected -> Play 3 sharp RED error flickers + FAIL tone
        playDeliveryFail();
        // Followed by 2 blue flickers showing Bluetooth is searching
        setRGB(0, 0, 255); delay(80);
        setRGB(0, 0, 0);   delay(80);
        setRGB(0, 0, 255); delay(80);
        setRGB(0, 0, 0);
      }
      unsigned long rel = millis();
      while (digitalRead(PIN_TOUCH) == HIGH && (millis() - rel < 2000)) delay(10);
      delay(200);
    }
  }

  // If disconnected, maintain advertising and status
  if (!deviceConnected && oldDeviceConnected) { 
    delay(500); 
    pServer->startAdvertising(); 
  }
  oldDeviceConnected = deviceConnected;
  delay(10);
}
