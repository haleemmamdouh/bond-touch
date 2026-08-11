/**
 * ====================================================================================
 * BOND TOUCH PRODUCTION SECURITY & CRYPTOGRAPHY ENGINE
 * ====================================================================================
 * Protocol: End-to-End Encryption (E2EE)
 * Cipher: AES-256-CBC
 * HMAC: HMAC-SHA256 (Message Authentication Code)
 * Nonce: Cryptographically Secure 16-byte Random Initialization Vector (IV)
 * Anti-Replay: Monotonic 64-bit Sequence Counter + Timestamp Window Verification
 * ====================================================================================
 */

class BondTouchCrypto {
  /**
   * Generate a secure random 256-bit (32-byte) hex key for a newly paired couple.
   */
  static async generatePairKeyHex() {
    const keyBytes = new Uint8Array(32);
    window.crypto.getRandomValues(keyBytes);
    return Array.from(keyBytes).map(b => b.toString(16).padStart(2, '0')).join('');
  }

  /**
   * Convert Hex string to ArrayBuffer / Uint8Array
   */
  static hexToBytes(hex) {
    const bytes = new Uint8Array(hex.length / 2);
    for (let i = 0; i < hex.length; i += 2) {
      bytes[i / 2] = parseInt(hex.substring(i, i + 2), 16);
    }
    return bytes;
  }

  /**
   * Convert ArrayBuffer / Uint8Array to Hex string
   */
  static bytesToHex(bytes) {
    return Array.from(new Uint8Array(bytes))
      .map(b => b.toString(16).padStart(2, '0'))
      .join('');
  }

  /**
   * Import Hex secret as WebCrypto CryptoKey (AES-CBC + HMAC)
   */
  static async importKeys(keyHex) {
    const rawKey = this.hexToBytes(keyHex);
    
    // Import AES-CBC Key
    const aesKey = await window.crypto.subtle.importKey(
      "raw",
      rawKey,
      { name: "AES-CBC" },
      false,
      ["encrypt", "decrypt"]
    );

    // Import HMAC Key
    const hmacKey = await window.crypto.subtle.importKey(
      "raw",
      rawKey,
      { name: "HMAC", hash: "SHA-256" },
      false,
      ["sign", "verify"]
    );

    return { aesKey, hmacKey };
  }

  /**
   * Encrypt a touch event payload with AES-256-CBC and sign with HMAC-SHA256.
   * @param {Object} payload - { type: "TOUCH", timestamp: number, seq: number, sender: string }
   * @param {string} keyHex - 64-character Hex string (32 bytes)
   * @returns {Object} { pair_id, iv, ciphertext, signature }
   */
  static async encryptPayload(payload, keyHex, pairId) {
    const { aesKey, hmacKey } = await this.importKeys(keyHex);

    // 1. Generate 16-byte random IV
    const iv = window.crypto.getRandomValues(new Uint8Array(16));

    // 2. Encode JSON string to UTF-8 bytes
    const jsonStr = JSON.stringify(payload);
    const textEncoder = new TextEncoder();
    const dataBytes = textEncoder.encode(jsonStr);

    // 3. Encrypt data with AES-256-CBC
    const encryptedBuffer = await window.crypto.subtle.encrypt(
      { name: "AES-CBC", iv },
      aesKey,
      dataBytes
    );

    const ciphertextHex = this.bytesToHex(encryptedBuffer);
    const ivHex = this.bytesToHex(iv);

    // 4. Compute HMAC-SHA256 signature over (pairId + ivHex + ciphertextHex)
    const signInput = new TextEncoder().encode(`${pairId}:${ivHex}:${ciphertextHex}`);
    const signatureBuffer = await window.crypto.subtle.sign(
      "HMAC",
      hmacKey,
      signInput
    );
    const signatureHex = this.bytesToHex(signatureBuffer);

    return {
      pair_id: pairId,
      iv: ivHex,
      ciphertext: ciphertextHex,
      signature: signatureHex
    };
  }

  /**
   * Decrypt and verify an incoming encrypted touch event payload.
   * @param {Object} encryptedPacket - { pair_id, iv, ciphertext, signature }
   * @param {string} keyHex - 64-character Hex string (32 bytes)
   * @param {number} lastSeq - Previous valid sequence number for anti-replay
   * @returns {Object} Decrypted payload object
   */
  static async decryptPayload(encryptedPacket, keyHex, lastSeq = 0) {
    const { pair_id, iv, ciphertext, signature } = encryptedPacket;
    const { aesKey, hmacKey } = await this.importKeys(keyHex);

    // 1. Verify HMAC-SHA256 signature first (Encrypt-then-MAC protection)
    const signInput = new TextEncoder().encode(`${pair_id}:${iv}:${ciphertext}`);
    const isValidSig = await window.crypto.subtle.verify(
      "HMAC",
      hmacKey,
      this.hexToBytes(signature),
      signInput
    );

    if (!isValidSig) {
      throw new Error("SECURITY_ERROR: Signature verification failed! Packet tampered with or invalid key.");
    }

    // 2. Decrypt AES-256-CBC ciphertext
    const ivBytes = this.hexToBytes(iv);
    const ciphertextBytes = this.hexToBytes(ciphertext);

    const decryptedBuffer = await window.crypto.subtle.decrypt(
      { name: "AES-CBC", iv: ivBytes },
      aesKey,
      ciphertextBytes
    );

    const textDecoder = new TextDecoder();
    const jsonStr = textDecoder.decode(decryptedBuffer);
    const payload = JSON.parse(jsonStr);

    // 3. Anti-Replay Check: Timestamp window (< 5000ms) & Sequence Counter check
    const now = Date.now();
    const timeDelta = Math.abs(now - payload.timestamp);
    if (timeDelta > 8000) {
      throw new Error(`REPLAY_ATTACK_ERROR: Packet timestamp expired (${timeDelta}ms old).`);
    }

    if (payload.seq <= lastSeq) {
      throw new Error(`REPLAY_ATTACK_ERROR: Sequence number (${payload.seq}) <= last seen (${lastSeq}).`);
    }

    return payload;
  }
}

// Export for ES Module or Global window scope
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { BondTouchCrypto };
} else {
  window.BondTouchCrypto = BondTouchCrypto;
}
