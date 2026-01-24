package com.xipher.wrapper.ws;

import com.google.gson.JsonObject;

public class CallSignaling {
    private final SocketManager socket;
    private final String token;

    public CallSignaling(SocketManager socket, String token) {
        this.socket = socket;
        this.token = token;
    }

    public boolean sendOffer(String targetUserId, String callType, String sdpB64) {
        JsonObject obj = new JsonObject();
        obj.addProperty("type", "call_offer");
        obj.addProperty("token", token);
        obj.addProperty("target_user_id", targetUserId);
        obj.addProperty("receiver_id", targetUserId);
        obj.addProperty("call_type", callType);
        obj.addProperty("offer", sdpB64);
        obj.addProperty("offer_encoding", "b64");
        return socket.send(obj);
    }

    public boolean sendAnswer(String targetUserId, String sdpB64) {
        JsonObject obj = new JsonObject();
        obj.addProperty("type", "call_answer");
        obj.addProperty("token", token);
        obj.addProperty("target_user_id", targetUserId);
        obj.addProperty("receiver_id", targetUserId);
        obj.addProperty("answer", sdpB64);
        obj.addProperty("answer_encoding", "b64");
        return socket.send(obj);
    }

    public boolean sendIce(String targetUserId, String candidateB64) {
        JsonObject obj = new JsonObject();
        obj.addProperty("type", "call_ice_candidate");
        obj.addProperty("token", token);
        obj.addProperty("target_user_id", targetUserId);
        obj.addProperty("receiver_id", targetUserId);
        obj.addProperty("candidate", candidateB64);
        obj.addProperty("candidate_encoding", "b64");
        return socket.send(obj);
    }

    public boolean sendEnd(String targetUserId) {
        JsonObject obj = new JsonObject();
        obj.addProperty("type", "call_end");
        obj.addProperty("token", token);
        obj.addProperty("target_user_id", targetUserId);
        return socket.send(obj);
    }

    public boolean sendMediaState(String targetUserId, String mediaType, boolean enabled) {
        JsonObject obj = new JsonObject();
        obj.addProperty("type", "call_media_state");
        obj.addProperty("token", token);
        obj.addProperty("target_user_id", targetUserId);
        obj.addProperty("media_type", mediaType);
        obj.addProperty("enabled", enabled);
        return socket.send(obj);
    }
}
