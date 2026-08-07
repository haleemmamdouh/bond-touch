import React, { useState, useEffect, useRef } from 'react';
import {
  View, Text, TouchableOpacity, TextInput, StyleSheet,
  ScrollView, Alert, Vibration, Dimensions, StatusBar,
  AppState, ActivityIndicator, PermissionsAndroid, Platform
} from 'react-native';
import { BleManager, State } from 'react-native-ble-plx';
import mqtt from 'mqtt';
import BackgroundService from 'react-native-background-actions';

// ─── FOREGROUND SERVICE CONFIG ────────────────────────────────────────────────
const sleep = (time) => new Promise((resolve) => setTimeout(() => resolve(), time));

const backgroundTask = async (taskDataArguments) => {
  const { delay } = taskDataArguments;
  await new Promise(async (resolve) => {
    for (let i = 0; BackgroundService.isRunning(); i++) {
      await sleep(delay);
    }
  });
};

const bgOptions = {
  taskName: 'BondTouchBG',
  taskTitle: 'Bond Touch Active ❤️',
  taskDesc: 'Listening for partner touches 24/7...',
  taskIcon: {
    name: 'ic_launcher',
    type: 'mipmap',
  },
  color: '#ff416c',
  parameters: {
    delay: 5000,
  },
};

// ─── BLE CONFIG ───────────────────────────────────────────────────────────────
const SERVICE_UUID = '4fafc201-1fb5-459e-8fcc-c5c9c331914b';
const CHAR_RX_UUID = 'beb5483e-36e1-4688-b7f5-ea07361b26a8';
const CHAR_TX_UUID = 'a3c87500-8ed3-4bdf-8a39-a01bebede295';
const MQTT_BROKER  = 'wss://broker.emqx.io:8084/mqtt';

const { width: SW } = Dimensions.get('window');
const BARS = 16;
const BAR_W = (SW - 48) / BARS;

const bleManager = new BleManager();

// ─── REQUEST BLE PERMISSIONS (Android 12+) ────────────────────────────────────
async function requestBLEPermissions() {
  if (Platform.OS !== 'android') return true;
  if (Platform.Version < 31) {
    const granted = await PermissionsAndroid.request(
      PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
      { title: 'Bond Touch', message: 'Bond Touch needs location to scan for Bluetooth devices.', buttonPositive: 'OK' }
    );
    return granted === PermissionsAndroid.RESULTS.GRANTED;
  }
  const results = await PermissionsAndroid.requestMultiple([
    PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
    PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
    PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
    PermissionsAndroid.PERMISSIONS.POST_NOTIFICATIONS,
  ]);
  return (
    results['android.permission.BLUETOOTH_SCAN']    === 'granted' &&
    results['android.permission.BLUETOOTH_CONNECT'] === 'granted'
  );
}

// ─── BASE64 ENCODE (React Native built-in) ───────────────────────────────────
// btoa() is available globally in React Native (JavaScriptCore)
function toB64(str) {
  try { return btoa(unescape(encodeURIComponent(str))); } catch(_) { return btoa(str); }
}

// ─── APP ROOT ─────────────────────────────────────────────────────────────────
export default function App() {
  const [screen, setScreen] = useState('login');
  const [myName, setMyName] = useState('');
  const [pairCode, setPairCode] = useState('');

  if (screen === 'login') {
    return (
      <LoginScreen
        myName={myName} setMyName={setMyName}
        pairCode={pairCode} setPairCode={setPairCode}
        onConnect={() => {
          if (!myName.trim() || !pairCode.trim()) { Alert.alert('Missing Info', 'Enter name and code!'); return; }
          setScreen('main');
        }}
      />
    );
  }
  return <MainScreen myName={myName.trim()} topicName={`bondtouch/pair_${pairCode.trim()}`} />;
}

// ─── LOGIN SCREEN ─────────────────────────────────────────────────────────────
function LoginScreen({ myName, setMyName, pairCode, setPairCode, onConnect }) {
  return (
    <View style={s.loginBg}>
      <StatusBar barStyle="light-content" backgroundColor="#0b0b0e" />
      <View style={s.card}>
        <Text style={s.emoji}>❤️</Text>
        <Text style={s.title}>Bond Touch</Text>
        <Text style={s.subtitle}>Wireless bracelet companion</Text>
        <TextInput style={s.input} placeholder="Your Name (e.g. Ahmed)" placeholderTextColor="#555" value={myName} onChangeText={setMyName} autoCapitalize="words" />
        <TextInput style={s.input} placeholder="Pair Code (e.g. 102030)" placeholderTextColor="#555" value={pairCode} onChangeText={setPairCode} keyboardType="numeric" maxLength={8} />
        <TouchableOpacity style={s.btnRed} onPress={onConnect} activeOpacity={0.8}>
          <Text style={s.btnText}>Connect ❤️</Text>
        </TouchableOpacity>
      </View>
    </View>
  );
}

// ─── MAIN SCREEN ─────────────────────────────────────────────────────────────
function MainScreen({ myName, topicName }) {
  const [bleStatus, setBleStatus]     = useState('off');
  const [mqttReady, setMqttReady]     = useState(false);
  const [log, setLog]                 = useState({ msg: 'Tap Pair ESP32 to begin!', type: '' });
  const [wave, setWave]               = useState([0,60,160,255,200,120,60,20,80,180,255,180,80,20,0,0]);
  const [durSec, setDurSec]           = useState(3);
  const [color, setColor]             = useState({ r:255, g:0, hex:'#ff416c', label:'Red' });
  const [inWave, setInWave]           = useState(null);

  const mqttRef  = useRef(null);
  const rxRef    = useRef(null);
  const deviceRef = useRef(null);

  const putLog = (msg, type = '') => setLog({ msg, type });

  // ── MQTT + FOREGROUND SERVICE ─────────────────────────────────────────────
  useEffect(() => {
    // Start Foreground Service so Android never pauses JS/Bluetooth in background!
    if (Platform.OS === 'android') {
      BackgroundService.start(backgroundTask, bgOptions).catch(() => {});
    }

    const client = mqtt.connect(MQTT_BROKER, {
      clientId: 'bt_' + Math.random().toString(16).slice(2, 10),
      keepalive: 15,
      reconnectPeriod: 2000,
    });
    client.on('connect', () => { setMqttReady(true); client.subscribe(topicName); putLog('☁️ Cloud ready! Now pair your ESP32.'); });
    client.on('offline', () => setMqttReady(false));
    client.on('message', (_t, msg) => {
      try {
        const p = JSON.parse(msg.toString());
        if (!p.sender || p.sender.toLowerCase() === myName.toLowerCase()) return;
        if (p.type === 'waveform') receiveWave(p.sender, p.data, p.color, p.duration || 3);
        else receiveTouch(p.sender);
      } catch (_) {}
    });
    mqttRef.current = client;
    return () => {
      client.end(true);
      if (Platform.OS === 'android') BackgroundService.stop();
    };
  }, []);

  // ── BLE SCAN + CONNECT ────────────────────────────────────────────────────
  const connectBLE = async () => {
    const granted = await requestBLEPermissions();
    if (!granted) { Alert.alert('Permission Denied', 'Bluetooth permission is required!'); return; }

    const bleState = await bleManager.state();
    if (bleState !== State.PoweredOn) { Alert.alert('Bluetooth Off', 'Please turn on Bluetooth!'); return; }

    setBleStatus('scanning');
    putLog('🔵 Scanning for BondTouch ESP32...');

    let found = false;
    const scanTimeout = setTimeout(() => {
      if (!found) {
        bleManager.stopDeviceScan();
        setBleStatus('off');
        putLog('⚠️ No ESP32 found. Is it powered on?', 'err');
      }
    }, 12000);

    bleManager.startDeviceScan(null, { allowDuplicates: false }, async (err, device) => {
      if (err) { putLog('⚠️ Scan error: ' + err.message, 'err'); setBleStatus('off'); clearTimeout(scanTimeout); return; }
      if (!device?.name?.startsWith('BondTouch')) return;

      found = true;
      clearTimeout(scanTimeout);
      bleManager.stopDeviceScan();
      putLog('🔵 Found ' + device.name + '! Connecting...');

      try {
        const conn = await device.connect({ autoConnect: false, requestMTU: 512 });
        await conn.discoverAllServicesAndCharacteristics();
        deviceRef.current = conn;

        const chars = await conn.characteristicsForService(SERVICE_UUID);
        for (const c of chars) {
          if (c.uuid.toLowerCase() === CHAR_RX_UUID.toLowerCase()) rxRef.current = c;
          if (c.uuid.toLowerCase() === CHAR_TX_UUID.toLowerCase()) {
            conn.monitorCharacteristicForService(SERVICE_UUID, CHAR_TX_UUID, (e, ch) => {
              if (!e && ch) { Vibration.vibrate([50,80,50]); putLog('👆 Physical touch! Sending to partner...', 'sent'); sendHeartTap(); }
            });
          }
        }

        setBleStatus('on');
        putLog('🟢 ' + device.name + ' connected!', 'sent');
        Vibration.vibrate(200);

        conn.onDisconnected(() => {
          setBleStatus('off'); rxRef.current = null;
          putLog('🔴 Disconnected. Tap to reconnect.', 'err');
        });
      } catch (e) {
        putLog('⚠️ Connect failed: ' + e.message, 'err');
        setBleStatus('off');
      }
    });
  };

  // ── WRITE TO ESP32 ────────────────────────────────────────────────────────
  const writeESP32 = async (str) => {
    if (!rxRef.current) {
      console.log('⚠️ Cannot write to ESP32: rxRef.current is null');
      return;
    }
    try {
      const b64 = toB64(str);
      await rxRef.current.writeWithResponse(b64);
      console.log('✅ Sent to ESP32 via BLE:', str);
    } catch (err) {
      console.log('❌ BLE Write Error:', err);
    }
  };

  // ── RECEIVE FROM PARTNER ──────────────────────────────────────────────────
  const receiveWave = (sender, dataStr, colorStr, dur) => {
    setInWave((dataStr || '').split(',').map(Number));
    Vibration.vibrate([80,40,160,40,80]);
    putLog(`🎨 Waveform from ${sender}! ESP32 lighting up...`, 'recv');
    writeESP32(`WAVE:${dur}:${colorStr || '255,0'}:${dataStr}`);
  };

  const receiveTouch = (sender) => {
    Vibration.vibrate([100,60,200,60,300]);
    putLog(`❤️ Touch from ${sender}! ESP32 lighting up...`, 'recv');
    writeESP32('TOUCH');
  };

  // ── SEND ─────────────────────────────────────────────────────────────────
  const sendHeartTap = () => {
    if (!mqttRef.current) return;
    Vibration.vibrate(60);
    putLog('❤️ Heart tap sent!', 'sent');
    mqttRef.current.publish(topicName, JSON.stringify({ sender: myName, timestamp: Date.now() }));
  };

  const sendWave = () => {
    if (!mqttRef.current) return;
    Vibration.vibrate(80);
    putLog(`⚡ Waveform sent (${durSec}s)!`, 'sent');
    mqttRef.current.publish(topicName, JSON.stringify({
      type: 'waveform', data: wave.join(','),
      color: `${color.r},${color.g}`, duration: durSec,
      sender: myName, timestamp: Date.now()
    }));
  };

  // ── DRAW WAVE ─────────────────────────────────────────────────────────────
  const onWaveDraw = (e) => {
    const t = e.nativeEvent.touches[0];
    if (!t) return;
    const idx = Math.min(15, Math.max(0, Math.floor((t.locationX / (SW - 48)) * BARS)));
    const val = Math.min(255, Math.max(0, Math.floor((1 - t.locationY / 100) * 255)));
    setWave(prev => { const n = [...prev]; n[idx] = val; return n; });
  };

  const PRESETS = {
    '🫀 Pulse':  [0,60,160,255,120,30,0,100,255,180,40,0,0,0,0,0],
    '🌅 Fade':   [10,40,100,170,220,255,220,170,100,40,10,0,0,0,0,0],
    '⚡ Strobe': [255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0],
    '❌ Clear':  new Array(16).fill(0),
  };

  const COLORS = [
    { r:255, g:0,   hex:'#ff416c', label:'Red'    },
    { r:0,   g:255, hex:'#38b000', label:'Green'  },
    { r:255, g:255, hex:'#ffb703', label:'Yellow' },
    { r:255, g:60,  hex:'#ff0055', label:'Pink'   },
  ];

  const bleColor = bleStatus === 'on' ? '#48bb78' : bleStatus === 'scanning' ? '#fbbf24' : '#ef4444';
  const mqttColor = mqttReady ? '#48bb78' : '#ef4444';

  return (
    <View style={s.mainBg}>
      <StatusBar barStyle="light-content" backgroundColor="#0b0b0e" />
      <ScrollView contentContainerStyle={s.scroll} keyboardShouldPersistTaps="handled">

        <Text style={s.title}>❤️ Bond Touch</Text>
        <Text style={s.subtitle}>Hi, {myName}!</Text>

        {/* STATUS */}
        <View style={s.row}>
          <View style={[s.badge, { borderColor: mqttColor + '66' }]}>
            <View style={[s.dot, { backgroundColor: mqttColor }]} />
            <Text style={[s.badgeTxt, { color: mqttColor }]}>☁️ {mqttReady ? 'CLOUD LIVE' : 'CONNECTING'}</Text>
          </View>
          <View style={[s.badge, { borderColor: bleColor + '66' }]}>
            <View style={[s.dot, { backgroundColor: bleColor }]} />
            <Text style={[s.badgeTxt, { color: bleColor }]}>🔵 {bleStatus === 'on' ? 'ESP32 ON' : bleStatus === 'scanning' ? 'SCANNING...' : 'ESP32 OFF'}</Text>
          </View>
        </View>

        {/* BLE BUTTON */}
        {bleStatus !== 'on' && (
          <TouchableOpacity style={s.btnBle} onPress={connectBLE} activeOpacity={0.8} disabled={bleStatus === 'scanning'}>
            {bleStatus === 'scanning'
              ? <ActivityIndicator color="#60a5fa" />
              : <Text style={s.btnBleTxt}>🔵 Pair ESP32 Bluetooth</Text>}
          </TouchableOpacity>
        )}

        {/* HEART */}
        <TouchableOpacity style={s.heart} onPress={sendHeartTap} activeOpacity={0.7}>
          <Text style={{ fontSize: 42 }}>❤️</Text>
        </TouchableOpacity>
        <Text style={s.hint}>Tap to send to partner</Text>

        {/* DURATION + COLOR */}
        <View style={s.row}>
          <View style={[s.box, { flex: 1 }]}>
            <Text style={s.boxLabel}>⏱ Duration</Text>
            <View style={s.row}>
              {[1,2,3,5,8,10].map(d => (
                <TouchableOpacity key={d} style={[s.durBtn, durSec===d && s.durBtnOn]} onPress={() => setDurSec(d)}>
                  <Text style={[s.durTxt, durSec===d && { color:'#fff' }]}>{d}s</Text>
                </TouchableOpacity>
              ))}
            </View>
          </View>
          <View style={[s.box, { marginLeft: 10 }]}>
            <Text style={s.boxLabel}>🎨 Color</Text>
            <View style={s.row}>
              {COLORS.map(c => (
                <TouchableOpacity key={c.label} style={[s.swatch, { backgroundColor: c.hex }, color.label===c.label && s.swatchOn]} onPress={() => setColor(c)} />
              ))}
            </View>
          </View>
        </View>

        {/* WAVE CANVAS */}
        <View style={s.section}>
          <Text style={s.secLabel}>🎨 DRAW WAVEFORM</Text>
          <View
            style={s.waveBox}
            onStartShouldSetResponder={() => true}
            onMoveShouldSetResponder={() => true}
            onResponderGrant={onWaveDraw}
            onResponderMove={onWaveDraw}
          >
            {wave.map((v, i) => (
              <View key={i} style={[s.bar, {
                height: Math.max(2, (v/255)*100),
                left: i * BAR_W + 2, width: BAR_W - 4,
                backgroundColor: color.hex
              }]} />
            ))}
          </View>
          <View style={s.row}>
            {Object.entries(PRESETS).map(([label, data]) => (
              <TouchableOpacity key={label} style={s.presetBtn} onPress={() => setWave([...data])}>
                <Text style={s.presetTxt}>{label}</Text>
              </TouchableOpacity>
            ))}
          </View>
        </View>

        {/* INCOMING */}
        {inWave && (
          <View style={s.section}>
            <Text style={s.secLabel}>📥 FROM PARTNER</Text>
            <View style={[s.waveBox, { height: 44, borderColor: 'rgba(192,132,252,0.4)' }]}>
              {inWave.map((v, i) => (
                <View key={i} style={[s.bar, {
                  height: Math.max(1, (v/255)*44),
                  left: i * ((SW-48)/BARS) + 1, width: (SW-48)/BARS - 2,
                  backgroundColor: '#c084fc'
                }]} />
              ))}
            </View>
          </View>
        )}

        {/* TRANSMIT */}
        <TouchableOpacity style={s.btnRed} onPress={sendWave} activeOpacity={0.8}>
          <Text style={s.btnText}>Transmit Waveform to Partner ⚡</Text>
        </TouchableOpacity>

        {/* LOG */}
        <View style={[s.logBox,
          log.type==='recv' && { borderColor:'#ff416c', backgroundColor:'rgba(255,65,108,0.08)' },
          log.type==='sent' && { borderColor:'#48bb78', backgroundColor:'rgba(72,187,120,0.08)' },
          log.type==='err'  && { borderColor:'#ef4444', backgroundColor:'rgba(239,68,68,0.08)' },
        ]}>
          <Text style={[s.logTxt,
            log.type==='recv' && { color:'#ff416c' },
            log.type==='sent' && { color:'#48bb78' },
            log.type==='err'  && { color:'#ef4444' },
          ]}>{log.msg}</Text>
        </View>

        <View style={{ height: 40 }} />
      </ScrollView>
    </View>
  );
}

// ─── STYLES ───────────────────────────────────────────────────────────────────
const s = StyleSheet.create({
  loginBg:   { flex:1, backgroundColor:'#0b0b0e', justifyContent:'center', alignItems:'center', padding:16 },
  mainBg:    { flex:1, backgroundColor:'#0b0b0e' },
  scroll:    { padding:16, alignItems:'center' },
  card:      { backgroundColor:'#141419', borderRadius:24, padding:26, width:'100%', borderWidth:1, borderColor:'#262633' },
  emoji:     { fontSize:48, textAlign:'center', marginBottom:8 },
  title:     { fontSize:26, fontWeight:'800', color:'#ff416c', textAlign:'center', marginBottom:4, marginTop:12 },
  subtitle:  { fontSize:13, color:'#8a8a9e', textAlign:'center', marginBottom:16 },
  input:     { backgroundColor:'#09090c', borderRadius:12, borderWidth:1.5, borderColor:'#262633', padding:14, color:'#fff', fontSize:15, marginBottom:12, width:'100%' },
  btnRed:    { width:'100%', backgroundColor:'#ff416c', borderRadius:14, padding:16, alignItems:'center', marginBottom:12, shadowColor:'#ff416c', shadowOpacity:0.4, shadowRadius:10, elevation:8 },
  btnText:   { color:'#fff', fontSize:15, fontWeight:'800' },
  row:       { flexDirection:'row', flexWrap:'wrap', gap:6, marginBottom:12, width:'100%' },
  badge:     { flexDirection:'row', alignItems:'center', gap:5, backgroundColor:'#14141f', borderWidth:1, paddingHorizontal:10, paddingVertical:5, borderRadius:20 },
  dot:       { width:7, height:7, borderRadius:4 },
  badgeTxt:  { fontSize:10, fontWeight:'700' },
  btnBle:    { width:'100%', backgroundColor:'rgba(59,130,246,0.15)', borderWidth:1.5, borderColor:'rgba(59,130,246,0.5)', borderRadius:14, padding:14, alignItems:'center', marginBottom:14 },
  btnBleTxt: { color:'#60a5fa', fontSize:14, fontWeight:'700' },
  heart:     { width:96, height:96, borderRadius:48, backgroundColor:'#7a0024', borderWidth:3, borderColor:'#ff416c', justifyContent:'center', alignItems:'center', marginBottom:6, elevation:10 },
  hint:      { color:'#555', fontSize:11, marginBottom:14 },
  box:       { backgroundColor:'#09090c', borderRadius:12, borderWidth:1, borderColor:'#262633', padding:10 },
  boxLabel:  { color:'#8a8a9e', fontSize:10, fontWeight:'700', textTransform:'uppercase', marginBottom:8 },
  durBtn:    { backgroundColor:'#1a1a24', borderRadius:6, paddingHorizontal:7, paddingVertical:4 },
  durBtnOn:  { backgroundColor:'#ff416c' },
  durTxt:    { color:'#8a8a9e', fontSize:10, fontWeight:'700' },
  swatch:    { width:22, height:22, borderRadius:11, borderWidth:2, borderColor:'transparent' },
  swatchOn:  { borderColor:'#fff' },
  section:   { width:'100%', marginBottom:12 },
  secLabel:  { color:'#ff416c', fontSize:10, fontWeight:'800', textTransform:'uppercase', letterSpacing:1.5, marginBottom:6 },
  waveBox:   { width:'100%', height:100, backgroundColor:'#09090c', borderRadius:12, borderWidth:1, borderColor:'rgba(255,65,108,0.3)', position:'relative', overflow:'hidden', marginBottom:8 },
  bar:       { position:'absolute', bottom:0, borderTopLeftRadius:3, borderTopRightRadius:3 },
  presetBtn: { flex:1, backgroundColor:'#09090c', borderWidth:1, borderColor:'#262633', borderRadius:8, padding:7, alignItems:'center' },
  presetTxt: { color:'#aaa', fontSize:9, fontWeight:'700' },
  logBox:    { width:'100%', backgroundColor:'#09090c', borderWidth:1, borderColor:'#262633', borderRadius:12, padding:14, alignItems:'center', minHeight:48, justifyContent:'center' },
  logTxt:    { color:'#8a8a9e', fontSize:13, textAlign:'center', lineHeight:18 },
});
