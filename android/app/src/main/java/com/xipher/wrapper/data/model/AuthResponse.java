package com.xipher.wrapper.data.model;

public class AuthResponse {
    public boolean success;
    public String message;
    public AuthData data;

    public static class AuthData {
        public String token;
        public String user_id;
        public String username;
    }
}
