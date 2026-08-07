import Foundation
import CocoaMQTT

class MQTTManager: ObservableObject {

    @Published var connected = false
    @Published var partnerOnline = false
    @Published var partnerName = ""
    @Published var myName = ""

    private var mqtt: CocoaMQTT?
    private var pairCode = ""
    private var myTopic = ""
    private var pairTopic = ""
    private var myStatusTopic = ""
    private var partnerStatusTopic = ""
    private var lastTapTime: Date = .distantPast

    // Injected callbacks (set by ContentView or App)
    var onTouchReceived: (() -> Void)?
    var bleConnected: Bool = false

    func configure(name: String, pairCode: String) {
        self.myName = name
        self.pairCode = pairCode
        self.myTopic = "bondtouch/pair_\(pairCode)"
        self.pairTopic = "bondtouch/pair_\(pairCode)"
        self.myStatusTopic = "bondtouch/status_\(pairCode)/\(name)"
        self.partnerStatusTopic = "bondtouch/status_\(pairCode)/+"
    }

    func connect() {
        let clientID = "bt_ios_" + UUID().uuidString.prefix(8)
        mqtt = CocoaMQTT(clientID: String(clientID), host: "broker.emqx.io", port: 1883)
        mqtt?.cleanSession = true
        mqtt?.keepAlive = 30
        mqtt?.delegate = self
        _ = mqtt?.connect()
    }

    func publishTouch() {
        let now = Date()
        guard now.timeIntervalSince(lastTapTime) > 0.8 else { return }
        lastTapTime = now
        let payload = ["sender": myName, "timestamp": Int(now.timeIntervalSince1970)] as [String: Any]
        if let data = try? JSONSerialization.data(withJSONObject: payload),
           let str = String(data: data, encoding: .utf8) {
            mqtt?.publish(pairTopic, withString: str, qos: .qos0, retained: false)
        }
    }

    func broadcastBLEStatus(_ connected: Bool) {
        guard mqtt?.connState == .connected else { return }
        let payload = ["type": connected ? "ble_connect" : "ble_disconnect", "sender": myName]
        if let data = try? JSONSerialization.data(withJSONObject: payload),
           let str = String(data: data, encoding: .utf8) {
            mqtt?.publish(myStatusTopic, withString: str, qos: .qos0, retained: true)
        }
    }

    private func handleMessage(topic: String, payload: String) {
        guard let data = payload.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let sender = json["sender"] as? String else { return }

        if sender.lowercased() == myName.lowercased() { return }

        let type = json["type"] as? String ?? ""

        // Status ping request
        if type == "request_status" {
            if bleConnected { broadcastBLEStatus(true) }
            return
        }

        // 🟢 Partner connected — send GREEN to ESP
        if type == "ble_connect" {
            DispatchQueue.main.async {
                self.partnerOnline = true
                self.partnerName = sender
            }
            onTouchReceived?()  // Will call BLE sendToESP("PARTNER_BLE_ON")
            return
        }

        // 🔴 Partner disconnected — no green light!
        if type == "ble_disconnect" {
            DispatchQueue.main.async {
                self.partnerOnline = false
                self.partnerName = sender
            }
            return
        }

        if type == "waveform", let waveData = json["data"] as? String, let color = json["color"] as? String {
            onTouchReceived?()  // Will call BLE sendToESP("WAVE:3:\(color):\(waveData)")
            return
        }

        // Regular touch
        DispatchQueue.main.async { self.partnerName = sender }
        onTouchReceived?()
    }
}

// ── CocoaMQTTDelegate ─────────────────────────────────────────
extension MQTTManager: CocoaMQTTDelegate {

    func mqtt(_ mqtt: CocoaMQTT, didConnectAck ack: CocoaMQTTConnAck) {
        guard ack == .accept else { return }
        DispatchQueue.main.async { self.connected = true }
        mqtt.subscribe(pairTopic, qos: .qos0)
        mqtt.subscribe(partnerStatusTopic, qos: .qos0)

        // Ping partner for status
        let ping = ["type": "request_status", "sender": myName]
        if let data = try? JSONSerialization.data(withJSONObject: ping),
           let str = String(data: data, encoding: .utf8) {
            mqtt.publish(pairTopic, withString: str)
        }
    }

    func mqtt(_ mqtt: CocoaMQTT, didReceiveMessage message: CocoaMQTTMessage, id: UInt16) {
        handleMessage(topic: message.topic, payload: message.string ?? "")
    }

    func mqtt(_ mqtt: CocoaMQTT, didUnsubscribeTopics topics: [String]) {}
    func mqtt(_ mqtt: CocoaMQTT, didSubscribeTopics success: NSDictionary, failed: [String]) {}
    func mqtt(_ mqtt: CocoaMQTT, didPublishMessage message: CocoaMQTTMessage, id: UInt16) {}
    func mqtt(_ mqtt: CocoaMQTT, didPublishAck id: UInt16) {}
    func mqttDidPing(_ mqtt: CocoaMQTT) {}
    func mqttDidReceivePong(_ mqtt: CocoaMQTT) {}
    func mqttDidDisconnect(_ mqtt: CocoaMQTT, withError err: Error?) {
        DispatchQueue.main.async { self.connected = false }
        DispatchQueue.main.asyncAfter(deadline: .now() + 3) { [weak self] in
            self?.mqtt?.connect()
        }
    }
}
