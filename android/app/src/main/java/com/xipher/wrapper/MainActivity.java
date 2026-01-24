package com.xipher.wrapper;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.net.ConnectivityManager;
import android.net.NetworkCapabilities;
import android.os.Build;
import android.os.Bundle;
import android.view.View;
import android.webkit.CookieManager;
import android.webkit.WebView;
import android.widget.FrameLayout;
import android.widget.ProgressBar;

import androidx.activity.EdgeToEdge;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import androidx.webkit.WebViewCompat;

import com.xipher.wrapper.databinding.ActivityMainBinding;

import org.json.JSONObject;

/**
 * Main entry: hosts hardened WebView, offline handling, update notifications, notifications permission.
 */
public class MainActivity extends AppCompatActivity implements WebViewConfig.PageCallbacks {

    private static final String HOST = "xipher.pro";
    private static final String START_URL = "https://xipher.pro/login";
    private static final String UPDATE_URL = "https://xipher.pro/app/version.json"; // TODO proxy RuStore version info
    private static final int REQ_NOTIFICATION = 100;

    private ActivityMainBinding binding;
    private UpdateManager updateManager;
    private WebView webView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        EdgeToEdge.enable(this);
        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        setupSwipe();
        setupWebView();
        requestNotificationPermission();
        initUpdateManager();

        loadStartUrl();
    }

    private void setupSwipe() {
        binding.swipeRefresh.setOnRefreshListener(() -> {
            if (isOnline()) {
                loadStartUrl();
            } else {
                showOffline();
            }
        });
    }

    private void setupWebView() {
        binding.btnRetry.setOnClickListener(v -> loadStartUrl());
        ensureWebView();
    }

    private void initUpdateManager() {
        updateManager = new UpdateManager(this, UPDATE_URL);
        updateManager.setListener(new UpdateManager.Listener() {
            @Override
            public void onNoUpdate() { /* no-op */ }

            @Override
            public void onUpdateAvailable(JSONObject payload) {
                runOnUiThread(() -> updateManager.promptToUpdate(MainActivity.this, payload));
            }

            @Override
            public void onError(String message) {
                // no-op
            }
        });
        updateManager.checkForUpdate();
    }

    private void loadStartUrl() {
        if (!isOnline()) {
            showOffline();
            return;
        }
        ensureWebView();
        if (webView == null) {
            showWebViewMissing();
            return;
        }
        hideOffline();
        webView.loadUrl(START_URL);
    }

    private boolean isOnline() {
        ConnectivityManager cm = (ConnectivityManager) getSystemService(Context.CONNECTIVITY_SERVICE);
        if (cm == null) return false;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            NetworkCapabilities caps = cm.getNetworkCapabilities(cm.getActiveNetwork());
            return caps != null && (caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)
                    || caps.hasTransport(NetworkCapabilities.TRANSPORT_CELLULAR)
                    || caps.hasTransport(NetworkCapabilities.TRANSPORT_ETHERNET));
        } else {
            android.net.NetworkInfo info = cm.getActiveNetworkInfo();
            return info != null && info.isConnected();
        }
    }

    private void showOffline() {
        setOfflineMessage(R.string.offline_title, R.string.offline_subtitle);
        binding.offlineContainer.setVisibility(View.VISIBLE);
        binding.webContainer.setVisibility(View.GONE);
        binding.swipeRefresh.setRefreshing(false);
    }

    private void hideOffline() {
        binding.offlineContainer.setVisibility(View.GONE);
        if (webView != null) {
            binding.webContainer.setVisibility(View.VISIBLE);
        }
    }

    private void requestNotificationPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.POST_NOTIFICATIONS}, REQ_NOTIFICATION);
            }
        }
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
    }

    @Override
    public void onPageStarted(String url, Bitmap favicon) {
        binding.progressBar.setVisibility(View.VISIBLE);
        binding.swipeRefresh.setRefreshing(false);
    }

    @Override
    public void onPageFinished(String url) {
        binding.progressBar.setVisibility(View.GONE);
        binding.swipeRefresh.setRefreshing(false);
        // Ensure responsive viewport if missing
        if (webView != null) {
            webView.evaluateJavascript("(function(){var m=document.querySelector('meta[name=viewport]');if(!m){m=document.createElement('meta');m.name='viewport';m.content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no';document.head.appendChild(m);}})();", null);
        }
    }

    @Override
    public void onProgress(int newProgress) {
        ProgressBar bar = binding.progressBar;
        bar.setVisibility(newProgress >= 100 ? View.GONE : View.VISIBLE);
        bar.setProgress(newProgress);
    }

    @Override
    public void onError(String description) {
        showOffline();
    }

    @Override
    public void onBackPressed() {
        if (webView != null && webView.canGoBack()) {
            webView.goBack();
        } else {
            super.onBackPressed();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        CookieManager.getInstance().flush();
        if (webView != null) {
            webView.destroy();
            webView = null;
        }
    }

    private void ensureWebView() {
        if (webView != null) return;
        if (!isWebViewAvailable()) {
            showWebViewMissing();
            return;
        }
        webView = new WebView(this);
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT
        );
        webView.setLayoutParams(lp);
        binding.webContainer.addView(webView);
        WebViewConfig.apply(this, webView, this, HOST);
    }

    private boolean isWebViewAvailable() {
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                return WebView.getCurrentWebViewPackage() != null;
            }
            return WebViewCompat.getCurrentWebViewPackage(this) != null;
        } catch (Throwable t) {
            return false;
        }
    }

    private void showWebViewMissing() {
        setOfflineMessage(R.string.webview_missing_title, R.string.webview_missing_subtitle);
        binding.offlineContainer.setVisibility(View.VISIBLE);
        binding.webContainer.setVisibility(View.GONE);
        binding.swipeRefresh.setRefreshing(false);
    }

    private void setOfflineMessage(int titleRes, int subtitleRes) {
        binding.offlineTitle.setText(titleRes);
        binding.offlineSubtitle.setText(subtitleRes);
    }
}
