package com.xipher.wrapper.ui;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.text.InputFilter;
import android.text.InputType;
import android.text.method.DigitsKeyListener;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;

import com.google.gson.JsonObject;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.db.DatabaseManager;
import com.xipher.wrapper.data.prefs.ChatListPrefs;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityPrivacySecurityBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.R;
import com.xipher.wrapper.security.DbKeyStore;
import com.xipher.wrapper.security.PanicModePrefs;
import com.xipher.wrapper.security.PinPrefs;
import com.xipher.wrapper.security.SecurityContext;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class PrivacySecurityActivity extends AppCompatActivity {
    private ActivityPrivacySecurityBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final String[] visibilityValues = {"everyone", "contacts", "nobody"};
    private boolean panicToggleGuard;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityPrivacySecurityBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);

        binding.btnBack.setOnClickListener(v -> finish());
        setupSpinners();
        binding.btnSave.setOnClickListener(v -> savePrivacy());

        SharedPreferences prefs = getSharedPreferences("xipher_prefs", MODE_PRIVATE);
        boolean incognitoRead = prefs.getBoolean("pref_incognito_read", false);
        binding.switchReadReceipts.setChecked(!incognitoRead);
        binding.switchReadReceipts.setOnCheckedChangeListener((button, isChecked) ->
                prefs.edit().putBoolean("pref_incognito_read", !isChecked).apply());

        binding.switchChatListPrivacy.setChecked(ChatListPrefs.isChatListPrivacyEnabled(this));
        binding.switchChatListPrivacy.setOnCheckedChangeListener((button, isChecked) ->
                ChatListPrefs.setChatListPrivacyEnabled(this, isChecked));

        setupPanicModeControls();
        loadPrivacy();

        BottomNavHelper.setup(this, binding.bottomNav, R.id.nav_settings);
    }

    private void setupSpinners() {
        String[] labels = {
                getString(com.xipher.wrapper.R.string.privacy_everyone),
                getString(com.xipher.wrapper.R.string.privacy_contacts),
                getString(com.xipher.wrapper.R.string.privacy_nobody)
        };
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, labels);
        binding.spinnerBio.setAdapter(adapter);
        binding.spinnerBirth.setAdapter(adapter);
        binding.spinnerLastSeen.setAdapter(adapter);
        binding.spinnerAvatar.setAdapter(adapter);
    }

    private void setupPanicModeControls() {
        boolean enabled = PanicModePrefs.isEnabled(this);
        binding.switchPanicEnabled.setChecked(enabled);
        binding.switchPanicEnabled.setOnCheckedChangeListener((button, isChecked) -> {
            if (panicToggleGuard) return;
            if (isChecked) {
                panicToggleGuard = true;
                binding.switchPanicEnabled.setChecked(false);
                panicToggleGuard = false;
                enablePanicMode();
            } else {
                disablePanicMode();
            }
        });

        binding.btnSetMainPin.setOnClickListener(v -> showPinDialog(false, null));
        binding.btnSetPanicPin.setOnClickListener(v -> showPinDialog(true, null));
        refreshPinButtons();
    }

    private void enablePanicMode() {
        if (!isMainPinConfigured()) {
            showPinDialog(false, this::enablePanicMode);
            return;
        }
        if (!DatabaseManager.getInstance().decoyDbExists(this)) {
            showPinDialog(true, this::enablePanicMode);
            return;
        }
        PanicModePrefs.setEnabled(this, true);
        panicToggleGuard = true;
        binding.switchPanicEnabled.setChecked(true);
        panicToggleGuard = false;
        Toast.makeText(this, R.string.panic_enabled, Toast.LENGTH_SHORT).show();
    }

    private void disablePanicMode() {
        if (!isMainPinConfigured()) {
            PanicModePrefs.setEnabled(this, false);
            panicToggleGuard = true;
            binding.switchPanicEnabled.setChecked(false);
            panicToggleGuard = false;
            Toast.makeText(this, R.string.panic_disabled, Toast.LENGTH_SHORT).show();
            return;
        }
        showDisablePanicPinDialog();
    }

    private void showDisablePanicPinDialog() {
        LinearLayout container = new LinearLayout(this);
        container.setOrientation(LinearLayout.VERTICAL);
        int padding = dp(12);
        container.setPadding(padding, padding, padding, padding);

        EditText currentInput = buildPinField(getString(R.string.pin_current_hint));
        container.addView(currentInput);

        AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle(R.string.panic_disable_title)
                .setMessage(R.string.panic_disable_message)
                .setView(container)
                .setPositiveButton(R.string.panic_disable_confirm, null)
                .setNegativeButton(android.R.string.cancel, (d, which) -> {
                    panicToggleGuard = true;
                    binding.switchPanicEnabled.setChecked(true);
                    panicToggleGuard = false;
                })
                .create();

        dialog.setOnShowListener(d -> {
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v -> {
                String currentPin = currentInput.getText().toString().trim();
                if (!isValidPin(currentPin)) {
                    currentInput.setError(getString(R.string.pin_invalid));
                    return;
                }
                dialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(false);
                io.execute(() -> {
                    try {
                        DatabaseManager.getInstance().rekeyMainToAutoKey(getApplicationContext(), currentPin);
                        PinPrefs.setMainPinSet(this, false);
                        PanicModePrefs.setEnabled(this, false);
                        runOnUiThread(() -> {
                            Toast.makeText(this, R.string.panic_disabled, Toast.LENGTH_SHORT).show();
                            panicToggleGuard = true;
                            binding.switchPanicEnabled.setChecked(false);
                            panicToggleGuard = false;
                            refreshPinButtons();
                            dialog.dismiss();
                        });
                    } catch (DatabaseManager.InvalidPasswordException e) {
                        runOnUiThread(() -> {
                            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(true);
                            currentInput.setError(getString(R.string.pin_wrong));
                            Toast.makeText(this, R.string.pin_save_failed, Toast.LENGTH_SHORT).show();
                        });
                    }
                });
            });
        });
        dialog.show();
    }

    private void refreshPinButtons() {
        boolean hasMain = isMainPinConfigured();
        boolean hasPanic = DatabaseManager.getInstance().decoyDbExists(this);
        binding.btnSetMainPin.setText(hasMain ? R.string.pin_change_main : R.string.pin_set_main);
        binding.btnSetPanicPin.setText(hasPanic ? R.string.panic_pin_change : R.string.panic_pin_set);
    }

    private void showPinDialog(boolean isPanic, Runnable onSuccess) {
        boolean exists = isPanic
                ? DatabaseManager.getInstance().decoyDbExists(this)
                : isMainPinConfigured();
        boolean wasExisting = exists;
        int title = isPanic
                ? (exists ? R.string.panic_pin_change_title : R.string.panic_pin_set_title)
                : (exists ? R.string.pin_change_title : R.string.pin_set_title);

        LinearLayout container = new LinearLayout(this);
        container.setOrientation(LinearLayout.VERTICAL);
        int padding = dp(12);
        container.setPadding(padding, padding, padding, padding);

        EditText currentInput = null;
        if (exists) {
            currentInput = buildPinField(getString(R.string.pin_current_hint));
            container.addView(currentInput);
        }
        EditText newInput = buildPinField(getString(R.string.pin_new_hint));
        EditText confirmInput = buildPinField(getString(R.string.pin_confirm_hint));
        container.addView(newInput);
        container.addView(confirmInput);

        AlertDialog dialog = new AlertDialog.Builder(this)
                .setTitle(title)
                .setView(container)
                .setPositiveButton(R.string.pin_save_action, null)
                .setNegativeButton(android.R.string.cancel, null)
                .create();

        EditText currentFinal = currentInput;
        dialog.setOnShowListener(d -> {
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(v -> {
                String currentPin = currentFinal != null ? currentFinal.getText().toString().trim() : null;
                String newPin = newInput.getText().toString().trim();
                String confirmPin = confirmInput.getText().toString().trim();

                if (currentFinal != null && !isValidPin(currentPin)) {
                    currentFinal.setError(getString(R.string.pin_invalid));
                    return;
                }
                if (!isValidPin(newPin)) {
                    newInput.setError(getString(R.string.pin_invalid));
                    return;
                }
                if (!newPin.equals(confirmPin)) {
                    confirmInput.setError(getString(R.string.pin_mismatch));
                    return;
                }

                dialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(false);
                io.execute(() -> {
                    try {
                        if (isPanic) {
                            DatabaseManager.getInstance().setPanicPin(getApplicationContext(), currentPin, newPin);
                            DatabaseManager.getInstance()
                                    .generateDecoyFromActive(getApplicationContext(), newPin, !wasExisting,
                                            authRepo.getUserId(), authRepo.getUsername());
                        } else {
                            if (exists) {
                                DatabaseManager.getInstance().setMainPin(getApplicationContext(), currentPin, newPin);
                            } else {
                                DatabaseManager.getInstance().setMainPinFromAutoKey(getApplicationContext(), newPin);
                            }
                        }
                        runOnUiThread(() -> {
                            if (!isPanic) {
                                PinPrefs.setMainPinSet(this, true);
                                DbKeyStore.clearMainKey(this);
                            }
                            Toast.makeText(this, R.string.pin_saved, Toast.LENGTH_SHORT).show();
                            refreshPinButtons();
                            if (onSuccess != null) {
                                onSuccess.run();
                            }
                            dialog.dismiss();
                        });
                    } catch (DatabaseManager.InvalidPasswordException e) {
                        runOnUiThread(() -> {
                            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setEnabled(true);
                            if (currentFinal != null) {
                                currentFinal.setError(getString(R.string.pin_wrong));
                            }
                            Toast.makeText(this, R.string.pin_save_failed, Toast.LENGTH_SHORT).show();
                        });
                    }
                });
            });
        });
        dialog.show();
    }

    private EditText buildPinField(String hint) {
        EditText input = new EditText(this);
        input.setHint(hint);
        input.setInputType(InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_VARIATION_PASSWORD);
        input.setKeyListener(DigitsKeyListener.getInstance("0123456789"));
        input.setFilters(new InputFilter[]{new InputFilter.LengthFilter(6)});
        input.setMaxLines(1);
        input.setBackgroundResource(R.drawable.input_bg);
        input.setTextColor(ContextCompat.getColor(this, R.color.x_text_primary));
        input.setHintTextColor(ContextCompat.getColor(this, R.color.x_text_secondary));
        int padding = dp(12);
        input.setPadding(padding, padding, padding, padding);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        );
        lp.topMargin = dp(8);
        input.setLayoutParams(lp);
        return input;
    }

    private boolean isValidPin(String pin) {
        if (pin == null) return false;
        int len = pin.length();
        if (len < 4 || len > 6) return false;
        for (int i = 0; i < len; i++) {
            char c = pin.charAt(i);
            if (c < '0' || c > '9') return false;
        }
        return true;
    }

    private int dp(int value) {
        float density = getResources().getDisplayMetrics().density;
        return Math.round(value * density);
    }

    private boolean isMainPinConfigured() {
        if (PinPrefs.isMainPinSet(this)) return true;
        String autoKey = DbKeyStore.getMainKey(this);
        return DatabaseManager.getInstance().realDbExists(this) && (autoKey == null || autoKey.isEmpty());
    }

    private void loadPrivacy() {
        if (SecurityContext.get().isPanicMode()) return;
        String token = authRepo.getToken();
        String userId = authRepo.getUserId();
        if (token == null || token.isEmpty() || userId == null || userId.isEmpty()) return;
        SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(this);
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.getUserProfile(token, userId);
                if (res != null && res.has("success") && res.get("success").getAsBoolean() && res.has("user")) {
                    JsonObject user = res.getAsJsonObject("user");
                    JsonObject privacy = user.has("privacy") ? user.getAsJsonObject("privacy") : null;
                    String bio = privacy != null && privacy.has("bio_visibility") ? privacy.get("bio_visibility").getAsString() : "everyone";
                    String birth = privacy != null && privacy.has("birth_visibility") ? privacy.get("birth_visibility").getAsString() : "everyone";
                    String lastSeen = privacy != null && privacy.has("last_seen_visibility") ? privacy.get("last_seen_visibility").getAsString() : "everyone";
                    String avatar = privacy != null && privacy.has("avatar_visibility") ? privacy.get("avatar_visibility").getAsString() : "everyone";
                    Boolean sendReadReceipts = privacy != null && privacy.has("send_read_receipts")
                            ? privacy.get("send_read_receipts").getAsBoolean()
                            : null;
                    runOnUiThread(() -> {
                        binding.spinnerBio.setSelection(indexForValue(bio));
                        binding.spinnerBirth.setSelection(indexForValue(birth));
                        binding.spinnerLastSeen.setSelection(indexForValue(lastSeen));
                        binding.spinnerAvatar.setSelection(indexForValue(avatar));
                        if (sendReadReceipts != null) {
                            binding.switchReadReceipts.setChecked(sendReadReceipts);
                            prefs.edit().putBoolean("pref_incognito_read", !sendReadReceipts).apply();
                        }
                    });
                }
            } catch (Exception ignored) {
            }
        });
    }

    private void savePrivacy() {
        if (SecurityContext.get().isPanicMode()) return;
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) return;
        int bioIndex = binding.spinnerBio.getSelectedItemPosition();
        int birthIndex = binding.spinnerBirth.getSelectedItemPosition();
        int lastIndex = binding.spinnerLastSeen.getSelectedItemPosition();
        int avatarIndex = binding.spinnerAvatar.getSelectedItemPosition();
        String bio = valueForIndex(bioIndex);
        String birth = valueForIndex(birthIndex);
        String lastSeen = valueForIndex(lastIndex);
        String avatar = valueForIndex(avatarIndex);
        boolean sendReadReceipts = binding.switchReadReceipts.isChecked();

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.updateMyPrivacy(token, bio, birth, lastSeen, avatar, sendReadReceipts);
                boolean ok = res != null && res.has("success") && res.get("success").getAsBoolean();
                runOnUiThread(() -> Toast.makeText(this, ok ? getString(R.string.save_success) : getString(R.string.save_failed), Toast.LENGTH_SHORT).show());
            } catch (Exception e) {
                runOnUiThread(() -> Toast.makeText(this, getString(R.string.network_error), Toast.LENGTH_SHORT).show());
            }
        });
    }

    private int indexForValue(String value) {
        if (value == null) return 0;
        for (int i = 0; i < visibilityValues.length; i++) {
            if (visibilityValues[i].equals(value)) return i;
        }
        return 0;
    }

    private String valueForIndex(int index) {
        if (index < 0 || index >= visibilityValues.length) return visibilityValues[0];
        return visibilityValues[index];
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        io.shutdownNow();
    }
}
