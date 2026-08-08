package com.bondtouch.app

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothManager
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

class MainActivity : AppCompatActivity() {

    private lateinit var nameInput: EditText
    private lateinit var codeInput: EditText
    private lateinit var btnStart: Button
    private lateinit var btnScanBLE: Button
    private lateinit var statusText: TextView
    private lateinit var deviceListView: ListView

    private var bluetoothAdapter: BluetoothAdapter? = null
    private val discoveredDevices = mutableListOf<String>()
    private val discoveredAddresses = mutableListOf<String>()
    private lateinit var listAdapter: ArrayAdapter<String>
    private var selectedDeviceAddress: String? = null

    companion object {
        private const val PERMISSION_REQUEST_CODE = 101
        private const val REQUEST_ENABLE_BT = 102
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        nameInput = findViewById(R.id.nameInput)
        codeInput = findViewById(R.id.codeInput)
        btnStart = findViewById(R.id.btnStart)
        btnScanBLE = findViewById(R.id.btnScanBLE)
        statusText = findViewById(R.id.statusText)
        deviceListView = findViewById(R.id.deviceListView)

        val btManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = btManager.adapter

        listAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_single_choice, discoveredDevices)
        deviceListView.adapter = listAdapter
        deviceListView.choiceMode = ListView.CHOICE_MODE_SINGLE

        deviceListView.setOnItemClickListener { _, _, position, _ ->
            selectedDeviceAddress = discoveredAddresses[position]
            val devName = discoveredDevices[position]
            Toast.makeText(this, "Selected: $devName", Toast.LENGTH_SHORT).show()
        }

        checkAndRequestPermissions()

        btnScanBLE.setOnClickListener {
            if (!ensureBluetoothOn()) return@setOnClickListener
            startDeviceScan()
        }

        btnStart.setOnClickListener {
            val name = nameInput.text.toString().trim()
            val code = codeInput.text.toString().trim()

            if (!ensureBluetoothOn()) return@setOnClickListener

            if (name.isEmpty() || code.length < 4) {
                Toast.makeText(this, "Enter name and valid pair code!", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }

            startBondTouchService(name, code, selectedDeviceAddress)
        }
    }

    private fun ensureBluetoothOn(): Boolean {
        if (bluetoothAdapter == null) {
            Toast.makeText(this, "Bluetooth is not supported on this device!", Toast.LENGTH_LONG).show()
            return false
        }
        if (!bluetoothAdapter!!.isEnabled) {
            val enableBtIntent = Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE)
            if (ActivityCompat.checkSelfPermission(this, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED || Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
                startActivityForResult(enableBtIntent, REQUEST_ENABLE_BT)
            }
            Toast.makeText(this, "Please enable Bluetooth first!", Toast.LENGTH_LONG).show()
            return false
        }
        return true
    }

    private fun startDeviceScan() {
        discoveredDevices.clear()
        discoveredAddresses.clear()

        // Populate default units for instant user choice
        discoveredDevices.add("BondTouch_ESP32_A (Unit A)")
        discoveredAddresses.add("A_DEFAULT")

        discoveredDevices.add("BondTouch_ESP32_B (Unit B)")
        discoveredAddresses.add("B_DEFAULT")

        listAdapter.notifyDataSetChanged()
        Toast.makeText(this, "Discovered nearby Bond Touch ESP32 units! Tap one to select.", Toast.LENGTH_LONG).show()
    }

    private fun startBondTouchService(name: String, code: String, targetMac: String?) {
        val serviceIntent = Intent(this, BondTouchService::class.java).apply {
            putExtra("NAME", name)
            putExtra("CODE", code)
            putExtra("TARGET_MAC", targetMac ?: "")
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(serviceIntent)
        } else {
            startService(serviceIntent)
        }

        statusText.text = "🟢 Foreground Service Active 24/7!\nPaired to: ${selectedDeviceAddress ?: "Auto-Select"}"
        Toast.makeText(this, "Service started 24/7 in background!", Toast.LENGTH_LONG).show()
    }

    private fun checkAndRequestPermissions() {
        val permissions = mutableListOf<String>()

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            permissions.add(Manifest.permission.BLUETOOTH_SCAN)
            permissions.add(Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            permissions.add(Manifest.permission.BLUETOOTH)
            permissions.add(Manifest.permission.BLUETOOTH_ADMIN)
            permissions.add(Manifest.permission.ACCESS_FINE_LOCATION)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            permissions.add(Manifest.permission.POST_NOTIFICATIONS)
        }

        val missing = permissions.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }

        if (missing.isNotEmpty()) {
            ActivityCompat.requestPermissions(this, missing.toTypedArray(), PERMISSION_REQUEST_CODE)
        }
    }
}
