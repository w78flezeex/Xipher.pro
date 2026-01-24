package com.xipher.wrapper.security;

import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.InputStreamReader;

/**
 * Root and Tamper Detection
 * OWASP Mobile Top 10 2024: M7 - Insufficient Binary Protections
 * 
 * Detects rooted devices, emulators, and debugging tools.
 */
public final class SecurityCheck {
    private static final String TAG = "SecurityCheck";
    
    // Common root paths
    private static final String[] ROOT_PATHS = {
        "/system/app/Superuser.apk",
        "/sbin/su",
        "/system/bin/su",
        "/system/xbin/su",
        "/data/local/xbin/su",
        "/data/local/bin/su",
        "/system/sd/xbin/su",
        "/system/bin/failsafe/su",
        "/data/local/su",
        "/su/bin/su",
        "/magisk/.core/bin/su"
    };
    
    // Dangerous packages (root managers, hooking frameworks)
    private static final String[] DANGEROUS_PACKAGES = {
        "com.noshufou.android.su",
        "com.noshufou.android.su.elite",
        "eu.chainfire.supersu",
        "com.koushikdutta.superuser",
        "com.thirdparty.superuser",
        "com.yellowes.su",
        "com.topjohnwu.magisk",
        "de.robv.android.xposed.installer",
        "com.saurik.substrate",
        "de.robv.android.xposed",
        "com.formyhm.hideroot",
        "com.amphoras.hidemyroot",
        "com.zachspong.temprootremovejb"
    };
    
    // Frida detection
    private static final String[] FRIDA_INDICATORS = {
        "frida-server",
        "linjector",
        "libfrida-gadget.so"
    };

    private SecurityCheck() {}

    /**
     * Check if device is rooted.
     */
    public static boolean isRooted() {
        return checkRootBinaries() || checkSuExists() || checkRootProperties();
    }
    
    /**
     * Check for dangerous packages (root managers, Xposed, etc.)
     */
    public static boolean hasDangerousPackages(Context context) {
        PackageManager pm = context.getPackageManager();
        for (String pkg : DANGEROUS_PACKAGES) {
            try {
                pm.getPackageInfo(pkg, PackageManager.GET_ACTIVITIES);
                Log.w(TAG, "Dangerous package detected: " + pkg);
                return true;
            } catch (PackageManager.NameNotFoundException ignored) {}
        }
        return false;
    }
    
    /**
     * Check if running on emulator.
     */
    public static boolean isEmulator() {
        return Build.FINGERPRINT.startsWith("generic")
            || Build.FINGERPRINT.startsWith("unknown")
            || Build.MODEL.contains("google_sdk")
            || Build.MODEL.contains("Emulator")
            || Build.MODEL.contains("Android SDK built for x86")
            || Build.MANUFACTURER.contains("Genymotion")
            || Build.BRAND.startsWith("generic")
            || Build.DEVICE.startsWith("generic")
            || "google_sdk".equals(Build.PRODUCT)
            || Build.HARDWARE.contains("goldfish")
            || Build.HARDWARE.contains("ranchu");
    }
    
    /**
     * Check for Frida hooking framework.
     */
    public static boolean isFridaDetected() {
        // Check for Frida server process
        try {
            Process process = Runtime.getRuntime().exec("ps");
            BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
            String line;
            while ((line = reader.readLine()) != null) {
                for (String indicator : FRIDA_INDICATORS) {
                    if (line.contains(indicator)) {
                        Log.w(TAG, "Frida indicator detected in process list");
                        return true;
                    }
                }
            }
            reader.close();
        } catch (Exception ignored) {}
        
        // Check for Frida default port
        try {
            java.net.Socket socket = new java.net.Socket("127.0.0.1", 27042);
            socket.close();
            Log.w(TAG, "Frida default port is open");
            return true;
        } catch (Exception ignored) {}
        
        return false;
    }
    
    /**
     * Check if debugger is attached.
     */
    public static boolean isDebuggerAttached() {
        return android.os.Debug.isDebuggerConnected();
    }
    
    /**
     * Comprehensive security check.
     * Returns true if device appears compromised.
     */
    public static boolean isDeviceCompromised(Context context) {
        if (isRooted()) {
            Log.w(TAG, "Device is rooted");
            return true;
        }
        if (hasDangerousPackages(context)) {
            Log.w(TAG, "Dangerous packages detected");
            return true;
        }
        if (isFridaDetected()) {
            Log.w(TAG, "Frida detected");
            return true;
        }
        if (isDebuggerAttached()) {
            Log.w(TAG, "Debugger attached");
            return true;
        }
        return false;
    }
    
    // Private helper methods
    
    private static boolean checkRootBinaries() {
        for (String path : ROOT_PATHS) {
            if (new File(path).exists()) {
                return true;
            }
        }
        return false;
    }
    
    private static boolean checkSuExists() {
        String[] paths = System.getenv("PATH").split(":");
        for (String path : paths) {
            if (new File(path, "su").exists()) {
                return true;
            }
        }
        return false;
    }
    
    private static boolean checkRootProperties() {
        String[] props = {
            "ro.debuggable",
            "ro.secure"
        };
        try {
            Process process = Runtime.getRuntime().exec("getprop ro.debuggable");
            BufferedReader reader = new BufferedReader(new InputStreamReader(process.getInputStream()));
            String line = reader.readLine();
            reader.close();
            if ("1".equals(line)) {
                return true;
            }
        } catch (Exception ignored) {}
        return false;
    }
}
