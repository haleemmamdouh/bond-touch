const express = require('express');
const cors = require('cors');
const app = express();

app.use(cors());
app.use(express.json());

// Memory store for touch events per pair code
// Format: { pairCode: { senderName, timestamp } }
const touchEvents = {};

// Health check
app.get('/', (req, res) => {
  res.json({ status: 'online', message: '❤️ Bond Touch Server is LIVE!' });
});

// Send a touch event
app.post('/api/touch', (req, res) => {
  const { pairCode, senderName } = req.body;
  if (!pairCode || !senderName) {
    return res.status(400).json({ error: 'Missing pairCode or senderName' });
  }

  touchEvents[pairCode] = {
    senderName: senderName,
    timestamp: Date.now()
  };

  console.log(`👉 Touch from "${senderName}" on pair code "${pairCode}"`);
  res.json({ success: true, message: `Touch sent from ${senderName}!` });
});

// Check for new touch events (called by receiver every 500ms)
app.get('/api/check', (req, res) => {
  const { pairCode, myName, lastSeen } = req.query;
  if (!pairCode || !myName) {
    return res.status(400).json({ error: 'Missing pairCode or myName' });
  }

  const event = touchEvents[pairCode];

  // Only return the event if:
  // 1. There IS an event for this pair code
  // 2. The sender is NOT me (so I don't feel my own touch)
  // 3. The event is newer than what I last saw
  if (
    event &&
    event.senderName.toLowerCase() !== myName.toLowerCase() &&
    event.timestamp > parseInt(lastSeen || 0)
  ) {
    return res.json({
      newTouch: true,
      senderName: event.senderName,
      timestamp: event.timestamp
    });
  }

  res.json({ newTouch: false });
});

app.listen(3000, () => console.log('Bond Touch Server running on port 3000!'));
