package com.xipher.wrapper.ui;

import android.app.TimePickerDialog;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.switchmaterial.SwitchMaterial;
import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.prefs.ChatListPrefs;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityPremiumBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.util.BusinessHoursHelper;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.HashMap;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class PremiumActivity extends AppCompatActivity {
    private static final String PREFS = "xipher_prefs";
    private static final String KEY_PAYMENT_LABEL = "xipher_premium_payment_label";
    private static final String KEY_PAYMENT_PLAN = "xipher_premium_payment_plan";
    private static final String KEY_PAYMENT_STARTED = "xipher_premium_payment_started_at";
    private static final int PAYMENT_POLL_MAX_ATTEMPTS = 12;
    private static final long PAYMENT_POLL_INTERVAL_MS = 5000L;
    private static final long PAYMENT_STALE_MS = 10 * 60 * 1000L;

    private ActivityPremiumBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private ActivityResultLauncher<Intent> paymentLauncher;

    private Runnable paymentTimer;
    private Runnable paymentPoller;
    private int paymentPollAttempts;

    private boolean isPremiumActive;
    private String premiumPlan = "";
    private String premiumExpiresAt = "";
    private String selectedPlan = "trial";
    private String paymentProvider = "yoomoney";
    private String paymentType = "AC";

    private boolean updatingBusinessRows;
    private final Map<String, BusinessHoursHelper.Entry> businessHours = new HashMap<>();
    private final Map<String, BusinessRow> businessRows = new HashMap<>();

    private static final class PendingPayment {
        final String label;
        final String plan;
        final long startedAt;

        PendingPayment(String label, String plan, long startedAt) {
            this.label = label;
            this.plan = plan;
            this.startedAt = startedAt;
        }
    }

    private static final class PremiumState {
        final boolean active;
        final String plan;
        final String expiresAt;

        PremiumState(boolean active, String plan, String expiresAt) {
            this.active = active;
            this.plan = plan;
            this.expiresAt = expiresAt;
        }
    }

    private static final class PaymentData {
        final String formAction;
        final String receiver;
        final String label;
        final String sum;
        final String successUrl;
        final String plan;
        final String provider;
        final String checkoutUrl;

        PaymentData(String formAction, String receiver, String label, String sum, String successUrl,
                    String plan, String provider, String checkoutUrl) {
            this.formAction = formAction;
            this.receiver = receiver;
            this.label = label;
            this.sum = sum;
            this.successUrl = successUrl;
            this.plan = plan;
            this.provider = provider;
            this.checkoutUrl = checkoutUrl;
        }
    }

    private static final class BusinessRow {
        final View row;
        final SwitchMaterial toggle;
        final TextView start;
        final TextView end;

        BusinessRow(View row, SwitchMaterial toggle, TextView start, TextView end) {
            this.row = row;
            this.toggle = toggle;
            this.start = start;
            this.end = end;
        }
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityPremiumBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);

        binding.btnBack.setOnClickListener(v -> finish());
        BottomNavHelper.setup(this, binding.bottomNav, R.id.nav_settings);

        setupPlanSelection();
        setupPaymentProvider();
        setupPaymentMethod();
        setupBusinessRows();
        applyBusinessHours(null);

        binding.premiumSubscribeBtn.setOnClickListener(v -> startPaymentFlow());
        binding.premiumManageBtn.setOnClickListener(v -> confirmPremiumCancel());
        binding.btnSaveBusinessHours.setOnClickListener(v -> saveBusinessHours());

        paymentLauncher = registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
            if (result.getResultCode() == RESULT_OK) {
                Toast.makeText(this, R.string.premium_payment_toast_success, Toast.LENGTH_SHORT).show();
                startPremiumPaymentPolling(true);
            }
        });

        binding.premiumPaymentStatus.setOnClickListener(v -> resetPendingPaymentState());
        binding.premiumPaymentTimer.setOnClickListener(v -> resetPendingPaymentState());

        loadProfile();
        updatePendingPaymentUi();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        stopPremiumPaymentPolling();
        stopPaymentTimer();
        io.shutdownNow();
    }

    @Override
    protected void onResume() {
        super.onResume();
        loadProfile();
        updatePendingPaymentUi();
    }

    private void setupPlanSelection() {
        binding.premiumPlanTrial.setOnClickListener(v -> selectPlan("trial"));
        binding.premiumPlanTrialRadio.setOnClickListener(v -> selectPlan("trial"));
        binding.premiumPlanMonth.setOnClickListener(v -> selectPlan("month"));
        binding.premiumPlanMonthRadio.setOnClickListener(v -> selectPlan("month"));
        binding.premiumPlanYear.setOnClickListener(v -> selectPlan("year"));
        binding.premiumPlanYearRadio.setOnClickListener(v -> selectPlan("year"));
        selectPlan(selectedPlan);
    }

    private void setupPaymentProvider() {
        binding.premiumPaymentProviderYooMoney.setChecked(true);
        binding.premiumPaymentProviderGroup.setOnCheckedChangeListener((group, checkedId) -> {
            if (checkedId == R.id.premiumPaymentProviderStripe) {
                paymentProvider = "stripe";
            } else {
                paymentProvider = "yoomoney";
            }
            updatePaymentMethodVisibility();
        });
        updatePaymentMethodVisibility();
    }

    private void setupPaymentMethod() {
        binding.premiumPaymentCard.setChecked(true);
        binding.premiumPaymentMethodGroup.setOnCheckedChangeListener((group, checkedId) -> {
            if (checkedId == R.id.premiumPaymentWallet) {
                paymentType = "PC";
            } else {
                paymentType = "AC";
            }
        });
    }

    private void updatePaymentMethodVisibility() {
        boolean isStripe = "stripe".equals(paymentProvider);
        binding.premiumPaymentMethodGroup.setVisibility(isStripe ? View.GONE : View.VISIBLE);
        if (isStripe) {
            paymentType = "AC";
        } else if (binding.premiumPaymentMethodGroup.getCheckedRadioButtonId() == View.NO_ID) {
            binding.premiumPaymentCard.setChecked(true);
        }
    }

    private void selectPlan(String plan) {
        selectedPlan = plan;
        binding.premiumPlanTrialRadio.setChecked("trial".equals(plan));
        binding.premiumPlanMonthRadio.setChecked("month".equals(plan));
        binding.premiumPlanYearRadio.setChecked("year".equals(plan));
        binding.premiumPlanTrial.setActivated("trial".equals(plan));
        binding.premiumPlanMonth.setActivated("month".equals(plan));
        binding.premiumPlanYear.setActivated("year".equals(plan));
        updateCta();
    }

    private void loadProfile() {
        String token = authRepo.getToken();
        String userId = authRepo.getUserId();
        if (token == null || token.isEmpty() || userId == null || userId.isEmpty()) {
            return;
        }
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.getUserProfile(token, userId);
                if (res != null && res.has("success") && res.get("success").getAsBoolean() && res.has("user")) {
                    JsonObject user = res.getAsJsonObject("user");
                    boolean active = user.has("is_premium") && user.get("is_premium").getAsBoolean();
                    String plan = user.has("premium_plan") ? user.get("premium_plan").getAsString() : "";
                    String expiresAt = user.has("premium_expires_at") ? user.get("premium_expires_at").getAsString() : "";
                    JsonObject businessRaw = user.has("business_hours") && user.get("business_hours").isJsonObject()
                            ? user.getAsJsonObject("business_hours") : null;

                    PremiumState state = new PremiumState(active, plan, expiresAt);
                    mainHandler.post(() -> {
                        applyPremiumState(state, false);
                        applyBusinessHours(businessRaw);
                    });
                    return;
                }
            } catch (Exception ignored) {
            }
            mainHandler.post(this::updatePremiumUi);
        });
    }

    private void applyPremiumState(PremiumState state, boolean toastOnActivate) {
        boolean wasPremium = isPremiumActive;
        isPremiumActive = state.active;
        premiumPlan = state.plan != null ? state.plan : "";
        premiumExpiresAt = state.expiresAt != null ? state.expiresAt : "";
        updatePremiumUi();
        if (!wasPremium && isPremiumActive && toastOnActivate) {
            Toast.makeText(this, R.string.premium_status_active, Toast.LENGTH_SHORT).show();
        }
    }

    private void updatePremiumUi() {
        updateStatusPill();
        updateExpiryNote();
        updateCta();
        updateFolderMetrics();
        updatePendingPaymentUi();
        updateBusinessLock();
    }

    private void updateStatusPill() {
        if (isPremiumActive) {
            binding.premiumStatusPill.setText(R.string.premium_status_active);
            binding.premiumStatusPill.setBackgroundResource(R.drawable.premium_status_active_bg);
            binding.premiumStatusPill.setTextColor(getColor(R.color.x_on_accent));
        } else {
            binding.premiumStatusPill.setText(R.string.premium_status_free);
            binding.premiumStatusPill.setBackgroundResource(R.drawable.premium_status_free_bg);
            binding.premiumStatusPill.setTextColor(getColor(R.color.x_text_secondary));
        }
    }

    private void updateExpiryNote() {
        String formatted = formatExpiryDate(premiumExpiresAt);
        if (formatted.isEmpty()) {
            binding.premiumExpiryNote.setVisibility(View.GONE);
            return;
        }
        binding.premiumExpiryNote.setVisibility(View.VISIBLE);
        binding.premiumExpiryNote.setText(getString(R.string.premium_expires, formatted));
    }

    private void updateCta() {
        if (isPremiumActive) {
            binding.premiumSubscribeBtn.setText(R.string.premium_cta_active);
            binding.premiumSubscribeBtn.setEnabled(false);
            binding.premiumManageBtn.setVisibility(View.VISIBLE);
        } else {
            String priceLabel = getPlanPriceLabel(selectedPlan);
            binding.premiumSubscribeBtn.setText(getString(R.string.premium_cta_subscribe, priceLabel));
            binding.premiumSubscribeBtn.setEnabled(true);
            binding.premiumManageBtn.setVisibility(View.GONE);
        }
    }

    private void updateFolderMetrics() {
        int limit = isPremiumActive ? 20 : 3;
        int used = ChatListPrefs.getFolders(this).size();
        int remaining = Math.max(0, limit - used);
        binding.premiumFolderLimitValue.setText(String.valueOf(limit));
        binding.premiumFolderUsedValue.setText(String.valueOf(used));
        binding.premiumFolderRemainingValue.setText(String.valueOf(remaining));
    }

    private String getPlanPriceLabel(String plan) {
        if ("year".equals(plan)) {
            return getString(R.string.premium_cta_price_year);
        }
        if ("month".equals(plan)) {
            return getString(R.string.premium_cta_price_month);
        }
        return getString(R.string.premium_cta_price_trial);
    }

    private void startPaymentFlow() {
        if (isPremiumActive) {
            return;
        }
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) {
            Toast.makeText(this, R.string.auth_missing_token, Toast.LENGTH_SHORT).show();
            return;
        }
        String plan = selectedPlan;
        String provider = paymentProvider;
        resetPendingPaymentState();
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.createPremiumPayment(token, plan, provider);
                if (res == null || !res.has("success") || !res.get("success").getAsBoolean()) {
                    String message = res != null && res.has("message") ? res.get("message").getAsString() : null;
                    String mapped = mapPremiumError(message);
                    mainHandler.post(() -> {
                        resetPendingPaymentState();
                        Toast.makeText(this,
                                mapped != null ? mapped : getString(R.string.premium_payment_toast_error),
                                Toast.LENGTH_SHORT).show();
                    });
                    return;
                }
                JsonObject data = res.has("data") && res.get("data").isJsonObject() ? res.getAsJsonObject("data") : null;
                if (data == null) {
                    mainHandler.post(() -> {
                        resetPendingPaymentState();
                        Toast.makeText(this, R.string.premium_payment_toast_error, Toast.LENGTH_SHORT).show();
                    });
                    return;
                }
                PaymentData payment = new PaymentData(
                        data.has("form_action") ? data.get("form_action").getAsString() : "",
                        data.has("receiver") ? data.get("receiver").getAsString() : "",
                        data.has("label") ? data.get("label").getAsString() : "",
                        data.has("sum") ? data.get("sum").getAsString() : "",
                        data.has("success_url") ? data.get("success_url").getAsString() : "",
                        data.has("plan") ? data.get("plan").getAsString() : plan,
                        data.has("provider") ? data.get("provider").getAsString() : provider,
                        data.has("checkout_url") ? data.get("checkout_url").getAsString() : ""
                );
                mainHandler.post(() -> openPayment(payment));
            } catch (Exception e) {
                mainHandler.post(() -> {
                    resetPendingPaymentState();
                    Toast.makeText(this, R.string.premium_payment_toast_error, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }

    private void openPayment(PaymentData payment) {
        if ("stripe".equalsIgnoreCase(payment.provider)) {
            if (payment.label == null || payment.label.isEmpty()
                    || payment.checkoutUrl == null || payment.checkoutUrl.isEmpty()) {
                Toast.makeText(this, R.string.premium_payment_toast_error, Toast.LENGTH_SHORT).show();
                return;
            }
            setPendingPayment(new PendingPayment(payment.label, payment.plan, System.currentTimeMillis()));
            updatePendingPaymentUi();
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(payment.checkoutUrl));
            startActivity(intent);
            Toast.makeText(this, R.string.premium_payment_toast_opened, Toast.LENGTH_SHORT).show();
            startPremiumPaymentPolling(false);
            return;
        }

        if (payment.label == null || payment.label.isEmpty() || payment.receiver == null || payment.receiver.isEmpty()) {
            Toast.makeText(this, R.string.premium_payment_toast_error, Toast.LENGTH_SHORT).show();
            return;
        }
        setPendingPayment(new PendingPayment(payment.label, payment.plan, System.currentTimeMillis()));
        updatePendingPaymentUi();

        Intent intent = new Intent(this, PremiumPaymentActivity.class);
        intent.putExtra(PremiumPaymentActivity.EXTRA_FORM_ACTION, payment.formAction);
        intent.putExtra(PremiumPaymentActivity.EXTRA_RECEIVER, payment.receiver);
        intent.putExtra(PremiumPaymentActivity.EXTRA_LABEL, payment.label);
        intent.putExtra(PremiumPaymentActivity.EXTRA_SUM, payment.sum);
        intent.putExtra(PremiumPaymentActivity.EXTRA_SUCCESS_URL, payment.successUrl);
        intent.putExtra(PremiumPaymentActivity.EXTRA_PAYMENT_TYPE, paymentType);
        paymentLauncher.launch(intent);

        Toast.makeText(this, R.string.premium_payment_toast_opened, Toast.LENGTH_SHORT).show();
        startPremiumPaymentPolling(false);
    }

    private void confirmPremiumCancel() {
        new AlertDialog.Builder(this)
                .setTitle(R.string.premium_cancel_title)
                .setMessage(R.string.premium_cancel_note)
                .setPositiveButton(R.string.premium_manage, (dialog, which) -> cancelPremium())
                .setNegativeButton(R.string.cancel, null)
                .show();
    }

    private void cancelPremium() {
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) {
            Toast.makeText(this, R.string.auth_missing_token, Toast.LENGTH_SHORT).show();
            return;
        }
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.setPremium(token, false, "");
                boolean ok = res != null && res.has("success") && res.get("success").getAsBoolean();
                mainHandler.post(() -> {
                    if (ok) {
                        refreshPremiumStatus(false, null);
                    } else {
                        Toast.makeText(this, R.string.premium_update_error, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                mainHandler.post(() -> Toast.makeText(this, R.string.premium_update_error, Toast.LENGTH_SHORT).show());
            }
        });
    }

    private void refreshPremiumStatus(boolean toastOnActivate, Runnable done) {
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) {
            if (done != null) {
                done.run();
            }
            return;
        }
        io.execute(() -> {
            PremiumState state = null;
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.validateTokenInfo(token);
                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    JsonObject data = res.has("data") && res.get("data").isJsonObject()
                            ? res.getAsJsonObject("data") : null;
                    if (data != null) {
                        boolean active = data.has("is_premium") && (data.get("is_premium").getAsBoolean()
                                || "true".equalsIgnoreCase(data.get("is_premium").getAsString()));
                        String plan = data.has("premium_plan") ? data.get("premium_plan").getAsString() : "";
                        String expiresAt = data.has("premium_expires_at") ? data.get("premium_expires_at").getAsString() : "";
                        state = new PremiumState(active, plan, expiresAt);
                    }
                }
            } catch (Exception ignored) {
            }
            PremiumState finalState = state;
            mainHandler.post(() -> {
                if (finalState != null) {
                    applyPremiumState(finalState, toastOnActivate);
                }
                if (done != null) {
                    done.run();
                }
            });
        });
    }

    private void startPremiumPaymentPolling(boolean force) {
        stopPremiumPaymentPolling();
        paymentPollAttempts = 0;
        paymentPoller = new Runnable() {
            @Override
            public void run() {
                paymentPollAttempts++;
                refreshPremiumStatus(true, () -> {
                    PendingPayment pending = getPendingPayment();
                    if (isPremiumActive) {
                        stopPremiumPaymentPolling();
                        return;
                    }
                    if (pending == null && !force) {
                        stopPremiumPaymentPolling();
                        return;
                    }
                    if (paymentPollAttempts >= PAYMENT_POLL_MAX_ATTEMPTS) {
                        stopPremiumPaymentPolling();
                        return;
                    }
                    mainHandler.postDelayed(this, PAYMENT_POLL_INTERVAL_MS);
                });
            }
        };
        mainHandler.post(paymentPoller);
    }

    private void stopPremiumPaymentPolling() {
        if (paymentPoller != null) {
            mainHandler.removeCallbacks(paymentPoller);
            paymentPoller = null;
        }
    }

    private void updatePendingPaymentUi() {
        PendingPayment pending = getPendingPayment();
        if (isPremiumActive) {
            binding.premiumPaymentStatus.setText(R.string.premium_payment_status_paid);
            binding.premiumPaymentStatus.setBackgroundResource(R.drawable.premium_status_success_bg);
            binding.premiumPaymentStatus.setTextColor(getColor(R.color.x_on_accent));
            binding.premiumPaymentStatus.setVisibility(View.VISIBLE);
            binding.premiumPaymentTimer.setVisibility(View.GONE);
            stopPaymentTimer();
            clearPendingPayment();
            return;
        }

        if (pending != null && pending.startedAt > 0
                && System.currentTimeMillis() - pending.startedAt > PAYMENT_STALE_MS) {
            clearPendingPayment();
            pending = null;
            stopPremiumPaymentPolling();
            stopPaymentTimer();
        }

        if (pending != null && pending.label != null && !pending.label.isEmpty()) {
            binding.premiumPaymentStatus.setText(R.string.premium_payment_status_pending);
            binding.premiumPaymentStatus.setBackgroundResource(R.drawable.premium_status_pending_bg);
            binding.premiumPaymentStatus.setTextColor(getColor(R.color.x_text_primary));
            binding.premiumPaymentStatus.setVisibility(View.VISIBLE);
            binding.premiumPaymentTimer.setVisibility(View.VISIBLE);
            startPaymentTimer();
            if (paymentPoller == null) {
                startPremiumPaymentPolling(false);
            }
            return;
        }

        binding.premiumPaymentStatus.setVisibility(View.GONE);
        binding.premiumPaymentTimer.setVisibility(View.GONE);
        stopPaymentTimer();
    }

    private void startPaymentTimer() {
        stopPaymentTimer();
        paymentTimer = new Runnable() {
            @Override
            public void run() {
                updatePaymentTimer();
                mainHandler.postDelayed(this, 1000L);
            }
        };
        mainHandler.post(paymentTimer);
    }

    private void stopPaymentTimer() {
        if (paymentTimer != null) {
            mainHandler.removeCallbacks(paymentTimer);
            paymentTimer = null;
        }
    }

    private void updatePaymentTimer() {
        PendingPayment pending = getPendingPayment();
        if (pending == null) return;
        if (pending.startedAt <= 0) return;
        long elapsed = System.currentTimeMillis() - pending.startedAt;
        if (elapsed > PAYMENT_STALE_MS) {
            resetPendingPaymentState();
            return;
        }
        String formatted = formatWaitTime(elapsed);
        binding.premiumPaymentTimer.setText(getString(R.string.premium_payment_timer, formatted));
    }

    private String formatWaitTime(long elapsedMs) {
        long totalSeconds = Math.max(0, elapsedMs / 1000L);
        long hours = totalSeconds / 3600L;
        long minutes = (totalSeconds % 3600L) / 60L;
        long seconds = totalSeconds % 60L;
        if (hours > 0) {
            return String.format(Locale.getDefault(), "%d:%02d:%02d", hours, minutes, seconds);
        }
        return String.format(Locale.getDefault(), "%d:%02d", minutes, seconds);
    }

    private SharedPreferences prefs() {
        return getApplicationContext().getSharedPreferences(PREFS, MODE_PRIVATE);
    }

    private PendingPayment getPendingPayment() {
        SharedPreferences prefs = prefs();
        String label = prefs.getString(KEY_PAYMENT_LABEL, "");
        if (label == null || label.isEmpty()) {
            return null;
        }
        String plan = prefs.getString(KEY_PAYMENT_PLAN, "");
        long startedAt = prefs.getLong(KEY_PAYMENT_STARTED, 0L);
        return new PendingPayment(label, plan != null ? plan : "", startedAt);
    }

    private void setPendingPayment(PendingPayment payment) {
        SharedPreferences prefs = prefs();
        prefs.edit()
                .putString(KEY_PAYMENT_LABEL, payment.label)
                .putString(KEY_PAYMENT_PLAN, payment.plan != null ? payment.plan : "")
                .putLong(KEY_PAYMENT_STARTED, payment.startedAt)
                .apply();
    }

    private void clearPendingPayment() {
        SharedPreferences prefs = prefs();
        prefs.edit()
                .remove(KEY_PAYMENT_LABEL)
                .remove(KEY_PAYMENT_PLAN)
                .remove(KEY_PAYMENT_STARTED)
                .apply();
    }

    private void resetPendingPaymentState() {
        clearPendingPayment();
        stopPremiumPaymentPolling();
        stopPaymentTimer();
        updatePendingPaymentUi();
    }

    private String mapPremiumError(String message) {
        if (message == null) return null;
        String lower = message.toLowerCase(Locale.US);
        if (lower.contains("trial") && lower.contains("already")) {
            return getString(R.string.premium_trial_used);
        }
        if (lower.contains("trial") && lower.contains("active")) {
            return getString(R.string.premium_trial_active);
        }
        return message;
    }

    private String formatExpiryDate(String raw) {
        if (raw == null || raw.isEmpty()) return "";
        String[] parts = raw.split(" ");
        String datePart = parts.length > 0 ? parts[0] : raw;
        try {
            SimpleDateFormat input = new SimpleDateFormat("yyyy-MM-dd", Locale.US);
            SimpleDateFormat output = new SimpleDateFormat("dd.MM.yyyy", Locale.getDefault());
            return output.format(input.parse(datePart));
        } catch (ParseException e) {
            return datePart;
        }
    }

    private void setupBusinessRows() {
        businessRows.put("mon", new BusinessRow(binding.businessRowMon, binding.businessToggleMon, binding.businessStartMon, binding.businessEndMon));
        businessRows.put("tue", new BusinessRow(binding.businessRowTue, binding.businessToggleTue, binding.businessStartTue, binding.businessEndTue));
        businessRows.put("wed", new BusinessRow(binding.businessRowWed, binding.businessToggleWed, binding.businessStartWed, binding.businessEndWed));
        businessRows.put("thu", new BusinessRow(binding.businessRowThu, binding.businessToggleThu, binding.businessStartThu, binding.businessEndThu));
        businessRows.put("fri", new BusinessRow(binding.businessRowFri, binding.businessToggleFri, binding.businessStartFri, binding.businessEndFri));
        businessRows.put("sat", new BusinessRow(binding.businessRowSat, binding.businessToggleSat, binding.businessStartSat, binding.businessEndSat));
        businessRows.put("sun", new BusinessRow(binding.businessRowSun, binding.businessToggleSun, binding.businessStartSun, binding.businessEndSun));

        for (String day : BusinessHoursHelper.DAY_ORDER) {
            BusinessRow row = businessRows.get(day);
            if (row == null) continue;
            row.toggle.setOnCheckedChangeListener((buttonView, isChecked) -> {
                if (updatingBusinessRows) return;
                BusinessHoursHelper.Entry entry = businessHours.get(day);
                if (entry != null) {
                    entry.enabled = isChecked;
                }
                updateBusinessRow(day);
            });
            row.start.setOnClickListener(v -> openTimePicker(day, true));
            row.end.setOnClickListener(v -> openTimePicker(day, false));
        }
    }

    private void applyBusinessHours(JsonObject raw) {
        businessHours.clear();
        businessHours.putAll(BusinessHoursHelper.normalize(raw));
        updatingBusinessRows = true;
        for (String day : BusinessHoursHelper.DAY_ORDER) {
            updateBusinessRow(day);
        }
        updatingBusinessRows = false;
        updateBusinessLock();
    }

    private void updateBusinessRow(String day) {
        BusinessRow row = businessRows.get(day);
        BusinessHoursHelper.Entry entry = businessHours.get(day);
        if (row == null || entry == null) return;
        row.toggle.setChecked(entry.enabled);
        row.start.setText(entry.start != null ? entry.start : "");
        row.end.setText(entry.end != null ? entry.end : "");
        boolean locked = !isPremiumActive;
        row.toggle.setEnabled(!locked);
        row.start.setEnabled(!locked && entry.enabled);
        row.end.setEnabled(!locked && entry.enabled);
        float alpha = entry.enabled ? 1f : 0.5f;
        row.row.setAlpha(locked ? 0.5f : alpha);
    }

    private void updateBusinessLock() {
        boolean locked = !isPremiumActive;
        binding.premiumBusinessLock.setVisibility(locked ? View.VISIBLE : View.GONE);
        binding.btnSaveBusinessHours.setEnabled(!locked);
        binding.btnSaveBusinessHours.setAlpha(locked ? 0.5f : 1f);
        for (String day : BusinessHoursHelper.DAY_ORDER) {
            updateBusinessRow(day);
        }
    }

    private void openTimePicker(String day, boolean isStart) {
        if (!isPremiumActive) {
            Toast.makeText(this, R.string.premium_business_locked_toast, Toast.LENGTH_SHORT).show();
            return;
        }
        BusinessHoursHelper.Entry entry = businessHours.get(day);
        if (entry == null) return;
        String rawTime = isStart ? entry.start : entry.end;
        int hour = 9;
        int minute = 0;
        Integer parsed = BusinessHoursHelper.parseTimeToMinutes(rawTime);
        if (parsed != null) {
            hour = parsed / 60;
            minute = parsed % 60;
        }
        TimePickerDialog dialog = new TimePickerDialog(this, (view, hourOfDay, minuteOfHour) -> {
            String formatted = String.format(Locale.getDefault(), "%02d:%02d", hourOfDay, minuteOfHour);
            if (isStart) {
                entry.start = formatted;
            } else {
                entry.end = formatted;
            }
            updateBusinessRow(day);
        }, hour, minute, true);
        dialog.show();
    }

    private void saveBusinessHours() {
        if (!isPremiumActive) {
            Toast.makeText(this, R.string.premium_business_locked_toast, Toast.LENGTH_SHORT).show();
            return;
        }
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) {
            Toast.makeText(this, R.string.auth_missing_token, Toast.LENGTH_SHORT).show();
            return;
        }
        JsonObject payload = BusinessHoursHelper.toJson(businessHours);
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.setBusinessHours(token, payload.toString());
                boolean ok = res != null && res.has("success") && res.get("success").getAsBoolean();
                mainHandler.post(() -> {
                    if (ok) {
                        Toast.makeText(this, R.string.premium_business_saved, Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(this, R.string.premium_business_error, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                mainHandler.post(() -> Toast.makeText(this, R.string.premium_business_error, Toast.LENGTH_SHORT).show());
            }
        });
    }
}
