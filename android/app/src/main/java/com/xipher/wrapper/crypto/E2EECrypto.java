package com.xipher.wrapper.crypto;

import android.os.Build;
import android.util.Base64;

import java.nio.ByteBuffer;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.MessageDigest;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.SecureRandom;
import java.security.spec.PKCS8EncodedKeySpec;
import java.security.spec.X509EncodedKeySpec;
import javax.crypto.Cipher;
import javax.crypto.KeyAgreement;
import javax.crypto.Mac;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;

/**
 * End-to-End Encryption provider for Xipher Android client.
 * 
 * Implements:
 * - X25519 key exchange (via XDH on API 31+, fallback to Curve25519 on older)
 * - AES-256-GCM authenticated encryption
 * - HKDF key derivation
 * 
 * OWASP 2025: A04 - Cryptographic Failures - Fix
 */
public final class E2EECrypto {
    
    private static final String TAG = "E2EECrypto";
    private static final int GCM_NONCE_LENGTH = 12;
    private static final int GCM_TAG_LENGTH = 128; // bits
    private static final int KEY_SIZE = 32;
    
    private static final SecureRandom secureRandom = new SecureRandom();
    
    // Private constructor - utility class
    private E2EECrypto() {}
    
    /**
     * Encrypted message container.
     */
    public static class EncryptedMessage {
        public final byte[] ciphertext;
        public final byte[] nonce;
        public final long messageId;
        public final long timestamp;
        
        public EncryptedMessage(byte[] ciphertext, byte[] nonce, long messageId, long timestamp) {
            this.ciphertext = ciphertext;
            this.nonce = nonce;
            this.messageId = messageId;
            this.timestamp = timestamp;
        }
        
        /**
         * Serialize for transmission.
         */
        public byte[] toBytes() {
            ByteBuffer buffer = ByteBuffer.allocate(
                8 + 8 + 4 + nonce.length + 4 + ciphertext.length
            );
            buffer.putLong(messageId);
            buffer.putLong(timestamp);
            buffer.putInt(nonce.length);
            buffer.put(nonce);
            buffer.putInt(ciphertext.length);
            buffer.put(ciphertext);
            return buffer.array();
        }
        
        /**
         * Deserialize from transmission bytes.
         */
        public static EncryptedMessage fromBytes(byte[] data) {
            ByteBuffer buffer = ByteBuffer.wrap(data);
            long messageId = buffer.getLong();
            long timestamp = buffer.getLong();
            int nonceLen = buffer.getInt();
            byte[] nonce = new byte[nonceLen];
            buffer.get(nonce);
            int cipherLen = buffer.getInt();
            byte[] ciphertext = new byte[cipherLen];
            buffer.get(ciphertext);
            return new EncryptedMessage(ciphertext, nonce, messageId, timestamp);
        }
        
        /**
         * Base64 encode for JSON transport.
         */
        public String toBase64() {
            return Base64.encodeToString(toBytes(), Base64.NO_WRAP);
        }
        
        /**
         * Decode from Base64.
         */
        public static EncryptedMessage fromBase64(String encoded) {
            return fromBytes(Base64.decode(encoded, Base64.NO_WRAP));
        }
    }
    
    /**
     * Generate a new key pair for key exchange.
     * Uses X25519 on API 31+, falls back to EC P-256 on older devices.
     */
    public static KeyPair generateKeyPair() throws Exception {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            // API 31+ supports XDH (X25519)
            KeyPairGenerator kpg = KeyPairGenerator.getInstance("XDH");
            kpg.initialize(255); // X25519 curve size
            return kpg.generateKeyPair();
        } else {
            // Fallback to EC P-256 for older devices
            KeyPairGenerator kpg = KeyPairGenerator.getInstance("EC");
            kpg.initialize(256);
            return kpg.generateKeyPair();
        }
    }
    
    /**
     * Derive shared secret from key exchange.
     */
    public static byte[] deriveSharedSecret(PrivateKey ownPrivateKey, PublicKey peerPublicKey) 
            throws Exception {
        String algorithm = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S ? "XDH" : "ECDH";
        KeyAgreement keyAgreement = KeyAgreement.getInstance(algorithm);
        keyAgreement.init(ownPrivateKey);
        keyAgreement.doPhase(peerPublicKey, true);
        return keyAgreement.generateSecret();
    }
    
    /**
     * Derive encryption key from shared secret using HKDF.
     */
    public static byte[] deriveMessageKey(byte[] sharedSecret, String context) throws Exception {
        // HKDF-Extract
        Mac hmac = Mac.getInstance("HmacSHA256");
        byte[] salt = new byte[32]; // Zero salt
        SecretKeySpec saltKey = new SecretKeySpec(salt, "HmacSHA256");
        hmac.init(saltKey);
        byte[] prk = hmac.doFinal(sharedSecret);
        
        // HKDF-Expand
        byte[] info = context.getBytes("UTF-8");
        byte[] okm = new byte[KEY_SIZE];
        
        hmac.init(new SecretKeySpec(prk, "HmacSHA256"));
        hmac.update(info);
        hmac.update((byte) 1);
        byte[] t = hmac.doFinal();
        System.arraycopy(t, 0, okm, 0, Math.min(t.length, KEY_SIZE));
        
        return okm;
    }
    
    /**
     * Encrypt a message using AES-256-GCM.
     * 
     * @param plaintext Message to encrypt
     * @param key 32-byte encryption key
     * @param associatedData Optional authenticated data (e.g., sender ID)
     * @return Encrypted message with nonce and ciphertext
     */
    public static EncryptedMessage encrypt(String plaintext, byte[] key, byte[] associatedData) 
            throws Exception {
        if (key.length != KEY_SIZE) {
            throw new IllegalArgumentException("Key must be 32 bytes");
        }
        
        // Generate random nonce
        byte[] nonce = new byte[GCM_NONCE_LENGTH];
        secureRandom.nextBytes(nonce);
        
        // Initialize cipher
        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        SecretKeySpec keySpec = new SecretKeySpec(key, "AES");
        GCMParameterSpec gcmSpec = new GCMParameterSpec(GCM_TAG_LENGTH, nonce);
        cipher.init(Cipher.ENCRYPT_MODE, keySpec, gcmSpec);
        
        // Add associated data if provided
        if (associatedData != null && associatedData.length > 0) {
            cipher.updateAAD(associatedData);
        }
        
        // Encrypt
        byte[] ciphertext = cipher.doFinal(plaintext.getBytes("UTF-8"));
        
        return new EncryptedMessage(
            ciphertext, 
            nonce, 
            0, // Set by caller
            System.currentTimeMillis()
        );
    }
    
    /**
     * Decrypt a message using AES-256-GCM.
     * 
     * @param encrypted Encrypted message with nonce and ciphertext
     * @param key 32-byte decryption key
     * @param associatedData Optional authenticated data
     * @return Decrypted plaintext, or null if decryption/authentication fails
     */
    public static String decrypt(EncryptedMessage encrypted, byte[] key, byte[] associatedData) {
        if (key.length != KEY_SIZE) {
            throw new IllegalArgumentException("Key must be 32 bytes");
        }
        
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            SecretKeySpec keySpec = new SecretKeySpec(key, "AES");
            GCMParameterSpec gcmSpec = new GCMParameterSpec(GCM_TAG_LENGTH, encrypted.nonce);
            cipher.init(Cipher.DECRYPT_MODE, keySpec, gcmSpec);
            
            // Add associated data if provided
            if (associatedData != null && associatedData.length > 0) {
                cipher.updateAAD(associatedData);
            }
            
            // Decrypt
            byte[] plaintext = cipher.doFinal(encrypted.ciphertext);
            return new String(plaintext, "UTF-8");
            
        } catch (Exception e) {
            // Authentication failed or decryption error
            // Do NOT log the exception to avoid leaking information
            return null;
        }
    }
    
    /**
     * Generate cryptographically secure random bytes.
     */
    public static byte[] randomBytes(int length) {
        byte[] bytes = new byte[length];
        secureRandom.nextBytes(bytes);
        return bytes;
    }
    
    /**
     * Compute SHA-256 hash.
     */
    public static byte[] sha256(byte[] data) throws Exception {
        MessageDigest digest = MessageDigest.getInstance("SHA-256");
        return digest.digest(data);
    }
    
    /**
     * Compute HMAC-SHA256.
     */
    public static byte[] hmacSha256(byte[] key, byte[] data) throws Exception {
        Mac hmac = Mac.getInstance("HmacSHA256");
        hmac.init(new SecretKeySpec(key, "HmacSHA256"));
        return hmac.doFinal(data);
    }
    
    /**
     * Constant-time comparison of byte arrays.
     */
    public static boolean constantTimeEquals(byte[] a, byte[] b) {
        if (a.length != b.length) {
            return false;
        }
        int result = 0;
        for (int i = 0; i < a.length; i++) {
            result |= a[i] ^ b[i];
        }
        return result == 0;
    }
    
    /**
     * Export public key to Base64 for transmission.
     */
    public static String exportPublicKey(PublicKey publicKey) {
        return Base64.encodeToString(publicKey.getEncoded(), Base64.NO_WRAP);
    }
    
    /**
     * Import public key from Base64.
     */
    public static PublicKey importPublicKey(String base64Key) throws Exception {
        byte[] keyBytes = Base64.decode(base64Key, Base64.NO_WRAP);
        X509EncodedKeySpec keySpec = new X509EncodedKeySpec(keyBytes);
        
        String algorithm = Build.VERSION.SDK_INT >= Build.VERSION_CODES.S ? "XDH" : "EC";
        KeyFactory keyFactory = KeyFactory.getInstance(algorithm);
        return keyFactory.generatePublic(keySpec);
    }
    
    /**
     * Simple message ratchet for forward secrecy.
     */
    public static class MessageRatchet {
        private byte[] chainKey;
        private int messageNumber;
        
        public MessageRatchet(byte[] initialChainKey) {
            if (initialChainKey.length != KEY_SIZE) {
                throw new IllegalArgumentException("Chain key must be 32 bytes");
            }
            this.chainKey = initialChainKey.clone();
            this.messageNumber = 0;
        }
        
        /**
         * Get next message key and advance the ratchet.
         */
        public synchronized byte[] nextMessageKey() throws Exception {
            // Derive message key
            String info = "message_key_" + messageNumber;
            byte[] messageKey = hmacSha256(chainKey, info.getBytes("UTF-8"));
            
            // Advance chain (forward secrecy - old keys cannot be recovered)
            chainKey = hmacSha256(chainKey, "chain_advance".getBytes("UTF-8"));
            messageNumber++;
            
            return messageKey;
        }
        
        /**
         * Get current chain key for serialization.
         */
        public byte[] getChainKey() {
            return chainKey.clone();
        }
        
        /**
         * Get current message number.
         */
        public int getMessageNumber() {
            return messageNumber;
        }
    }
}
