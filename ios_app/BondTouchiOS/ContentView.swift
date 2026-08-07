import SwiftUI

struct ContentView: View {
    @EnvironmentObject var ble: BLEManager
    @EnvironmentObject var mqtt: MQTTManager

    @State private var nameInput = ""
    @State private var codeInput = ""
    @State private var sessionStarted = false

    var body: some View {
        ZStack {
            Color(hex: "#0a0a0f").ignoresSafeArea()

            if !sessionStarted {
                loginView
            } else {
                dashView
            }
        }
        .preferredColorScheme(.dark)
    }

    // ── LOGIN ──────────────────────────────────────────────────
    var loginView: some View {
        VStack(spacing: 20) {
            Spacer()
            Text("Bond Touch")
                .font(.system(size: 28, weight: .heavy))
                .foregroundStyle(LinearGradient(
                    colors: [Color(hex:"#ff3358"), Color(hex:"#ff8c55")],
                    startPoint: .leading, endPoint: .trailing))

            Text("Long Distance Wearable")
                .font(.system(size: 12, weight: .semibold))
                .foregroundColor(Color(hex:"#4a4a5a"))
                .tracking(2)
                .textCase(.uppercase)

            VStack(spacing: 12) {
                TextField("Your Name (e.g. Ahmed)", text: $nameInput)
                    .padding(14)
                    .background(Color(hex:"#18181f"))
                    .cornerRadius(10)
                    .foregroundColor(.white)

                TextField("6-Digit Pair Code (e.g. 102030)", text: $codeInput)
                    .keyboardType(.numberPad)
                    .padding(14)
                    .background(Color(hex:"#18181f"))
                    .cornerRadius(10)
                    .foregroundColor(.white)

                Button(action: startSession) {
                    Text("Start Session →")
                        .font(.system(size: 15, weight: .bold))
                        .foregroundColor(.white)
                        .frame(maxWidth: .infinity)
                        .padding(14)
                        .background(LinearGradient(
                            colors: [Color(hex:"#ff3358"), Color(hex:"#cc1f3a")],
                            startPoint: .leading, endPoint: .trailing))
                        .cornerRadius(10)
                }
            }
            .padding(.horizontal, 24)

            Spacer()
        }
    }

    // ── DASHBOARD ─────────────────────────────────────────────
    var dashView: some View {
        ScrollView {
            VStack(spacing: 16) {
                // Header
                VStack(spacing: 4) {
                    Text("Bond Touch")
                        .font(.system(size: 22, weight: .heavy))
                        .foregroundStyle(LinearGradient(
                            colors: [Color(hex:"#ff3358"), Color(hex:"#ff8c55")],
                            startPoint: .leading, endPoint: .trailing))
                    Text("Hi, \(mqtt.myName)")
                        .font(.system(size: 12))
                        .foregroundColor(Color(hex:"#8a8a9a"))
                }
                .padding(.top, 20)

                // Status Grid
                HStack(spacing: 8) {
                    statusPill(label: "Cloud", active: mqtt.connected, icon: "cloud.fill")
                    statusPill(label: "My ESP", active: ble.connected, icon: "antenna.radiowaves.left.and.right")
                    statusPill(label: "Partner", active: mqtt.partnerOnline, icon: "heart.fill")
                }
                .padding(.horizontal, 16)

                // Partner Banner
                HStack(spacing: 12) {
                    ZStack {
                        Circle()
                            .fill(Color(hex:"#18181f"))
                            .frame(width: 36, height: 36)
                        Text("❤️")
                            .font(.system(size: 16))
                    }
                    VStack(alignment: .leading, spacing: 2) {
                        Text(mqtt.partnerName.isEmpty ? "Partner" : mqtt.partnerName)
                            .font(.system(size: 13, weight: .bold))
                        Text(mqtt.partnerOnline ? "Online & Connected" : "Offline")
                            .font(.system(size: 11))
                            .foregroundColor(Color(hex:"#8a8a9a"))
                    }
                    Spacer()
                }
                .padding(14)
                .background(mqtt.partnerOnline
                    ? Color(hex:"#22c55e").opacity(0.08)
                    : Color(hex:"#ff3358").opacity(0.06))
                .overlay(RoundedRectangle(cornerRadius: 10)
                    .stroke(mqtt.partnerOnline
                        ? Color(hex:"#22c55e").opacity(0.4)
                        : Color(hex:"#ff3358").opacity(0.3), lineWidth: 1))
                .cornerRadius(10)
                .padding(.horizontal, 16)

                // BLE Connect Button
                if !ble.connected {
                    Button(action: { ble.startScan() }) {
                        HStack {
                            Image(systemName: "wave.3.right")
                            Text("Pair ESP32 via Bluetooth")
                                .fontWeight(.bold)
                        }
                        .foregroundColor(.white)
                        .frame(maxWidth: .infinity)
                        .padding(14)
                        .background(Color(hex:"#18181f"))
                        .cornerRadius(10)
                        .overlay(RoundedRectangle(cornerRadius: 10)
                            .stroke(Color(hex:"#3b82f6").opacity(0.5), lineWidth: 1))
                    }
                    .padding(.horizontal, 16)
                }

                // Touch Button
                Button(action: sendTouch) {
                    ZStack {
                        Circle()
                            .fill(LinearGradient(
                                colors: [Color(hex:"#ff3358"), Color(hex:"#cc1f3a")],
                                startPoint: .top, endPoint: .bottom))
                            .frame(width: 180, height: 180)
                            .shadow(color: Color(hex:"#ff3358").opacity(0.4), radius: 30)
                        Text("❤️")
                            .font(.system(size: 60))
                    }
                }
                .padding(.vertical, 10)

                Text("Tap to send a touch to your partner")
                    .font(.system(size: 12))
                    .foregroundColor(Color(hex:"#4a4a5a"))

                // Status Log
                Text(ble.statusLog)
                    .font(.system(size: 12, design: .monospaced))
                    .foregroundColor(Color(hex:"#22c55e"))
                    .multilineTextAlignment(.center)
                    .padding(14)
                    .frame(maxWidth: .infinity)
                    .background(Color(hex:"#111118"))
                    .cornerRadius(10)
                    .padding(.horizontal, 16)
                    .padding(.bottom, 30)
            }
        }
    }

    func statusPill(label: String, active: Bool, icon: String) -> some View {
        VStack(spacing: 6) {
            Circle()
                .fill(active ? Color(hex:"#22c55e") : Color(hex:"#4a4a5a"))
                .frame(width: 7, height: 7)
                .shadow(color: active ? Color(hex:"#22c55e").opacity(0.6) : .clear, radius: 4)
            Text(label)
                .font(.system(size: 9, weight: .bold))
                .foregroundColor(Color(hex:"#4a4a5a"))
                .textCase(.uppercase)
                .tracking(1)
            Text(active ? "On" : "Off")
                .font(.system(size: 11, weight: .bold))
                .foregroundColor(Color(hex:"#8a8a9a"))
        }
        .frame(maxWidth: .infinity)
        .padding(.vertical, 10)
        .background(Color(hex:"#18181f"))
        .cornerRadius(10)
        .overlay(RoundedRectangle(cornerRadius: 10)
            .stroke(Color.white.opacity(0.08), lineWidth: 1))
    }

    func startSession() {
        let name = nameInput.trimmingCharacters(in: .whitespaces)
        let code = codeInput.trimmingCharacters(in: .whitespaces)
        guard !name.isEmpty, code.count >= 4 else { return }
        mqtt.configure(name: name, pairCode: code)
        mqtt.connect()
        sessionStarted = true
    }

    func sendTouch() {
        guard mqtt.connected else { return }
        mqtt.publishTouch()
        ble.sendToESP("TOUCH")
    }
}

// ── Color Hex Extension ──────────────────────────────────────
extension Color {
    init(hex: String) {
        let hex = hex.trimmingCharacters(in: CharacterSet.alphanumerics.inverted)
        var int: UInt64 = 0
        Scanner(string: hex).scanHexInt64(&int)
        let a, r, g, b: UInt64
        switch hex.count {
        case 3:  (a, r, g, b) = (255, (int>>8)*17, (int>>4&0xF)*17, (int&0xF)*17)
        case 6:  (a, r, g, b) = (255, int>>16, int>>8&0xFF, int&0xFF)
        case 8:  (a, r, g, b) = (int>>24, int>>16&0xFF, int>>8&0xFF, int&0xFF)
        default: (a, r, g, b) = (255, 0, 0, 0)
        }
        self.init(.sRGB, red: Double(r)/255, green: Double(g)/255, blue: Double(b)/255, opacity: Double(a)/255)
    }
}
