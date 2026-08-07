import SwiftUI

@main
struct BondTouchApp: App {
    @StateObject private var bleManager = BLEManager()
    @StateObject private var mqttManager = MQTTManager()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(bleManager)
                .environmentObject(mqttManager)
        }
    }
}
