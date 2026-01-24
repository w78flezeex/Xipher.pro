package com.xipher.wrapper.wallet;

import android.content.Context;
import android.util.Log;
import android.webkit.JavascriptInterface;
import android.os.Handler;
import android.os.Looper;

import org.json.JSONObject;
import org.json.JSONArray;

import java.util.Map;

/**
 * JavaScript Bridge for Wallet
 * 
 * Provides secure interface between WebView and native wallet.
 * All methods are validated and rate-limited.
 */
public class WalletBridge {
    
    private static final String TAG = "WalletBridge";
    
    private final Context context;
    private final WalletManager walletManager;
    private final WalletSecurity security;
    private final Handler mainHandler;
    
    public WalletBridge(Context context) {
        this.context = context;
        this.walletManager = WalletManager.getInstance(context);
        this.security = WalletSecurity.getInstance(context);
        this.mainHandler = new Handler(Looper.getMainLooper());
    }
    
    // ============= WALLET STATUS =============
    
    @JavascriptInterface
    public String hasWallet() {
        try {
            JSONObject result = new JSONObject();
            result.put("success", true);
            result.put("hasWallet", walletManager.hasWallet());
            return result.toString();
        } catch (Exception e) {
            return errorJson("Failed to check wallet status");
        }
    }
    
    @JavascriptInterface
    public String getAddresses() {
        try {
            if (!walletManager.hasWallet()) {
                return errorJson("No wallet");
            }
            
            JSONObject result = new JSONObject();
            result.put("success", true);
            
            JSONObject addresses = new JSONObject();
            String[] chains = {"ethereum", "polygon", "solana", "ton"};
            for (String chain : chains) {
                String address = walletManager.getAddress(chain);
                if (address != null) {
                    addresses.put(chain, address);
                }
            }
            result.put("addresses", addresses);
            
            return result.toString();
        } catch (Exception e) {
            return errorJson("Failed to get addresses");
        }
    }
    
    // ============= CREATE WALLET =============
    
    @JavascriptInterface
    public String createWallet(String password) {
        try {
            // Rate limit
            if (!security.checkRateLimit("create_wallet")) {
                return errorJson("Rate limit exceeded");
            }
            
            // Validate password
            WalletSecurity.ValidationResult passResult = security.validatePassword(password);
            if (!passResult.valid) {
                return errorJson(passResult.error);
            }
            
            // Create wallet synchronously for JS bridge
            String[] seedHolder = new String[1];
            String[] errorHolder = new String[1];
            final Object lock = new Object();
            
            walletManager.createWallet(password, new WalletManager.WalletCallback<String>() {
                @Override
                public void onSuccess(String mnemonic) {
                    synchronized (lock) {
                        seedHolder[0] = mnemonic;
                        lock.notify();
                    }
                }
                
                @Override
                public void onError(String error) {
                    synchronized (lock) {
                        errorHolder[0] = error;
                        lock.notify();
                    }
                }
            });
            
            synchronized (lock) {
                try {
                    lock.wait(30000); // 30 sec timeout
                } catch (InterruptedException e) {
                    return errorJson("Timeout");
                }
            }
            
            if (errorHolder[0] != null) {
                return errorJson(errorHolder[0]);
            }
            
            JSONObject result = new JSONObject();
            result.put("success", true);
            result.put("mnemonic", seedHolder[0]);
            return result.toString();
            
        } catch (Exception e) {
            Log.e(TAG, "createWallet error", e);
            return errorJson("Failed to create wallet");
        }
    }
    
    // ============= IMPORT WALLET =============
    
    @JavascriptInterface
    public String importWallet(String mnemonic, String password) {
        try {
            // Rate limit
            if (!security.checkRateLimit("import_wallet")) {
                return errorJson("Rate limit exceeded");
            }
            
            // Validate seed phrase
            WalletSecurity.ValidationResult seedResult = security.validateSeedPhrase(mnemonic);
            if (!seedResult.valid) {
                return errorJson(seedResult.error);
            }
            
            // Validate password
            WalletSecurity.ValidationResult passResult = security.validatePassword(password);
            if (!passResult.valid) {
                return errorJson(passResult.error);
            }
            
            final Boolean[] successHolder = new Boolean[1];
            final String[] errorHolder = new String[1];
            final Object lock = new Object();
            
            walletManager.importWallet(mnemonic, password, new WalletManager.WalletCallback<Boolean>() {
                @Override
                public void onSuccess(Boolean result) {
                    synchronized (lock) {
                        successHolder[0] = result;
                        lock.notify();
                    }
                }
                
                @Override
                public void onError(String error) {
                    synchronized (lock) {
                        errorHolder[0] = error;
                        lock.notify();
                    }
                }
            });
            
            synchronized (lock) {
                try {
                    lock.wait(30000);
                } catch (InterruptedException e) {
                    return errorJson("Timeout");
                }
            }
            
            if (errorHolder[0] != null) {
                return errorJson(errorHolder[0]);
            }
            
            JSONObject result = new JSONObject();
            result.put("success", true);
            return result.toString();
            
        } catch (Exception e) {
            Log.e(TAG, "importWallet error", e);
            return errorJson("Failed to import wallet");
        }
    }
    
    // ============= GET BALANCES =============
    
    @JavascriptInterface
    public String getBalances() {
        try {
            if (!walletManager.hasWallet()) {
                return errorJson("No wallet");
            }
            
            Map<String, WalletManager.WalletBalance> balances = walletManager.getBalances();
            
            JSONObject result = new JSONObject();
            result.put("success", true);
            
            JSONArray balanceArray = new JSONArray();
            for (Map.Entry<String, WalletManager.WalletBalance> entry : balances.entrySet()) {
                JSONObject balance = new JSONObject();
                balance.put("chain", entry.getKey());
                balance.put("amount", entry.getValue().crypto);
                balance.put("usd", entry.getValue().usd);
                balanceArray.put(balance);
            }
            result.put("balances", balanceArray);
            
            return result.toString();
        } catch (Exception e) {
            return errorJson("Failed to get balances");
        }
    }
    
    @JavascriptInterface
    public void refreshBalances(String callbackId) {
        walletManager.fetchAllBalances(new WalletManager.WalletCallback<Map<String, WalletManager.WalletBalance>>() {
            @Override
            public void onSuccess(Map<String, WalletManager.WalletBalance> result) {
                try {
                    JSONObject json = new JSONObject();
                    json.put("success", true);
                    
                    JSONArray balanceArray = new JSONArray();
                    for (Map.Entry<String, WalletManager.WalletBalance> entry : result.entrySet()) {
                        JSONObject balance = new JSONObject();
                        balance.put("chain", entry.getKey());
                        balance.put("amount", entry.getValue().crypto);
                        balance.put("usd", entry.getValue().usd);
                        balanceArray.put(balance);
                    }
                    json.put("balances", balanceArray);
                    
                    executeCallback(callbackId, json.toString());
                } catch (Exception e) {
                    executeCallback(callbackId, errorJson("Parse error"));
                }
            }
            
            @Override
            public void onError(String error) {
                executeCallback(callbackId, errorJson(error));
            }
        });
    }
    
    // ============= SEND TRANSACTION =============
    
    @JavascriptInterface
    public String sendTransaction(String recipient, String amount, String chain, String password) {
        try {
            // Rate limit
            if (!security.checkRateLimit("send")) {
                return errorJson("Rate limit exceeded");
            }
            
            // Validate address
            WalletSecurity.ValidationResult addrResult = security.validateAddress(recipient, chain);
            if (!addrResult.valid) {
                return errorJson(addrResult.error);
            }
            
            // Validate amount (18 decimals, 0.000001 min, 1000000 max)
            WalletSecurity.ValidationResult amtResult = security.validateAmount(amount, 18, 0.000001, 1000000);
            if (!amtResult.valid) {
                return errorJson(amtResult.error);
            }
            
            final String[] txHashHolder = new String[1];
            final String[] errorHolder = new String[1];
            final Object lock = new Object();
            
            walletManager.sendTransaction(recipient, amount, chain, password, 
                new WalletManager.WalletCallback<String>() {
                    @Override
                    public void onSuccess(String txHash) {
                        synchronized (lock) {
                            txHashHolder[0] = txHash;
                            lock.notify();
                        }
                    }
                    
                    @Override
                    public void onError(String error) {
                        synchronized (lock) {
                            errorHolder[0] = error;
                            lock.notify();
                        }
                    }
                });
            
            synchronized (lock) {
                try {
                    lock.wait(60000); // 60 sec timeout for tx
                } catch (InterruptedException e) {
                    return errorJson("Timeout");
                }
            }
            
            if (errorHolder[0] != null) {
                return errorJson(errorHolder[0]);
            }
            
            JSONObject result = new JSONObject();
            result.put("success", true);
            result.put("txHash", txHashHolder[0]);
            return result.toString();
            
        } catch (Exception e) {
            Log.e(TAG, "sendTransaction error", e);
            return errorJson("Failed to send transaction");
        }
    }
    
    // ============= SECURITY =============
    
    @JavascriptInterface
    public String validateAddress(String address, String chain) {
        try {
            WalletSecurity.ValidationResult result = security.validateAddress(address, chain);
            JSONObject json = new JSONObject();
            json.put("valid", result.valid);
            if (!result.valid) {
                json.put("error", result.error);
            }
            return json.toString();
        } catch (Exception e) {
            return errorJson("Validation error");
        }
    }
    
    @JavascriptInterface
    public String validatePassword(String password) {
        try {
            WalletSecurity.ValidationResult result = security.validatePassword(password);
            JSONObject json = new JSONObject();
            json.put("valid", result.valid);
            if (!result.valid) {
                json.put("error", result.error);
            }
            return json.toString();
        } catch (Exception e) {
            return errorJson("Validation error");
        }
    }
    
    @JavascriptInterface
    public String isLocked() {
        try {
            JSONObject result = new JSONObject();
            result.put("locked", security.isLocked());
            return result.toString();
        } catch (Exception e) {
            return errorJson("Error");
        }
    }
    
    // ============= HELPERS =============
    
    private String errorJson(String message) {
        try {
            JSONObject error = new JSONObject();
            error.put("success", false);
            error.put("error", security.escapeHtml(message));
            return error.toString();
        } catch (Exception e) {
            return "{\"success\":false,\"error\":\"Unknown error\"}";
        }
    }
    
    private void executeCallback(String callbackId, String data) {
        // Execute JavaScript callback via WebView
        // This needs to be implemented based on your WebView reference
        Log.d(TAG, "Callback " + callbackId + ": " + data);
    }
}
