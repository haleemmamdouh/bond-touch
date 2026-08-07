/**
 * BOND TOUCH — DUAL WAVEFORM USB BRIDGE (SEPARATE LIGHT + SOUND ARRAYS)
 */

const path = require('path');
const readline = require('readline');

let mqtt, SerialPort;
try {
  mqtt = require('mqtt');
  SerialPort = require('serialport').SerialPort;
} catch(e) {
  try {
    mqtt = require(path.join(__dirname, 'node_modules', 'mqtt'));
    SerialPort = require(path.join(__dirname, 'node_modules', 'serialport')).SerialPort;
  } catch(err) {
    console.error("❌ Packages missing. Run: npm install mqtt serialport");
    process.exit(1);
  }
}

const COM_PORT  = process.argv[2] || 'COM9';
const PAIR_CODE = process.argv[3] || '102030';
const MY_NAME   = process.argv[4] || 'Arduino Nano (Physical)';

console.log("==================================================");
console.log(`⚡ BOND TOUCH — DUAL WAVEFORM USB BRIDGE`);
console.log(`🔌 USB Port: ${COM_PORT}  |  📡 Pair Code: ${PAIR_CODE}`);
console.log("==================================================");

const client = mqtt.connect('wss://broker.emqx.io:8084/mqtt');

client.on('connect', () => {
  console.log("⚡ MQTT Connected!");
  client.subscribe(`bondtouch/pair_${PAIR_CODE}`);
  console.log(`❤️  Listening on bondtouch/pair_${PAIR_CODE}...`);
});

client.on('message', (topic, message) => {
  try {
    const payload = JSON.parse(message.toString());
    if (payload.sender && payload.sender.toLowerCase() === MY_NAME.toLowerCase()) return;

    if (payload.type === 'waveform') {
      const dur   = payload.duration || 1;
      const light = payload.light    || payload.data || '0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0';
      const sound = payload.sound    || payload.data || '0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0';

      // Format: WAVE:duration:lightCSV:soundCSV
      const cmd = `WAVE:${dur}:${light}:${sound}\n`;
      console.log(`\n🎨 [${payload.sender}] Dual Waveform → ${dur}s`);
      console.log(`   💡 Light: ${light}`);
      console.log(`   🔊 Sound: ${sound}`);

      if (serial && serial.isOpen) {
        serial.write(cmd);
      }
    } else {
      console.log(`\n❤️  Touch from [${payload.sender}]`);
      if (serial && serial.isOpen) serial.write('V\n');
    }
  } catch(e) {}
});

let serial = null;
try {
  serial = new SerialPort({ path: COM_PORT, baudRate: 9600 });

  serial.on('open', () => console.log(`✅ Arduino Nano connected on ${COM_PORT}!`));

  const lineReader = readline.createInterface({ input: serial });
  lineReader.on('line', (line) => {
    const l = line.trim();
    if (l === 'T' || l.startsWith('T:')) {
      console.log(`\n👉 Physical button pressed! Sending to cloud...`);
      client.publish(`bondtouch/pair_${PAIR_CODE}`, JSON.stringify({
        sender: MY_NAME, timestamp: Date.now()
      }));
    }
  });

  serial.on('error', (err) => console.log(`⚠️  Serial Error: ${err.message}`));
} catch(e) {
  console.log(`⚠️  Could not open ${COM_PORT}: ${e.message}`);
}
