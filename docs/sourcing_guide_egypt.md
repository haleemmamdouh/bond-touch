# Task 1 & 7: Egyptian Electronics Sourcing Guide & Component Budget

This guide is specifically tailored for sourcing prototype components in **Al Rehab City, New Cairo, Cairo, Egypt**.

---

## 1. Where to Buy Electronics Components in Egypt

While residential areas like Al Rehab City do not have hobbyist microcontroller shops, there are premier Egyptian electronics suppliers located nearby (in Abbassia/Downtown Cairo) as well as online stores offering **fast 24–48 hour shipping (or Cash on Delivery)** directly to New Cairo.

### A. Top Recommended Online & Local Electronics Stores

1. **Future Electronics (Arduino Egypt)**
   - **Location:** Abbassia, Cairo (Near Ain Shams University, Faculty of Engineering) — ~25-30 mins from New Cairo.
   - **Website:** [fut-electronics.com](https://fut-electronics.com)
   - **Phone / WhatsApp:** 01229923337 / 02 26825325
   - **Best For:** ESP32 boards, micro vibration motors, breadboards, jumper wires, resistors, diodes, transistors.

2. **RAM Electronics**
   - **Location:** Downtown Cairo (El Bustan Mall) & Abbassia.
   - **Website:** [ram-e-shop.com](https://ram-e-shop.com)
   - **Phone:** 02 23910000
   - **Best For:** Microcontroller dev boards, LiPo batteries, TP4056 charging modules, components, soldering supplies.

3. **El-Gammal Electronics**
   - **Website:** [elgammalelectronic.com](https://elgammalelectronic.com)
   - **Best For:** Fast online delivery via Aramex/Bosta to New Cairo, development modules, sensors.

4. **Makers Electronics**
   - **Website:** [makerselectronics.com](https://makerselectronics.com)
   - **Best For:** ESP32, Arduino boards, battery shields, sensors.

5. **2B Electronics (Local Store in Al Rehab City)**
   - **Location:** Rehab City Commercial Market (الموق التجاري بالرحاب) / Souq El Rehab.
   - **Best For:** Standard USB Micro/Type-C cables, power banks, basic tools.

---

## 2. Itemized Component Bill of Materials (BOM) & Budget for 2 Bracelets

Quantities listed are for **2 complete prototype bracelets**.

| # | Item Description | Recommended Model / Specs | Qty | Est. Price per Unit (EGP) | Est. Total Cost (EGP) | Preferred Egyptian Source |
|---|---|---|---|---|---|---|
| 1 | Microcontroller Dev Board | **ESP32 Devkit V1** (30-pin) *OR* Arduino Nano 33 BLE | 2 | ~300 EGP (ESP32) / ~1,500 EGP (Nano 33 BLE) | **600 EGP** (ESP32) | Future Electronics / RAM |
| 2 | Vibration Motor | Mini Coin / Disc Vibration Motor (3V DC, 10mm, ERM) | 2 | ~50 EGP | **100 EGP** | Future Electronics / RAM |
| 3 | NPN Transistor | 2N2222 or 2N3904 (TO-92 package) | 2 | ~5 EGP | **10 EGP** | Any electronics store |
| 4 | Flyback Diode | 1N4007 or 1N4148 | 2 | ~3 EGP | **6 EGP** | Any electronics store |
| 5 | Resistor | 1 kΩ (1/4W through-hole) | 2 | ~2 EGP | **4 EGP** | Any electronics store |
| 6 | LED | 5mm LED (Red/Green/Blue) | 2 | ~3 EGP | **6 EGP** | Any electronics store |
| 7 | Rechargeable Battery | 3.7V LiPo Battery (300mAh - 500mAh) *OR* 18650 Cell + Holder | 2 | ~150 EGP | **300 EGP** | RAM Electronics |
| 8 | Battery Charger Module | TP4056 Micro-USB / Type-C Charging Board | 2 | ~35 EGP | **70 EGP** | Future Electronics / RAM |
| 9 | Mini Breadboard | 170-point mini breadboard (SyB-170) | 2 | ~30 EGP | **60 EGP** | Future Electronics / RAM |
| 10 | Jumper Wires Pack | Male-to-Male & Male-to-Female mix (40 pcs) | 1 pack | ~60 EGP | **60 EGP** | Future Electronics / RAM |
| 11 | USB Cables | USB-A to Micro-USB or USB-C (for power/flashing) | 2 | ~50 EGP | **100 EGP** | 2B Rehab City / RAM |
| **TOTAL** | | *(Using ESP32 boards)* | | | **~1,316 EGP** | *(Shipping ~60-80 EGP)* |

---

## 3. Recommended Purchasing & Substitution Strategy

1. **Board Choice:**
   - **Primary Recommendation:** Buy **ESP32 DevKit V1 (30-pin)**. It is universally available in Cairo, costs only ~300 EGP per board, and has built-in BLE & Bluetooth 4.2/5.0.
   - **Alternative:** **Arduino Nano 33 BLE** (nRF52840). If available, it includes an onboard IMU (accelerometer) but costs significantly more in Egypt.

2. **Tap Sensor Choice:**
   - **With ESP32:** You can use the built-in **Capacitive Touch Pins** (Touch0 / GPIO4) by touching a tiny piece of aluminum foil taped to the lid, or a standard mini tactile push button (5 EGP).
   - **With Nano 33 BLE:** Uses the built-in LSM6DS3 3-axis accelerometer for tap detection.

3. **Battery Solution:**
   - If a small flat LiPo battery is out of stock, buy a **3.7V 18650 Li-ion battery** (~70 EGP) with a **single 18650 battery holder** (~25 EGP). Combine it with a **TP4056 charger board** (~35 EGP) for safe USB recharging.
