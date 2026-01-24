package com.xipher.wrapper.ui;

import android.Manifest;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.media.Ringtone;
import android.media.RingtoneManager;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.util.Log;
import android.view.View;
import android.view.WindowManager;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.xipher.wrapper.BuildConfig;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.db.AppDatabase;
import com.xipher.wrapper.data.db.entity.CallLogEntity;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityCallBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.notifications.PushTokenManager;
import com.xipher.wrapper.security.SecurityContext;
import com.xipher.wrapper.ws.CallSignaling;
import com.xipher.wrapper.ws.SocketManager;

import org.webrtc.AudioSource;
import org.webrtc.AudioTrack;
import org.webrtc.IceCandidate;
import org.webrtc.MediaConstraints;
import org.webrtc.PeerConnection;
import org.webrtc.PeerConnectionFactory;
import org.webrtc.RtpTransceiver;
import org.webrtc.SessionDescription;
import org.webrtc.SoftwareVideoDecoderFactory;
import org.webrtc.SoftwareVideoEncoderFactory;
import org.webrtc.audio.JavaAudioDeviceModule;

import java.nio.charset.StandardCharsets;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Deque;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

public class CallActivity extends AppCompatActivity implements SocketManager.Listener {
    private static final String TAG = "CallActivity";
    private static final int DEBUG_LINE_MAX = 12;
    public static final String EXTRA_PEER_ID = "peer_id";
    public static final String EXTRA_PEER_NAME = "peer_name";
    public static final String EXTRA_CALL_ID = "call_id";
    public static final String EXTRA_INCOMING = "incoming";
    private static final long INCOMING_DEDUP_MS = 1500L;
    private static final int OFFER_RETRY_MAX = 50;
    private static final long OFFER_RETRY_DELAY_MS = 600L;
    private static final long AUTH_WAIT_MS = OFFER_RETRY_DELAY_MS * 2;
    private static final int TURN_TTL_MINUTES = 60;
    private static final String STUN_FALLBACK_URL = "stun:turn.xipher.pro:3478";
    private static volatile boolean callInProgress = false;
    private static volatile String lastIncomingPeerId;
    private static volatile long lastIncomingAtMs;

    private static final int REQ_RECORD_AUDIO = 3001;

    public static void startOutgoing(Context context, String peerId, String peerName) {
        Intent i = new Intent(context, CallActivity.class);
        i.putExtra(EXTRA_PEER_ID, peerId);
        i.putExtra(EXTRA_PEER_NAME, peerName);
        i.putExtra(EXTRA_INCOMING, false);
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(i);
    }

    public static void startIncoming(Context context, String peerId, String peerName) {
        if (context == null || peerId == null || peerId.isEmpty()) return;
        long now = System.currentTimeMillis();
        if (callInProgress) return;
        if (peerId.equals(lastIncomingPeerId) && (now - lastIncomingAtMs) < INCOMING_DEDUP_MS) return;
        lastIncomingPeerId = peerId;
        lastIncomingAtMs = now;
        Intent i = new Intent(context, CallActivity.class);
        i.putExtra(EXTRA_PEER_ID, peerId);
        i.putExtra(EXTRA_PEER_NAME, peerName);
        i.putExtra(EXTRA_INCOMING, true);
        i.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(i);
    }

    public static boolean isCallInProgress() {
        return callInProgress;
    }

    private ActivityCallBinding binding;
    private AuthRepository authRepo;
    private ApiClient api;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final ExecutorService signalingIo = Executors.newSingleThreadExecutor();
    private final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();
    private final Gson gson = new Gson();
    private final Deque<String> debugLines = new ArrayDeque<>();

    private SocketManager socket;
    private CallSignaling signaling;
    private enum SignalChannel { WS, HTTP }
    private volatile SignalChannel signalChannel;
    private volatile SignalChannel offerChannel;

    private PeerConnectionFactory factory;
    private PeerConnection peerConnection;
    private AudioSource audioSource;
    private AudioTrack localAudioTrack;
    private JavaAudioDeviceModule audioDeviceModule;
    private List<PeerConnection.IceServer> iceServers = new ArrayList<>();
    private volatile List<PeerConnection.IceServer> cachedIceServers;

    private String peerId;
    private String peerName;
    private String callId;
    private boolean incoming;
    private boolean callActive;
    private volatile boolean answerApplied;
    private boolean callAnswered;
    private boolean callLogged;
    private long callCreatedAtMs;
    private long callStartedAtMs;
    private String endReason;
    private volatile String pendingOfferJson;
    private volatile boolean acceptingCall;
    private final AtomicBoolean answerSent = new AtomicBoolean(false);
    private final AtomicBoolean answerSendInFlight = new AtomicBoolean(false);
    private final AtomicBoolean acceptedSent = new AtomicBoolean(false);
    private final AtomicBoolean signalChannelLocked = new AtomicBoolean(false);
    private volatile boolean waitAcceptAfterAuth;
    private ScheduledFuture<?> authWaitTimeout;
    private ScheduledFuture<?> offerRetry;
    private final Set<String> seenCandidates = Collections.newSetFromMap(new ConcurrentHashMap<>());
    private final Set<String> sentIceCandidates = Collections.newSetFromMap(new ConcurrentHashMap<>());
    private final Set<String> queuedLocalIceCandidates = Collections.newSetFromMap(new ConcurrentHashMap<>());
    private final AtomicInteger sentIceCount = new AtomicInteger(0);
    private final List<IceCandidate> pendingIceCandidates = new ArrayList<>();
    private final List<IceCandidate> pendingLocalIceCandidates = new ArrayList<>();
    private volatile boolean remoteDescriptionReady;
    private volatile boolean remoteOfferHasVideo;
    private long lastIceCheck = 0L;
    private static final long CALL_RECOVERY_GRACE_MS = 120_000L;
    private static final long CALL_RECOVERY_RETRY_MS = 5_000L;
    private static final int CALL_RECOVERY_MAX_ATTEMPTS = 6;
    private final AtomicBoolean recoveryActive = new AtomicBoolean(false);
    private final AtomicBoolean recoveryInFlight = new AtomicBoolean(false);
    private final AtomicBoolean recoveryNoticeSent = new AtomicBoolean(false);
    private final AtomicInteger recoveryAttempts = new AtomicInteger(0);
    private volatile long recoveryStartedAtMs;
    private ScheduledFuture<?> recoveryTicker;

    private AudioManager audioManager;
    private AudioFocusRequest focusRequest;
    private boolean speakerOn;
    private boolean micMuted;

    // Remote media state
    private boolean remoteMicEnabled = true;
    private boolean remoteSpeakerEnabled = true;

    private Ringtone ringtone;
    private Vibrator vibrator;

    private ScheduledFuture<?> responsePoller;
    private ScheduledFuture<?> icePoller;

    private Runnable pendingPermissionAction;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityCallBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        if (SecurityContext.get().isPanicMode()) {
            finish();
            return;
        }
        if (BuildConfig.DEBUG) {
            binding.debugInfo.setVisibility(View.VISIBLE);
            debugLog("DBG start");
        }

        authRepo = AppServices.authRepository(this);
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) {
            finish();
            return;
        }
        api = new ApiClient(token);
        callInProgress = true;

        Intent intent = getIntent();
        peerId = intent.getStringExtra(EXTRA_PEER_ID);
        peerName = intent.getStringExtra(EXTRA_PEER_NAME);
        callId = intent.getStringExtra(EXTRA_CALL_ID);
        incoming = intent.getBooleanExtra(EXTRA_INCOMING, false);
        callCreatedAtMs = System.currentTimeMillis();

        String displayName = peerName != null && !peerName.isEmpty() ? peerName : "Unknown";
        binding.callName.setText(displayName);
        binding.callAvatar.setText(displayName.substring(0, 1).toUpperCase());

        binding.btnAccept.setOnClickListener(v -> acceptCall());
        binding.btnReject.setOnClickListener(v -> rejectCall());
        binding.btnEnd.setOnClickListener(v -> endCall(true));
        binding.btnMute.setOnClickListener(v -> toggleMute());
        binding.btnSpeaker.setOnClickListener(v -> toggleSpeaker());

        if (incoming) {
            showIncomingUi();
            startRingtone();
        } else {
            showOutgoingUi();
        }

        connectSocket();
        prefetchIceServers();

        if (incoming) {
            fetchPendingOffer();
        } else {
            ensureAudioPermission(this::startOutgoingCall);
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        callInProgress = false;
        stopRingtone();
        stopPolling();
        cancelAuthWait();
        releaseWebRtc();
        releaseAudioFocus();
        if (socket != null) socket.close();
        io.shutdownNow();
        signalingIo.shutdownNow();
        scheduler.shutdownNow();
    }

    @Override
    public void onBackPressed() {
        endCall(true);
        super.onBackPressed();
    }

    private void connectSocket() {
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) {
            finish();
            return;
        }
        PushTokenManager.CachedToken cached = PushTokenManager.isPushEnabled(this)
                ? PushTokenManager.getCachedTokenInfo(this)
                : null;
        String pushToken = cached != null ? cached.token : null;
        String platform = cached != null ? cached.platform : null;
        socket = new SocketManager(token, pushToken, platform);
        socket.setListener(this);
        socket.connect();
        signaling = new CallSignaling(socket, token);
    }

    private void showIncomingUi() {
        binding.callStatus.setText(R.string.call_status_incoming);
        binding.incomingActions.setVisibility(android.view.View.VISIBLE);
        binding.activeActions.setVisibility(android.view.View.GONE);
        binding.btnEnd.setVisibility(android.view.View.GONE);
        binding.callTimer.setVisibility(android.view.View.INVISIBLE);
    }

    private void showOutgoingUi() {
        binding.callStatus.setText(R.string.call_status_calling);
        binding.incomingActions.setVisibility(android.view.View.GONE);
        binding.activeActions.setVisibility(android.view.View.GONE);
        binding.btnEnd.setVisibility(android.view.View.VISIBLE);
        binding.callTimer.setVisibility(android.view.View.INVISIBLE);
    }

    private void showActiveUi() {
        binding.callStatus.setText(R.string.call_status_in_call);
        binding.incomingActions.setVisibility(android.view.View.GONE);
        binding.activeActions.setVisibility(android.view.View.VISIBLE);
        binding.btnEnd.setVisibility(android.view.View.VISIBLE);
        binding.callTimer.setBase(SystemClock.elapsedRealtime());
        binding.callTimer.setVisibility(android.view.View.VISIBLE);
        binding.callTimer.start();
        markCallAnswered();
        // Reset remote media state and send initial state
        resetRemoteMediaState();
        scheduler.schedule(this::sendInitialMediaState, 500, TimeUnit.MILLISECONDS);
    }

    private void beginCallRecovery(String reason) {
        if (!callActive && !callAnswered) return;
        if (peerConnection == null) return;
        if (recoveryActive.compareAndSet(false, true)) {
            recoveryStartedAtMs = System.currentTimeMillis();
            recoveryAttempts.set(0);
            runOnUiThread(() -> binding.callStatus.setText(R.string.call_status_reconnecting));
        }
        if (!recoveryNoticeSent.getAndSet(true)) {
            debugLog("recovery start: " + reason);
        }
        scheduleRecoveryTick();
    }

    private void clearCallRecovery(String reason) {
        if (!recoveryActive.get() && recoveryTicker == null) return;
        resetCallRecoveryState();
        if (callActive) {
            runOnUiThread(() -> binding.callStatus.setText(R.string.call_status_in_call));
        }
        debugLog("recovery cleared: " + reason);
    }

    private void resetCallRecoveryState() {
        recoveryActive.set(false);
        recoveryInFlight.set(false);
        recoveryNoticeSent.set(false);
        recoveryAttempts.set(0);
        recoveryStartedAtMs = 0L;
        if (recoveryTicker != null) {
            recoveryTicker.cancel(true);
            recoveryTicker = null;
        }
    }

    private void scheduleRecoveryTick() {
        if (scheduler.isShutdown()) return;
        if (recoveryTicker != null && !recoveryTicker.isDone()) return;
        recoveryTicker = scheduler.scheduleAtFixedRate(() -> {
            if (!recoveryActive.get()) return;
            PeerConnection pc = peerConnection;
            if (pc == null) {
                resetCallRecoveryState();
                return;
            }
            PeerConnection.PeerConnectionState state = pc.connectionState();
            PeerConnection.IceConnectionState iceState = pc.iceConnectionState();
            if (state == PeerConnection.PeerConnectionState.CONNECTED
                    || iceState == PeerConnection.IceConnectionState.CONNECTED
                    || iceState == PeerConnection.IceConnectionState.COMPLETED) {
                clearCallRecovery("connected");
                return;
            }
            long elapsed = System.currentTimeMillis() - recoveryStartedAtMs;
            if (elapsed >= CALL_RECOVERY_GRACE_MS) {
                debugLog("recovery timeout, ending call");
                resetCallRecoveryState();
                runOnUiThread(() -> endCall(true));
                return;
            }
            attemptIceRestartOffer("tick");
        }, 0, CALL_RECOVERY_RETRY_MS, TimeUnit.MILLISECONDS);
    }

    private void attemptIceRestartOffer(String reason) {
        if (incoming) return;
        PeerConnection pc = peerConnection;
        if (pc == null) return;
        if (recoveryAttempts.get() >= CALL_RECOVERY_MAX_ATTEMPTS) return;
        if (pc.signalingState() != PeerConnection.SignalingState.STABLE) return;
        if (!recoveryInFlight.compareAndSet(false, true)) return;
        debugLog("recovery offer: " + reason);
        try {
            pc.restartIce();
        } catch (Exception e) {
            Log.w(TAG, logPrefix() + " restartIce failed", e);
        }
        MediaConstraints constraints = new MediaConstraints();
        constraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveAudio", "true"));
        constraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveVideo", remoteOfferHasVideo ? "true" : "false"));
        constraints.mandatory.add(new MediaConstraints.KeyValuePair("IceRestart", "true"));
        pc.createOffer(new SdpObserverAdapter() {
            @Override
            public void onCreateSuccess(SessionDescription sdp) {
                pc.setLocalDescription(new SdpObserverAdapter() {
                    @Override
                    public void onSetSuccess() {
                        sendOffer(sdp);
                        recoveryAttempts.incrementAndGet();
                        recoveryInFlight.set(false);
                    }

                    @Override
                    public void onSetFailure(String s) {
                        debugLog("recovery offer setLocalDescription failed: " + s);
                        recoveryInFlight.set(false);
                    }
                }, sdp);
            }

            @Override
            public void onCreateFailure(String s) {
                debugLog("recovery offer createOffer failed: " + s);
                recoveryInFlight.set(false);
            }
        }, constraints);
    }

    private String logPrefix() {
        String dir = incoming ? "incoming" : "outgoing";
        String id = callId != null ? callId : "-";
        String peer = peerId != null ? peerId : "-";
        return System.currentTimeMillis() + " dir=" + dir + " call=" + id + " peer=" + peer;
    }

    private void debugLog(String message) {
        if (!BuildConfig.DEBUG) return;
        String logLine = logPrefix() + " " + message;
        Log.d(TAG, logLine);
        if (binding == null) return;
        runOnUiThread(() -> {
            if (binding == null) return;
            if (binding.debugInfo.getVisibility() != View.VISIBLE) {
                binding.debugInfo.setVisibility(View.VISIBLE);
            }
            if (debugLines.size() >= DEBUG_LINE_MAX) {
                debugLines.removeFirst();
            }
            debugLines.addLast(logLine);
            StringBuilder sb = new StringBuilder();
            for (String line : debugLines) {
                if (sb.length() > 0) sb.append('\n');
                sb.append(line);
            }
            binding.debugInfo.setText(sb.toString());
        });
    }

    private void ensureAudioPermission(Runnable onGranted) {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            onGranted.run();
            return;
        }
        pendingPermissionAction = onGranted;
        ActivityCompat.requestPermissions(this, new String[]{Manifest.permission.RECORD_AUDIO}, REQ_RECORD_AUDIO);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, @NonNull String[] permissions, @NonNull int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        if (requestCode == REQ_RECORD_AUDIO) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                if (pendingPermissionAction != null) pendingPermissionAction.run();
            } else {
                Toast.makeText(this, R.string.mic_permission_required, Toast.LENGTH_LONG).show();
                finish();
            }
        }
    }

    private void startOutgoingCall() {
        if (peerId == null || peerId.isEmpty()) {
            Toast.makeText(this, R.string.call_missing_peer, Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        if (socket != null) {
            debugLog("startOutgoingCall: preferring WS signaling");
            setSignalChannel(SignalChannel.WS);
        }
        requestAudioFocus();
        signalingIo.execute(() -> {
            List<PeerConnection.IceServer> servers = loadIceServers();
            runOnUiThread(() -> {
                initWebRtc();
                createPeerConnection(servers);
                createOffer();
                startPolling();
            });
            try {
                JsonObject res = api.callNotification(authRepo.getToken(), peerId, "audio");
                if (res == null || !res.has("success") || !res.get("success").getAsBoolean()) {
                    runOnUiThread(() -> Toast.makeText(this, R.string.call_notification_failed, Toast.LENGTH_SHORT).show());
                }
            } catch (Exception ignored) {
            }
        });
    }

    private void acceptCall() {
        stopRingtone();
        ensureAudioPermission(() -> {
            if (acceptingCall) return;
            acceptingCall = true;
            binding.btnAccept.setEnabled(false);
            requestAudioFocus();
            waitAcceptAfterAuth = false;
            cancelAuthWait();
            if (!isWsReady()) {
                waitAcceptAfterAuth = true;
                scheduleAuthWait();
                runOnUiThread(() -> binding.callStatus.setText("Connecting..."));
                debugLog("acceptCall: waiting for WS auth");
                return;
            }
            setSignalChannel(SignalChannel.WS);
            SignalChannel resolved = resolveSignalChannel();
            debugLog("acceptCall: resolved=" + resolved + " wsReady=" + isWsReady()
                    + " offerChannel=" + offerChannel + " signalChannel=" + signalChannel);
            attemptAccept(0);
        });
    }

    private void attemptAccept(int attempt) {
        if (!acceptingCall || isFinishing() || isDestroyed()) return;
        String offerJson = pendingOfferJson;
        if (offerJson != null && !offerJson.isEmpty()) {
            debugLog("attemptAccept: fast path offer len=" + offerJson.length());
            runAcceptWithOffer(offerJson);
            return;
        }
        signalingIo.execute(() -> {
            if (!acceptingCall || isFinishing() || isDestroyed()) return;
            debugLog("attemptAccept: start attempt=" + attempt);
            String fetchedOffer = fetchOfferFromServer();
            if (fetchedOffer != null && !fetchedOffer.isEmpty()) {
                markOfferChannel(SignalChannel.HTTP);
                pendingOfferJson = fetchedOffer;
                debugLog("attemptAccept: offer fetched len=" + fetchedOffer.length());
                runAcceptWithOffer(fetchedOffer);
                return;
            }
            debugLog("attemptAccept: no offer, retry=" + (attempt < OFFER_RETRY_MAX));
            if (attempt < OFFER_RETRY_MAX) {
                scheduleOfferRetry(attempt + 1);
                return;
            }
            acceptingCall = false;
            runOnUiThread(() -> {
                binding.btnAccept.setEnabled(true);
                Toast.makeText(this, R.string.call_no_offer, Toast.LENGTH_SHORT).show();
                finish();
            });
        });
    }

    private void runAcceptWithOffer(String offerJson) {
        List<PeerConnection.IceServer> fastServers = getIceServersFast();
        debugLog("attemptAccept: iceServers fast " + summarizeIceServers(fastServers));
        runOnUiThread(() -> {
            if (isFinishing() || isDestroyed()) {
                debugLog("attemptAccept: activity finishing before handleOffer");
                return;
            }
            initWebRtc();
            createPeerConnection(fastServers);
            debugLog("attemptAccept: handleOffer len=" + offerJson.length());
            handleOffer(offerJson);
            startPolling();
            acceptingCall = false;
        });
        signalingIo.execute(() -> {
            List<PeerConnection.IceServer> servers = loadIceServers();
            if (servers == null || servers.isEmpty()) return;
            runOnUiThread(() -> applyIceServersUpdate(servers, "accept-refresh"));
        });
    }

    private void scheduleOfferRetry(int attempt) {
        if (scheduler.isShutdown()) return;
        try {
            if (offerRetry != null) {
                offerRetry.cancel(true);
                offerRetry = null;
            }
            offerRetry = scheduler.schedule(() -> attemptAccept(attempt), OFFER_RETRY_DELAY_MS, TimeUnit.MILLISECONDS);
        } catch (Exception ignored) {
        }
    }

    private void rejectCall() {
        stopRingtone();
        acceptingCall = false;
        waitAcceptAfterAuth = false;
        cancelAuthWait();
        if (offerRetry != null) {
            offerRetry.cancel(true);
            offerRetry = null;
        }
        endReason = "rejected";
        sendCallResponse("rejected");
        sendCallEnd();
        endCall(false);
    }

    private void endCall(boolean notifyRemote) {
        stopRingtone();
        acceptingCall = false;
        waitAcceptAfterAuth = false;
        cancelAuthWait();
        resetCallRecoveryState();
        if (offerRetry != null) {
            offerRetry.cancel(true);
            offerRetry = null;
        }
        if (notifyRemote) {
            sendCallResponse("ended");
            sendCallEnd();
        }
        saveCallLog(resolveFinalStatus());
        finish();
    }

    private void sendCallEnd() {
        if (signaling != null && peerId != null) {
            signaling.sendEnd(peerId);
        }
    }

    private void sendCallResponse(String response) {
        String token = authRepo.getToken();
        if (token == null || token.isEmpty() || peerId == null) return;
        signalingIo.execute(() -> {
            try {
                api.callResponse(token, peerId, response);
            } catch (Exception ignored) {
            }
        });
    }

    private void initWebRtc() {
        if (factory != null) return;
        PeerConnectionFactory.initialize(
                PeerConnectionFactory.InitializationOptions.builder(this)
                        .setEnableInternalTracer(false)
                        .createInitializationOptions()
        );
        JavaAudioDeviceModule.Builder admBuilder = JavaAudioDeviceModule.builder(this)
                .setUseHardwareAcousticEchoCanceler(true)
                .setUseHardwareNoiseSuppressor(true);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            AudioAttributes attrs = new AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                    .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                    .build();
            admBuilder.setAudioAttributes(attrs);
        }
        audioDeviceModule = admBuilder.createAudioDeviceModule();
        factory = PeerConnectionFactory.builder()
                .setAudioDeviceModule(audioDeviceModule)
                .setVideoEncoderFactory(new SoftwareVideoEncoderFactory())
                .setVideoDecoderFactory(new SoftwareVideoDecoderFactory())
                .createPeerConnectionFactory();

        MediaConstraints audioConstraints = new MediaConstraints();
        audioConstraints.mandatory.add(new MediaConstraints.KeyValuePair("googEchoCancellation", "true"));
        audioConstraints.mandatory.add(new MediaConstraints.KeyValuePair("googNoiseSuppression", "true"));
        audioConstraints.mandatory.add(new MediaConstraints.KeyValuePair("googAutoGainControl", "true"));
        audioSource = factory.createAudioSource(audioConstraints);
        localAudioTrack = factory.createAudioTrack("xipher_audio", audioSource);
    }

    private void createPeerConnection(List<PeerConnection.IceServer> servers) {
        iceServers = servers;
        PeerConnection.RTCConfiguration config = new PeerConnection.RTCConfiguration(servers);
        config.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;
        peerConnection = factory.createPeerConnection(config, new PeerObserver());
        remoteDescriptionReady = false;
        if (peerConnection == null) {
            Toast.makeText(this, R.string.call_connection_failed, Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        if (localAudioTrack != null) {
            peerConnection.addTrack(localAudioTrack, java.util.Collections.singletonList("xipher_stream"));
        }
    }

    private void applyIceServersUpdate(List<PeerConnection.IceServer> servers, String reason) {
        if (servers == null || servers.isEmpty()) return;
        cacheIceServers(servers);
        if (peerConnection == null) {
            debugLog("applyIceServersUpdate: no peerConnection, cached only");
            return;
        }
        PeerConnection.RTCConfiguration config = new PeerConnection.RTCConfiguration(servers);
        config.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;
        boolean ok = peerConnection.setConfiguration(config);
        debugLog("applyIceServersUpdate: " + reason + " ok=" + ok + " " + summarizeIceServers(servers));
        if (ok) {
            try {
                peerConnection.restartIce();
            } catch (Exception e) {
                Log.w(TAG, logPrefix() + " restartIce failed", e);
            }
        }
    }

    private void createOffer() {
        if (peerConnection == null) return;
        MediaConstraints constraints = new MediaConstraints();
        constraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveAudio", "true"));
        constraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveVideo", "false"));
        peerConnection.createOffer(new SdpObserverAdapter() {
            @Override
            public void onCreateSuccess(SessionDescription sdp) {
                peerConnection.setLocalDescription(new SdpObserverAdapter(), sdp);
                sendOffer(sdp);
            }

            @Override
            public void onCreateFailure(String s) {
                runOnUiThread(() -> {
                    Toast.makeText(CallActivity.this, R.string.call_offer_failed, Toast.LENGTH_SHORT).show();
                    endCall(true);
                });
            }
        }, constraints);
    }

    private void handleOffer(String offerJson) {
        if (peerConnection == null) return;
        SessionDescription offer = parseSessionDescription(offerJson, SessionDescription.Type.OFFER);
        if (offer == null) {
            debugLog("handleOffer: invalid offer (len=" + (offerJson != null ? offerJson.length() : 0) + ")");
            Toast.makeText(this, R.string.call_invalid_offer, Toast.LENGTH_SHORT).show();
            finish();
            return;
        }
        remoteOfferHasVideo = sdpHasMedia(offer.description, "video");
        debugLog("handleOffer: remote video=" + remoteOfferHasVideo);
        peerConnection.setRemoteDescription(new SdpObserverAdapter() {
            @Override
            public void onSetSuccess() {
                debugLog("handleOffer: remoteDescription set");
                remoteDescriptionReady = true;
                drainPendingIceCandidates();
                createAnswer();
            }

            @Override
            public void onSetFailure(String s) {
                debugLog("handleOffer: setRemoteDescription failed: " + s);
                runOnUiThread(() -> {
                    Toast.makeText(CallActivity.this, R.string.call_remote_offer_failed, Toast.LENGTH_SHORT).show();
                    endCall(true);
                });
            }
        }, offer);
    }

    private void createAnswer() {
        if (peerConnection == null) return;
        MediaConstraints constraints = new MediaConstraints();
        constraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveAudio", "true"));
        constraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveVideo", remoteOfferHasVideo ? "true" : "false"));
        peerConnection.createAnswer(new SdpObserverAdapter() {
            @Override
            public void onCreateSuccess(SessionDescription sdp) {
                peerConnection.setLocalDescription(new SdpObserverAdapter() {
                    @Override
                    public void onSetSuccess() {
                        debugLog("createAnswer: localDescription set");
                        sendAnswer(sdp);
                    }

                    @Override
                    public void onSetFailure(String s) {
                        debugLog("createAnswer: setLocalDescription failed: " + s);
                        runOnUiThread(() -> {
                            Toast.makeText(CallActivity.this, R.string.call_answer_failed, Toast.LENGTH_SHORT).show();
                            endCall(true);
                        });
                    }
                }, sdp);
            }

            @Override
            public void onCreateFailure(String s) {
                debugLog("createAnswer: createAnswer failed: " + s);
                runOnUiThread(() -> {
                    Toast.makeText(CallActivity.this, R.string.call_answer_failed, Toast.LENGTH_SHORT).show();
                    endCall(true);
                });
            }
        }, constraints);
    }

    private void handleReoffer(SessionDescription offer) {
        if (peerConnection == null) return;
        debugLog("handleReoffer: start");
        remoteOfferHasVideo = sdpHasMedia(offer.description, "video");
        peerConnection.setRemoteDescription(new SdpObserverAdapter() {
            @Override
            public void onSetSuccess() {
                debugLog("handleReoffer: remoteDescription set");
                remoteDescriptionReady = true;
                drainPendingIceCandidates();
                createReofferAnswer();
            }

            @Override
            public void onSetFailure(String s) {
                debugLog("handleReoffer: setRemoteDescription failed: " + s);
            }
        }, offer);
    }

    private void createReofferAnswer() {
        if (peerConnection == null) return;
        MediaConstraints constraints = new MediaConstraints();
        constraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveAudio", "true"));
        constraints.mandatory.add(new MediaConstraints.KeyValuePair("OfferToReceiveVideo", remoteOfferHasVideo ? "true" : "false"));
        peerConnection.createAnswer(new SdpObserverAdapter() {
            @Override
            public void onCreateSuccess(SessionDescription sdp) {
                peerConnection.setLocalDescription(new SdpObserverAdapter() {
                    @Override
                    public void onSetSuccess() {
                        debugLog("createReofferAnswer: localDescription set");
                        sendReofferAnswer(sdp);
                        clearCallRecovery("reoffer-answer");
                    }

                    @Override
                    public void onSetFailure(String s) {
                        debugLog("createReofferAnswer: setLocalDescription failed: " + s);
                    }
                }, sdp);
            }

            @Override
            public void onCreateFailure(String s) {
                debugLog("createReofferAnswer: createAnswer failed: " + s);
            }
        }, constraints);
    }

    private void sendOffer(SessionDescription sdp) {
        if (signaling == null || peerId == null) return;
        String payload = sessionToJson(sdp);
        String b64 = android.util.Base64.encodeToString(payload.getBytes(StandardCharsets.UTF_8), android.util.Base64.NO_WRAP);
        sendWhenAuthed(() -> signaling.sendOffer(peerId, "audio", b64));
    }

    private void sendAnswer(SessionDescription sdp) {
        if (peerId == null) return;
        if (answerSent.get()) {
            debugLog("sendAnswer: already sent");
            return;
        }
        if (!answerSendInFlight.compareAndSet(false, true)) {
            debugLog("sendAnswer: send already in-flight");
            return;
        }
        String payload = sessionToJson(sdp);
        String b64 = android.util.Base64.encodeToString(payload.getBytes(StandardCharsets.UTF_8), android.util.Base64.NO_WRAP);
        SignalChannel channel = resolveSignalChannel();
        debugLog("sendAnswer: channel=" + channel + " wsReady=" + isWsReady());
        if (channel == SignalChannel.WS) {
            if (!isWsReady() || signaling == null) {
                debugLog("sendAnswer: WS not ready, fallback to HTTP");
                forceSignalChannel(SignalChannel.HTTP);
                sendAnswerHttp(b64, () -> {
                    if (answerSent.compareAndSet(false, true)) {
                        debugLog("sendAnswer: HTTP sent");
                        sendAcceptedOnce();
                    } else {
                        debugLog("sendAnswer: already sent (http)");
                    }
                    answerSendInFlight.set(false);
                }, () -> answerSendInFlight.set(false));
                return;
            }
            boolean queued = signaling.sendAnswer(peerId, b64);
            if (!queued) {
                debugLog("sendAnswer: WS enqueue failed, fallback to HTTP");
                forceSignalChannel(SignalChannel.HTTP);
                sendAnswerHttp(b64, () -> {
                    if (answerSent.compareAndSet(false, true)) {
                        debugLog("sendAnswer: HTTP sent");
                        sendAcceptedOnce();
                    } else {
                        debugLog("sendAnswer: already sent (http fallback)");
                    }
                    answerSendInFlight.set(false);
                }, () -> answerSendInFlight.set(false));
                return;
            }
            lockSignalChannel(SignalChannel.WS, "answer-ws");
            answerSent.set(true);
            debugLog("sendAnswer: WS sent");
            sendAcceptedOnce();
            flushPendingLocalIceCandidates();
            answerSendInFlight.set(false);
            return;
        }
        lockSignalChannel(SignalChannel.HTTP, "answer-http");
        sendAnswerHttp(b64, () -> {
            if (answerSent.compareAndSet(false, true)) {
                debugLog("sendAnswer: HTTP sent");
                sendAcceptedOnce();
                flushPendingLocalIceCandidates();
            } else {
                debugLog("sendAnswer: already sent (http)");
            }
            answerSendInFlight.set(false);
        }, () -> answerSendInFlight.set(false));
    }

    private void sendReofferAnswer(SessionDescription sdp) {
        if (peerId == null) return;
        String payload = sessionToJson(sdp);
        String b64 = android.util.Base64.encodeToString(payload.getBytes(StandardCharsets.UTF_8), android.util.Base64.NO_WRAP);
        SignalChannel channel = resolveSignalChannel();
        debugLog("sendReofferAnswer: channel=" + channel + " wsReady=" + isWsReady());
        if (channel == SignalChannel.WS && isWsReady() && signaling != null) {
            boolean queued = signaling.sendAnswer(peerId, b64);
            if (!queued) {
                debugLog("sendReofferAnswer: WS enqueue failed, fallback to HTTP");
                forceSignalChannel(SignalChannel.HTTP);
                sendAnswerHttp(b64, null, null);
            }
            return;
        }
        sendAnswerHttp(b64, null, null);
    }

    private void sendIce(IceCandidate candidate) {
        if (peerId == null || candidate == null) return;
        String key = candidate.sdpMid + "|" + candidate.sdpMLineIndex + "|" + candidate.sdp;
        if (incoming && !answerSent.get()) {
            queueLocalIceCandidate(candidate, key, "before answer");
            return;
        }
        String payload = candidateToJson(candidate);
        String b64 = android.util.Base64.encodeToString(payload.getBytes(StandardCharsets.UTF_8), android.util.Base64.NO_WRAP);
        SignalChannel channel = resolveSignalChannel();
        if (channel == SignalChannel.WS) {
            if (!isWsReady() || signaling == null) {
                if (signalChannelLocked.get() && signalChannel == SignalChannel.WS) {
                    queueLocalIceCandidate(candidate, key, "ws not ready");
                    return;
                }
                if (!sentIceCandidates.add(key)) {
                    debugLog("sendIce: duplicate candidate skipped");
                    return;
                }
                debugLog("sendIce: WS not ready, fallback to HTTP");
                forceSignalChannel(SignalChannel.HTTP);
                sendIceHttp(b64);
                return;
            }
            if (!sentIceCandidates.add(key)) {
                debugLog("sendIce: duplicate candidate skipped");
                return;
            }
            int count = sentIceCount.incrementAndGet();
            debugLog("sendIce: channel=WS count=" + count);
            boolean queued = signaling.sendIce(peerId, b64);
            if (!queued) {
                sentIceCandidates.remove(key);
                debugLog("sendIce: WS enqueue failed, fallback to HTTP");
                forceSignalChannel(SignalChannel.HTTP);
                if (!sentIceCandidates.add(key)) {
                    debugLog("sendIce: duplicate candidate skipped");
                    return;
                }
                count = sentIceCount.incrementAndGet();
                debugLog("sendIce: channel=HTTP count=" + count);
                sendIceHttp(b64);
            }
            return;
        }
        if (!sentIceCandidates.add(key)) {
            debugLog("sendIce: duplicate candidate skipped");
            return;
        }
        int count = sentIceCount.incrementAndGet();
        debugLog("sendIce: channel=HTTP count=" + count);
        sendIceHttp(b64);
    }

    private void queueLocalIceCandidate(IceCandidate candidate, String key, String reason) {
        if (!queuedLocalIceCandidates.add(key)) {
            debugLog("sendIce: duplicate candidate queued");
            return;
        }
        synchronized (pendingLocalIceCandidates) {
            pendingLocalIceCandidates.add(candidate);
        }
        debugLog("sendIce: queued local candidate (" + reason + ")");
    }

    private void flushPendingLocalIceCandidates() {
        List<IceCandidate> candidates;
        synchronized (pendingLocalIceCandidates) {
            if (pendingLocalIceCandidates.isEmpty()) return;
            candidates = new ArrayList<>(pendingLocalIceCandidates);
            pendingLocalIceCandidates.clear();
        }
        queuedLocalIceCandidates.clear();
        debugLog("flushPendingLocalIceCandidates: count=" + candidates.size());
        for (IceCandidate candidate : candidates) {
            sendIce(candidate);
        }
    }

    private void sendWhenAuthed(Runnable send) {
        if (socket != null && socket.isAuthed()) {
            send.run();
            return;
        }
        if (isFinishing() || isDestroyed()) return;
        if (scheduler.isShutdown()) return;
        try {
            scheduler.schedule(() -> sendWhenAuthed(send), 300, TimeUnit.MILLISECONDS);
        } catch (Exception ignored) {
        }
    }

    private boolean isWsReady() {
        return signaling != null && socket != null && socket.isAuthed();
    }

    private void markOfferChannel(SignalChannel channel) {
        if (channel == null) return;
        synchronized (this) {
            if (offerChannel == null) {
                offerChannel = channel;
                debugLog("offerChannel selected: " + offerChannel);
                return;
            }
            if (offerChannel == SignalChannel.HTTP
                    && channel == SignalChannel.WS
                    && !answerSent.get()
                    && !callActive) {
                offerChannel = channel;
                debugLog("offerChannel upgraded to WS");
            }
        }
    }

    private void setSignalChannel(SignalChannel channel) {
        synchronized (this) {
            if (signalChannelLocked.get()) return;
            if (signalChannel != null) return;
            signalChannel = channel;
            debugLog("signalChannel selected: " + signalChannel);
        }
    }

    private void forceSignalChannel(SignalChannel channel) {
        if (channel == null) return;
        synchronized (this) {
            signalChannel = channel;
            signalChannelLocked.set(true);
            debugLog("signalChannel forced: " + signalChannel);
        }
    }

    private void lockSignalChannel(SignalChannel channel, String reason) {
        if (channel == null) return;
        synchronized (this) {
            if (signalChannelLocked.get()) return;
            signalChannel = channel;
            signalChannelLocked.set(true);
            debugLog("signalChannel locked: " + signalChannel + " reason=" + reason);
        }
    }

    private void scheduleAuthWait() {
        if (scheduler.isShutdown()) return;
        cancelAuthWait();
        authWaitTimeout = scheduler.schedule(() -> {
            if (!waitAcceptAfterAuth || !acceptingCall) return;
            waitAcceptAfterAuth = false;
            setSignalChannel(SignalChannel.HTTP);
            debugLog("WS auth timeout, falling back to HTTP signaling");
            attemptAccept(0);
        }, AUTH_WAIT_MS, TimeUnit.MILLISECONDS);
    }

    private void cancelAuthWait() {
        if (authWaitTimeout != null) {
            authWaitTimeout.cancel(true);
            authWaitTimeout = null;
        }
    }

    private SignalChannel resolveSignalChannel() {
        boolean wsReady = isWsReady();
        synchronized (this) {
            if (wsReady && !answerSent.get()) {
                if (signalChannelLocked.get() && signalChannel == SignalChannel.HTTP) {
                    signalChannelLocked.set(false);
                    debugLog("signalChannel unlocked for WS upgrade");
                }
                if (signalChannel != SignalChannel.WS) {
                    signalChannel = SignalChannel.WS;
                    debugLog("signalChannel upgraded to WS: wsReady=" + wsReady + " offerChannel=" + offerChannel);
                }
                return signalChannel;
            }
            if (signalChannel != null && signalChannelLocked.get()) return signalChannel;
            if (signalChannel != null) return signalChannel;
            if (offerChannel != null) {
                signalChannel = offerChannel;
                debugLog("signalChannel selected from offerChannel: " + signalChannel + " wsReady=" + wsReady);
                return signalChannel;
            }
            SignalChannel selected = wsReady ? SignalChannel.WS : SignalChannel.HTTP;
            signalChannel = selected;
            debugLog("signalChannel selected: " + signalChannel + " wsReady=" + wsReady
                    + " offerChannel=" + offerChannel);
            return signalChannel;
        }
    }

    private void sendAnswerHttp(String answerB64, @Nullable Runnable onSent, @Nullable Runnable onFailure) {
        String token = authRepo.getToken();
        if (token == null || token.isEmpty() || peerId == null || peerId.isEmpty()) {
            debugLog("sendAnswerHttp: missing token or peerId");
            if (onFailure != null) {
                onFailure.run();
            }
            return;
        }
        signalingIo.execute(() -> {
            try {
                debugLog("sendAnswerHttp: sending via HTTP");
                api.callAnswer(token, peerId, answerB64);
                debugLog("sendAnswerHttp: success");
                if (onSent != null) {
                    onSent.run();
                }
            } catch (Exception e) {
                Log.w(TAG, logPrefix() + " sendAnswerHttp: failed", e);
                if (onFailure != null) {
                    onFailure.run();
                }
            }
        });
    }

    private void sendIceHttp(String candidateB64) {
        String token = authRepo.getToken();
        if (token == null || token.isEmpty() || peerId == null || peerId.isEmpty()) {
            debugLog("sendIceHttp: missing token or peerId");
            return;
        }
        signalingIo.execute(() -> {
            try {
                debugLog("sendIceHttp: sending via HTTP");
                api.callIce(token, peerId, candidateB64);
                debugLog("sendIceHttp: success");
            } catch (Exception e) {
                Log.w(TAG, logPrefix() + " sendIceHttp: failed", e);
            }
        });
    }

    private void queueOrAddIceCandidate(IceCandidate candidate) {
        if (candidate == null) return;
        if (peerConnection == null || !remoteDescriptionReady) {
            synchronized (pendingIceCandidates) {
                pendingIceCandidates.add(candidate);
            }
            return;
        }
        peerConnection.addIceCandidate(candidate);
    }

    private void drainPendingIceCandidates() {
        List<IceCandidate> candidates;
        synchronized (pendingIceCandidates) {
            if (pendingIceCandidates.isEmpty()) return;
            candidates = new ArrayList<>(pendingIceCandidates);
            pendingIceCandidates.clear();
        }
        if (peerConnection == null) return;
        for (IceCandidate candidate : candidates) {
            peerConnection.addIceCandidate(candidate);
        }
    }

    private void sendAcceptedOnce() {
        if (peerId == null || peerId.isEmpty()) return;
        if (!acceptedSent.compareAndSet(false, true)) {
            debugLog("sendAcceptedOnce: already sent");
            return;
        }
        debugLog("sendAcceptedOnce: sending accepted");
        sendCallResponse("accepted");
    }

    private boolean sdpHasMedia(@Nullable String sdp, String media) {
        if (sdp == null || media == null || media.isEmpty()) return false;
        String needle = "m=" + media;
        return sdp.contains("\n" + needle) || sdp.startsWith(needle);
    }

    private void applyRemoteAnswer(SessionDescription answer) {
        if (answer == null || peerConnection == null || answerApplied) return;
        peerConnection.setRemoteDescription(new SdpObserverAdapter() {
            @Override
            public void onSetSuccess() {
                debugLog("applyRemoteAnswer: remoteDescription set");
                answerApplied = true;
                remoteDescriptionReady = true;
                drainPendingIceCandidates();
                runOnUiThread(() -> {
                    if (!callActive) {
                        showActiveUi();
                        callActive = true;
                    }
                });
            }
        }, answer);
    }

    private SessionDescription parseSessionDescription(String json, SessionDescription.Type fallbackType) {
        try {
            String normalized = normalizeSignalingPayload(json);
            if (normalized == null || normalized.isEmpty()) return null;
            if (looksLikeJson(normalized)) {
                try {
                    JsonObject obj = gson.fromJson(normalized, JsonObject.class);
                    if (obj != null && obj.has("sdp")) {
                        SessionDescription.Type type = fallbackType;
                        if (obj.has("type")) {
                            try {
                                type = SessionDescription.Type.fromCanonicalForm(obj.get("type").getAsString());
                            } catch (Exception ignored) {
                            }
                        }
                        if (type != null) {
                            return new SessionDescription(type, obj.get("sdp").getAsString());
                        }
                    }
                } catch (Exception ignored) {
                    // fall through to manual SDP extraction
                }
                String sdp = extractSdpFromJson(normalized);
                if (sdp != null && fallbackType != null) {
                    return new SessionDescription(fallbackType, sdp);
                }
            }
            if (fallbackType != null && looksLikeSdp(normalized)) {
                return new SessionDescription(fallbackType, normalized);
            }
            return null;
        } catch (Exception e) {
            return null;
        }
    }

    private IceCandidate parseIceCandidate(String json) {
        try {
            String normalized = normalizeSignalingPayload(json);
            if (normalized == null || normalized.isEmpty()) return null;
            if (!looksLikeJson(normalized) && normalized.contains("candidate:")) {
                return new IceCandidate("0", 0, normalized);
            }
            JsonObject obj = gson.fromJson(normalized, JsonObject.class);
            if (obj == null || !obj.has("candidate")) return null;
            String sdpMid = "0";
            if (obj.has("sdpMid")) {
                try {
                    sdpMid = obj.get("sdpMid").getAsString();
                } catch (Exception ignored) {
                }
            } else if (obj.has("sdp_mid")) {
                try {
                    sdpMid = obj.get("sdp_mid").getAsString();
                } catch (Exception ignored) {
                }
            }
            int sdpMLineIndex = 0;
            JsonElement indexEl = obj.has("sdpMLineIndex")
                    ? obj.get("sdpMLineIndex")
                    : (obj.has("sdp_mline_index") ? obj.get("sdp_mline_index") : null);
            if (indexEl != null) {
                try {
                    sdpMLineIndex = indexEl.getAsInt();
                } catch (Exception ignored) {
                    try {
                        sdpMLineIndex = Integer.parseInt(indexEl.getAsString());
                    } catch (Exception ignored2) {
                    }
                }
            }
            String cand = obj.get("candidate").getAsString();
            return new IceCandidate(sdpMid, sdpMLineIndex, cand);
        } catch (Exception e) {
            return null;
        }
    }

    private String normalizeSignalingPayload(String raw) {
        if (raw == null) return null;
        String trimmed = unwrapQuotedJson(raw.trim());
        if (looksLikeJson(trimmed)) return sanitizeJsonString(trimmed);
        String decoded = tryDecodeBase64(trimmed);
        if (decoded == null) return trimmed;
        String decodedTrimmed = unwrapQuotedJson(decoded.trim());
        if (looksLikeJson(decodedTrimmed)) return sanitizeJsonString(decodedTrimmed);
        return decodedTrimmed.isEmpty() ? trimmed : decodedTrimmed;
    }

    private boolean looksLikeJson(String value) {
        return value != null && (value.startsWith("{") || value.startsWith("["));
    }

    private boolean looksLikeSdp(String value) {
        return value != null && (value.startsWith("v=") || value.contains("\nv=") || value.contains("\r\nv="));
    }

    private String sanitizeJsonString(String value) {
        if (value == null || value.isEmpty()) return value;
        String cleaned = value.replace("\\c", "\\r");
        cleaned = cleaned.replace("\r", "\\r").replace("\n", "\\n");
        StringBuilder sb = new StringBuilder(cleaned.length());
        for (int i = 0; i < cleaned.length(); i++) {
            char c = cleaned.charAt(i);
            if (c == '\\' && i + 1 < cleaned.length()) {
                char next = cleaned.charAt(i + 1);
                if ("\"\\/bfnrtu".indexOf(next) == -1) {
                    sb.append('\\');
                }
            }
            sb.append(c);
        }
        return sb.toString();
    }

    private String unwrapQuotedJson(String value) {
        if (value == null || value.length() < 2) return value;
        char first = value.charAt(0);
        char last = value.charAt(value.length() - 1);
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return value.substring(1, value.length() - 1);
        }
        return value;
    }

    private String tryDecodeBase64(String raw) {
        try {
            byte[] decoded = android.util.Base64.decode(raw, android.util.Base64.DEFAULT);
            return new String(decoded, StandardCharsets.UTF_8);
        } catch (Exception e) {
            try {
                byte[] decoded = android.util.Base64.decode(raw, android.util.Base64.URL_SAFE | android.util.Base64.NO_WRAP);
                return new String(decoded, StandardCharsets.UTF_8);
            } catch (Exception ignored) {
                return null;
            }
        }
    }

    private String extractSdpFromJson(String json) {
        if (json == null) return null;
        int keyIndex = json.indexOf("\"sdp\"");
        if (keyIndex < 0) return null;
        int colon = json.indexOf(':', keyIndex);
        if (colon < 0) return null;
        int startQuote = json.indexOf('"', colon + 1);
        if (startQuote < 0) return null;
        StringBuilder out = new StringBuilder();
        boolean escape = false;
        for (int i = startQuote + 1; i < json.length(); i++) {
            char c = json.charAt(i);
            if (escape) {
                switch (c) {
                    case 'n':
                        out.append('\n');
                        break;
                    case 'r':
                        out.append('\r');
                        break;
                    case 't':
                        out.append('\t');
                        break;
                    case '"':
                        out.append('"');
                        break;
                    case '\\':
                        out.append('\\');
                        break;
                    case 'u':
                        if (i + 4 < json.length()) {
                            String hex = json.substring(i + 1, i + 5);
                            try {
                                out.append((char) Integer.parseInt(hex, 16));
                                i += 4;
                                break;
                            } catch (Exception ignored) {
                            }
                        }
                        out.append('u');
                        break;
                    default:
                        out.append(c);
                        break;
                }
                escape = false;
                continue;
            }
            if (c == '\\') {
                escape = true;
                continue;
            }
            if (c == '"') {
                return out.toString();
            }
            out.append(c);
        }
        return null;
    }

    private String extractPayload(JsonObject obj, String key) {
        if (obj == null || key == null || !obj.has(key)) return null;
        try {
            JsonElement element = obj.get(key);
            if (element == null) return null;
            if (element.isJsonPrimitive()) {
                return element.getAsString();
            }
            return element.toString();
        } catch (Exception e) {
            return null;
        }
    }

    private String sessionToJson(SessionDescription sdp) {
        JsonObject obj = new JsonObject();
        obj.addProperty("type", sdp.type.canonicalForm());
        obj.addProperty("sdp", sdp.description);
        return obj.toString();
    }

    private String candidateToJson(IceCandidate candidate) {
        JsonObject obj = new JsonObject();
        obj.addProperty("sdpMid", candidate.sdpMid);
        obj.addProperty("sdpMLineIndex", candidate.sdpMLineIndex);
        obj.addProperty("candidate", candidate.sdp);
        return obj.toString();
    }

    private List<String> parseUrls(JsonElement urlsElement) {
        List<String> urls = new ArrayList<>();
        if (urlsElement == null || urlsElement.isJsonNull()) return urls;
        if (urlsElement.isJsonArray()) {
            JsonArray urlArr = urlsElement.getAsJsonArray();
            for (int i = 0; i < urlArr.size(); i++) {
                JsonElement el = urlArr.get(i);
                if (el != null && el.isJsonPrimitive()) {
                    String url = el.getAsString();
                    if (url != null && !url.isEmpty()) {
                        urls.add(url);
                    }
                }
            }
            return urls;
        }
        if (urlsElement.isJsonPrimitive()) {
            String raw = urlsElement.getAsString();
            if (raw != null && !raw.isEmpty()) {
                if (raw.contains(",")) {
                    String[] parts = raw.split(",");
                    for (String part : parts) {
                        String trimmed = part != null ? part.trim() : null;
                        if (trimmed != null && !trimmed.isEmpty()) {
                            urls.add(trimmed);
                        }
                    }
                } else {
                    urls.add(raw);
                }
            }
        }
        return urls;
    }

    private void addIceServer(List<PeerConnection.IceServer> servers,
                              Set<String> seenUrls,
                              List<String> urls,
                              @Nullable String username,
                              @Nullable String credential) {
        if (urls == null || urls.isEmpty()) return;
        List<String> unique = new ArrayList<>();
        for (String url : urls) {
            if (url == null || url.isEmpty()) continue;
            if (seenUrls.add(url)) {
                unique.add(url);
            }
        }
        if (unique.isEmpty()) return;
        PeerConnection.IceServer.Builder builder = PeerConnection.IceServer.builder(unique);
        if (username != null && credential != null) {
            builder.setUsername(username);
            builder.setPassword(credential);
        }
        servers.add(builder.createIceServer());
    }

    private void ensureStunFallback(List<PeerConnection.IceServer> servers, Set<String> seenUrls) {
        if (seenUrls.contains(STUN_FALLBACK_URL)) return;
        seenUrls.add(STUN_FALLBACK_URL);
        servers.add(PeerConnection.IceServer.builder(STUN_FALLBACK_URL).createIceServer());
    }

    private List<PeerConnection.IceServer> loadIceServers() {
        List<PeerConnection.IceServer> servers = new ArrayList<>();
        Set<String> seenUrls = new HashSet<>();

        try {
            String userId = authRepo != null ? authRepo.getUserId() : null;
            if (userId != null && !userId.isEmpty()) {
                JsonObject res = api.getTurnCredentials(userId, TURN_TTL_MINUTES);
                if (res != null && res.has("urls") && res.has("username") && res.has("credential")) {
                    List<String> urls = parseUrls(res.get("urls"));
                    String username = res.get("username").getAsString();
                    String credential = res.get("credential").getAsString();
                    addIceServer(servers, seenUrls, urls, username, credential);
                    if (!servers.isEmpty()) {
                        ensureStunFallback(servers, seenUrls);
                        cacheIceServers(servers);
                        return servers;
                    }
                }
            }
        } catch (Exception ignored) {
        }

        try {
            JsonObject res = api.getTurnConfig(authRepo.getToken());
            if (res != null && res.has("ice_servers")) {
                JsonArray arr = res.getAsJsonArray("ice_servers");
                for (int i = 0; i < arr.size(); i++) {
                    JsonObject ice = arr.get(i).getAsJsonObject();
                    if (ice == null || !ice.has("urls")) continue;
                    String username = ice.has("username") ? ice.get("username").getAsString() : null;
                    String credential = ice.has("credential") ? ice.get("credential").getAsString() : null;
                    List<String> urls = parseUrls(ice.get("urls"));
                    addIceServer(servers, seenUrls, urls, username, credential);
                }
                if (!servers.isEmpty()) {
                    ensureStunFallback(servers, seenUrls);
                    cacheIceServers(servers);
                    return servers;
                }
            }
        } catch (Exception ignored) {
        }
        ensureStunFallback(servers, seenUrls);
        cacheIceServers(servers);
        return servers;
    }

    private void cacheIceServers(List<PeerConnection.IceServer> servers) {
        if (servers == null || servers.isEmpty()) return;
        cachedIceServers = new ArrayList<>(servers);
    }

    private void prefetchIceServers() {
        List<PeerConnection.IceServer> cached = cachedIceServers;
        if (cached != null && !cached.isEmpty()) return;
        io.execute(() -> {
            List<PeerConnection.IceServer> servers = loadIceServers();
            if (servers != null && !servers.isEmpty()) {
                debugLog("prefetchIceServers: cached size=" + servers.size());
            }
        });
    }

    private List<PeerConnection.IceServer> getIceServersFast() {
        List<PeerConnection.IceServer> cached = cachedIceServers;
        if (cached != null && !cached.isEmpty()) {
            debugLog("getIceServersFast: using cached size=" + cached.size());
            return new ArrayList<>(cached);
        }
        debugLog("getIceServersFast: using STUN fallback");
        List<PeerConnection.IceServer> fallback = new ArrayList<>();
        fallback.add(PeerConnection.IceServer.builder(STUN_FALLBACK_URL).createIceServer());
        return fallback;
    }

    private String summarizeIceServers(@Nullable List<PeerConnection.IceServer> servers) {
        if (servers == null || servers.isEmpty()) return "none";
        int urlsCount = 0;
        int stunCount = 0;
        int turnCount = 0;
        for (PeerConnection.IceServer server : servers) {
            if (server == null || server.urls == null) continue;
            urlsCount += server.urls.size();
            for (String url : server.urls) {
                if (url == null) continue;
                String lower = url.toLowerCase();
                if (lower.startsWith("turn:") || lower.startsWith("turns:")) {
                    turnCount++;
                } else if (lower.startsWith("stun:") || lower.startsWith("stuns:")) {
                    stunCount++;
                }
            }
        }
        return "servers=" + servers.size() + " urls=" + urlsCount + " stun=" + stunCount + " turn=" + turnCount;
    }

    private void fetchPendingOffer() {
        if (peerId == null || peerId.isEmpty()) return;
        io.execute(() -> {
            String offer = fetchOfferFromServer();
            if (offer != null) {
                if (answerSent.get() || callActive) {
                    debugLog("fetchPendingOffer: ignored, already answered/active");
                    return;
                }
                synchronized (this) {
                    if (offerChannel == SignalChannel.WS
                            && pendingOfferJson != null
                            && !pendingOfferJson.isEmpty()) {
                        debugLog("fetchPendingOffer: ignored, WS offer already set");
                        return;
                    }
                }
                markOfferChannel(SignalChannel.HTTP);
                pendingOfferJson = offer;
                debugLog("fetchPendingOffer: offer fetched len=" + offer.length());
            }
        });
    }

    private String fetchOfferFromServer() {
        try {
            JsonObject res = api.getCallOffer(authRepo.getToken(), peerId);
            if (res != null && res.has("data")) {
                JsonObject data = res.getAsJsonObject("data");
                if (data != null) {
                    String offer = extractPayload(data, "offer");
                    if (offer != null) return offer;
                }
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    private void startPolling() {
        stopPolling();
        responsePoller = scheduler.scheduleAtFixedRate(() -> {
            if (peerId == null || peerId.isEmpty()) return;
            if (incoming) return;
            try {
                JsonObject res = api.checkCallResponse(authRepo.getToken(), peerId);
                if (res != null && res.has("response") && !res.get("response").isJsonNull()) {
                    String response = res.get("response").getAsString();
                    if ("rejected".equals(response) || "ended".equals(response)) {
                        endReason = response;
                        runOnUiThread(() -> endCall(false));
                    }
                }
            } catch (Exception ignored) {
            }
            if (!answerApplied) {
                try {
                    JsonObject ans = api.getCallAnswer(authRepo.getToken(), peerId);
                    if (ans != null && ans.has("data")) {
                        JsonObject data = ans.getAsJsonObject("data");
                        if (data != null) {
                            String answerJson = extractPayload(data, "answer");
                            if (answerJson != null) {
                                SessionDescription answer = parseSessionDescription(answerJson, SessionDescription.Type.ANSWER);
                                if (answer != null) {
                                    applyRemoteAnswer(answer);
                                }
                            }
                        }
                    }
                } catch (Exception ignored) {
                }
            }
        }, 0, 2, TimeUnit.SECONDS);

        icePoller = scheduler.scheduleAtFixedRate(() -> {
            if (peerConnection == null || peerId == null || peerId.isEmpty()) return;
            try {
                JsonObject res = api.getCallIce(authRepo.getToken(), peerId, lastIceCheck);
                lastIceCheck = System.currentTimeMillis() / 1000;
                if (res != null && res.has("data")) {
                    JsonObject data = res.getAsJsonObject("data");
                    if (data != null && data.has("candidates")) {
                        JsonElement candidates = data.get("candidates");
                        JsonArray arr;
                        if (candidates != null && candidates.isJsonArray()) {
                            arr = candidates.getAsJsonArray();
                        } else {
                            String arrJson = candidates != null ? candidates.getAsString() : null;
                            arr = arrJson != null ? gson.fromJson(arrJson, JsonArray.class) : null;
                        }
                        if (arr != null) {
                            for (int i = 0; i < arr.size(); i++) {
                                JsonElement el = arr.get(i);
                                String candJson = el == null ? null
                                        : (el.isJsonPrimitive() ? el.getAsString() : el.toString());
                                if (candJson == null || candJson.isEmpty()) {
                                    continue;
                                }
                                if (seenCandidates.add(candJson)) {
                                    IceCandidate candidate = parseIceCandidate(candJson);
                                    if (candidate != null) {
                                        queueOrAddIceCandidate(candidate);
                                    }
                                }
                            }
                        }
                    }
                }
            } catch (Exception ignored) {
            }
        }, 1, 2, TimeUnit.SECONDS);
    }

    private void stopPolling() {
        if (responsePoller != null) {
            responsePoller.cancel(true);
            responsePoller = null;
        }
        if (icePoller != null) {
            icePoller.cancel(true);
            icePoller = null;
        }
    }

    private void releaseWebRtc() {
        resetCallRecoveryState();
        if (peerConnection != null) {
            peerConnection.close();
            peerConnection.dispose();
            peerConnection = null;
        }
        remoteDescriptionReady = false;
        synchronized (pendingIceCandidates) {
            pendingIceCandidates.clear();
        }
        synchronized (pendingLocalIceCandidates) {
            pendingLocalIceCandidates.clear();
        }
        queuedLocalIceCandidates.clear();
        sentIceCandidates.clear();
        sentIceCount.set(0);
        if (localAudioTrack != null) {
            localAudioTrack.dispose();
            localAudioTrack = null;
        }
        if (audioSource != null) {
            audioSource.dispose();
            audioSource = null;
        }
        if (factory != null) {
            factory.dispose();
            factory = null;
        }
        if (audioDeviceModule != null) {
            audioDeviceModule.release();
            audioDeviceModule = null;
        }
    }

    private void requestAudioFocus() {
        audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        if (audioManager == null) return;
        audioManager.setMode(AudioManager.MODE_IN_COMMUNICATION);
        audioManager.setSpeakerphoneOn(false);
        speakerOn = false;
        AudioAttributes attrs = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                .build();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            focusRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN_TRANSIENT)
                    .setAudioAttributes(attrs)
                    .setOnAudioFocusChangeListener(focusChange -> {
                    })
                    .build();
            audioManager.requestAudioFocus(focusRequest);
        } else {
            audioManager.requestAudioFocus(null, AudioManager.STREAM_VOICE_CALL, AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
        }
    }

    private void releaseAudioFocus() {
        if (audioManager == null) return;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O && focusRequest != null) {
            audioManager.abandonAudioFocusRequest(focusRequest);
        } else {
            audioManager.abandonAudioFocus(null);
        }
        audioManager.setSpeakerphoneOn(false);
        audioManager.setMode(AudioManager.MODE_NORMAL);
    }

    private void toggleSpeaker() {
        if (audioManager == null) {
            audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        }
        if (audioManager == null) return;
        speakerOn = !speakerOn;
        audioManager.setSpeakerphoneOn(speakerOn);
        binding.btnSpeaker.setText(speakerOn ? "Speaker On" : "Speaker");
    }

    private void toggleMute() {
        micMuted = !micMuted;
        if (localAudioTrack != null) {
            localAudioTrack.setEnabled(!micMuted);
        }
        binding.btnMute.setText(micMuted ? "Muted" : "Mute");
        // Send media state to peer
        sendMediaState("mic", !micMuted);
    }

    private void sendMediaState(String mediaType, boolean enabled) {
        if (signaling == null || peerId == null || !callActive) return;
        signaling.sendMediaState(peerId, mediaType, enabled);
        debugLog("sendMediaState: " + mediaType + "=" + enabled);
    }

    private void sendInitialMediaState() {
        if (signaling == null || peerId == null) return;
        // Send mic state - this is what the peer needs to know
        sendMediaState("mic", !micMuted);
    }

    private void updateRemoteMediaIndicators() {
        // Only show mic indicator (speaker is local setting, not relevant for peer)
        boolean showIndicators = !remoteMicEnabled;
        binding.remoteMediaIndicators.setVisibility(showIndicators ? View.VISIBLE : View.GONE);
        binding.indicatorMicOff.setVisibility(remoteMicEnabled ? View.GONE : View.VISIBLE);
        // Hide speaker indicator as it's not relevant
        binding.indicatorSpeakerOff.setVisibility(View.GONE);
    }

    private void resetRemoteMediaState() {
        remoteMicEnabled = true;
        remoteSpeakerEnabled = true;
        runOnUiThread(this::updateRemoteMediaIndicators);
    }

    private void startRingtone() {
        try {
            Uri tone = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_RINGTONE);
            ringtone = RingtoneManager.getRingtone(this, tone);
            if (ringtone != null) ringtone.play();
        } catch (Exception ignored) {
        }
        vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
        if (vibrator != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                vibrator.vibrate(VibrationEffect.createWaveform(new long[]{0, 700, 600}, 0));
            } else {
                vibrator.vibrate(new long[]{0, 700, 600}, 0);
            }
        }
    }

    private void stopRingtone() {
        try {
            if (ringtone != null && ringtone.isPlaying()) ringtone.stop();
        } catch (Exception ignored) {
        }
        ringtone = null;
        if (vibrator != null) {
            vibrator.cancel();
            vibrator = null;
        }
    }

    @Override
    public void onAuthed() {
        debugLog("onAuthed: ws authenticated");
        if (waitAcceptAfterAuth && acceptingCall) {
            waitAcceptAfterAuth = false;
            cancelAuthWait();
            setSignalChannel(SignalChannel.WS);
            attemptAccept(0);
        }
    }

    @Override
    public void onMessage(JsonObject obj) {
        if (obj == null || !obj.has("type")) return;
        String type = obj.get("type").getAsString();
        if ("call_offer".equals(type)) {
            String fromId = obj.has("from_user_id") ? obj.get("from_user_id").getAsString() : null;
            if (peerId != null && fromId != null && !peerId.equals(fromId)) return;
            if (peerId == null) peerId = fromId;
            if (peerName == null && obj.has("from_username")) {
                peerName = obj.get("from_username").getAsString();
                runOnUiThread(() -> {
                    binding.callName.setText(peerName);
                    if (peerName != null && !peerName.isEmpty()) {
                        binding.callAvatar.setText(peerName.substring(0, 1).toUpperCase());
                    }
                });
            }
            String offer = extractPayload(obj, "offer");
            if (offer != null) {
                boolean samePeer = peerId != null && fromId != null && peerId.equals(fromId);
                boolean canReoffer = callActive || callAnswered;
                if (canReoffer && samePeer) {
                    debugLog("onMessage: call_offer re-offer len=" + offer.length());
                    SessionDescription reoffer = parseSessionDescription(offer, SessionDescription.Type.OFFER);
                    if (reoffer != null) {
                        handleReoffer(reoffer);
                    } else {
                        debugLog("re-offer invalid");
                    }
                    return;
                }
                if (answerSent.get() || callActive) {
                    debugLog("call_offer ignored: already answered/active");
                    return;
                }
                debugLog("onMessage: call_offer len=" + offer.length());
                markOfferChannel(SignalChannel.WS);
                if (isWsReady()) {
                    setSignalChannel(SignalChannel.WS);
                }
                pendingOfferJson = offer;
                if (incoming) {
                    runOnUiThread(this::showIncomingUi);
                }
            }
        } else if ("call_answer".equals(type)) {
            String fromId = obj.has("from_user_id") ? obj.get("from_user_id").getAsString() : null;
            if (peerId != null && fromId != null && !peerId.equals(fromId)) return;
            String answerJson = extractPayload(obj, "answer");
            if (answerJson == null) return;
            debugLog("onMessage: call_answer len=" + answerJson.length());
            SessionDescription answer = parseSessionDescription(answerJson, SessionDescription.Type.ANSWER);
            if (answer != null) {
                applyRemoteAnswer(answer);
            }
        } else if ("call_ice_candidate".equals(type)) {
            String fromId = obj.has("from_user_id") ? obj.get("from_user_id").getAsString() : null;
            if (peerId != null && fromId != null && !peerId.equals(fromId)) return;
            String candJson = extractPayload(obj, "candidate");
            if (candJson == null) return;
            if (seenCandidates.add(candJson)) {
                IceCandidate candidate = parseIceCandidate(candJson);
                if (candidate != null) {
                    queueOrAddIceCandidate(candidate);
                }
            }
        } else if ("call_error".equals(type)) {
            String message = obj.has("error_message") ? obj.get("error_message").getAsString() : "Call error";
            debugLog("onMessage: call_error " + message);
            runOnUiThread(() -> {
                Toast.makeText(CallActivity.this, message, Toast.LENGTH_LONG).show();
                endCall(false);
            });
        } else if ("call_end".equals(type)) {
            String pcState = peerConnection != null ? peerConnection.connectionState().toString() : "none";
            debugLog("onMessage: call_end received pc=" + pcState);
            if (endReason == null) {
                endReason = callAnswered ? "ended" : (incoming ? "missed" : "cancelled");
            }
            runOnUiThread(() -> endCall(false));
        } else if ("call_media_state".equals(type)) {
            String fromId = obj.has("from_user_id") ? obj.get("from_user_id").getAsString() : null;
            if (peerId != null && fromId != null && !peerId.equals(fromId)) return;
            String mediaType = obj.has("media_type") ? obj.get("media_type").getAsString() : "";
            boolean enabled = !obj.has("enabled") || obj.get("enabled").getAsBoolean();
            debugLog("onMessage: call_media_state " + mediaType + "=" + enabled);
            if ("mic".equals(mediaType)) {
                remoteMicEnabled = enabled;
            } else if ("speaker".equals(mediaType)) {
                remoteSpeakerEnabled = enabled;
            }
            runOnUiThread(this::updateRemoteMediaIndicators);
        }
    }

    @Override
    public void onClosed() {
        debugLog("onClosed: ws closed");
    }

    @Override
    public void onFailure(Throwable t) {
        Log.w(TAG, logPrefix() + " onFailure: ws error", t);
    }

    private class PeerObserver implements PeerConnection.Observer {
        @Override
        public void onIceCandidate(IceCandidate iceCandidate) {
            sendIce(iceCandidate);
        }

        @Override
        public void onTrack(RtpTransceiver transceiver) {
            if (!callActive) {
                runOnUiThread(() -> {
                    showActiveUi();
                    callActive = true;
                });
            }
            clearCallRecovery("track");
        }

        @Override
        public void onConnectionChange(PeerConnection.PeerConnectionState newState) {
            debugLog("pc state=" + newState);
            if (newState == PeerConnection.PeerConnectionState.CONNECTED) {
                runOnUiThread(() -> {
                    showActiveUi();
                    callActive = true;
                });
                clearCallRecovery("pc-connected");
                return;
            }
            if (newState == PeerConnection.PeerConnectionState.DISCONNECTED
                    || newState == PeerConnection.PeerConnectionState.FAILED) {
                beginCallRecovery("pc-" + newState);
            }
        }

        @Override public void onSignalingChange(PeerConnection.SignalingState signalingState) { }
        @Override public void onIceConnectionChange(PeerConnection.IceConnectionState iceConnectionState) {
            debugLog("ice state=" + iceConnectionState);
            if (iceConnectionState == PeerConnection.IceConnectionState.CONNECTED
                    || iceConnectionState == PeerConnection.IceConnectionState.COMPLETED) {
                clearCallRecovery("ice-connected");
                return;
            }
            if (iceConnectionState == PeerConnection.IceConnectionState.DISCONNECTED
                    || iceConnectionState == PeerConnection.IceConnectionState.FAILED) {
                beginCallRecovery("ice-" + iceConnectionState);
            }
        }
        @Override public void onIceCandidatesRemoved(IceCandidate[] iceCandidates) { }
        @Override public void onIceConnectionReceivingChange(boolean b) { }
        @Override public void onIceGatheringChange(PeerConnection.IceGatheringState iceGatheringState) { }
        @Override public void onAddStream(org.webrtc.MediaStream mediaStream) { }
        @Override public void onRemoveStream(org.webrtc.MediaStream mediaStream) { }
        @Override public void onDataChannel(org.webrtc.DataChannel dataChannel) { }
        @Override public void onRenegotiationNeeded() { }
    }

    private static class SdpObserverAdapter implements org.webrtc.SdpObserver {
        @Override public void onCreateSuccess(SessionDescription sessionDescription) { }
        @Override public void onSetSuccess() { }
        @Override public void onCreateFailure(String s) { }
        @Override public void onSetFailure(String s) { }
    }

    private void markCallAnswered() {
        if (callAnswered) return;
        callAnswered = true;
        callStartedAtMs = System.currentTimeMillis();
    }

    private String resolveFinalStatus() {
        if (callAnswered) return "ended";
        if ("rejected".equals(endReason)) return "rejected";
        if ("cancelled".equals(endReason)) return "cancelled";
        if ("missed".equals(endReason)) return "missed";
        return incoming ? "missed" : "cancelled";
    }

    private void saveCallLog(String status) {
        if (callLogged) return;
        callLogged = true;
        long now = System.currentTimeMillis();
        CallLogEntity log = new CallLogEntity();
        log.peerId = peerId != null ? peerId : "";
        String displayName = peerName != null && !peerName.isEmpty() ? peerName : binding.callName.getText().toString();
        log.peerName = displayName != null ? displayName : "";
        log.direction = incoming ? "incoming" : "outgoing";
        log.status = status;
        log.startedAt = callAnswered ? callStartedAtMs : callCreatedAtMs;
        log.endedAt = now;
        if (log.startedAt <= 0) log.startedAt = now;
        log.durationSec = callAnswered ? Math.max(0, (now - callStartedAtMs) / 1000L) : 0L;
        io.execute(() -> AppDatabase.get(this).callLogDao().insert(log));
    }
}
