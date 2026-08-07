/*
  ====================================================================================
  BOND TOUCH PROTOTYPE FIRMWARE — ESP32 DEV MODULE (ESP32-WROOM-32)
  كود السوفتوير للسوار الذكي - كارت إي إس بي 32 (النسخة الأكثر توفراً بمصر)
  ====================================================================================

  WIRING DIAGRAM / مخطط التوصيل الكهربائي:
  ------------------------------------------------------------------------------------
                                  +-------------------+
                                  |   ESP32 DevKit    |
                                  +-------------------+
                                  | GPIO16      GPIO2 |---> [Resistor 330Ω] ---> [LED Anode(+)] ---> GND
                                  +--|----------------+
                                     |
                                 [Resistor 1kΩ]
                                     |
                                  Base (B)
                               +-------------+
       +3.3V/5V ---> [Motor +] |  NPN 2N2222 |
       GND  <--- [Diode 1N4007]|  Transistor |
                 [Motor -] --->| Collector(C)|
                               | Emitter (E) | ---> GND
                               +-------------+

  TAP SENSOR OPTIONS / خيارات مستشعر اللمس:
  Option A: Touch Pad Wire connected to GPIO 4 (Touch Sensor Pin 0)
  Option B: Push Button connected between GPIO 15 and GND (Internal Pull-Up enabled)
  ====================================================================================
*/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ----------------------------------------------------------------------------------
// HARDWARE PIN DEFINITIONS / تعريف الأطراف
// ----------------------------------------------------------------------------------
const int MOTOR_PIN = 16;      // GPIO16 connected to Transistor Base / طرف المحرك
const int LED_PIN = 2;         // GPIO2 (Onboard LED on ESP32) / طرف الليد الضوئي
const int BUTTON_PIN = 15;     // GPIO15 Push Button Input / طرف الزر
const int TOUCH_PIN = 4;       // GPIO4 Capacitive Touch Sensor / طرف اللمس السعوي

// Touch sensitivity threshold for GPIO4 / حد حساسية اللمس السعوي
const int TOUCH_THRESHOLD = 35; 

// ----------------------------------------------------------------------------------
// BLE SERVICE & CHARACTERISTIC UUIDs / معرفات البلوتوث
// ----------------------------------------------------------------------------------
#define SERVICE_UUID           "19B10000-E8F2-537E-4F6C-D104768A1214"
#define TOUCH_NOTIFY_UUID      "19B10001-E8F2-537E-4F6C-D104768A1214"
#define VIBRATE_CMD_UUID       "19B10002-E8F2-537E-4F6C-D104768A1214"

BLEServer* pServer = NULL;
BLECharacteristic* pTouchNotifyCharacteristic = NULL;
BLECharacteristic* pVibrateCmdCharacteristic = NULL;

bool deviceConnected = false;
bool oldDeviceConnected = false;
unsigned long lastTapTime = 0;
const unsigned long DEBOUNCE_DELAY = 400; // ms

// ----------------------------------------------------------------------------------
// BLE SERVER CALLBACKS / أحداث اتصال البلوتوث
// ----------------------------------------------------------------------------------
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Phone Connected! / تم اتصل الموبايل بالسوار");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Phone Disconnected! / انقطع الاتصال بالموبايل");
    }
};

// ----------------------------------------------------------------------------------
// BLE CHARACTERISTIC CALLBACKS (Incoming Commands) / أحداث استقبال الأوامر من الموبايل
// ----------------------------------------------------------------------------------
class VibrateCmdCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() > 0) {
        uint8_t cmd = rxValue[0];
        if (cmd == 1) {
          Serial.println("Received Vibrate Command from Partner! / تم استقبال أمر اهتزاز من الشريك!");
          triggerVibration(500); // Vibrate for 500ms
        }
      }
    }
};

void setup() {
  Serial.begin(115200);

  // Hardware pin configuration / تهيئة الأطراف
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP); // Enable internal pullup resistor for button / تفعيل المداخل

  digitalWrite(MOTOR_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  // Initialize BLE Device / تشغيل جهاز البلوتوث
  BLEDevice::init("BondTouch_ESP32");

  // Create BLE Server / إنشاء سيرفر البلوتوث
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create BLE Service / إنشاء خدمة البلوتوث
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create Touch Notification Characteristic / إنشاء خاصية الإشعار للموبايل
  pTouchNotifyCharacteristic = pService->createCharacteristic(
                                 TOUCH_NOTIFY_UUID,
                                 BLECharacteristic::PROPERTY_READ   |
                                 BLECharacteristic::PROPERTY_NOTIFY
                               );
  pTouchNotifyCharacteristic->addDescriptor(new BLE2902());

  // Create Vibrate Command Characteristic / إنشاء خاصية استقبال أمر الاهتزاز
  pVibrateCmdCharacteristic = pService->createCharacteristic(
                                VIBRATE_CMD_UUID,
                                BLECharacteristic::PROPERTY_WRITE
                              );
  pVibrateCmdCharacteristic->setCallbacks(new VibrateCmdCallbacks());

  // Start the BLE service / تشغيل الخدمة
  pService->start();

  // Start BLE advertising / بدء البث للبحث عن السوار
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value for iPhone compatibility
  BLEDevice::startAdvertising();
  Serial.println("ESP32 Bond Touch Bracelet Ready & Advertising... / السوار جاهز ومتاح للاتصال");
}

void loop() {
  // Blink LED if not connected to indicate waiting status / إضاءة خفيفة للتنبيه بحالة الانتظار
  if (!deviceConnected) {
    digitalWrite(LED_PIN, (millis() / 500) % 2);
  } else {
    digitalWrite(LED_PIN, HIGH); // Solid ON when connected / إضاءة مستمرة عند الاتصال
  }

  // 1. Detect physical tap (Option A: Touch pin / Option B: Button press) / كشف النقر
  bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);
  int touchValue = touchRead(TOUCH_PIN);
  bool touchDetected = (touchValue > 0 && touchValue < TOUCH_THRESHOLD);

  if ((buttonPressed || touchDetected) && (millis() - lastTapTime > DEBOUNCE_DELAY)) {
    lastTapTime = millis();
    Serial.println("Tap / Touch Detected! Sending notification... / تم كشف لمسة أو ضغطة زر!");

    if (deviceConnected) {
      uint8_t value = 1;
      pTouchNotifyCharacteristic->setValue(&value, 1);
      pTouchNotifyCharacteristic->notify(); // Send BLE notification to phone
      
      // Quick visual blink
      digitalWrite(LED_PIN, LOW);
      delay(80);
      digitalWrite(LED_PIN, HIGH);
      
      // Reset characteristic
      value = 0;
      pTouchNotifyCharacteristic->setValue(&value, 1);
    } else {
      Serial.println("Not connected to phone! Touch ignored. / السوار غير متصل بالموبايل");
    }
  }

  // Handle re-advertising on disconnect / إعادة البث في حالة انقطاع الاتصال
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // Give BLE stack time to reset
    pServer->startAdvertising(); // Restart advertising
    Serial.println("Restarted advertising... / إعاده بث البلوتوث");
    oldDeviceConnected = deviceConnected;
  }
  
  if (deviceConnected && !oldDeviceConnected) {
    oldDeviceConnected = deviceConnected;
  }

  delay(20);
}

// ----------------------------------------------------------------------------------
// HELPER FUNCTION: Trigger Vibration Motor / دالة تشغيل الهزاز
// ----------------------------------------------------------------------------------
void triggerVibration(int durationMs) {
  digitalWrite(MOTOR_PIN, HIGH);
  digitalWrite(LED_PIN, HIGH);
  delay(durationMs);
  digitalWrite(MOTOR_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
}
