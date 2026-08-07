package com.bondtouch.app

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat

class MainActivity : AppCompatActivity() {

    private lateinit var nameInput: EditText
    private lateinit var codeInput: EditText
    private lateinit var btnStart: Button
    private lateinit var statusText: TextView

    companion object {
        private const val PERMISSION_REQUEST_CODE = 101
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        nameInput = findViewById(R.id.nameInput)
        codeInput = findViewById(R.id.codeInput)
        btnStart = findViewById(R.id.btnStart)
        statusText = findViewById(R.id.statusText)

        checkAndRequestPermissions()

        btnStart.setOnClickListener {
            val name = nameInput.text.toString().trim()
            val code = codeInput.text.toString().trim()

            if (name.isEmpty() || code.length < 4) {
                Toast.makeText(this, "Enter name and valid pair code!", Toast.LENGTH_SHORT).show()
                return@setOnClickListener
            }

            startBondTouchService(name, code)
        }
    }

    private fun startBondTouchService(name: String, code: String) {
        val serviceIntent = Intent(this, BondTouchService::class.java).apply {
            putExtra("NAME", name)
            putExtra("CODE", code)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            startForegroundService(serviceIntent)
        } else {
            startService(serviceIntent)
        }

        statusText.text = "🟢 Foreground Service Running 24/7!\nBluetooth & MQTT Active in OS Kernel."
        Toast.makeText(this, "Service started! You can swipe away the app window.", Toast.LENGTH_LONG).show()
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
