/*
  ====================================================================================
  HAPTIC BRACELET — ESP32-C3 SUPERMINI — UNIT A
  ====================================================================================
  CONFIRMED PIN WIRING:
  - GPIO 0  --> RGB LED: GREEN  (through 220Ω resistor)
  - GPIO 1  --> RGB LED: BLUE   (through 220Ω resistor)
  - GPIO 2  --> RGB LED: RED    (through 220Ω resistor)
  - GND     --> RGB LED: Longest Leg (Common Cathode GND)
  - GPIO 7  --> Touch Sensor TTP223 (SIG/I-O pin)
  - GPIO 5  --> Vibration Motor (via 2N2222 transistor Base through 1kΩ)
  ====================================================================================
  HOW IT WORKS:
  - Unit A connects to phone app over BLE
  - When YOU touch Unit A -> sends "TOUCH" to app -> app relays to Unit B
  - When OTHER person touches Unit B -> app relays "TOUCH" to Unit A -> vibrates + LED
  ====================================================================================
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ── PINS ─────────────────────────────────────────────────────────────────────
#define PIN_G     0   // RGB Green
#define PIN_B     1   // RGB Blue
#define PIN_R     2   // RGB Red
#define PIN_TOUCH 7   // TTP223 Touch Sensor
#define PIN_MOTOR 5   // Vibration Motor (Transistor)

// ── BLE CONFIG ────────────────────────────────────────────────────────────────
#define DEVICE_NAME     "BraceletA"
#define SERVICE_UUID    "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define UUID_TX         "a3c87500-8ed3-4bdf-8a39-a01bebede295"  // ESP32 → App
#define UUID_RX         "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // App → ESP32

// ── STATE ─────────────────────────────────────────────────────────────────────
BLEServer*         pServer  = nullptr;
BLECharacteristic* pTxChar  = nullptr;
bool connected = false;
unsigned long lastTouchSent = 0;
const unsigned long DEBOUNCE = 800;

// ── RGB HELPER ────────────────────────────────────────────────────────────────
void rgb(int r, int g, int b) {
  analogWrite(PIN_R, constrain(r, 0, 255));
  analogWrite(PIN_G, constrain(g, 0, 255));
  analogWrite(PIN_B, constrain(b, 0, 255));
}

void rgbOff() { rgb(0, 0, 0); }

// ── MOTOR HELPER ──────────────────────────────────────────────────────────────
void vibrate(int ms) {
  digitalWrite(PIN_MOTOR, HIGH);
  delay(ms);
  digitalWrite(PIN_MOTOR, LOW);
}

// ── HEARTBEAT ANIMATION (waiting for connection) ──────────────────────────────
void idlePulse() {
  // Soft slow blue pulse
  for (int i = 0; i < 80; i++) { rgb(0, 0, i * 2); delay(6); }
  for (int i = 80; i >= 0; i--) { rgb(0, 0, i * 2); delay(6); }
}

// ── INCOMING TOUCH PATTERN ────────────────────────────────────────────────────
void incomingTouch() {
  // Vibrate in heartbeat pattern + pink/magenta LED
  for (int p = 0; p < 2; p++) {
    rgb(255, 0, 80);        // warm pink
    vibrate(120);
    delay(80);
    vibrate(180);
    rgbOff();
    delay(600);
  }
}

// ── SEND TOUCH PATTERN (outgoing confirm) ─────────────────────────────────────
void outgoingConfirm() {
  // Quick green flash to confirm touch was sent
  rgb(0, 255, 0);
  delay(100);
  rgbOff();
  delay(60);
  rgb(0, 255, 0);
  delay(100);
  rgbOff();
}

// ── BLE CALLBACKS ─────────────────────────────────────────────────────────────
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    connected = true;
    // Slow green breathe on connect
    for (int i = 0; i < 200; i += 5) { rgb(0, i, 0); delay(4); }
    for (int i = 200; i >= 0; i -= 5) { rgb(0, i, 0); delay(4); }
    rgbOff();
  }
  void onDisconnect(BLEServer* s) override {
    connected = false;
    BLEDevice::startAdvertising();
    rgbOff();
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    std::string val = c->getValue();
    if (val == "TOUCH") {
      incomingTouch();
    }
    // Color commands from app: "RGB:255,0,128"
    else if (val.rfind("RGB:", 0) == 0) {
      int r = 0, g = 0, b = 0;
      sscanf(val.c_str(), "RGB:%d,%d,%d", &r, &g, &b);
      rgb(r, g, b);
    }
    // VIB:ms — vibrate custom duration
    else if (val.rfind("VIB:", 0) == 0) {
      int ms = atoi(val.c_str() + 4);
      vibrate(ms);
    }
    else if (val == "OFF") {
      rgbOff();
    }
  }
};

// ── SETUP ─────────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  pinMode(PIN_MOTOR, OUTPUT);
  pinMode(PIN_TOUCH, INPUT);

  rgbOff();
  digitalWrite(PIN_MOTOR, LOW);

  // Boot blink: RED > GREEN > BLUE
  rgb(255, 0, 0); delay(200); rgbOff(); delay(80);
  rgb(0, 255, 0); delay(200); rgbOff(); delay(80);
  rgb(0, 0, 255); delay(200); rgbOff();

  // BLE Init
  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* svc = pServer->createService(SERVICE_UUID);

  // TX Characteristic (ESP32 -> App)
  pTxChar = svc->createCharacteristic(UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  pTxChar->addDescriptor(new BLE2902());

  // RX Characteristic (App -> ESP32)
  BLECharacteristic* pRxChar = svc->createCharacteristic(UUID_RX,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pRxChar->setCallbacks(new RxCallbacks());

  svc->start();
  BLEDevice::startAdvertising();

  Serial.println("Bracelet A — BLE advertising started");
}

// ── MAIN LOOP ─────────────────────────────────────────────────────────────────
void loop() {
  if (!connected) {
    idlePulse(); // slow blue pulse when waiting
    return;
  }

  // Read touch sensor
  if (digitalRead(PIN_TOUCH) == HIGH) {
    unsigned long now = millis();
    if (now - lastTouchSent > DEBOUNCE) {
      lastTouchSent = now;

      // Send touch signal to app
      pTxChar->setValue("TOUCH");
      pTxChar->notify();

      Serial.println("Touch sent!");
      outgoingConfirm();

      // Wait for finger to lift
      while (digitalRead(PIN_TOUCH) == HIGH) { delay(10); }
    }
  }

  delay(10);
}
