import Foundation
import CoreBluetooth
import Combine

class BLEManager: NSObject, ObservableObject, CBCentralManagerDelegate, CBPeripheralDelegate {

    @Published var connected = false
    @Published var statusLog = "Pair your ESP32 Bluetooth to begin."

    private var centralManager: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var rxCharacteristic: CBCharacteristic?

    private let serviceUUID = CBUUID(string: "4fafc201-1fb5-459e-8fcc-c5c9c331914b")
    private let rxUUID      = CBUUID(string: "beb5483e-36e1-4688-b7f5-ea07361b26a8")
    private let txUUID      = CBUUID(string: "a3c87500-8ed3-4bdf-8a39-a01bebede295")

    // Callback to report ESP touch button press to MQTT
    var onESPButtonPressed: (() -> Void)?
    var onBLEConnected: (() -> Void)?
    var onBLEDisconnected: (() -> Void)?

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil,
            options: [CBCentralManagerOptionRestoreIdentifierKey: "BondTouchCentral"])
    }

    func startScan() {
        guard centralManager.state == .poweredOn else { return }
        statusLog = "🔍 Scanning for ESP32..."
        centralManager.scanForPeripherals(withServices: [serviceUUID], options: nil)
    }

    // ── CBCentralManagerDelegate ──────────────────────────────
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            startScan()
        }
    }

    func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        // State restoration for background reconnect
        if let peripherals = dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral], let p = peripherals.first {
            self.peripheral = p
            p.delegate = self
        }
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral,
                        advertisementData: [String: Any], rssi RSSI: NSNumber) {
        central.stopScan()
        self.peripheral = peripheral
        peripheral.delegate = self
        central.connect(peripheral, options: [
            CBConnectPeripheralOptionNotifyOnConnectionKey: true,
            CBConnectPeripheralOptionNotifyOnDisconnectionKey: true
        ])
        statusLog = "🔗 Connecting to ESP32..."
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        DispatchQueue.main.async {
            self.connected = false
            self.rxCharacteristic = nil
            self.statusLog = "🟡 ESP32 disconnected — auto-reconnecting..."
            self.onBLEDisconnected?()
        }
        // Auto-reconnect loop — iOS CoreBluetooth auto-reconnects with stored peripheral
        centralManager.connect(peripheral, options: nil)
    }

    // ── CBPeripheralDelegate ──────────────────────────────────
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        guard let services = peripheral.services else { return }
        for service in services where service.uuid == serviceUUID {
            peripheral.discoverCharacteristics([rxUUID, txUUID], for: service)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        guard let chars = service.characteristics else { return }
        for char in chars {
            if char.uuid == rxUUID { rxCharacteristic = char }
            if char.uuid == txUUID {
                peripheral.setNotifyValue(true, for: char)
            }
        }
        DispatchQueue.main.async {
            self.connected = true
            self.statusLog = "🔵 ESP32 connected via Bluetooth!"
            self.onBLEConnected?()
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard let data = characteristic.value, let str = String(data: data, encoding: .utf8) else { return }
        if str.contains("BLE_ON") {
            onBLEConnected?()
            return
        }
        // Physical ESP button pressed → relay touch over MQTT
        onESPButtonPressed?()
    }

    func sendToESP(_ payload: String) {
        guard let p = peripheral, let char = rxCharacteristic, connected else { return }
        if let data = payload.data(using: .utf8) {
            p.writeValue(data, for: char, type: .withResponse)
        }
    }
}
