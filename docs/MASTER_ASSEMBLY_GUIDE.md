# Task 6: Master Assembly & End-to-End Testing Guide

Follow this numbered, step-by-step master guide to build, assemble, program, and test your long-distance touch bracelet prototype pair.

---

## Phase 1: Software Setup on Laptop (Arduino IDE)

1. **Install Arduino IDE:**
   - Download Arduino IDE 2.3+ for Windows from [arduino.cc/en/software](https://www.arduino.cc/en/software).
   - Install using standard options.

2. **Install Board Support:**
   - Open Arduino IDE -> Tools -> Board -> **Boards Manager**.
   - If using **Arduino Nano 33 BLE**: Search `nRF528x` -> Install **Arduino mbedOS Nano Boards**.
   - If using **ESP32**: Go to File -> Preferences -> Additional Board Manager URLs -> Add `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`. Then open Boards Manager, search `ESP32` by Espressif -> Install.

3. **Install Required Libraries:**
   - Open Tools -> **Manage Libraries...**
   - Search & Install `ArduinoBLE` by Arduino.
   - Search & Install `Arduino_LSM6DS3` by Arduino (Only needed if using Nano 33 BLE).

---

## Phase 2: Upload Firmware to Hardware

1. Connect Bracelet Board A to PC using USB cable.
2. Select Board (`ESP32 Dev Module` or `Arduino Nano 33 BLE`) and Port under **Tools -> Port**.
3. Open `firmware/bracelet_firmware_esp32/bracelet_firmware_esp32.ino` (or `nano33ble`).
4. Click the **Upload** arrow button (➡️). Wait for `Done uploading`.
5. Repeat step 1-4 for Bracelet Board B.

---

## Phase 3: Hardware Breadboard Wiring

Follow this exact wiring for both bracelets:

```
                          +-------------------+
                          |     Microboard    |
                          +-------------------+
                          | GPIO16 / D2  GPIO2|---> [330Ω Resistor] ---> [LED (+)] ---> GND
                          +------|------------+
                                 |
                            [1kΩ Resistor]
                                 |
                              Base (B)
                           +-------------+
   +3.3V/5V ---> [Motor +] |  NPN 2N2222 |
   GND  <--- [Diode 1N4007]|  Transistor |
             [Motor -] --->| Collector(C)|
                           | Emitter (E) | ---> GND
                           +-------------+
```

### Wiring Checklist:
- [ ] Connect Motor (+) red wire to 3.3V or 5V pin.
- [ ] Connect Motor (-) black wire to Transistor Collector (middle pin of 2N2222 facing flat side).
- [ ] Connect Transistor Base (left pin) through a 1kΩ resistor to GPIO 16 (ESP32) or D2 (Nano 33 BLE).
- [ ] Connect Transistor Emitter (right pin) to GND.
- [ ] Connect 1N4007 Diode across Motor (+) and Motor (-), with cathode stripe facing Motor (+).

---

## Phase 4: Full End-to-End Test Protocol

1. **Start Backend Server:**
   - Deploy server to Railway/Render (or run `npm start` in `backend/` folder locally).
2. **Open Mobile Apps:**
   - Install Flutter app on Phone A and Phone B.
   - On Phone A: Enter Name `Ahmed`, Pair Code `102030` -> Click **Save & Synchronize**.
   - On Phone B: Enter Name `Sarah`, Pair Code `102030` -> Click **Save & Synchronize**.
3. **Connect Bluetooth Bracelets:**
   - Click **Connect** in Phone A app -> Select `BondTouch_ESP32` or `Nano33`.
   - Click **Connect** in Phone B app -> Select `BondTouch_ESP32` or `Nano33`.
4. **Execute Touch Test:**
   - Tap Bracelet A (or press manual button in Phone A app).
   - **Result:** Within < 2 seconds, Bracelet B (connected to Phone B) vibrates for 500ms and blinks LED! ❤️

---

## Phase 5: Troubleshooting Matrix

| Problem | Probable Cause | Fix Solution |
|---|---|---|
| Phone cannot find BLE bracelet | Bluetooth disabled on phone or location permission denied | Enable Bluetooth & Location permissions in Phone Settings. |
| Motor vibrates weakly | Motor connected to 3.3V instead of 5V or transistor base resistor too high | Connect Motor (+) to 5V pin or USB power rail. |
| Touch event doesn't reach partner | FCM Push Token not registered or different Pair Code | Check both phones share exact same 6-digit Pair Code. |
| Board resets when motor turns on | Motor drawing voltage spike from microcontroller | Ensure 1N4007 flyback diode is connected across motor terminals. |
