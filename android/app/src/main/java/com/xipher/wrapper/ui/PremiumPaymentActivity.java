package com.xipher.wrapper.ui;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.view.View;
import android.webkit.CookieManager;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.xipher.wrapper.R;
import com.xipher.wrapper.databinding.ActivityPremiumPaymentBinding;

public class PremiumPaymentActivity extends AppCompatActivity {
    public static final String EXTRA_FORM_ACTION = "form_action";
    public static final String EXTRA_RECEIVER = "receiver";
    public static final String EXTRA_LABEL = "label";
    public static final String EXTRA_SUM = "sum";
    public static final String EXTRA_SUCCESS_URL = "success_url";
    public static final String EXTRA_PAYMENT_TYPE = "payment_type";

    private ActivityPremiumPaymentBinding binding;
    private String successUrl;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityPremiumPaymentBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        binding.btnClose.setOnClickListener(v -> finish());

        String formAction = getIntent().getStringExtra(EXTRA_FORM_ACTION);
        String receiver = getIntent().getStringExtra(EXTRA_RECEIVER);
        String label = getIntent().getStringExtra(EXTRA_LABEL);
        String sum = getIntent().getStringExtra(EXTRA_SUM);
        successUrl = getIntent().getStringExtra(EXTRA_SUCCESS_URL);
        String paymentType = getIntent().getStringExtra(EXTRA_PAYMENT_TYPE);
        if (paymentType == null || paymentType.isEmpty()) {
            paymentType = "AC";
        }

        setupWebView(formAction, receiver, label, sum, successUrl, paymentType);
    }

    @SuppressLint("SetJavaScriptEnabled")
    private void setupWebView(String formAction, String receiver, String label, String sum,
                              String successUrl, String paymentType) {
        WebSettings settings = binding.paymentWebView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setLoadWithOverviewMode(true);
        settings.setUseWideViewPort(true);

        CookieManager cookieManager = CookieManager.getInstance();
        cookieManager.setAcceptCookie(true);
        cookieManager.setAcceptThirdPartyCookies(binding.paymentWebView, true);

        binding.paymentWebView.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onProgressChanged(WebView view, int newProgress) {
                binding.paymentProgress.setVisibility(newProgress < 100 ? View.VISIBLE : View.GONE);
            }
        });

        binding.paymentWebView.setWebViewClient(new WebViewClient() {
            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                return handleUrl(request.getUrl().toString());
            }

            @Override
            public boolean shouldOverrideUrlLoading(WebView view, String url) {
                return handleUrl(url);
            }
        });

        binding.paymentWebView.loadDataWithBaseURL("https://xipher.pro",
                buildPaymentHtml(formAction, receiver, label, sum, successUrl, paymentType),
                "text/html", "utf-8", null);
    }

    private boolean handleUrl(String url) {
        if (url == null) return false;
        if (successUrl != null && !successUrl.isEmpty() && url.startsWith(successUrl)) {
            setResult(RESULT_OK);
            finish();
            return true;
        }
        if (url.contains("premium=success")) {
            setResult(RESULT_OK);
            finish();
            return true;
        }
        if (url.contains("gift=success")) {
            setResult(RESULT_OK);
            finish();
            return true;
        }
        return false;
    }

    private String buildPaymentHtml(String formAction, String receiver, String label, String sum,
                                    String successUrl, String paymentType) {
        String safeAction = formAction != null ? formAction : "https://yoomoney.ru/quickpay/confirm";
        String safeReceiver = receiver != null ? receiver : "";
        String safeLabel = label != null ? label : "";
        String safeSum = sum != null ? sum : "";
        String safeType = paymentType != null ? paymentType : "AC";
        String safeSuccess = successUrl != null ? successUrl : "";
        StringBuilder html = new StringBuilder();
        html.append("<!doctype html><html><head><meta charset='utf-8'/>")
                .append("<meta name='viewport' content='width=device-width, initial-scale=1' />")
                .append("</head><body onload='document.forms[0].submit()'>")
                .append("<form method='POST' action='").append(escapeHtml(safeAction)).append("'>")
                .append("<input type='hidden' name='receiver' value='").append(escapeHtml(safeReceiver)).append("' />")
                .append("<input type='hidden' name='quickpay-form' value='button' />")
                .append("<input type='hidden' name='sum' value='").append(escapeHtml(safeSum)).append("' />")
                .append("<input type='hidden' name='paymentType' value='").append(escapeHtml(safeType)).append("' />")
                .append("<input type='hidden' name='label' value='").append(escapeHtml(safeLabel)).append("' />");
        if (safeSuccess != null && !safeSuccess.isEmpty()) {
            html.append("<input type='hidden' name='successURL' value='").append(escapeHtml(safeSuccess)).append("' />");
        }
        html.append("</form></body></html>");
        return html.toString();
    }

    private static String escapeHtml(String raw) {
        if (raw == null) return "";
        return raw.replace("&", "&amp;")
                .replace("\"", "&quot;")
                .replace("'", "&#39;")
                .replace("<", "&lt;")
                .replace(">", "&gt;");
    }

    @Override
    public void onBackPressed() {
        if (binding.paymentWebView.canGoBack()) {
            binding.paymentWebView.goBack();
            return;
        }
        super.onBackPressed();
    }
}
