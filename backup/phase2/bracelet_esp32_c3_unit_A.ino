/*
  ====================================================================================
  BOND TOUCH — ESP32-C3 SUPERMINI (UNIT A - WITH 16x2 I2C LCD DISPLAY)
  ====================================================================================
  EXACT PIN WIRING:
  - GPIO 0  --> 4-Pin RGB LED: RED Pin (R) [through 1kΩ resistor]
  - GPIO 1  --> 4-Pin RGB LED: GREEN Pin (G) [through 1kΩ resistor]
  - GPIO 2  --> 4-Pin RGB LED: BLUE Pin (B) [through 1kΩ resistor]
  - GND     --> 4-Pin RGB LED: Longest Pin (Common Cathode GND)
  - GPIO 3  --> Buzzer / Speaker (+) [PURE SOFTWARE SQUARE-WAVE]
  - GPIO 4  --> Touch Sensor SIG (Touch OUT)
  - GPIO 5  --> 16x2 LCD Module: SDA Pin 
  - GPIO 6  --> 16x2 LCD Module: SCL Pin 
  - GPIO 21 --> Vibrator Motor (+) [ONLY VIBRATES ON INCOMING TOUCH]
  - 5V / 3.3V --> 16x2 LCD VCC
  - GND     --> 16x2 LCD GND
  ====================================================================================
  REQUIRED ARDUINO LIBRARY:
  - Search & Install in Arduino IDE: "LiquidCrystal_I2C by Frank de Brabander"
  ====================================================================================
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
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

#define SDA_PIN 5
#define SCL_PIN 6

#define DEVICE_BLE_NAME        "BondTouch_ESP32_A"
#define SERVICE_UUID           "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_RX "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define CHARACTERISTIC_UUID_TX "a3c87500-8ed3-4bdf-8a39-a01bebede295"

LiquidCrystal_I2C lcd(0x27, 16, 2);

BLEServer* pServer = NULL;
BLECharacteristic* pTxCharacteristic = NULL;
bool deviceConnected    = false;
bool oldDeviceConnected = false;
unsigned long lastTouchTime   = 0;
const unsigned long DEBOUNCE_DELAY = 800;

int curR = 0, curG = 0, curB = 0;

byte heartIcon[8] = {
  0b00000, 0b01010, 0b11111, 0b11111, 0b01110, 0b00100, 0b00000, 0b00000
};

void setLCDMessage(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void setRGB(int r, int g, int b) {
  curR = constrain(r, 0, 255); curG = constrain(g, 0, 255); curB = constrain(b, 0, 255);
  analogWrite(PIN_LED_R, curR); analogWrite(PIN_LED_G, curG); analogWrite(PIN_LED_B, curB);
}

void setVibrator(bool on) { digitalWrite(PIN_VIBRATOR, on ? HIGH : LOW); }

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

void parseAndPlaySequence(String seqData) {
  setLCDMessage("Custom Sequence", "Playing...");
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
          setRGB(r, g, b); delay(durMs); setRGB(0, 0, 0);
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
      int durMs = item.substring(6).toInt(); delay(durMs);
    }
    else if (item.startsWith("VIB:")) {
      int durMs = item.substring(4).toInt();
      setVibrator(true); delay(durMs); setVibrator(false);
    }

    pos = pipe + 1;
  }
  setRGB(0, 0, 0); setVibrator(false); noBuzz();
  setLCDMessage("Bond Touch UnitA", "Ready...");
}

void playHeartbeat() {
  setLCDMessage("TOUCH RECEIVED!", "<3 Heartbeat <3");
  setVibrator(true); setRGB(255,0,0);
  makeBuzzerSound(80, 1400);
  setVibrator(false); fadeRGB(40,0,0, 6, 6); delay(60);
  setVibrator(true); setRGB(255,0,0);
  makeBuzzerSound(100, 1200);
  setVibrator(false); fadeRGB(0,0,0, 12, 7);
  setVibrator(false); noBuzz();
  setLCDMessage("Bond Touch UnitA", "Ready...");
}

void playLocalTouch() {
  setLCDMessage("Local Touch!", "Sending...");
  setVibrator(true); setRGB(255,255,255);
  makeBuzzerSound(40, 1500);
  setVibrator(false); fadeRGB(0,0,0, 8, 5);
  setVibrator(false); noBuzz();
}

void playConnectedBlueFlicker() {
  setLCDMessage("BLE Connected!", "Phone Linked ");
  setVibrator(true);
  for (int i = 0; i < 3; i++) {
    setRGB(0,0,255); makeBuzzerSound(40, 1200); delay(40);
    setRGB(0,0,0);   delay(40);
  }
  setRGB(0,0,255); delay(120); fadeRGB(0,0,0, 10, 6);
  setVibrator(false); noBuzz();
}

void playDisconnectedBlueStrobe() {
  setLCDMessage("BLE DISCONNECTED", "5s Blue Strobe");
  setVibrator(false); noBuzz();
  unsigned long start = millis();
  while (millis() - start < 5000) {
    setRGB(0,0,255); delay(100); setRGB(0,0,0); delay(100);
  }
  setRGB(0,0,0);
  setLCDMessage("Bond Touch UnitA", "BLE Disconnected");
}

void playPartnerConnectedGreenFlash() {
  setLCDMessage("PARTNER ONLINE!", "Phone Connected");
  setVibrator(true); setRGB(0,255,0);
  makeBuzzerSound(80, 1600); delay(50); makeBuzzerSound(100, 1800);
  fadeRGB(0,0,0, 15, 8);
  setVibrator(false); noBuzz();
  setLCDMessage("Bond Touch UnitA", "Partner Online!");
}

void playDeliveryFail() {
  setLCDMessage("DELIVERY FAIL!", "Partner Offline");
  for (int i = 0; i < 3; i++) {
    setVibrator(true); setRGB(255, 0, 0);
    makeBuzzerSound(80, 600);
    setVibrator(false); setRGB(0, 0, 0);
    delay(120);
  }
  setRGB(0, 0, 0);
  setLCDMessage("Bond Touch UnitA", "Ready...");
}

void playBootChime() {
  setVibrator(true);
  setRGB(255,0,0); delay(40);
  setRGB(0,255,0); delay(40);
  setRGB(0,0,255); delay(40);
  makeBuzzerSound(60, 1000); makeBuzzerSound(60, 1500);
  setVibrator(false); setRGB(0,0,0); noBuzz();
}

void processIncomingCommand(String input) {
  input.trim();
  if (input.length() == 0) return;
  Serial.print("RX: "); Serial.println(input);

  if (input.startsWith("SEQ:")) parseAndPlaySequence(input.substring(4));
  else if (input == "TOUCH" || input == "V" || input == "A") playHeartbeat();
  else if (input == "PARTNER_BLE_ON" || input == "GREEN") playPartnerConnectedGreenFlash();
  else if (input == "FAIL") playDeliveryFail();
}

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) {
    deviceConnected = true; digitalWrite(PIN_LED_BUILTIN, LOW);
    playConnectedBlueFlicker();
    delay(200);
    if (pTxCharacteristic) { pTxCharacteristic->setValue("BLE_ON"); pTxCharacteristic->notify(); }
  }
  void onDisconnect(BLEServer*) {
    deviceConnected = false; digitalWrite(PIN_LED_BUILTIN, HIGH);
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

  // Initialize I2C LCD
  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, heartIcon);

  setLCDMessage("Bond Touch UnitA", "Booting up...");
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
  
  setLCDMessage("Bond Touch UnitA", "Waiting BLE...");
}

void loop() {
  if (digitalRead(PIN_TOUCH) == HIGH) {
    unsigned long now = millis();
    if (now - lastTouchTime > DEBOUNCE_DELAY) {
      lastTouchTime = now;
      playLocalTouch();
      if (deviceConnected && pTxCharacteristic) {
        pTxCharacteristic->setValue("TOUCH"); pTxCharacteristic->notify();
      }
      unsigned long rel = millis();
      while (digitalRead(PIN_TOUCH) == HIGH && (millis()-rel < 2000)) delay(10);
      delay(200);
    }
  }
  if (!deviceConnected && oldDeviceConnected) { delay(500); pServer->startAdvertising(); }
  oldDeviceConnected = deviceConnected;
  delay(10);
}
