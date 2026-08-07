/**
 * ╔══════════════════════════════════════════════════════════════╗
 * ║        BOND TOUCH — PREMIUM ANDROID APP (Phase 2)           ║
 * ║  Dark OLED Glassmorphism · 60fps · Low & High-End Phones    ║
 * ╚══════════════════════════════════════════════════════════════╝
 */

import React, {
  useState, useEffect, useRef, useCallback, useMemo, memo
} from 'react';
import {
  View, Text, TouchableOpacity, TextInput, StyleSheet,
  ScrollView, Alert, Vibration, Dimensions, StatusBar,
  Animated, Easing, Platform, PermissionsAndroid,
  ActivityIndicator, PanResponder,
} from 'react-native';
import { BleManager, State } from 'react-native-ble-plx';
import mqtt from 'mqtt';
import BackgroundService from 'react-native-background-actions';

// ─── CONSTANTS ────────────────────────────────────────────────────────────────
const SERVICE_UUID  = '4fafc201-1fb5-459e-8fcc-c5c9c331914b';
const CHAR_RX_UUID  = 'beb5483e-36e1-4688-b7f5-ea07361b26a8';
const CHAR_TX_UUID  = 'a3c87500-8ed3-4bdf-8a39-a01bebede295';
const MQTT_BROKER   = 'wss://broker.emqx.io:8084/mqtt';
const BARS          = 16;
const { width: SW } = Dimensions.get('window');
const BAR_W         = (SW - 48) / BARS;

// ─── COLORS / DESIGN TOKENS ───────────────────────────────────────────────────
const C = {
  bg:           '#050508',
  card:         '#0f0f18',
  cardBorder:   '#1c1c2e',
  accent:       '#ff2a6d',
  accentDark:   '#7a0028',
  accentGlow:   '#ff0055',
  cyan:         '#00f5d4',
  amber:        '#ffb703',
  purple:       '#c084fc',
  textMain:     '#ffffff',
  textMuted:    '#8e8ea8',
  green:        '#48bb78',
  blue:         '#60a5fa',
  err:          '#ef4444',
  barFill:      'rgba(255,42,109,0.85)',
};

// ─── FOREGROUND SERVICE ───────────────────────────────────────────────────────
const sleep = (ms) => new Promise(r => setTimeout(r, ms));
const backgroundTask = async ({ delay }) => {
  await new Promise(async () => {
    while (BackgroundService.isRunning()) await sleep(delay);
  });
};
const bgOptions = {
  taskName: 'BondTouchBG',
  taskTitle: 'Bond Touch Active ❤️',
  taskDesc: 'Listening for partner touches 24/7...',
  taskIcon: { name: 'ic_launcher', type: 'mipmap' },
  color: '#ff2a6d',
  parameters: { delay: 5000 },
};

// ─── PERMISSIONS ──────────────────────────────────────────────────────────────
async function requestBLEPermissions() {
  if (Platform.OS !== 'android') return true;
  if (Platform.Version < 31) {
    const g = await PermissionsAndroid.request(
      PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
      { title: 'Bond Touch', message: 'Needs location for Bluetooth scan.', buttonPositive: 'OK' }
    );
    return g === PermissionsAndroid.RESULTS.GRANTED;
  }
  const res = await PermissionsAndroid.requestMultiple([
    PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
    PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT,
    PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION,
    PermissionsAndroid.PERMISSIONS.POST_NOTIFICATIONS,
  ]);
  return res['android.permission.BLUETOOTH_SCAN'] === 'granted' &&
         res['android.permission.BLUETOOTH_CONNECT'] === 'granted';
}

// ─── BASE64 ENCODE ────────────────────────────────────────────────────────────
function toB64(str) {
  try { return btoa(unescape(encodeURIComponent(str))); } catch { return btoa(str); }
}

// ─── THROTTLE HELPER ─────────────────────────────────────────────────────────
function throttle(fn, ms) {
  let last = 0;
  return (...args) => {
    const now = Date.now();
    if (now - last >= ms) { last = now; fn(...args); }
  };
}

// ─── BLE MANAGER (singleton) ──────────────────────────────────────────────────
const bleManager = new BleManager();

// ═════════════════════════════════════════════════════════════════════════════
// ROOT APP
// ═════════════════════════════════════════════════════════════════════════════
export default function App() {
  const [screen, setScreen]     = useState('login');
  const [myName, setMyName]     = useState('');
  const [pairCode, setPairCode] = useState('');

  const handleConnect = useCallback(() => {
    if (!myName.trim() || !pairCode.trim()) {
      Alert.alert('Missing Info', 'Enter your name and pair code!');
      return;
    }
    setScreen('main');
  }, [myName, pairCode]);

  if (screen === 'login') {
    return (
      <LoginScreen
        myName={myName} setMyName={setMyName}
        pairCode={pairCode} setPairCode={setPairCode}
        onConnect={handleConnect}
      />
    );
  }
  return (
    <MainScreen
      myName={myName.trim()}
      topicName={`bondtouch/pair_${pairCode.trim()}`}
    />
  );
}

// ═════════════════════════════════════════════════════════════════════════════
// LOGIN SCREEN — PREMIUM GLASSMORPHISM
// ═════════════════════════════════════════════════════════════════════════════
const LoginScreen = memo(({ myName, setMyName, pairCode, setPairCode, onConnect }) => {
  const fadeAnim  = useRef(new Animated.Value(0)).current;
  const slideAnim = useRef(new Animated.Value(30)).current;
  const pulseAnim = useRef(new Animated.Value(1)).current;

  useEffect(() => {
    // Entrance animation
    Animated.parallel([
      Animated.timing(fadeAnim,  { toValue: 1, duration: 700, useNativeDriver: true }),
      Animated.timing(slideAnim, { toValue: 0, duration: 600, easing: Easing.out(Easing.cubic), useNativeDriver: true }),
    ]).start();

    // Pulse heart loop
    const pulse = Animated.loop(
      Animated.sequence([
        Animated.timing(pulseAnim, { toValue: 1.12, duration: 600, easing: Easing.inOut(Easing.sin), useNativeDriver: true }),
        Animated.timing(pulseAnim, { toValue: 1.00, duration: 600, easing: Easing.inOut(Easing.sin), useNativeDriver: true }),
      ])
    );
    pulse.start();
    return () => pulse.stop();
  }, []);

  return (
    <View style={s.bg}>
      <StatusBar barStyle="light-content" backgroundColor={C.bg} />

      {/* Background glow orbs */}
      <View style={s.orb1} pointerEvents="none" />
      <View style={s.orb2} pointerEvents="none" />

      <Animated.View style={[s.loginWrap, { opacity: fadeAnim, transform: [{ translateY: slideAnim }] }]}>

        {/* Heart badge */}
        <Animated.View style={[s.heartBadge, { transform: [{ scale: pulseAnim }] }]}>
          <Text style={s.heartBadgeEmoji}>❤️</Text>
        </Animated.View>

        <Text style={s.brandBadge}>BOND TOUCH</Text>
        <Text style={s.loginTitle}>Long Distance{'\n'}Connection</Text>
        <Text style={s.loginSub}>Stay close, no matter the distance</Text>

        <View style={s.loginCard}>
          <Text style={s.inputLabel}>YOUR NAME</Text>
          <TextInput
            style={s.input}
            placeholder="e.g. Ahmed"
            placeholderTextColor={C.textMuted}
            value={myName}
            onChangeText={setMyName}
            autoCapitalize="words"
            returnKeyType="next"
          />

          <Text style={[s.inputLabel, { marginTop: 12 }]}>PAIR CODE</Text>
          <TextInput
            style={s.input}
            placeholder="6-digit shared code"
            placeholderTextColor={C.textMuted}
            value={pairCode}
            onChangeText={setPairCode}
            keyboardType="numeric"
            maxLength={8}
            returnKeyType="done"
            onSubmitEditing={onConnect}
          />

          <TouchableOpacity style={s.btnConnect} onPress={onConnect} activeOpacity={0.85}>
            <Text style={s.btnConnectText}>Connect  ❤️</Text>
          </TouchableOpacity>
        </View>

        <Text style={s.loginFooter}>Powered by BLE + MQTT Cloud Sync</Text>
      </Animated.View>
    </View>
  );
});

// ═════════════════════════════════════════════════════════════════════════════
// MAIN SCREEN
// ═════════════════════════════════════════════════════════════════════════════
function MainScreen({ myName, topicName }) {
  const [bleStatus, setBleStatus] = useState('off');   // 'off' | 'scanning' | 'on'
  const [mqttReady, setMqttReady] = useState(false);
  const [log, setLog]             = useState({ msg: 'Tap "Pair ESP32" to begin!', type: '' });
  const [wave, setWave]           = useState([0,60,160,255,200,120,60,20,80,180,255,180,80,14,0,0]);
  const [durSec, setDurSec]       = useState(3);
  const [colorIdx, setColorIdx]   = useState(0);
  const [inWave, setInWave]       = useState(null);
  const [touchLog, setTouchLog]   = useState([]);

  const mqttRef   = useRef(null);
  const rxRef     = useRef(null);
  const deviceRef = useRef(null);

  // Animated values
  const heartScale   = useRef(new Animated.Value(1)).current;
  const heartGlow    = useRef(new Animated.Value(0)).current;
  const rippleScale  = useRef(new Animated.Value(0.85)).current;
  const rippleOpacity= useRef(new Animated.Value(0.8)).current;
  const recvPulse    = useRef(new Animated.Value(1)).current;
  const connBadgeAnim= useRef(new Animated.Value(0)).current;

  const putLog = useCallback((msg, type = '') => setLog({ msg, type }), []);

  const addTouchLog = useCallback((from, emoji) => {
    setTouchLog(prev => [
      { id: Date.now(), from, emoji, time: new Date().toLocaleTimeString() },
      ...prev.slice(0, 4),
    ]);
  }, []);

  // ── IDLE RIPPLE LOOP ────────────────────────────────────────────────────────
  useEffect(() => {
    const anim = Animated.loop(
      Animated.parallel([
        Animated.sequence([
          Animated.timing(rippleScale,   { toValue: 1.18, duration: 2800, easing: Easing.inOut(Easing.sin), useNativeDriver: true }),
          Animated.timing(rippleScale,   { toValue: 0.85, duration: 2800, easing: Easing.inOut(Easing.sin), useNativeDriver: true }),
        ]),
        Animated.sequence([
          Animated.timing(rippleOpacity, { toValue: 0.25, duration: 2800, useNativeDriver: true }),
          Animated.timing(rippleOpacity, { toValue: 0.80, duration: 2800, useNativeDriver: true }),
        ]),
      ])
    );
    anim.start();
    return () => anim.stop();
  }, []);

  // ── MQTT + FOREGROUND SERVICE ───────────────────────────────────────────────
  useEffect(() => {
    if (Platform.OS === 'android') {
      BackgroundService.start(backgroundTask, bgOptions).catch(() => {});
    }

    const client = mqtt.connect(MQTT_BROKER, {
      clientId: 'bt_' + Math.random().toString(16).slice(2, 10),
      keepalive: 15,
      reconnectPeriod: 2000,
    });

    client.on('connect', () => {
      setMqttReady(true);
      client.subscribe(topicName);
      putLog('☁️ Cloud ready — now pair your ESP32.', '');
    });
    client.on('offline', () => setMqttReady(false));
    client.on('message', (_t, buf) => {
      try {
        const p = JSON.parse(buf.toString());
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

  // ── BLE CONNECT ─────────────────────────────────────────────────────────────
  const connectBLE = useCallback(async () => {
    const granted = await requestBLEPermissions();
    if (!granted) { Alert.alert('Permission Denied', 'Bluetooth permission required!'); return; }

    const st = await bleManager.state();
    if (st !== State.PoweredOn) { Alert.alert('Bluetooth Off', 'Please enable Bluetooth first.'); return; }

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
      if (err) { clearTimeout(scanTimeout); setBleStatus('off'); putLog('⚠️ Scan error: ' + err.message, 'err'); return; }
      if (!device?.name?.startsWith('BondTouch')) return;

      found = true;
      clearTimeout(scanTimeout);
      bleManager.stopDeviceScan();
      putLog('⚡ Found ' + device.name + '! Connecting...');

      try {
        const conn = await device.connect({ autoConnect: false, requestMTU: 512 });
        await conn.discoverAllServicesAndCharacteristics();
        deviceRef.current = conn;

        const chars = await conn.characteristicsForService(SERVICE_UUID);
        for (const c of chars) {
          if (c.uuid.toLowerCase() === CHAR_RX_UUID.toLowerCase()) rxRef.current = c;
          if (c.uuid.toLowerCase() === CHAR_TX_UUID.toLowerCase()) {
            conn.monitorCharacteristicForService(SERVICE_UUID, CHAR_TX_UUID, (e, ch) => {
              if (!e && ch) {
                Vibration.vibrate([50, 80, 50]);
                putLog('👆 Physical touch detected — sending to partner!', 'sent');
                addTouchLog('You', '👆');
                sendHeartTap();
              }
            });
          }
        }

        setBleStatus('on');
        putLog('🟢 ' + device.name + ' connected!', 'sent');
        Vibration.vibrate(200);

        conn.onDisconnected(() => {
          setBleStatus('off');
          rxRef.current = null;
          putLog('🔴 Disconnected. Tap to reconnect.', 'err');
        });
      } catch (e) {
        setBleStatus('off');
        putLog('⚠️ Connect failed: ' + e.message, 'err');
      }
    });
  }, []);

  // ── WRITE TO ESP32 ──────────────────────────────────────────────────────────
  const writeESP32 = useCallback(async (str) => {
    if (!rxRef.current) return;
    try {
      await rxRef.current.writeWithResponse(toB64(str));
    } catch (e) { /* silent fail */ }
  }, []);

  // ── RECEIVE FROM PARTNER ────────────────────────────────────────────────────
  const receiveTouch = useCallback((sender) => {
    Vibration.vibrate([100, 60, 200, 60, 300]);
    addTouchLog(sender, '❤️');
    putLog(`❤️ Touch from ${sender}!`, 'recv');
    writeESP32('TOUCH');

    // Flash animate receive pulse
    Animated.sequence([
      Animated.timing(recvPulse, { toValue: 1.3, duration: 180, useNativeDriver: true }),
      Animated.timing(recvPulse, { toValue: 1.0, duration: 300, useNativeDriver: true }),
    ]).start();
  }, []);

  const receiveWave = useCallback((sender, dataStr, colorStr, dur) => {
    setInWave((dataStr || '').split(',').map(Number));
    Vibration.vibrate([80, 40, 160, 40, 80]);
    addTouchLog(sender, '🎨');
    putLog(`🎨 Waveform from ${sender}!`, 'recv');
    writeESP32(`WAVE:${dur}:${colorStr || '255,0'}:${dataStr}`);
  }, []);

  // ── SEND HEART TAP ──────────────────────────────────────────────────────────
  const sendHeartTap = useCallback(() => {
    if (!mqttRef.current) return;
    Vibration.vibrate(50);
    putLog('❤️ Heart tap sent to partner!', 'sent');
    mqttRef.current.publish(topicName, JSON.stringify({
      sender: myName,
      timestamp: Date.now(),
    }));

    // Heart press animation
    Animated.sequence([
      Animated.timing(heartScale, { toValue: 0.88, duration: 100, useNativeDriver: true }),
      Animated.spring(heartScale, { toValue: 1, friction: 3, useNativeDriver: true }),
    ]).start();
    Animated.sequence([
      Animated.timing(heartGlow,  { toValue: 1, duration: 200, useNativeDriver: false }),
      Animated.timing(heartGlow,  { toValue: 0, duration: 500, useNativeDriver: false }),
    ]).start();
  }, [topicName, myName]);

  // ── SEND WAVEFORM ───────────────────────────────────────────────────────────
  const sendWave = useCallback(() => {
    if (!mqttRef.current) return;
    Vibration.vibrate(80);
    putLog(`⚡ Waveform sent (${durSec}s)!`, 'sent');
    mqttRef.current.publish(topicName, JSON.stringify({
      type: 'waveform',
      data: wave.join(','),
      color: `${COLORS[colorIdx].r},${COLORS[colorIdx].g}`,
      duration: durSec,
      sender: myName,
      timestamp: Date.now(),
    }));
  }, [wave, durSec, colorIdx, topicName, myName]);

  // ── WAVE DRAW (throttled for low-end phones) ─────────────────────────────────
  const onWaveDraw = useMemo(() => throttle((e) => {
    const t = e.nativeEvent.touches?.[0];
    if (!t) return;
    const idx = Math.min(BARS - 1, Math.max(0, Math.floor((t.locationX / (SW - 48)) * BARS)));
    const val = Math.min(255, Math.max(0, Math.floor((1 - t.locationY / 100) * 255)));
    setWave(prev => {
      if (prev[idx] === val) return prev;  // skip re-render if no change
      const n = [...prev];
      n[idx] = val;
      return n;
    });
  }, 30), []);

  // Presets & Colors
  const PRESETS = useMemo(() => ({
    '🫀 Pulse':  [0,60,160,255,120,30,0,100,255,180,40,0,0,0,0,0],
    '🌅 Fade':   [10,40,100,170,220,255,220,170,100,40,10,0,0,0,0,0],
    '⚡ Strobe': [255,0,255,0,255,0,255,0,255,0,255,0,255,0,255,0],
    '❌ Clear':  new Array(16).fill(0),
  }), []);

  // Derived
  const bleColor  = bleStatus === 'on' ? C.green : bleStatus === 'scanning' ? C.amber : C.err;
  const mqttColor = mqttReady ? C.cyan : C.err;

  const glowStyle = {
    shadowColor: C.accent,
    shadowOpacity: heartGlow,
    shadowRadius: 30,
    elevation: heartGlow.interpolate({ inputRange: [0,1], outputRange: [6, 20] }),
  };

  return (
    <View style={s.bg}>
      <StatusBar barStyle="light-content" backgroundColor={C.bg} />

      {/* Background orbs */}
      <View style={s.orb1} pointerEvents="none" />
      <View style={s.orb3} pointerEvents="none" />

      <ScrollView
        contentContainerStyle={s.scroll}
        keyboardShouldPersistTaps="handled"
        showsVerticalScrollIndicator={false}
      >
        {/* HEADER */}
        <View style={s.headerRow}>
          <View>
            <Text style={s.greeting}>Hi, {myName} 👋</Text>
            <Text style={s.brandSmall}>❤️ Bond Touch</Text>
          </View>
          <View style={s.statusCol}>
            <StatusPill color={mqttColor} label={mqttReady ? 'CLOUD' : 'OFFLINE'} active={mqttReady} />
            <StatusPill color={bleColor}  label={bleStatus === 'on' ? 'ESP32' : bleStatus === 'scanning' ? 'SCANNING' : 'BLE OFF'} active={bleStatus === 'on'} />
          </View>
        </View>

        {/* BLE PAIR BUTTON */}
        {bleStatus !== 'on' && (
          <TouchableOpacity
            style={[s.btnBle, bleStatus === 'scanning' && s.btnBleScanning]}
            onPress={connectBLE}
            activeOpacity={0.8}
            disabled={bleStatus === 'scanning'}
          >
            {bleStatus === 'scanning'
              ? <><ActivityIndicator color={C.blue} size="small" style={{ marginRight: 8 }} /><Text style={s.btnBleTxt}>Scanning...</Text></>
              : <Text style={s.btnBleTxt}>🔵  Pair ESP32 Bluetooth</Text>
            }
          </TouchableOpacity>
        )}

        {/* TOUCH PAD */}
        <View style={s.touchSection}>
          <Text style={s.sectionLabel}>TOUCH PAD</Text>

          <View style={s.touchPadWrap}>
            {/* Outer ripple ring */}
            <Animated.View style={[s.rippleOuter, {
              transform: [{ scale: rippleScale }],
              opacity: rippleOpacity,
            }]} />

            {/* Inner glow ring */}
            <Animated.View style={[s.rippleInner, {
              transform: [{ scale: recvPulse }],
            }]} />

            {/* Main heart button */}
            <Animated.View style={[s.heartWrap, glowStyle, {
              transform: [{ scale: heartScale }],
            }]}>
              <TouchableOpacity
                style={s.heartBtn}
                onPress={sendHeartTap}
                activeOpacity={1}
              >
                <Text style={s.heartEmoji}>❤️</Text>
              </TouchableOpacity>
            </Animated.View>
          </View>

          <Text style={s.touchHint}>Tap to send a touch to your partner</Text>
        </View>

        {/* DURATION + COLOR ROW */}
        <View style={s.dualRow}>

          {/* Duration */}
          <View style={[s.miniCard, { flex: 1.5 }]}>
            <Text style={s.miniCardLabel}>⏱  DURATION</Text>
            <View style={s.durationRow}>
              {[1,2,3,5,8].map(d => (
                <TouchableOpacity
                  key={d}
                  style={[s.durBtn, durSec === d && s.durBtnOn]}
                  onPress={() => setDurSec(d)}
                >
                  <Text style={[s.durTxt, durSec === d && s.durTxtOn]}>{d}s</Text>
                </TouchableOpacity>
              ))}
            </View>
          </View>

          {/* Color */}
          <View style={[s.miniCard, { flex: 1, marginLeft: 10 }]}>
            <Text style={s.miniCardLabel}>🎨  COLOR</Text>
            <View style={s.colorRow}>
              {COLORS.map((c, i) => (
                <TouchableOpacity
                  key={c.label}
                  style={[s.colorSwatch, { backgroundColor: c.hex }, colorIdx === i && s.colorSwatchOn]}
                  onPress={() => setColorIdx(i)}
                />
              ))}
            </View>
          </View>
        </View>

        {/* WAVE EDITOR */}
        <View style={s.waveCard}>
          <View style={s.waveCardHeader}>
            <Text style={s.sectionLabel}>WAVEFORM EDITOR</Text>
            <Text style={[s.sectionLabel, { color: COLORS[colorIdx].hex, letterSpacing: 0 }]}>
              {COLORS[colorIdx].label}
            </Text>
          </View>

          <View
            style={s.waveBox}
            onStartShouldSetResponder={() => true}
            onMoveShouldSetResponder={() => true}
            onResponderGrant={onWaveDraw}
            onResponderMove={onWaveDraw}
          >
            {wave.map((v, i) => (
              <View
                key={i}
                style={[s.bar, {
                  height: Math.max(3, (v / 255) * 100),
                  left: i * BAR_W + 2,
                  width: BAR_W - 4,
                  backgroundColor: COLORS[colorIdx].hex,
                  opacity: 0.88 + (v / 255) * 0.12,
                }]}
              />
            ))}
          </View>

          {/* Presets */}
          <View style={s.presetRow}>
            {Object.entries(PRESETS).map(([label, data]) => (
              <TouchableOpacity
                key={label}
                style={s.presetBtn}
                onPress={() => setWave([...data])}
              >
                <Text style={s.presetTxt}>{label}</Text>
              </TouchableOpacity>
            ))}
          </View>
        </View>

        {/* INCOMING WAVE */}
        {inWave && (
          <View style={s.waveCard}>
            <View style={s.waveCardHeader}>
              <Text style={[s.sectionLabel, { color: C.purple }]}>📥  FROM PARTNER</Text>
            </View>
            <View style={[s.waveBox, { borderColor: 'rgba(192,132,252,0.25)', height: 52 }]}>
              {inWave.map((v, i) => (
                <View key={i} style={[s.bar, {
                  height: Math.max(2, (v / 255) * 52),
                  left: i * BAR_W + 2,
                  width: BAR_W - 4,
                  backgroundColor: C.purple,
                }]} />
              ))}
            </View>
          </View>
        )}

        {/* TRANSMIT BUTTON */}
        <TouchableOpacity style={s.btnTransmit} onPress={sendWave} activeOpacity={0.85}>
          <Text style={s.btnTransmitTxt}>Transmit Waveform  ⚡</Text>
        </TouchableOpacity>

        {/* STATUS LOG */}
        <LogBox log={log} />

        {/* TOUCH HISTORY */}
        {touchLog.length > 0 && (
          <View style={s.waveCard}>
            <Text style={s.sectionLabel}>📋  TOUCH HISTORY</Text>
            {touchLog.map(entry => (
              <TouchHistoryRow key={entry.id} entry={entry} />
            ))}
          </View>
        )}

        <View style={{ height: 48 }} />
      </ScrollView>
    </View>
  );
}

// ═════════════════════════════════════════════════════════════════════════════
// SUB-COMPONENTS (memoized for performance)
// ═════════════════════════════════════════════════════════════════════════════

const StatusPill = memo(({ color, label, active }) => (
  <View style={[s.pill, { borderColor: color + '55' }]}>
    <View style={[s.pillDot, { backgroundColor: color }]} />
    <Text style={[s.pillTxt, { color }]}>{label}</Text>
  </View>
));

const LogBox = memo(({ log }) => {
  const borderColor =
    log.type === 'recv' ? C.accent :
    log.type === 'sent' ? C.cyan :
    log.type === 'err'  ? C.err  : C.cardBorder;
  const textColor =
    log.type === 'recv' ? C.accent :
    log.type === 'sent' ? C.cyan :
    log.type === 'err'  ? C.err  : C.textMuted;
  const bgColor =
    log.type === 'recv' ? 'rgba(255,42,109,0.07)' :
    log.type === 'sent' ? 'rgba(0,245,212,0.06)' :
    log.type === 'err'  ? 'rgba(239,68,68,0.07)' : 'transparent';

  return (
    <View style={[s.logBox, { borderColor, backgroundColor: bgColor }]}>
      <Text style={[s.logTxt, { color: textColor }]}>{log.msg}</Text>
    </View>
  );
});

const TouchHistoryRow = memo(({ entry }) => (
  <View style={s.histRow}>
    <Text style={s.histEmoji}>{entry.emoji}</Text>
    <View style={{ flex: 1, marginLeft: 10 }}>
      <Text style={s.histFrom}>{entry.from}</Text>
      <Text style={s.histTime}>{entry.time}</Text>
    </View>
  </View>
));

// ─── COLORS DATA ──────────────────────────────────────────────────────────────
const COLORS = [
  { r:255, g:0,   hex:'#ff2a6d', label:'Pink'   },
  { r:0,   g:255, hex:'#00f5d4', label:'Cyan'   },
  { r:255, g:255, hex:'#ffb703', label:'Amber'  },
  { r:60,  g:0,   hex:'#c084fc', label:'Purple' },
];

// ═════════════════════════════════════════════════════════════════════════════
// STYLES
// ═════════════════════════════════════════════════════════════════════════════
const s = StyleSheet.create({
  bg:           { flex:1, backgroundColor: C.bg },

  // Background orb blobs
  orb1: {
    position:'absolute', top:-120, left:-80,
    width:300, height:300, borderRadius:150,
    backgroundColor:'rgba(255,42,109,0.12)',
  },
  orb2: {
    position:'absolute', bottom:80, right:-60,
    width:220, height:220, borderRadius:110,
    backgroundColor:'rgba(0,245,212,0.07)',
  },
  orb3: {
    position:'absolute', top:300, right:-40,
    width:180, height:180, borderRadius:90,
    backgroundColor:'rgba(192,132,252,0.06)',
  },

  // ── LOGIN ─────────────────────────
  loginWrap: {
    flex:1, width:'100%', alignItems:'center', justifyContent:'center', padding:24,
  },
  heartBadge: {
    width:90, height:90, borderRadius:45,
    backgroundColor:'rgba(255,42,109,0.15)',
    borderWidth:2, borderColor:'rgba(255,42,109,0.4)',
    alignItems:'center', justifyContent:'center',
    marginBottom:20,
    shadowColor:C.accent, shadowOpacity:0.6, shadowRadius:20, elevation:12,
  },
  heartBadgeEmoji: { fontSize:44 },
  brandBadge: {
    fontSize:11, fontWeight:'800', letterSpacing:3,
    color:C.accent, marginBottom:10,
  },
  loginTitle: {
    fontSize:34, fontWeight:'800', color:C.textMain,
    textAlign:'center', lineHeight:40, marginBottom:8,
  },
  loginSub: {
    fontSize:14, color:C.textMuted, textAlign:'center', marginBottom:32,
  },
  loginCard: {
    width:'100%', backgroundColor:C.card,
    borderRadius:24, borderWidth:1, borderColor:C.cardBorder,
    padding:22,
    shadowColor:'#000', shadowOpacity:0.5, shadowRadius:16, elevation:10,
  },
  inputLabel: {
    fontSize:10, fontWeight:'800', letterSpacing:1.5,
    color:C.textMuted, marginBottom:6,
  },
  input: {
    backgroundColor:'rgba(5,5,8,0.9)',
    borderRadius:14, borderWidth:1.5, borderColor:C.cardBorder,
    padding:14, color:C.textMain, fontSize:15, fontWeight:'500',
  },
  btnConnect: {
    marginTop:20, backgroundColor:C.accent,
    borderRadius:16, padding:17, alignItems:'center',
    shadowColor:C.accent, shadowOpacity:0.5, shadowRadius:14, elevation:10,
  },
  btnConnectText: { color:'#fff', fontSize:16, fontWeight:'800', letterSpacing:0.5 },
  loginFooter: { fontSize:11, color:'#3d3d52', marginTop:24 },

  // ── MAIN ──────────────────────────
  scroll: { padding:16, paddingTop:56, alignItems:'center' },

  headerRow: {
    width:'100%', flexDirection:'row', justifyContent:'space-between',
    alignItems:'center', marginBottom:20,
  },
  greeting: { fontSize:22, fontWeight:'800', color:C.textMain },
  brandSmall: { fontSize:12, color:C.textMuted, marginTop:2 },
  statusCol: { gap:5, alignItems:'flex-end' },

  pill: {
    flexDirection:'row', alignItems:'center', gap:5,
    borderWidth:1, paddingHorizontal:10, paddingVertical:4,
    borderRadius:20, backgroundColor:'rgba(15,15,24,0.8)',
  },
  pillDot: { width:7, height:7, borderRadius:4 },
  pillTxt: { fontSize:9, fontWeight:'800', letterSpacing:0.5 },

  // BLE button
  btnBle: {
    width:'100%', flexDirection:'row', alignItems:'center', justifyContent:'center',
    backgroundColor:'rgba(59,130,246,0.1)',
    borderWidth:1.5, borderColor:'rgba(59,130,246,0.4)',
    borderRadius:16, padding:14, marginBottom:20,
  },
  btnBleScanning: { borderColor:'rgba(251,191,36,0.4)', backgroundColor:'rgba(251,191,36,0.08)' },
  btnBleTxt: { color:C.blue, fontSize:14, fontWeight:'700' },

  // Touch pad
  touchSection: { width:'100%', alignItems:'center', marginBottom:20 },
  sectionLabel: {
    width:'100%', fontSize:10, fontWeight:'800', letterSpacing:1.5,
    color:C.textMuted, marginBottom:14,
  },
  touchPadWrap: {
    width:200, height:200, alignItems:'center', justifyContent:'center',
    marginBottom:12,
  },
  rippleOuter: {
    position:'absolute', width:200, height:200, borderRadius:100,
    backgroundColor:'rgba(255,42,109,0.15)',
    borderWidth:1, borderColor:'rgba(255,42,109,0.2)',
  },
  rippleInner: {
    position:'absolute', width:156, height:156, borderRadius:78,
    backgroundColor:'rgba(255,42,109,0.1)',
    borderWidth:1, borderColor:'rgba(255,42,109,0.3)',
  },
  heartWrap: {
    width:120, height:120, borderRadius:60,
    shadowColor:C.accent, shadowOpacity:0.6, shadowRadius:20, elevation:12,
  },
  heartBtn: {
    width:120, height:120, borderRadius:60,
    backgroundColor:C.accentDark,
    borderWidth:3, borderColor:'rgba(255,42,109,0.7)',
    alignItems:'center', justifyContent:'center',
    overflow:'hidden',
  },
  heartEmoji: { fontSize:50 },
  touchHint: { fontSize:12, color:C.textMuted },

  // Duration & Color
  dualRow: { flexDirection:'row', width:'100%', marginBottom:16 },
  miniCard: {
    backgroundColor:C.card,
    borderRadius:16, borderWidth:1, borderColor:C.cardBorder,
    padding:14,
  },
  miniCardLabel: {
    fontSize:9, fontWeight:'800', letterSpacing:1.5,
    color:C.textMuted, marginBottom:10,
  },
  durationRow: { flexDirection:'row', gap:5, flexWrap:'wrap' },
  durBtn: {
    backgroundColor:'rgba(255,255,255,0.04)',
    borderRadius:8, paddingHorizontal:9, paddingVertical:5,
    borderWidth:1, borderColor:C.cardBorder,
  },
  durBtnOn: { backgroundColor:C.accent, borderColor:C.accent },
  durTxt: { color:C.textMuted, fontSize:11, fontWeight:'700' },
  durTxtOn: { color:'#fff' },
  colorRow: { flexDirection:'row', gap:8, marginTop:2 },
  colorSwatch: {
    width:24, height:24, borderRadius:12,
    borderWidth:2, borderColor:'transparent',
  },
  colorSwatchOn: { borderColor:'#fff', shadowColor:'#fff', shadowOpacity:0.4, shadowRadius:4 },

  // Wave editor
  waveCard: {
    width:'100%', backgroundColor:C.card,
    borderRadius:20, borderWidth:1, borderColor:C.cardBorder,
    padding:16, marginBottom:14,
  },
  waveCardHeader: {
    flexDirection:'row', justifyContent:'space-between',
    alignItems:'center', marginBottom:12,
  },
  waveBox: {
    width:'100%', height:100,
    backgroundColor:'rgba(5,5,8,0.9)',
    borderRadius:12, borderWidth:1, borderColor:'rgba(255,42,109,0.2)',
    position:'relative', overflow:'hidden', marginBottom:12,
  },
  bar: {
    position:'absolute', bottom:0,
    borderTopLeftRadius:3, borderTopRightRadius:3,
  },
  presetRow: { flexDirection:'row', gap:6 },
  presetBtn: {
    flex:1, backgroundColor:'rgba(255,255,255,0.04)',
    borderRadius:10, borderWidth:1, borderColor:C.cardBorder,
    padding:8, alignItems:'center',
  },
  presetTxt: { color:C.textMuted, fontSize:9, fontWeight:'700' },

  // Transmit
  btnTransmit: {
    width:'100%', backgroundColor:C.accent,
    borderRadius:16, padding:17, alignItems:'center',
    marginBottom:14,
    shadowColor:C.accent, shadowOpacity:0.4, shadowRadius:12, elevation:8,
  },
  btnTransmitTxt: { color:'#fff', fontSize:15, fontWeight:'800', letterSpacing:0.5 },

  // Log
  logBox: {
    width:'100%', borderWidth:1, borderColor:C.cardBorder,
    borderRadius:14, padding:14, alignItems:'center',
    minHeight:50, justifyContent:'center', marginBottom:14,
  },
  logTxt: { fontSize:13, color:C.textMuted, textAlign:'center', lineHeight:20 },

  // History
  histRow: {
    flexDirection:'row', alignItems:'center',
    paddingVertical:8, borderTopWidth:1, borderTopColor:C.cardBorder,
  },
  histEmoji: { fontSize:24 },
  histFrom:  { fontSize:13, fontWeight:'700', color:C.textMain },
  histTime:  { fontSize:11, color:C.textMuted, marginTop:1 },
});
