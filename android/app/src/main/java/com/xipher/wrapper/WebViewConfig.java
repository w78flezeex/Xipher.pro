package com.xipher.wrapper;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.webkit.CookieManager;
import android.webkit.SslErrorHandler;
import android.webkit.WebChromeClient;
import android.webkit.WebResourceRequest;
import android.webkit.WebSettings;
import android.webkit.WebView;
import android.webkit.WebViewClient;
import android.net.http.SslError;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;

/**
 * Centralized WebView setup to keep MainActivity lean.
 */
public final class WebViewConfig {

    public interface PageCallbacks {
        void onPageStarted(String url, Bitmap favicon);
        void onPageFinished(String url);
        void onProgress(int newProgress);
        void onError(String description);
    }

    private WebViewConfig() {
    }

    @SuppressLint({"SetJavaScriptEnabled", "ClickableViewAccessibility"})
    public static void apply(@NonNull Context context,
                             @NonNull WebView webView,
                             @NonNull PageCallbacks callbacks,
                             @NonNull String appHost) {
        WebSettings settings = webView.getSettings();
        settings.setJavaScriptEnabled(true);
        settings.setDomStorageEnabled(true);
        settings.setDatabaseEnabled(true);
        settings.setUseWideViewPort(true);
        settings.setLoadWithOverviewMode(true);
        settings.setSupportZoom(true);
        settings.setDisplayZoomControls(false);
        settings.setBuiltInZoomControls(true);
        settings.setAllowContentAccess(false);
        settings.setAllowFileAccess(false);
        settings.setCacheMode(WebSettings.LOAD_DEFAULT);
        settings.setMediaPlaybackRequiresUserGesture(false);
        // Use mobile UA to trigger responsive layout
        settings.setUserAgentString("Mozilla/5.0 (Linux; Android 13; Mobile) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/117.0.0.0 Mobile Safari/537.36 XipherAndroidWrapper/1.0");

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            settings.setSafeBrowsingEnabled(true);
        }

        CookieManager cookieManager = CookieManager.getInstance();
        cookieManager.setAcceptCookie(true);
        cookieManager.setAcceptThirdPartyCookies(webView, true);

        webView.setWebChromeClient(new WebChromeClient() {
            @Override
            public void onProgressChanged(WebView view, int newProgress) {
                callbacks.onProgress(newProgress);
            }
        });

        webView.setWebViewClient(new WebViewClient() {
            @Override
            public void onPageStarted(WebView view, String url, Bitmap favicon) {
                callbacks.onPageStarted(url, favicon);
            }

            @Override
            public void onPageFinished(WebView view, String url) {
                callbacks.onPageFinished(url);
                CookieManager.getInstance().flush();
            }

            @Override
            public boolean shouldOverrideUrlLoading(WebView view, WebResourceRequest request) {
                Uri uri = request.getUrl();
                if (!uri.getHost().contains(appHost)) {
                    openExternal(context, uri.toString());
                    return true;
                }
                return false;
            }

            @Override
            public void onReceivedError(WebView view, int errorCode, String description, String failingUrl) {
                callbacks.onError(description);
            }

            @Override
            public void onReceivedSslError(WebView view, SslErrorHandler handler, SslError error) {
                // Enforce HTTPS by cancelling invalid certificates
                handler.cancel();
                callbacks.onError("SSL error" + (error != null ? (": " + error.toString()) : ""));
            }
        });
    }

    private static void openExternal(@NonNull Context context, @NonNull String url) {
        try {
            CustomTabsIntent intent = new CustomTabsIntent.Builder().build();
            intent.launchUrl(context, Uri.parse(url));
        } catch (Exception ignored) {
            // Fallback to default browser
            context.startActivity(new android.content.Intent(android.content.Intent.ACTION_VIEW, Uri.parse(url)));
        }
    }
}
