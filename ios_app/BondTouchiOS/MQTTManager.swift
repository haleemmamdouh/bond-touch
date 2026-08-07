import Foundation
import Combine

class MQTTManager: NSObject, ObservableObject, URLSessionWebSocketDelegate {

    @Published var connected = false
    @Published var partnerOnline = false
    @Published var partnerName = ""
    @Published var myName = ""

    private var webSocketTask: URLSessionWebSocketTask?
    private var pairCode = ""
    private var myTopic = ""
    private var pairTopic = ""
    private var myStatusTopic = ""
    private var partnerStatusTopic = ""
    private var lastTapTime: Date = .distantPast
    private var pingTimer: Timer?

    var onTouchReceived: (() -> Void)?
    var bleConnected: Bool = false

    func configure(name: String, pairCode: String) {
        self.myName = name
        self.pairCode = pairCode
        self.myTopic = "bondtouch/pair_\(pairCode)"
        self.pairTopic = "bondtouch/pair_\(pairCode)"
        self.myStatusTopic = "bondtouch/status_\(pairCode)/\(name)"
        self.partnerStatusTopic = "bondtouch/status_\(pairCode)"
    }

    func connect() {
        guard let url = URL(string: "wss://broker.emqx.io:8084/mqtt") else { return }
        let session = URLSession(configuration: .default, delegate: self, delegateQueue: OperationQueue.main)
        webSocketTask = session.webSocketTask(with: url)
        webSocketTask?.resume()
        receiveMessage()
        sendConnectPacket()
    }

    private func sendConnectPacket() {
        let clientID = "bt_ios_" + String(UUID().uuidString.prefix(6))
        var packet = Data([0x10]) // CONNECT
        var variableHeaderAndPayload = Data([
            0x00, 0x04, 0x4D, 0x51, 0x54, 0x54, // Protocol Name: MQTT
            0x04, // Protocol Level: 4 (3.1.1)
            0x02, // Connect Flags: Clean Session
            0x00, 0x1E // Keep Alive: 30s
        ])
        
        // Client ID
        let idData = clientID.data(using: .utf8)!
        variableHeaderAndPayload.append(UInt8(idData.count >> 8))
        variableHeaderAndPayload.append(UInt8(idData.count & 0xFF))
        variableHeaderAndPayload.append(idData)

        encodeRemainingLength(variableHeaderAndPayload.count, into: &packet)
        packet.append(variableHeaderAndPayload)

        webSocketTask?.send(.data(packet)) { error in
            if error == nil {
                DispatchQueue.main.async {
                    self.connected = true
                    self.subscribe(topic: self.pairTopic)
                    self.subscribe(topic: self.partnerStatusTopic)
                    self.startPingTimer()
                }
            }
        }
    }

    private func subscribe(topic: String) {
        var packet = Data([0x82]) // SUBSCRIBE
        var payload = Data([0x00, 0x01]) // Packet ID: 1
        
        let topicData = topic.data(using: .utf8)!
        payload.append(UInt8(topicData.count >> 8))
        payload.append(UInt8(topicData.count & 0xFF))
        payload.append(topicData)
        payload.append(0x00) // QoS 0

        encodeRemainingLength(payload.count, into: &packet)
        packet.append(payload)

        webSocketTask?.send(.data(packet), completionHandler: { _ in })
    }

    func publishTouch() {
        let now = Date()
        guard now.timeIntervalSince(lastTapTime) > 0.8 else { return }
        lastTapTime = now
        let json: [String: Any] = ["sender": myName, "timestamp": Int(now.timeIntervalSince1970)]
        if let data = try? JSONSerialization.data(withJSONObject: json),
           let str = String(data: data, encoding: .utf8) {
            publish(topic: pairTopic, payload: str)
        }
    }

    func broadcastBLEStatus(_ isConnected: Bool) {
        guard connected else { return }
        let json: [String: Any] = ["type": isConnected ? "ble_connect" : "ble_disconnect", "sender": myName]
        if let data = try? JSONSerialization.data(withJSONObject: json),
           let str = String(data: data, encoding: .utf8) {
            publish(topic: myStatusTopic, payload: str)
        }
    }

    private func publish(topic: String, payload: String) {
        var packet = Data([0x30]) // PUBLISH QoS 0
        var payloadData = Data()

        let topicData = topic.data(using: .utf8)!
        payloadData.append(UInt8(topicData.count >> 8))
        payloadData.append(UInt8(topicData.count & 0xFF))
        payloadData.append(topicData)
        payloadData.append(payload.data(using: .utf8)!)

        encodeRemainingLength(payloadData.count, into: &packet)
        packet.append(payloadData)

        webSocketTask?.send(.data(packet)) { _ in }
    }

    private func receiveMessage() {
        webSocketTask?.receive { [weak self] result in
            guard let self = self else { return }
            switch result {
            case .success(let message):
                switch message {
                case .data(let data):
                    self.parseMQTTPacket(data)
                case .string(let str):
                    if let data = str.data(using: .utf8) {
                        self.parseMQTTPacket(data)
                    }
                @unknown default:
                    break
                }
                self.receiveMessage()
            case .failure:
                DispatchQueue.main.async { self.connected = false }
            }
        }
    }

    private func parseMQTTPacket(_ data: Data) {
        guard data.count > 2 else { return }
        let packetType = data[0] >> 4
        if packetType == 3 { // PUBLISH packet
            // Extract payload from publish packet
            var index = 1
            while index < data.count && (data[index] & 0x80) != 0 { index += 1 }
            index += 1
            guard index + 2 < data.count else { return }
            let topicLen = Int(data[index]) << 8 | Int(data[index+1])
            index += 2 + topicLen
            guard index < data.count else { return }

            let payloadData = data.subdata(in: index..<data.count)
            if let payloadStr = String(data: payloadData, encoding: .utf8) {
                handleIncomingMessage(payloadStr)
            }
        }
    }

    private func handleIncomingMessage(_ payload: String) {
        guard let data = payload.data(using: .utf8),
              let json = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let sender = json["sender"] as? String else { return }

        if sender.lowercased() == myName.lowercased() { return }

        let type = json["type"] as? String ?? ""

        if type == "ble_connect" {
            DispatchQueue.main.async {
                self.partnerOnline = true
                self.partnerName = sender
            }
            onTouchReceived?()
            return
        }

        if type == "ble_disconnect" {
            DispatchQueue.main.async {
                self.partnerOnline = false
                self.partnerName = sender
            }
            return
        }

        DispatchQueue.main.async { self.partnerName = sender }
        onTouchReceived?()
    }

    private func startPingTimer() {
        pingTimer?.invalidate()
        pingTimer = Timer.scheduledTimer(withTimeInterval: 20.0, repeats: true) { [weak self] _ in
            self?.webSocketTask?.send(.data(Data([0xC0, 0x00]))) { _ in } // PINGREQ
        }
    }

    private func encodeRemainingLength(_ length: Int, into data: inout Data) {
        var len = length
        repeat {
            var digit = UInt8(len % 128)
            len /= 128
            if len > 0 { digit |= 0x80 }
            data.append(digit)
        } while len > 0
    }
}
