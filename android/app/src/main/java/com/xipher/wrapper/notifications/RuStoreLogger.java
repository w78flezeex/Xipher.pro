package com.xipher.wrapper.notifications;

import android.util.Log;

public class RuStoreLogger implements ru.rustore.sdk.pushclient.common.logger.Logger {
    private final String tag;

    public RuStoreLogger() {
        this("RuStorePush");
    }

    private RuStoreLogger(String tag) {
        this.tag = tag != null && !tag.isEmpty() ? tag : "RuStorePush";
    }

    @Override
    public void verbose(String message, Throwable throwable) {
        Log.v(tag, safeMessage(message), throwable);
    }

    @Override
    public void debug(String message, Throwable throwable) {
        Log.d(tag, safeMessage(message), throwable);
    }

    @Override
    public void info(String message, Throwable throwable) {
        Log.i(tag, safeMessage(message), throwable);
    }

    @Override
    public void warn(String message, Throwable throwable) {
        Log.w(tag, safeMessage(message), throwable);
    }

    @Override
    public void error(String message, Throwable throwable) {
        Log.e(tag, safeMessage(message), throwable);
    }

    @Override
    public ru.rustore.sdk.pushclient.common.logger.Logger createLogger(String tag) {
        return new RuStoreLogger(tag);
    }

    @Override
    public ru.rustore.sdk.pushclient.common.logger.Logger createLogger(Object owner) {
        String derived = owner != null ? owner.getClass().getSimpleName() : tag;
        return new RuStoreLogger(derived);
    }

    private String safeMessage(String message) {
        return message != null ? message : "";
    }
}
