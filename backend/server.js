/**
 * ====================================================================================
 * BOND TOUCH PROTOTYPE BACKEND RELAY SERVER
 * سيرفر التتابع والربط لأسورة بوند تاتش الذكية
 * ====================================================================================
 * Tech Stack / التقنيات المستعملة:
 * - Node.js + Express.js
 * - Supabase Client (User pairing & database storage)
 * - Firebase Admin SDK (High-priority FCM Push Notifications)
 * ====================================================================================
 */

require('dotenv').config();
const express = require('express');
const cors = require('cors');
const admin = require('firebase-admin');
const { createClient } = require('@supabase/supabase-js');

const app = express();
const PORT = process.env.PORT || 3000;

// Middleware
app.use(cors());
app.use(express.json());

// ------------------------------------------------------------------------------------
// 1. SUPABASE INITIALIZATION / تهيئة قاعدة بيانات سوبابيز
// ------------------------------------------------------------------------------------
const supabaseUrl = process.env.SUPABASE_URL;
const supabaseKey = process.env.SUPABASE_SERVICE_ROLE_KEY;

let supabase = null;
if (supabaseUrl && supabaseKey) {
  supabase = createClient(supabaseUrl, supabaseKey);
  console.log('✅ Supabase Client initialized / تم الاتصال بقاعدة بيانات سوبابيز');
} else {
  console.warn('⚠️ SUPABASE_URL or SUPABASE_SERVICE_ROLE_KEY missing in env / لم يتم إدخال مفتاح سوبابيز');
}

// ------------------------------------------------------------------------------------
// 2. FIREBASE ADMIN SDK INITIALIZATION / تهيئة خدمة إشعارات فيربيز
// ------------------------------------------------------------------------------------
try {
  let serviceAccount = null;
  if (process.env.FIREBASE_SERVICE_ACCOUNT_JSON) {
    serviceAccount = JSON.parse(process.env.FIREBASE_SERVICE_ACCOUNT_JSON);
  } else {
    // Fallback attempt to load serviceAccountKey.json file if present locally
    serviceAccount = require('./serviceAccountKey.json');
  }

  admin.initializeApp({
    credential: admin.credential.cert(serviceAccount)
  });
  console.log('✅ Firebase Admin SDK initialized / تم الاتصال بخدمة إشعارات فيربيز');
} catch (err) {
  console.warn('⚠️ Firebase Admin SDK initialization warning:', err.message);
  console.warn('   (Make sure FIREBASE_SERVICE_ACCOUNT_JSON is set in Railway/Render env vars)');
}

// Memory fallback store (Used if Supabase credentials are not configured yet during early prototype test)
const memoryStore = {
  // Format: { [pairCode]: { partnerA: { name, token, lastTouch }, partnerB: { name, token, lastTouch } } }
};

// ------------------------------------------------------------------------------------
// REST ENDPOINTS / نقاط الاتصال
// ------------------------------------------------------------------------------------

/**
 * GET /api/health
 * Healthcheck route for monitoring / اختبار عمل السيرفر
 */
app.get('/api/health', (req, res) => {
  res.json({
    status: 'online',
    timestamp: new Date().toISOString(),
    message: 'Bond Touch Prototype Relay Server Operational 🚀'
  });
});

/**
 * POST /api/register-token
 * Register or update FCM Push Token for a user & pair code
 * تسجيل أو تحديث رمز الإشعارات للهاتف
 */
app.post('/api/register-token', async (req, res) => {
  const { pairCode, userName, fcmToken } = req.body;

  if (!pairCode || !userName || !fcmToken) {
    return res.status(400).json({ error: 'Missing pairCode, userName, or fcmToken' });
  }

  try {
    if (supabase) {
      // Upsert into Supabase "devices" table
      const { data, error } = await supabase
        .from('devices')
        .upsert({
          pair_code: pairCode.trim(),
          user_name: userName.trim(),
          fcm_token: fcmToken.trim(),
          updated_at: new Date().toISOString()
        }, { onConflict: 'pair_code, user_name' });

      if (error) throw error;
    } else {
      // Memory Store Fallback
      if (!memoryStore[pairCode]) memoryStore[pairCode] = {};
      memoryStore[pairCode][userName] = {
        name: userName,
        fcmToken: fcmToken,
        updatedAt: new Date()
      };
    }

    console.log(`📱 Registered FCM Token for User: "${userName}" with Pair Code: "${pairCode}"`);
    return res.json({ success: true, message: 'FCM Token registered successfully' });
  } catch (err) {
    console.error('Error registering token:', err);
    return res.status(500).json({ error: err.message });
  }
});

/**
 * POST /api/touch
 * Send a Touch event from Phone A to Partner B
 * إرسال لمسة من هاتف إلى هاتف الشريك الآخر
 */
app.post('/api/touch', async (req, res) => {
  const { pairCode, senderName } = req.body;

  if (!pairCode || !senderName) {
    return res.status(400).json({ error: 'Missing pairCode or senderName' });
  }

  console.log(`👉 Touch event received from sender: "${senderName}" (Pair Code: "${pairCode}")`);

  try {
    let partnerToken = null;
    let partnerName = null;

    if (supabase) {
      // Query Supabase for all devices in this pair_code EXCEPT the sender
      const { data: devices, error } = await supabase
        .from('devices')
        .select('*')
        .eq('pair_code', pairCode.trim());

      if (error) throw error;

      const partnerDevice = (devices || []).find(d => d.user_name.toLowerCase() !== senderName.trim().toLowerCase());
      if (partnerDevice) {
        partnerToken = partnerDevice.fcm_token;
        partnerName = partnerDevice.user_name;
      }
    } else {
      // Memory Store Query
      const pair = memoryStore[pairCode];
      if (pair) {
        const partnerKey = Object.keys(pair).find(k => k.toLowerCase() !== senderName.trim().toLowerCase());
        if (partnerKey) {
          partnerToken = pair[partnerKey].fcmToken;
          partnerName = pair[partnerKey].name;
        }
      }
    }

    if (!partnerToken) {
      console.warn(`⚠️ Partner not found or partner token missing for pair code: ${pairCode}`);
      return res.status(444).json({
        success: false,
        message: 'Partner device not found or partner app has not registered FCM token yet.'
      });
    }

    // Prepare FCM High-Priority Data Payload / إعداد الإشعار فائق الأولوية
    const messagePayload = {
      token: partnerToken,
      data: {
        type: 'TOUCH_EVENT',
        senderName: senderName,
        timestamp: new Date().toISOString()
      },
      android: {
        priority: 'high'
      },
      apns: {
        headers: {
          'apns-priority': '10'
        },
        payload: {
          aps: {
            'content-available': 1
          }
        }
      }
    };

    // Dispatch FCM Notification / إرسال الإشعار
    if (admin.apps.length > 0) {
      const response = await admin.messaging().send(messagePayload);
      console.log(`⚡ FCM Push Notification dispatched successfully to ${partnerName}:`, response);
    } else {
      console.log(`[SIMULATED PUSH] Would send FCM Push Notification to partner ${partnerName}`);
    }

    return res.json({
      success: true,
      message: `Touch delivered to partner ${partnerName || ''}`,
      timestamp: new Date().toISOString()
    });

  } catch (err) {
    console.error('Error sending touch payload:', err);
    return res.status(500).json({ error: err.message });
  }
});

// Start listening / تشغيل السيرفر
app.listen(PORT, () => {
  console.log(`
  =============================================================
  ❤️ BOND TOUCH RELAY SERVER IS RUNNING ON PORT ${PORT} ❤️
  =============================================================
  Local URL: http://localhost:${PORT}
  Health check: http://localhost:${PORT}/api/health
  =============================================================
  `);
});
