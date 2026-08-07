import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_messaging/firebase_messaging.dart';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';
import 'package:intl/intl.dart';

// ------------------------------------------------------------------------------------
// BLE UUID CONSTANTS (Must match Arduino/ESP32 firmware)
// ------------------------------------------------------------------------------------
const String SERVICE_UUID        = "19b10000-e8f2-537e-4f6c-d104768a1214";
const String TOUCH_NOTIFY_UUID   = "19b10001-e8f2-537e-4f6c-d104768a1214";
const String VIBRATE_CMD_UUID    = "19b10002-e8f2-537e-4f6c-d104768a1214";

// Global reference for background message handler
BluetoothCharacteristic? globalVibrateCharacteristic;

// ------------------------------------------------------------------------------------
// FIREBASE BACKGROUND MESSAGE HANDLER / معالج إشعارات الخلفية
// ------------------------------------------------------------------------------------
@pragma('vm:entry-point')
Future<void> _firebaseMessagingBackgroundHandler(RemoteMessage message) async {
  print("⚡ Received Background FCM Touch Payload: ${message.data}");
  
  if (message.data['type'] == 'TOUCH_EVENT') {
    if (globalVibrateCharacteristic != null) {
      try {
        await globalVibrateCharacteristic!.write([1]);
        print("✅ Sent BLE Vibrate Command to Bracelet from Background Handler!");
      } catch (e) {
        print("❌ Error writing BLE vibrate in background: $e");
      }
    }
  }
}

void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  
  try {
    await Firebase.initializeApp();
    FirebaseMessaging.onBackgroundMessage(_firebaseMessagingBackgroundHandler);
  } catch (e) {
    print("⚠️ Firebase initialization warning: $e");
  }

  runApp(const BondTouchApp());
}

class BondTouchApp extends StatelessWidget {
  const BondTouchApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Bond Touch Prototype',
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark().copyWith(
        scaffoldBackgroundColor: const Color(0xFF0D0D10),
        colorScheme: const ColorScheme.dark(
          primary: Color(0xFFE85D9A),
          secondary: Color(0xFF7C5CBF),
          surface: Color(0xFF16161C),
        ),
      ),
      home: const DashboardScreen(),
    );
  }
}

class DashboardScreen extends StatefulWidget {
  const DashboardScreen({super.key});

  @override
  State<DashboardScreen> createState() => _DashboardScreenState();
}

class _DashboardScreenState extends State<DashboardScreen> {
  final TextEditingController _nameController = TextEditingController();
  final TextEditingController _pairCodeController = TextEditingController();
  final TextEditingController _serverUrlController = TextEditingController(
    text: "https://bond-touch-production.up.railway.app" // Default fallback backend
  );

  bool _isPaired = false;
  bool _isBleConnected = false;
  bool _isScanning = false;
  String _statusMessage = "Disconnected";
  String _lastTouchReceived = "No touches yet";
  
  BluetoothDevice? _connectedDevice;
  BluetoothCharacteristic? _touchNotifyChar;
  BluetoothCharacteristic? _vibrateCmdChar;
  StreamSubscription? _bleScanSubscription;
  StreamSubscription? _bleNotifySubscription;

  String _fcmToken = "";

  @override
  void initState() {
    super.initState();
    _loadSavedData();
    _initFirebaseFCM();
  }

  @override
  void dispose() {
    _bleScanSubscription?.cancel();
    _bleNotifySubscription?.cancel();
    _nameController.dispose();
    _pairCodeController.dispose();
    _serverUrlController.dispose();
    super.dispose();
  }

  // Load saved credentials from local storage
  Future<void> _loadSavedData() async {
    final prefs = await SharedPreferences.getInstance();
    setState(() {
      _nameController.text = prefs.getString('userName') ?? '';
      _pairCodeController.text = prefs.getString('pairCode') ?? '';
      _serverUrlController.text = prefs.getString('serverUrl') ?? _serverUrlController.text;
      _isPaired = _nameController.text.isNotEmpty && _pairCodeController.text.isNotEmpty;
    });
  }

  // Initialize Firebase FCM Push Notifications
  Future<void> _initFirebaseFCM() async {
    try {
      FirebaseMessaging messaging = FirebaseMessaging.instance;
      NotificationSettings settings = await messaging.requestPermission(
        alert: true,
        badge: true,
        sound: true,
      );

      if (settings.authorizationStatus == AuthorizationStatus.authorized) {
        String? token = await messaging.getToken();
        if (token != null) {
          setState(() {
            _fcmToken = token;
          });
          print("🔑 FCM Push Token: $token");
          _registerFcmTokenWithBackend();
        }
      }

      // Handle FCM Foreground Messages (App open)
      FirebaseMessaging.onMessage.listen((RemoteMessage message) {
        print("📩 Foreground FCM Touch Message: ${message.data}");
        if (message.data['type'] == 'TOUCH_EVENT') {
          String sender = message.data['senderName'] ?? 'Partner';
          _handleIncomingPartnerTouch(sender);
        }
      });

    } catch (e) {
      print("⚠️ FCM setup note: $e");
    }
  }

  // Register device FCM Push Token to Express backend
  Future<void> _registerFcmTokenWithBackend() async {
    if (_nameController.text.isEmpty || _pairCodeController.text.isEmpty || _fcmToken.isEmpty) return;

    try {
      final url = Uri.parse("${_serverUrlController.text}/api/register-token");
      await http.post(
        url,
        headers: {"Content-Type": "application/json"},
        body: jsonEncode({
          "pairCode": _pairCodeController.text.trim(),
          "userName": _nameController.text.trim(),
          "fcmToken": _fcmToken,
        }),
      );
      print("✅ Registered FCM token with backend server!");
    } catch (e) {
      print("❌ Error registering token: $e");
    }
  }

  // Start BLE Scan for Bracelet Hardware
  void _startBleScan() async {
    setState(() {
      _isScanning = true;
      _statusMessage = "Scanning for Bond Touch Bracelet...";
    });

    try {
      await FlutterBluePlus.startScan(timeout: const Duration(seconds: 10));

      _bleScanSubscription = FlutterBluePlus.scanResults.listen((results) {
        for (ScanResult r in results) {
          String deviceName = r.device.platformName;
          if (deviceName.contains("BondTouch_Nano33") || deviceName.contains("BondTouch_ESP32")) {
            FlutterBluePlus.stopScan();
            _connectToDevice(r.device);
            break;
          }
        }
      });
    } catch (e) {
      setState(() {
        _isScanning = false;
        _statusMessage = "BLE Scan Error: $e";
      });
    }
  }

  // Connect to discovered BLE Bracelet
  Future<void> _connectToDevice(BluetoothDevice device) async {
    setState(() {
      _statusMessage = "Connecting to ${device.platformName}...";
    });

    try {
      await device.connect(autoConnect: true);
      _connectedDevice = device;

      List<BluetoothService> services = await device.discoverServices();
      for (var service in services) {
        if (service.uuid.toString().toLowerCase() == SERVICE_UUID) {
          for (var char in service.characteristics) {
            if (char.uuid.toString().toLowerCase() == TOUCH_NOTIFY_UUID) {
              _touchNotifyChar = char;
              await char.setNotifyValue(true);
              _bleNotifySubscription = char.lastValueStream.listen((value) {
                if (value.isNotEmpty && value[0] == 1) {
                  _onBraceletTapDetected();
                }
              });
            } else if (char.uuid.toString().toLowerCase() == VIBRATE_CMD_UUID) {
              _vibrateCmdChar = char;
              globalVibrateCharacteristic = char;
            }
          }
        }
      }

      setState(() {
        _isBleConnected = true;
        _isScanning = false;
        _statusMessage = "Connected to ${device.platformName} 🟢";
      });
    } catch (e) {
      setState(() {
        _isBleConnected = false;
        _isScanning = false;
        _statusMessage = "Connection Failed: $e";
      });
    }
  }

  // When physical bracelet is tapped by user -> Send POST request to backend
  void _onBraceletTapDetected() {
    print("👉 Physical Bracelet Tap Event Triggered!");
    _sendTouchToBackend();
  }

  // Send HTTP POST touch request to backend relay server
  Future<void> _sendTouchToBackend() async {
    if (_nameController.text.isEmpty || _pairCodeController.text.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Please save your Pair Code and Name first!')),
      );
      return;
    }

    try {
      final url = Uri.parse("${_serverUrlController.text}/api/touch");
      final response = await http.post(
        url,
        headers: {"Content-Type": "application/json"},
        body: jsonEncode({
          "pairCode": _pairCodeController.text.trim(),
          "senderName": _nameController.text.trim(),
        }),
      );

      if (response.statusCode == 200) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(content: Text('Touch sent to partner! ❤️')),
        );
      } else {
        print("Backend response error: ${response.body}");
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text('Failed to send touch: $e')),
      );
    }
  }

  // When partner sends touch payload via FCM push -> command local bracelet to vibrate
  void _handleIncomingPartnerTouch(String partnerName) async {
    final now = DateFormat('hh:mm a').format(DateTime.now());
    setState(() {
      _lastTouchReceived = "$partnerName touched you at $now ❤️";
    });

    if (_vibrateCmdChar != null) {
      try {
        await _vibrateCmdChar!.write([1]);
        print("✅ Sent BLE vibrate command to bracelet!");
      } catch (e) {
        print("Error sending BLE vibe command: $e");
      }
    } else {
      print("⚠️ Bracelet not connected over BLE. Mobile vibration triggered.");
    }
  }

  // Save pair preferences
  void _savePairInfo() async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('userName', _nameController.text.trim());
    await prefs.setString('pairCode', _pairCodeController.text.trim());
    await prefs.setString('serverUrl', _serverUrlController.text.trim());
    
    setState(() {
      _isPaired = true;
    });

    _registerFcmTokenWithBackend();

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Pairing info saved successfully!')),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Bond Touch Prototype', style: TextStyle(fontWeight: FontWeight.bold)),
        backgroundColor: const Color(0xFF16161C),
        actions: [
          IconButton(
            icon: const Icon(Icons.settings),
            onPressed: () => _showSettingsDialog(),
          ),
        ],
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(24.0),
        child: Column(
          crossAxisAlignment: CrossAlignment.stretch,
          children: [
            // Status Card
            Container(
              padding: const EdgeInsets.all(20),
              decoration: BoxDecoration(
                color: const Color(0xFF16161C),
                borderRadius: BorderRadius.circular(16),
                border: Border.all(color: const Color(0xFF2A2A38)),
              ),
              child: Row(
                children: [
                  Container(
                    width: 14,
                    height: 14,
                    decoration: BoxDecoration(
                      shape: BoxShape.circle,
                      color: _isBleConnected ? Colors.greenAccent : Colors.redAccent,
                    ),
                  ),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Text(
                      _statusMessage,
                      style: const TextStyle(color: Colors.white70, fontSize: 14),
                    ),
                  ),
                  if (!_isBleConnected)
                    ElevatedButton(
                      style: ElevatedButton.styleFrom(
                        backgroundColor: const Color(0xFFE85D9A),
                      ),
                      onPressed: _isScanning ? null : _startBleScan,
                      child: Text(_isScanning ? 'Scanning...' : 'Connect'),
                    ),
                ],
              ),
            ),
            const SizedBox(height: 30),

            // Main Touch Button Card
            Container(
              padding: const EdgeInsets.symmetric(vertical: 40, horizontal: 20),
              decoration: BoxDecoration(
                gradient: const LinearGradient(
                  colors: [Color(0xFF1A0A1E), Color(0xFF16161C)],
                  begin: Alignment.topCenter,
                  end: Alignment.bottomCenter,
                ),
                borderRadius: BorderRadius.circular(24),
                border: Border.all(color: const Color(0xFFE85D9A).withOpacity(0.3)),
              ),
              child: Column(
                children: [
                  GestureDetector(
                    onTap: _sendTouchToBackend,
                    child: Container(
                      width: 140,
                      height: 140,
                      decoration: BoxDecoration(
                        shape: BoxShape.circle,
                        gradient: const LinearGradient(
                          colors: [Color(0xFFE85D9A), Color(0xFF7C5CBF)],
                        ),
                        boxShadow: [
                          BoxShadow(
                            color: const Color(0xFFE85D9A).withOpacity(0.4),
                            blurRadius: 25,
                            spreadRadius: 5,
                          ),
                        ],
                      ),
                      child: const Center(
                        child: Icon(Icons.favorite, size: 70, color: Colors.white),
                      ),
                    ),
                  ),
                  const SizedBox(height: 24),
                  const Text(
                    'TAP TO SEND TOUCH',
                    style: TextStyle(
                      color: Colors.white,
                      fontSize: 16,
                      fontWeight: FontWeight.bold,
                      letterSpacing: 1.2,
                    ),
                  ),
                  const SizedBox(height: 8),
                  Text(
                    _lastTouchReceived,
                    style: const TextStyle(color: Colors.white54, fontSize: 13),
                  ),
                ],
              ),
            ),
            const SizedBox(height: 30),

            // Pair Configuration Section
            Container(
              padding: const EdgeInsets.all(20),
              decoration: BoxDecoration(
                color: const Color(0xFF16161C),
                borderRadius: BorderRadius.circular(16),
                border: Border.all(color: const Color(0xFF2A2A38)),
              ),
              child: Column(
                crossAxisAlignment: CrossAlignment.start,
                children: [
                  const Text(
                    'Pairing Setup / إعدادات الاقتران',
                    style: TextStyle(fontSize: 16, fontWeight: FontWeight.bold, color: Colors.white),
                  ),
                  const SizedBox(height: 16),
                  TextField(
                    controller: _nameController,
                    decoration: const InputDecoration(
                      labelText: 'Your Name (e.g. Ahmed / Sarah)',
                      border: OutlineInputBorder(),
                    ),
                  ),
                  const SizedBox(height: 14),
                  TextField(
                    controller: _pairCodeController,
                    keyboardType: TextInputType.number,
                    decoration: const InputDecoration(
                      labelText: '6-Digit Shared Pair Code (e.g. 123456)',
                      border: OutlineInputBorder(),
                    ),
                  ),
                  const SizedBox(height: 16),
                  SizedBox(
                    width: double.infinity,
                    child: ElevatedButton(
                      style: ElevatedButton.styleFrom(
                        backgroundColor: const Color(0xFF7C5CBF),
                        padding: const EdgeInsets.symmetric(vertical: 14),
                      ),
                      onPressed: _savePairInfo,
                      child: const Text('Save & Synchronize', style: TextStyle(fontSize: 15)),
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  void _showSettingsDialog() {
    showDialog(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Backend Server URL'),
        content: TextField(
          controller: _serverUrlController,
          decoration: const InputDecoration(
            hintText: 'https://your-app.up.railway.app',
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(context),
            child: const Text('Cancel'),
          ),
          ElevatedButton(
            onPressed: () async {
              final prefs = await SharedPreferences.getInstance();
              await prefs.setString('serverUrl', _serverUrlController.text.trim());
              Navigator.pop(context);
            },
            child: const Text('Save'),
          ),
        ],
      ),
    );
  }
}
