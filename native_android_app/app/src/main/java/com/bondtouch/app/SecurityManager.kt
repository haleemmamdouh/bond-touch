package com.bondtouch.app

import java.nio.charset.StandardCharsets
import java.security.SecureRandom
import javax.crypto.Cipher
import javax.crypto.Mac
import javax.crypto.spec.IvParameterSpec
import javax.crypto.spec.SecretKeySpec
import org.json.JSONObject

/**
 * BOND TOUCH NATIVE ANDROID SECURITY MANAGER
 * Implements End-to-End Encryption (E2EE) using AES-256-CBC + HMAC-SHA256
 */
object SecurityManager {

    fun hexToBytes(hex: String): ByteArray {
        val len = hex.length
        val data = ByteArray(len / 2)
        var i = 0
        while (i < len) {
            data[i / 2] = ((Character.digit(hex[i], 16) shl 4) + Character.digit(hex[i + 1], 16)).toByte()
            i += 2
        }
        return data
    }

    fun bytesToHex(bytes: ByteArray): String {
        val sb = StringBuilder()
        for (b in bytes) {
            sb.append(String.format("%02x", b))
        }
        return sb.toString()
    }

    fun encryptPayload(payloadJson: String, keyHex: String, pairId: String): String {
        val keyBytes = hexToBytes(keyHex)
        val secretKey = SecretKeySpec(keyBytes, "AES")

        // 1. Generate 16-byte random IV
        val iv = ByteArray(16)
        SecureRandom().nextBytes(iv)
        val ivSpec = IvParameterSpec(iv)

        // 2. Encrypt JSON with AES-256-CBC
        val cipher = Cipher.getInstance("AES/CBC/PKCS5Padding")
        cipher.init(Cipher.ENCRYPT_MODE, secretKey, ivSpec)
        val ciphertext = cipher.doFinal(payloadJson.toByteArray(StandardCharsets.UTF_8))

        val ivHex = bytesToHex(iv)
        val ciphertextHex = bytesToHex(ciphertext)

        // 3. Compute HMAC-SHA256 signature over (pairId + ivHex + ciphertextHex)
        val signInput = "$pairId:$ivHex:$ciphertextHex".toByteArray(StandardCharsets.UTF_8)
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(keyBytes, "HmacSHA256"))
        val signatureHex = bytesToHex(mac.doFinal(signInput))

        val packet = JSONObject()
        packet.put("pair_id", pairId)
        packet.put("iv", ivHex)
        packet.put("ciphertext", ciphertextHex)
        packet.put("signature", signatureHex)

        return packet.toString()
    }

    fun decryptPayload(packetJsonStr: String, keyHex: String): JSONObject {
        val packet = JSONObject(packetJsonStr)
        val pairId = packet.getString("pair_id")
        val ivHex = packet.getString("iv")
        val ciphertextHex = packet.getString("ciphertext")
        val signatureHex = packet.getString("signature")

        val keyBytes = hexToBytes(keyHex)

        // 1. Verify HMAC-SHA256 signature first
        val signInput = "$pairId:$ivHex:$ciphertextHex".toByteArray(StandardCharsets.UTF_8)
        val mac = Mac.getInstance("HmacSHA256")
        mac.init(SecretKeySpec(keyBytes, "HmacSHA256"))
        val calculatedSignature = bytesToHex(mac.doFinal(signInput))

        if (calculatedSignature != signatureHex) {
            throw SecurityException("HMAC signature mismatch! Packet tampered with.")
        }

        // 2. Decrypt AES-256-CBC ciphertext
        val secretKey = SecretKeySpec(keyBytes, "AES")
        val ivSpec = IvParameterSpec(hexToBytes(ivHex))

        val cipher = Cipher.getInstance("AES/CBC/PKCS5Padding")
        cipher.init(Cipher.DECRYPT_MODE, secretKey, ivSpec)
        val decryptedBytes = cipher.doFinal(hexToBytes(ciphertextHex))

        return JSONObject(String(decryptedBytes, StandardCharsets.UTF_8))
    }
}
