package com.xipher.wrapper.security;

import okhttp3.CertificatePinner;
import okhttp3.OkHttpClient;
import okhttp3.logging.HttpLoggingInterceptor;

import com.xipher.wrapper.BuildConfig;

import java.util.concurrent.TimeUnit;

/**
 * Secure HTTP Client Factory
 * OWASP Mobile Top 10 2024: M5 - Insecure Communication
 * 
 * Provides OkHttpClient instances with:
 * - Certificate pinning
 * - Secure TLS configuration
 * - No logging in release builds
 */
public final class SecureHttpClient {
    
    // Certificate pins for xipher.pro (generated 2026-01-21)
    // Regenerate when certificates are renewed
    private static final String PIN_XIPHER_PRO = "sha256/2tLOZ+z+3mmOMwAzZ9AGxhG8v6Egp7qM6WmBIL6n68k=";
    private static final String PIN_API_XIPHER_PRO = "sha256/Y8iUIdHeBywQt6qu7TmRHl/5MmwziF7cBqi+ET/snF4=";
    
    private static OkHttpClient instance;
    
    private SecureHttpClient() {}
    
    /**
     * Get a secure OkHttpClient instance with certificate pinning.
     */
    public static synchronized OkHttpClient getInstance() {
        if (instance == null) {
            instance = createClient();
        }
        return instance;
    }
    
    private static OkHttpClient createClient() {
        OkHttpClient.Builder builder = new OkHttpClient.Builder()
                .connectTimeout(30, TimeUnit.SECONDS)
                .readTimeout(30, TimeUnit.SECONDS)
                .writeTimeout(30, TimeUnit.SECONDS);
        
        // Certificate pinning
        CertificatePinner certificatePinner = new CertificatePinner.Builder()
                .add("xipher.pro", PIN_XIPHER_PRO)
                .add("xipher.pro", PIN_API_XIPHER_PRO) // backup
                .add("api.xipher.pro", PIN_API_XIPHER_PRO)
                .add("api.xipher.pro", PIN_XIPHER_PRO) // backup
                .build();
        
        builder.certificatePinner(certificatePinner);
        
        // Only add logging interceptor in debug builds
        if (BuildConfig.DEBUG) {
            HttpLoggingInterceptor logging = new HttpLoggingInterceptor();
            logging.setLevel(HttpLoggingInterceptor.Level.BASIC);
            builder.addInterceptor(logging);
        }
        
        return builder.build();
    }
    
    /**
     * Create a client without certificate pinning (for external URLs only).
     * Use with caution!
     */
    public static OkHttpClient createExternalClient() {
        OkHttpClient.Builder builder = new OkHttpClient.Builder()
                .connectTimeout(30, TimeUnit.SECONDS)
                .readTimeout(30, TimeUnit.SECONDS)
                .writeTimeout(30, TimeUnit.SECONDS);
        
        // Only add logging interceptor in debug builds
        if (BuildConfig.DEBUG) {
            HttpLoggingInterceptor logging = new HttpLoggingInterceptor();
            logging.setLevel(HttpLoggingInterceptor.Level.BASIC);
            builder.addInterceptor(logging);
        }
        
        return builder.build();
    }
}
