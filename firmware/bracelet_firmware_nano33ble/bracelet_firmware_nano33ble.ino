/*
  ====================================================================================
  BOND TOUCH PROTOTYPE FIRMWARE — ARDUINO NANO 33 BLE (nRF52840)
  كود السوفتوير للسوار الذكي - كارت أردوينو نانو 33 بي إل إي
  ====================================================================================

  WIRING DIAGRAM / مخطط التوصيل الكهربائي:
  ------------------------------------------------------------------------------------
                                  +-------------------+
                                  | Arduino Nano 33   |
                                  |      BLE          |
                                  +-------------------+
                                  | D2             D13|---> [Resistor 330Ω] ---> [LED Anode(+)] ---> GND
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

  Note on Flyback Diode (1N4007):
  - Cathode (striped end) connects to Motor + (3.3V/5V)
  - Anode connects to Motor - (Transistor Collector)
  ====================================================================================
*/

#include <ArduinoBLE.h>       // مكتبة البلوتوث منخفض الطاقة / BLE Library
#include <Arduino_LSM6DS3.h>  // مكتبة المستشعر (تسارع للحركة واللمس) / IMU Accelerometer Library

// ----------------------------------------------------------------------------------
// HARDWARE PIN DEFINITIONS / تعريف الأطراف
// ----------------------------------------------------------------------------------
const int MOTOR_PIN = 2;       // Pin D2 connected to Transistor Base for Vibe Motor / طرف المحرك
const int LED_PIN = 13;        // Pin D13 for visual feedback LED / طرف الليد الضوئي

// ----------------------------------------------------------------------------------
// BLE UUID DEFINITIONS / معرفات البلوتوث الفريدة
// ----------------------------------------------------------------------------------
// Service UUID for Bond Touch Prototype / معرف الخدمة
BLEService touchService("19B10000-E8F2-537E-4F6C-D104768A1214");

// Characteristic to NOTIFY mobile phone when bracelet is tapped / خاصية إرسال إشعار اللمس للموبايل
BLEByteCharacteristic touchNotifyChar("19B10001-E8F2-537E-4F6C-D104768A1214", BLERead | BLENotify);

// Characteristic to RECEIVE vibrate command from mobile phone / خاصية استقبال أمر الاهتزاز من الموبايل
BLEByteCharacteristic vibrateCommandChar("19B10002-E8F2-537E-4F6C-D104768A1214", BLEWrite);

// ----------------------------------------------------------------------------------
// TAP DETECTION VARIABLES / متغيرات كشف النقر
// ----------------------------------------------------------------------------------
const float TAP_THRESHOLD = 1.85;  // Acceleration threshold in Gs to trigger tap / حد شدة اللمسة
unsigned long lastTapTime = 0;     // Timestamp of last detected tap / وقت آخر لمسة
const unsigned long DEBOUNCE_DELAY = 400; // Debounce delay in ms / وقت منع التكرار

void setup() {
  Serial.begin(115200); // Initialize serial communication for debugging / تفعيل الاتصال التسلسلي للتجربة

  // Configure hardware pins as output / تهيئة الأطراف كمخرج
  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW); // Ensure motor is OFF initially / إيقاف المحرك عند البدء
  digitalWrite(LED_PIN, LOW);   // Ensure LED is OFF initially / إيقاف الليد عند البدء

  // Initialize LSM6DS3 IMU Accelerometer / تشغيل مستشعر الحركة
  if (!IMU.begin()) {
    Serial.println("Failed to initialize IMU! / فشل تشغيل مستشعر الحركة");
    while (1); // Halt if IMU fails / إيقاف التشغيل في حالة الخطأ
  }

  // Initialize BLE module / تشغيل البلوتوث
  if (!BLE.begin()) {
    Serial.println("Starting BLE failed! / فشل تشغيل البلوتوث");
    while (1);
  }

  // Set BLE local name & advertised service / ضبط اسم البلوتوث والخدمة المعلنة
  BLE.setLocalName("BondTouch_Nano33");
  BLE.setAdvertisedService(touchService);

  // Add characteristics to service / إضافة الخصائص للخدمة
  touchService.addCharacteristic(touchNotifyChar);
  touchService.addCharacteristic(vibrateCommandChar);

  // Add service to BLE stack / إضافة الخدمة للبلوتوث
  BLE.addService(touchService);

  // Initial values / القيم الابتدائية
  touchNotifyChar.writeValue(0);
  vibrateCommandChar.writeValue(0);

  // Start advertising / بدء بث البلوتوث للبحث عنه من الموبايل
  BLE.advertise();
  Serial.println("BLE Touch Bracelet Ready & Advertising... / السوار جاهز ويبث إشارة البلوتوث");
}

void loop() {
  // Listen for BLE central devices (Mobile Phone) / البحث عن الموبايل المقترن
  BLEDevice central = BLE.central();

  if (central) {
    Serial.print("Connected to central: ");
    Serial.println(central.address());
    digitalWrite(LED_PIN, HIGH); // Turn LED ON when connected / إضاءة الليد عند الاتصال

    while (central.connected()) {
      // 1. Check for incoming Vibrate Command from mobile phone / الفحص هل وصل أمر اهتزاز من الموبايل
      if (vibrateCommandChar.written()) {
        uint8_t commandValue = vibrateCommandChar.value();
        if (commandValue == 1) {
          Serial.println("Received Vibrate Command! Vibrating... / تم استقبال أمر اهتزاز من الطرف الآخر!");
          triggerVibration(500); // Vibrate for 500 ms / تشغيل الهزاز لمدة نصف ثانية
          vibrateCommandChar.writeValue(0); // Reset command / إعادة ضبط الأمر
        }
      }

      // 2. Read Accelerometer to detect physical tap / قراءة مستشعر التسارع لكشف اللمسة
      float x, y, z;
      if (IMU.accelerationAvailable()) {
        IMU.readAcceleration(x, y, z);
        // Calculate total G-force magnitude / حساب محصلة القوة التسارعية
        float gMagnitude = sqrt(x * x + y * y + z * z);

        // Check if tap threshold exceeded and debounce time passed / التأكد من تجاوز شدة اللمسة ووقت الامان
        if (gMagnitude > TAP_THRESHOLD && (millis() - lastTapTime > DEBOUNCE_DELAY)) {
          lastTapTime = millis();
          Serial.println("Physical Tap Detected! Sending BLE Notification... / تم كشف لمسة! جاري الإرسال للموبايل...");
          
          // Blink LED / إضاءة سريعة لليد
          digitalWrite(LED_PIN, LOW);
          delay(50);
          digitalWrite(LED_PIN, HIGH);

          // Send BLE Notification to Phone (Value 1 = Touch Sent) / إرسال القيمة 1 للموبايل
          touchNotifyChar.writeValue(1);
          delay(100);
          touchNotifyChar.writeValue(0); // Clear state
        }
      }

      delay(10); // Small delay for power stability / التأخير للاستقرار
    }

    digitalWrite(LED_PIN, LOW); // Turn LED OFF when disconnected / إطفاء الليد عند انقطاع الاتصال
    Serial.println("Disconnected from central / انقطع الاتصال بالموبايل");
  }
}

// ----------------------------------------------------------------------------------
// HELPER FUNCTION: Trigger Vibration Motor / دالة تشغيل محرك الاهتزاز
// ----------------------------------------------------------------------------------
void triggerVibration(int durationMs) {
  digitalWrite(MOTOR_PIN, HIGH); // Motor ON / تشغيل المحرك
  digitalWrite(LED_PIN, HIGH);   // LED ON / تشغيل الليد
  delay(durationMs);
  digitalWrite(MOTOR_PIN, LOW);  // Motor OFF / إيقاف المحرك
  digitalWrite(LED_PIN, LOW);    // LED OFF / إيقاف الليد
}
