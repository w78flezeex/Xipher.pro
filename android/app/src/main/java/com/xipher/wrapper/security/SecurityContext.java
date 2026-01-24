package com.xipher.wrapper.security;

public final class SecurityContext {
    private static final SecurityContext INSTANCE = new SecurityContext();

    private volatile boolean panicMode;
    private volatile String currentSessionToken;

    private SecurityContext() {}

    public static SecurityContext get() {
        return INSTANCE;
    }

    public boolean isPanicMode() {
        return panicMode;
    }

    public void setPanicMode(boolean panicMode) {
        this.panicMode = panicMode;
    }

    public String getCurrentSessionToken() {
        return currentSessionToken;
    }

    public void setCurrentSessionToken(String currentSessionToken) {
        this.currentSessionToken = currentSessionToken;
    }

    public void clearSession() {
        currentSessionToken = null;
        panicMode = false;
    }
}
