# Bond Touch — Native iOS App

## 📂 Project Structure
```
BondTouchiOS/
├── BondTouchApp.swift      ← App entry point
├── ContentView.swift       ← Login + Dashboard UI (SwiftUI)
├── BLEManager.swift        ← CoreBluetooth 24/7 background BLE
├── MQTTManager.swift       ← MQTT cloud touch relay (CocoaMQTT)
└── Info.plist              ← Bluetooth background permissions
```

## ⚡ How to Build on Codemagic (No Mac Needed)

1. Create a GitHub repo and push the `ios_app/` folder.
2. Go to **[https://codemagic.io](https://codemagic.io)** → Sign up free.
3. Connect your GitHub repo.
4. Add **CocoaMQTT** as a Swift Package:
   - URL: `https://github.com/emqx/CocoaMQTT`
   - Version: `2.1.0`
5. Set **Bundle ID**: `com.bondtouch.app`
6. Click **Start Build** → Codemagic builds the `.ipa` on a Mac server.
7. Download and install via **TestFlight** on your iPhone!

## 🔑 Features
- ✅ **CoreBluetooth** native BLE — 24/7 background connection
- ✅ **`bluetooth-central`** background mode — BLE NEVER disconnects
- ✅ **CocoaMQTT** — real-time touch relay over cloud
- ✅ **Auto-reconnect** — iOS reconnects to ESP32 automatically
- ✅ **Physical button** relay — pressing ESP sends touch over MQTT
- ✅ **Partner Green LED sync** — partner's ESP turns green ONLY when you connect
- ✅ **Cross-platform** — iPhone ↔ Android works perfectly via MQTT

## 📱 Pair Code Logic
Both partners enter the SAME 6-digit pair code.
Touches travel: **Your ESP → Your Phone → MQTT Cloud → Partner Phone → Partner ESP ❤️**
