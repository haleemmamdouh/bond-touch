package com.bondtouch.app

import android.app.*
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.content.Intent
import android.os.Binder
import android.os.Build
import android.os.IBinder
import android.os.ParcelUuid
import android.util.Log
import androidx.core.app.NotificationCompat
import org.eclipse.paho.client.mqttv3.*
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence
import org.json.JSONObject
import java.util.*

class BondTouchService : Service() {

    companion object {
        const val TAG = "BondTouchService"
        const val NOTIFICATION_ID = 1001
        const val CHANNEL_ID = "bond_touch_foreground_channel"

        val SVC_UUID: UUID = UUID.fromString("4fafc201-1fb5-459e-8fcc-c5c9c331914b")
        val RX_UUID: UUID  = UUID.fromString("beb5483e-36e1-4688-b7f5-ea07361b26a8")
        val TX_UUID: UUID  = UUID.fromString("a3c87500-8ed3-4bdf-8a39-a01bebede295")
        val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

        var isServiceRunning = false
        var currentStatus = "Disconnected"
    }

    private val binder = LocalBinder()
    private var bluetoothAdapter: BluetoothAdapter? = null
    private var bluetoothGatt: BluetoothGatt? = null
    private var rxCharacteristic: BluetoothGattCharacteristic? = null
    private var mqttClient: MqttAsyncClient? = null

    private var myName = ""
    private var pairCode = ""
    private var topicName = ""
    private var partnerStatusTopic = ""
    private var lastTapTime = 0L
    private var partnerOnline = false

    inner class LocalBinder : Binder() {
        fun getService(): BondTouchService = this@BondTouchService
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onCreate() {
        super.onCreate()
        val btManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = btManager.adapter
        createNotificationChannel()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        intent?.let {
            myName = it.getStringExtra("NAME") ?: ""
            pairCode = it.getStringExtra("CODE") ?: ""
            if (myName.isNotEmpty() && pairCode.isNotEmpty()) {
                topicName = "bondtouch/pair_$pairCode"
                partnerStatusTopic = "bondtouch/status_$pairCode/+"
                startForeground(NOTIFICATION_ID, buildNotification("Connecting to Wearable & Cloud..."))
                isServiceRunning = true
                connectMQTT()
                scanAndConnectBLE()
            }
        }
        return START_STICKY
    }

    // ────────────────────────────────────────────────────────────
    // MQTT BACKGROUND CLIENT (PRESENCE & TOUCH ACK ROUTING)
    // ────────────────────────────────────────────────────────────
    private fun connectMQTT() {
        try {
            val serverUri = "tcp://broker.emqx.io:1883"
            val clientId = "bt_native_" + UUID.randomUUID().toString().substring(0, 8)
            mqttClient = MqttAsyncClient(serverUri, clientId, MemoryPersistence())

            val options = MqttConnectOptions().apply {
                isCleanSession = true
                connectionTimeout = 10
                keepAliveInterval = 30
            }

            mqttClient?.connect(options, null, object : IMqttActionListener {
                override fun onSuccess(asyncActionToken: IMqttToken?) {
                    Log.d(TAG, "MQTT Connected")
                    mqttClient?.subscribe(topicName, 0)
                    mqttClient?.subscribe(partnerStatusTopic, 0)

                    // Ask if partner is online right away
                    requestPartnerStatus()

                    if (rxCharacteristic != null) {
                        broadcastBLEStatus(true)
                    }
                }

                override fun onFailure(asyncActionToken: IMqttToken?, exception: Throwable?) {
                    Log.e(TAG, "MQTT Failed", exception)
                }
            })

            mqttClient?.setCallback(object : MqttCallback {
                override fun connectionLost(cause: Throwable?) {
                    Log.w(TAG, "MQTT Connection lost, retrying...")
                }

                override fun messageArrived(topic: String?, message: MqttMessage?) {
                    message?.let {
                        try {
                            val json = JSONObject(String(it.payload))
                            val sender = json.optString("sender", "")
                            val type = json.optString("type", "")

                            // Status query from partner
                            if (type == "request_status" && !sender.equals(myName, ignoreCase = true)) {
                                if (rxCharacteristic != null) {
                                    broadcastBLEStatus(true)
                                }
                                return
                            }

                            // Ignore my own broadcasts
                            if (sender.equals(myName, ignoreCase = true)) return

                            // 🟣 Partner came online -> PURPLE flash + buzz on ESP32!
                            if (type == "ble_connect") {
                                partnerOnline = true
                                sendToESP("PARTNER_ON")
                                Log.d(TAG, "Partner $sender connected -> Sent PARTNER_ON to ESP.")
                                return
                            }

                            // 🔴 Partner disconnected
                            if (type == "ble_disconnect") {
                                partnerOnline = false
                                Log.d(TAG, "Partner $sender disconnected -> Marked offline.")
                                return
                            }

                            if (type == "waveform") {
                                val data = json.optString("data", "")
                                val color = json.optString("color", "255,0,0")
                                sendToESP("WAVE:3:$color:$data")
                            } else {
                                sendToESP("TOUCH")
                            }
                        } catch (e: Exception) {
                            Log.e(TAG, "Error handling MQTT message", e)
                        }
                    }
                }

                override fun deliveryComplete(token: IMqttDeliveryToken?) {}
            })
        } catch (e: Exception) {
            Log.e(TAG, "MQTT Init Error", e)
        }
    }

    private fun requestPartnerStatus() {
        if (mqttClient != null && mqttClient?.isConnected == true && pairCode.isNotEmpty()) {
            try {
                val payload = JSONObject().apply {
                    put("type", "request_status")
                    put("sender", myName)
                }
                mqttClient?.publish(topicName, MqttMessage(payload.toString().toByteArray()))
            } catch (e: Exception) {
                Log.e(TAG, "Failed to request partner status", e)
            }
        }
    }

    private fun broadcastBLEStatus(connected: Boolean) {
        if (mqttClient != null && mqttClient?.isConnected == true && pairCode.isNotEmpty()) {
            try {
                val myStatusTopic = "bondtouch/status_$pairCode/$myName"
                val payload = JSONObject().apply {
                    put("type", if (connected) "ble_connect" else "ble_disconnect")
                    put("sender", myName)
                }
                val msg = MqttMessage(payload.toString().toByteArray()).apply {
                    isRetained = true
                }
                mqttClient?.publish(myStatusTopic, msg)
                Log.d(TAG, "Published status ($myStatusTopic): ${if (connected) "ble_connect" else "ble_disconnect"}")
            } catch (e: Exception) {
                Log.e(TAG, "Broadcast BLE status error", e)
            }
        }
    }

    private fun sendTouchOverMQTT() {
        val now = System.currentTimeMillis()
        if (now - lastTapTime < 800) return
        lastTapTime = now

        if (partnerOnline && mqttClient != null && mqttClient?.isConnected == true && topicName.isNotEmpty()) {
            try {
                val payload = JSONObject().apply {
                    put("sender", myName)
                    put("timestamp", now)
                }
                mqttClient?.publish(topicName, MqttMessage(payload.toString().toByteArray()))
                sendToESP("ACK_OK") // Touch delivered -> GREEN flash!
                Log.d(TAG, "Touch delivered over MQTT -> Sent ACK_OK (Green) to ESP.")
            } catch (e: Exception) {
                sendToESP("ACK_FAIL") // Touch failed -> RED strobe!
                Log.e(TAG, "Failed to send touch over MQTT", e)
            }
        } else {
            sendToESP("ACK_FAIL") // Partner offline -> RED strobe!
            Log.w(TAG, "Partner offline -> Sent ACK_FAIL (Red Strobe) to ESP.")
        }
    }

    // ────────────────────────────────────────────────────────────
    // NATIVE BLUETOOTH LE AUTO-CONNECT LOOP & NOTIFICATIONS
    // ────────────────────────────────────────────────────────────
    private fun scanAndConnectBLE() {
        val scanner = bluetoothAdapter?.bluetoothLeScanner ?: return
        val filter = ScanFilter.Builder()
            .setServiceUuid(ParcelUuid(SVC_UUID))
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        scanner.startScan(listOf(filter), settings, object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult?) {
                result?.device?.let { device ->
                    scanner.stopScan(this)
                    connectToGatt(device)
                }
            }
        })
    }

    private fun connectToGatt(device: BluetoothDevice) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            bluetoothGatt = device.connectGatt(this, true, gattCallback, BluetoothDevice.TRANSPORT_LE)
        } else {
            bluetoothGatt = device.connectGatt(this, true, gattCallback)
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                Log.d(TAG, "GATT Connected! Discovering services...")
                currentStatus = "Connected to Wearable"
                updateNotification("🟢 Wearable Connected 24/7 (Background Active)")
                gatt?.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                Log.w(TAG, "GATT Disconnected! Auto-reconnecting...")
                currentStatus = "Reconnecting..."
                updateNotification("🟡 Wearable Disconnected — Auto-reconnecting...")
                rxCharacteristic = null
                broadcastBLEStatus(false)
                gatt?.connect()
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
            if (status == BluetoothGatt.GATT_SUCCESS) {
                val service = gatt?.getService(SVC_UUID)
                rxCharacteristic = service?.getCharacteristic(RX_UUID)

                val txChar = service?.getCharacteristic(TX_UUID)
                txChar?.let {
                    gatt.setCharacteristicNotification(it, true)
                    val descriptor = it.getDescriptor(CCCD_UUID)
                    if (descriptor != null) {
                        descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                        gatt.writeDescriptor(descriptor)
                    }
                }

                // Notify cloud that OUR BLE connected -> Partner ESP turns PURPLE!
                broadcastBLEStatus(true)
                requestPartnerStatus()
            }
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?) {
            characteristic?.let {
                val str = String(it.value ?: byteArrayOf())
                Log.d(TAG, "ESP BLE Notification received: $str")
                if (str.contains("BLE_ON")) {
                    broadcastBLEStatus(true)
                    return
                }
                // Physical ESP button pressed!
                sendTouchOverMQTT()
            }
        }
    }

    fun sendToESP(payload: String) {
        val gatt = bluetoothGatt ?: return
        val rx = rxCharacteristic ?: return
        try {
            rx.value = payload.toByteArray()
            gatt.writeCharacteristic(rx)
        } catch (e: Exception) {
            Log.e(TAG, "BLE Write failed", e)
        }
    }

    // ────────────────────────────────────────────────────────────
    // PERSISTENT NOTIFICATION & SERVICE LIFECYCLE
    // ────────────────────────────────────────────────────────────
    private fun createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val channel = NotificationChannel(
                CHANNEL_ID,
                "Bond Touch Persistent Wearable Service",
                NotificationManager.IMPORTANCE_LOW
            )
            val manager = getSystemService(NotificationManager::class.java)
            manager.createNotificationChannel(channel)
        }
    }

    private fun buildNotification(text: String): Notification {
        val intent = Intent(this, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(
            this, 0, intent,
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )

        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setContentTitle("Bond Touch Native Service")
            .setContentText(text)
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .setContentIntent(pendingIntent)
            .build()
    }

    private fun updateNotification(text: String) {
        val notification = buildNotification(text)
        val manager = getSystemService(NotificationManager::class.java)
        manager.notify(NOTIFICATION_ID, notification)
    }

    override fun onDestroy() {
        super.onDestroy()
        isServiceRunning = false
        broadcastBLEStatus(false)
        try {
            mqttClient?.disconnect()
            bluetoothGatt?.close()
        } catch (_: Exception) {}
    }
}
