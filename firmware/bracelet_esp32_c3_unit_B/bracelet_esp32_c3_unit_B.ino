/*
  ====================================================================================
  HAPTIC BRACELET — ESP32-C3 SUPERMINI — UNIT B (OTA FIXED + ACK DELIVERY)
  ====================================================================================
  CONFIRMED PIN WIRING:
  - GPIO 0  --> RGB LED: GREEN  (through 220Ω resistor)
  - GPIO 1  --> RGB LED: BLUE   (through 220Ω resistor)
  - GPIO 2  --> RGB LED: RED    (through 220Ω resistor)
  - GND     --> RGB LED: Longest Leg (Common Cathode GND)
  - GPIO 7  --> Touch Sensor TTP223 (SIG/I-O pin)
  - GPIO 5  --> Vibration Motor (via 2N2222 transistor Base through 1kΩ)
  ====================================================================================
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <esp_wifi.h>

// ── WIFI (for wireless OTA updates) ────────────────────────────────
const char* WIFI_SSID = "H";                  // hidden network
const char* WIFI_PASS = "1122334455hHh@";
#define OTA_HOSTNAME "BraceletB"              // shows in Arduino IDE ports

// ── PINS ─────────────────────────────────────────────────────────────────────
#define PIN_G     0   // RGB Green
#define PIN_B     1   // RGB Blue
#define PIN_R     2   // RGB Red
#define PIN_TOUCH 7   // TTP223 Touch Sensor
#define PIN_MOTOR 5   // Vibration Motor (Transistor)

// ── BLE CONFIG ────────────────────────────────────────────────────────────────
#define DEVICE_NAME  "BraceletB"
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define UUID_TX      "a3c87500-8ed3-4bdf-8a39-a01bebede295"  // ESP32 → App
#define UUID_RX      "beb5483e-36e1-4688-b7f5-ea07361b26a8"  // App → ESP32

// ── STATE ─────────────────────────────────────────────────────────────────────
BLEServer*         pServer = nullptr;
BLECharacteristic* pTxChar = nullptr;
bool connected = false;
bool otaInProgress = false;
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

// ── NON-BLOCKING IDLE PULSE (WiFi OTA Friendly - Purple) ──────────────────────
void idlePulse() {
  static unsigned long lastUpdate = 0;
  static int brightness = 0;
  static int direction = 2;

  unsigned long now = millis();
  if (now - lastUpdate > 15) {
    lastUpdate = now;
    brightness += direction;
    if (brightness >= 160 || brightness <= 0) {
      direction = -direction;
    }
    rgb(brightness, 0, brightness); // Purple idle for Unit B
  }
}

// ── PARTNER CONNECTED (their bracelet just came online) ───────────────────────
void partnerConnected() {
  for (int i = 0; i < 3; i++) {
    rgb(160, 0, 255);   // purple
    delay(140);
    rgbOff();
    delay(90);
  }
  vibrate(80);          // small confirmation buzz
}

// ── INCOMING TOUCH PATTERN ────────────────────────────────────────────────────
void incomingTouch() {
  for (int p = 0; p < 2; p++) {
    rgb(255, 0, 80);        // warm pink
    vibrate(120);
    delay(80);
    vibrate(180);
    rgbOff();
    delay(600);
  }
}

// ── OUTGOING TOUCH DELIVERED CONFIRM (Green Flash) ────────────────────────────
void outgoingConfirm() {
  rgb(0, 255, 0); delay(100); rgbOff();
  delay(60);
  rgb(0, 255, 0); delay(100); rgbOff();
}

// ── OUTGOING TOUCH FAILED CONFIRM (Red Strobe) ────────────────────────────────
void failConfirm() {
  for (int i = 0; i < 3; i++) {
    rgb(255, 0, 0); delay(120); rgbOff(); delay(80);
  }
}

// ── BLE CALLBACKS ─────────────────────────────────────────────────────────────
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* s) override {
    connected = true;
    for (int i = 0; i < 200; i += 5) { rgb(0, i, 0); delay(4); }
    for (int i = 200; i >= 0; i -= 5) { rgb(0, i, 0); delay(4); }
    rgbOff();
    vibrate(80); // small buzz — connection confirmed
  }
  void onDisconnect(BLEServer* s) override {
    connected = false;
    BLEDevice::startAdvertising();
    rgbOff();
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* c) override {
    String val = String(c->getValue().c_str());
    if (val == "TOUCH") {
      incomingTouch();          // partner touched their sensor
    }
    else if (val == "ACK_OK") {
      outgoingConfirm();        // touch reached partner -> GREEN flash
    }
    else if (val == "ACK_FAIL") {
      failConfirm();            // touch failed -> RED strobe
    }
    else if (val == "PARTNER_ON") {
      partnerConnected();       // partner's bracelet came online -> PURPLE flash + buzz
    }
    else if (val.startsWith("RGB:")) {
      int r = 0, g = 0, b = 0;
      sscanf(val.c_str(), "RGB:%d,%d,%d", &r, &g, &b);
      rgb(r, g, b);
    }
    else if (val.startsWith("VIB:")) {
      int ms = val.substring(4).toInt();
      vibrate(ms);
    }
    else if (val == "OFF") {
      rgbOff();
    }
  }
};

// ── SETUP ─────────────────────────────────────────────────────────────────────
void setup() {
  setCpuFrequencyMhz(80); // 80MHz power optimization

  Serial.begin(115200);

  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  pinMode(PIN_MOTOR, OUTPUT);
  pinMode(PIN_TOUCH, INPUT);

  rgbOff();
  digitalWrite(PIN_MOTOR, LOW);

  // Boot blink: RED → GREEN → BLUE
  rgb(255, 0, 0); delay(200); rgbOff(); delay(80);
  rgb(0, 255, 0); delay(200); rgbOff(); delay(80);
  rgb(0, 0, 255); delay(200); rgbOff();

  // BLE Init
  BLEDevice::init(DEVICE_NAME);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService* svc = pServer->createService(SERVICE_UUID);

  pTxChar = svc->createCharacteristic(UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  pTxChar->addDescriptor(new BLE2902());

  BLECharacteristic* pRxChar = svc->createCharacteristic(UUID_RX,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  pRxChar->setCallbacks(new RxCallbacks());

  svc->start();
  BLEDevice::startAdvertising();

  // ── WIFI + OTA SETUP ──────────────────────────────────────────────────────
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS, 0, nullptr, true);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 6000) {
    delay(300);
  }
  if (WiFi.status() == WL_CONNECTED) {
    rgb(0, 255, 100); delay(300); rgbOff(); // teal flash = WiFi OK
    WiFi.setSleep(true);
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  }

  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    for (int i = 0; i < 6; i++) { rgb(255,255,255); delay(80); rgbOff(); delay(80); }
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int pct = progress * 255 / total;
    rgb(0, 0, pct);
  });
  ArduinoOTA.onEnd([]() {
    otaInProgress = false;
    rgb(0, 255, 0); delay(500); rgbOff();
  });
  ArduinoOTA.onError([](ota_error_t err) {
    otaInProgress = false;
    rgb(255, 0, 0); delay(800); rgbOff();
  });
  ArduinoOTA.begin();
}

// ── MAIN LOOP ─────────────────────────────────────────────────────────────────
void loop() {
  ArduinoOTA.handle(); // check for wireless update first

  if (otaInProgress) {
    delay(1);
    return; // Dedicated CPU focus during OTA transfer (no timeouts!)
  }

  if (!connected) {
    idlePulse(); // non-blocking purple pulse
    return;
  }

  if (digitalRead(PIN_TOUCH) == HIGH) {
    unsigned long now = millis();
    if (now - lastTouchSent > DEBOUNCE) {
      lastTouchSent = now;

      if (connected) {
        // Send touch to app — app checks if partner is online & responds ACK_OK or ACK_FAIL
        pTxChar->setValue("TOUCH");
        pTxChar->notify();
      } else {
        // Not connected to app — direct red failure
        failConfirm();
      }

      while (digitalRead(PIN_TOUCH) == HIGH) { delay(10); }
    }
  }

  delay(10);
}
