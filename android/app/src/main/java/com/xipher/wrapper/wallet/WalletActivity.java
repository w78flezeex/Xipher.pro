package com.xipher.wrapper.wallet;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.xipher.wrapper.R;

import java.text.NumberFormat;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;

/**
 * Xipher Wallet Activity
 * 
 * Main wallet interface with:
 * - Balance display
 * - Send/Receive functionality
 * - Security integration
 */
public class WalletActivity extends AppCompatActivity {
    
    private static final String TAG = "WalletActivity";
    private static final long CLIPBOARD_CLEAR_DELAY = 60_000; // 60 seconds
    
    private WalletManager walletManager;
    private WalletSecurity security;
    private Handler mainHandler;
    
    // Views
    private LinearLayout setupLayout;
    private LinearLayout mainLayout;
    private TextView totalBalanceText;
    private TextView balanceChangeText;
    private RecyclerView assetsRecycler;
    private ProgressBar loadingProgress;
    
    // State
    private boolean isLoading = false;
    
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_wallet);
        
        mainHandler = new Handler(Looper.getMainLooper());
        walletManager = WalletManager.getInstance(this);
        security = WalletSecurity.getInstance(this);
        
        initViews();
        updateUI();
    }
    
    @Override
    protected void onResume() {
        super.onResume();
        security.updateActivity();
        
        // Check session
        if (security.isLocked()) {
            showUnlockDialog();
        } else if (walletManager.hasWallet()) {
            refreshBalances();
        }
    }
    
    @Override
    protected void onPause() {
        super.onPause();
        // Lock wallet after leaving activity (optional, based on settings)
    }
    
    private void initViews() {
        setupLayout = findViewById(R.id.wallet_setup_layout);
        mainLayout = findViewById(R.id.wallet_main_layout);
        totalBalanceText = findViewById(R.id.wallet_total_balance);
        balanceChangeText = findViewById(R.id.wallet_balance_change);
        assetsRecycler = findViewById(R.id.wallet_assets_recycler);
        loadingProgress = findViewById(R.id.wallet_loading);
        
        // Setup buttons
        Button createBtn = findViewById(R.id.btn_create_wallet);
        Button importBtn = findViewById(R.id.btn_import_wallet);
        
        if (createBtn != null) {
            createBtn.setOnClickListener(v -> showCreateWalletDialog());
        }
        if (importBtn != null) {
            importBtn.setOnClickListener(v -> showImportWalletDialog());
        }
        
        // Main action buttons
        Button sendBtn = findViewById(R.id.btn_send);
        Button receiveBtn = findViewById(R.id.btn_receive);
        Button earnBtn = findViewById(R.id.btn_earn);
        Button settingsBtn = findViewById(R.id.btn_settings);
        
        if (sendBtn != null) sendBtn.setOnClickListener(v -> showSendDialog());
        if (receiveBtn != null) receiveBtn.setOnClickListener(v -> showReceiveDialog());
        if (earnBtn != null) earnBtn.setOnClickListener(v -> showEarnScreen());
        if (settingsBtn != null) settingsBtn.setOnClickListener(v -> showSettingsScreen());
        
        // Setup RecyclerView
        if (assetsRecycler != null) {
            assetsRecycler.setLayoutManager(new LinearLayoutManager(this));
        }
    }
    
    private void updateUI() {
        boolean hasWallet = walletManager.hasWallet();
        
        if (setupLayout != null) {
            setupLayout.setVisibility(hasWallet ? View.GONE : View.VISIBLE);
        }
        if (mainLayout != null) {
            mainLayout.setVisibility(hasWallet ? View.VISIBLE : View.GONE);
        }
        
        if (hasWallet) {
            updateBalanceDisplay();
        }
    }
    
    private void updateBalanceDisplay() {
        Map<String, WalletManager.WalletBalance> balances = walletManager.getBalances();
        
        double total = 0;
        for (WalletManager.WalletBalance balance : balances.values()) {
            total += balance.usd;
        }
        
        NumberFormat formatter = NumberFormat.getCurrencyInstance(Locale.US);
        if (totalBalanceText != null) {
            totalBalanceText.setText(formatter.format(total));
        }
    }
    
    private void refreshBalances() {
        if (isLoading) return;
        isLoading = true;
        
        showLoading(true);
        
        walletManager.fetchAllBalances(new WalletManager.WalletCallback<Map<String, WalletManager.WalletBalance>>() {
            @Override
            public void onSuccess(Map<String, WalletManager.WalletBalance> result) {
                mainHandler.post(() -> {
                    isLoading = false;
                    showLoading(false);
                    updateBalanceDisplay();
                });
            }
            
            @Override
            public void onError(String error) {
                mainHandler.post(() -> {
                    isLoading = false;
                    showLoading(false);
                    showToast("Ошибка: " + error);
                });
            }
        });
    }
    
    // ============= CREATE WALLET =============
    
    private void showCreateWalletDialog() {
        View dialogView = getLayoutInflater().inflate(R.layout.dialog_create_wallet, null);
        
        EditText passwordInput = dialogView.findViewById(R.id.input_password);
        EditText confirmInput = dialogView.findViewById(R.id.input_password_confirm);
        TextView strengthText = dialogView.findViewById(R.id.password_strength);
        
        // Password strength indicator
        if (passwordInput != null && strengthText != null) {
            passwordInput.addTextChangedListener(new TextWatcher() {
                @Override
                public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
                
                @Override
                public void onTextChanged(CharSequence s, int start, int before, int count) {}
                
                @Override
                public void afterTextChanged(Editable s) {
                    WalletSecurity.ValidationResult result = security.validatePassword(s.toString());
                    if (result.valid) {
                        strengthText.setText("✓ Надёжный пароль");
                        strengthText.setTextColor(getResources().getColor(android.R.color.holo_green_dark));
                    } else {
                        strengthText.setText(result.error);
                        strengthText.setTextColor(getResources().getColor(android.R.color.holo_red_dark));
                    }
                }
            });
        }
        
        new AlertDialog.Builder(this)
            .setTitle("Создать кошелёк")
            .setView(dialogView)
            .setPositiveButton("Создать", (dialog, which) -> {
                String password = passwordInput != null ? passwordInput.getText().toString() : "";
                String confirm = confirmInput != null ? confirmInput.getText().toString() : "";
                
                if (!password.equals(confirm)) {
                    showToast("Пароли не совпадают");
                    return;
                }
                
                createWallet(password);
            })
            .setNegativeButton("Отмена", null)
            .show();
    }
    
    private void createWallet(String password) {
        showLoading(true);
        
        walletManager.createWallet(password, new WalletManager.WalletCallback<String>() {
            @Override
            public void onSuccess(String mnemonic) {
                mainHandler.post(() -> {
                    showLoading(false);
                    showSeedPhraseDialog(mnemonic);
                });
            }
            
            @Override
            public void onError(String error) {
                mainHandler.post(() -> {
                    showLoading(false);
                    showToast("Ошибка: " + error);
                });
            }
        });
    }
    
    private void showSeedPhraseDialog(String mnemonic) {
        View dialogView = getLayoutInflater().inflate(R.layout.dialog_seed_phrase, null);
        
        TextView seedText = dialogView.findViewById(R.id.seed_phrase_text);
        Button copyBtn = dialogView.findViewById(R.id.btn_copy_seed);
        
        if (seedText != null) {
            seedText.setText(mnemonic);
        }
        
        AlertDialog dialog = new AlertDialog.Builder(this)
            .setTitle("🔐 Сохраните seed-фразу")
            .setView(dialogView)
            .setPositiveButton("Я сохранил", (d, which) -> {
                updateUI();
                refreshBalances();
                showToast("Кошелёк создан!");
            })
            .setCancelable(false)
            .create();
        
        if (copyBtn != null) {
            copyBtn.setOnClickListener(v -> {
                secureCopyToClipboard(mnemonic);
                showToast("Скопировано (очистится через 60 сек)");
            });
        }
        
        dialog.show();
    }
    
    // ============= IMPORT WALLET =============
    
    private void showImportWalletDialog() {
        View dialogView = getLayoutInflater().inflate(R.layout.dialog_import_wallet, null);
        
        EditText seedInput = dialogView.findViewById(R.id.input_seed_phrase);
        EditText passwordInput = dialogView.findViewById(R.id.input_password);
        
        new AlertDialog.Builder(this)
            .setTitle("Импорт кошелька")
            .setView(dialogView)
            .setPositiveButton("Импортировать", (dialog, which) -> {
                String seed = seedInput != null ? seedInput.getText().toString() : "";
                String password = passwordInput != null ? passwordInput.getText().toString() : "";
                
                importWallet(seed, password);
            })
            .setNegativeButton("Отмена", null)
            .show();
    }
    
    private void importWallet(String seed, String password) {
        showLoading(true);
        
        walletManager.importWallet(seed, password, new WalletManager.WalletCallback<Boolean>() {
            @Override
            public void onSuccess(Boolean result) {
                mainHandler.post(() -> {
                    showLoading(false);
                    updateUI();
                    refreshBalances();
                    showToast("Кошелёк импортирован!");
                });
            }
            
            @Override
            public void onError(String error) {
                mainHandler.post(() -> {
                    showLoading(false);
                    showToast("Ошибка: " + error);
                });
            }
        });
    }
    
    // ============= SEND =============
    
    private void showSendDialog() {
        if (security.isLocked()) {
            showUnlockDialog();
            return;
        }
        
        View dialogView = getLayoutInflater().inflate(R.layout.dialog_send, null);
        
        EditText recipientInput = dialogView.findViewById(R.id.input_recipient);
        EditText amountInput = dialogView.findViewById(R.id.input_amount);
        EditText passwordInput = dialogView.findViewById(R.id.input_password);
        
        new AlertDialog.Builder(this)
            .setTitle("Отправить")
            .setView(dialogView)
            .setPositiveButton("Отправить", (dialog, which) -> {
                String recipient = recipientInput != null ? recipientInput.getText().toString().trim() : "";
                String amount = amountInput != null ? amountInput.getText().toString().trim() : "";
                String password = passwordInput != null ? passwordInput.getText().toString() : "";
                
                sendTransaction(recipient, amount, "polygon", password);
            })
            .setNegativeButton("Отмена", null)
            .show();
    }
    
    private void sendTransaction(String recipient, String amount, String chain, String password) {
        showLoading(true);
        
        walletManager.sendTransaction(recipient, amount, chain, password, 
            new WalletManager.WalletCallback<String>() {
                @Override
                public void onSuccess(String txHash) {
                    mainHandler.post(() -> {
                        showLoading(false);
                        showToast("Отправлено! TX: " + txHash.substring(0, 10) + "...");
                        refreshBalances();
                    });
                }
                
                @Override
                public void onError(String error) {
                    mainHandler.post(() -> {
                        showLoading(false);
                        showToast("Ошибка: " + error);
                    });
                }
            });
    }
    
    // ============= RECEIVE =============
    
    private void showReceiveDialog() {
        String address = walletManager.getAddress("polygon");
        if (address == null) {
            showToast("Адрес не найден");
            return;
        }
        
        View dialogView = getLayoutInflater().inflate(R.layout.dialog_receive, null);
        
        TextView addressText = dialogView.findViewById(R.id.address_text);
        Button copyBtn = dialogView.findViewById(R.id.btn_copy_address);
        
        if (addressText != null) {
            addressText.setText(address);
        }
        
        AlertDialog dialog = new AlertDialog.Builder(this)
            .setTitle("Получить")
            .setView(dialogView)
            .setPositiveButton("Закрыть", null)
            .create();
        
        if (copyBtn != null) {
            copyBtn.setOnClickListener(v -> {
                copyToClipboard(address);
                showToast("Адрес скопирован");
            });
        }
        
        dialog.show();
    }
    
    // ============= OTHER SCREENS =============
    
    private void showEarnScreen() {
        showToast("Staking - скоро!");
    }
    
    private void showSettingsScreen() {
        showToast("Настройки - скоро!");
    }
    
    private void showUnlockDialog() {
        View dialogView = getLayoutInflater().inflate(R.layout.dialog_unlock, null);
        EditText passwordInput = dialogView.findViewById(R.id.input_password);
        
        new AlertDialog.Builder(this)
            .setTitle("🔒 Кошелёк заблокирован")
            .setView(dialogView)
            .setPositiveButton("Разблокировать", (dialog, which) -> {
                // Verify password and unlock
                security.initSession();
            })
            .setCancelable(false)
            .show();
    }
    
    // ============= UTILITIES =============
    
    private void showLoading(boolean show) {
        if (loadingProgress != null) {
            loadingProgress.setVisibility(show ? View.VISIBLE : View.GONE);
        }
    }
    
    private void showToast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show();
    }
    
    private void copyToClipboard(String text) {
        ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboard != null) {
            clipboard.setPrimaryClip(ClipData.newPlainText("address", text));
        }
    }
    
    /**
     * Secure copy with auto-clear after delay
     */
    private void secureCopyToClipboard(String text) {
        ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboard != null) {
            clipboard.setPrimaryClip(ClipData.newPlainText("seed", text));
            
            // Auto-clear after 60 seconds
            mainHandler.postDelayed(() -> {
                try {
                    ClipData current = clipboard.getPrimaryClip();
                    if (current != null && current.getItemCount() > 0) {
                        CharSequence currentText = current.getItemAt(0).getText();
                        if (text.equals(currentText)) {
                            clipboard.setPrimaryClip(ClipData.newPlainText("", ""));
                        }
                    }
                } catch (Exception e) {
                    // Ignore
                }
            }, CLIPBOARD_CLEAR_DELAY);
        }
    }
    
    private void hideKeyboard() {
        View view = getCurrentFocus();
        if (view != null) {
            InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
            }
        }
    }
}
