package com.xipher.wrapper.ui.fragment;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.media.MediaPlayer;
import android.media.MediaRecorder;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.provider.OpenableColumns;
import android.text.TextUtils;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.PopupMenu;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.core.content.ContextCompat;
import androidx.core.content.FileProvider;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.ViewModelProvider;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.emoji2.widget.EmojiTextView;

import com.google.android.material.bottomsheet.BottomSheetDialog;
import com.google.android.material.switchmaterial.SwitchMaterial;
import com.xipher.wrapper.R;
import com.google.gson.JsonObject;
import com.xipher.wrapper.BuildConfig;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.db.AppDatabase;
import com.xipher.wrapper.data.db.entity.ChatEntity;
import com.xipher.wrapper.data.db.entity.MessageEntity;
import com.xipher.wrapper.data.model.ChecklistItem;
import com.xipher.wrapper.data.model.ChecklistPayload;
import com.xipher.wrapper.databinding.FragmentChatBinding;
import com.xipher.wrapper.ui.CallActivity;
import com.xipher.wrapper.ui.ChatDetailActivity;
import com.xipher.wrapper.ui.PremiumPaymentActivity;
import com.xipher.wrapper.ui.adapter.MessageAdapter;
import com.xipher.wrapper.ui.videonote.CameraRecorderHelper;
import com.xipher.wrapper.ui.videonote.CircularCameraView;
import com.xipher.wrapper.ui.videonote.MediaRecorderButton;
import com.xipher.wrapper.ui.vm.ChatViewModel;
import com.xipher.wrapper.ui.TopicTabsHelper;
import com.xipher.wrapper.data.model.TopicDto;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.theme.ChatThemeManager;

import com.google.gson.Gson;
import com.google.gson.JsonArray;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.HashSet;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.UUID;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.Response;
import okhttp3.ResponseBody;
import androidx.recyclerview.widget.ItemTouchHelper;
import com.xipher.wrapper.ui.helper.SwipeToReplyCallback;

public class ChatFragment extends Fragment {

    private static final String ARG_CHAT_ID = "chat_id";
    private static final String ARG_CHAT_TITLE = "chat_title";
    private static final String ARG_IS_GROUP = "is_group";
    private static final String ARG_IS_CHANNEL = "is_channel";
    private static final String ARG_IS_SAVED = "is_saved";
    private static final long MAX_FILE_BYTES = 25L * 1024 * 1024;
    private static final long MAX_VOICE_BYTES = 15L * 1024 * 1024;
    private static final long AUTO_REFRESH_MS = 2000L;
    private static final long TTL_PRUNE_INTERVAL_MS = 1000L;
    private static final long TYPING_DOTS_INTERVAL_MS = 400L;
    private static final String GIFT_PLAN_MONTH = "month";
    private static final String GIFT_PLAN_HALF = "half";
    private static final String GIFT_PLAN_YEAR = "year";

    public static ChatFragment newInstance(String chatId, String title) {
        return newInstance(chatId, title, false, false, false);
    }

    public static ChatFragment newInstance(String chatId, String title, boolean isGroup, boolean isChannel) {
        return newInstance(chatId, title, isGroup, isChannel, false);
    }

    public static ChatFragment newInstance(String chatId, String title, boolean isGroup, boolean isChannel, boolean isSaved) {
        Bundle b = new Bundle();
        b.putString(ARG_CHAT_ID, chatId);
        b.putString(ARG_CHAT_TITLE, title);
        b.putBoolean(ARG_IS_GROUP, isGroup);
        b.putBoolean(ARG_IS_CHANNEL, isChannel);
        b.putBoolean(ARG_IS_SAVED, isSaved);
        ChatFragment f = new ChatFragment();
        f.setArguments(b);
        return f;
    }

    private FragmentChatBinding binding;
    private ChatViewModel vm;
    private MessageAdapter adapter;
    private String chatId;
    private boolean isGroup;
    private boolean isChannel;
    private boolean isSaved;
    private boolean channelCanPost = false;
    private boolean channelSubscribed = false;
    private boolean spoilerModeEnabled = false;
    private long selectedTtlSeconds = 0L;
    private String selfAvatarUrl;
    private String peerAvatarUrl;
    private final Map<String, String> avatarByUserId = new HashMap<>();
    private final Set<String> avatarRequests = new HashSet<>();
    private ActivityResultLauncher<String[]> filePicker;
    private ActivityResultLauncher<String[]> imagePicker;
    private ActivityResultLauncher<String> audioPermission;
    private ActivityResultLauncher<String[]> videoPermissions;
    private ActivityResultLauncher<Intent> locationShareLauncher;
    private ActivityResultLauncher<Intent> premiumGiftPaymentLauncher;
    private MediaRecorderButton mediaButton;
    private CircularCameraView videoNotePreview;
    private CameraRecorderHelper cameraRecorder;
    private boolean isAudioRecording = false;
    private boolean isVideoRecording = false;
    private boolean recordingLocked = false;
    private MediaRecorder recorder;
    private File recordFile;
    private long recordStart = 0L;
    private final Handler timerHandler = new Handler(Looper.getMainLooper());
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private MediaPlayer mediaPlayer;
    private String playingId;
    private final Handler playbackHandler = new Handler(Looper.getMainLooper());
    private Runnable playbackRunnable = () -> {};
    private boolean emojiVisible = false;
    private String baseSubtitleText;
    private boolean baseSubtitleVisible = false;
    private boolean typingVisible = false;
    private String typingBaseText;
    private int typingDotsStep = 0;
    private Runnable typingDotsRunnable;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final OkHttpClient downloadClient = new OkHttpClient();
    private final Map<String, okhttp3.Call> downloadCalls = new HashMap<>();
    private final Map<String, File> downloadedFiles = new HashMap<>();
    private final Handler autoRefreshHandler = new Handler(Looper.getMainLooper());
    private Runnable autoRefreshRunnable = () -> {};
    private final Handler ttlPruneHandler = new Handler(Looper.getMainLooper());
    private Runnable ttlPruneRunnable = () -> {};
    private MessageEntity replyToMessage = null;
    
    // Forum Topics (Telegram-style)
    private boolean isForumMode = false;
    private List<TopicDto> currentTopics = new ArrayList<>();
    private TopicDto currentTopic = null;
    private String groupUserRole = "member";
    private final Gson topicGson = new Gson();
    private boolean selectionMode = false;
    private String selectedGiftPlan = GIFT_PLAN_HALF;
    private final List<String> emojis = Arrays.asList(
            "😀", "😁", "😂", "🤣", "😊", "😍", "😘", "😉",
            "😎", "🤔", "😅", "😢", "😭", "😡", "👍", "👌",
            "🙏", "👏", "💯", "🔥", "✨", "🎉", "🚀", "❤️"
    );

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // CRITICAL: registerForActivityResult MUST be called BEFORE onStart()
        // When using commitNow(), this can race with lifecycle, so we register in onCreate
        try {
            filePicker = registerForActivityResult(new ActivityResultContracts.OpenDocument(), this::handlePickedFile);
            imagePicker = registerForActivityResult(new ActivityResultContracts.OpenDocument(), this::handlePickedImage);
            audioPermission = registerForActivityResult(new ActivityResultContracts.RequestPermission(), granted -> {
                if (!granted) {
                    Toast.makeText(getContext(), R.string.mic_permission_required, Toast.LENGTH_SHORT).show();
                    if (mediaButton != null) mediaButton.resetState();
                }
            });
            videoPermissions = registerForActivityResult(new ActivityResultContracts.RequestMultiplePermissions(), result -> {
                boolean cameraGranted = Boolean.TRUE.equals(result.get(Manifest.permission.CAMERA));
                boolean audioGranted = Boolean.TRUE.equals(result.get(Manifest.permission.RECORD_AUDIO));
                if (!cameraGranted || !audioGranted) {
                    Toast.makeText(getContext(), R.string.media_permission_required, Toast.LENGTH_SHORT).show();
                    if (mediaButton != null) mediaButton.resetState();
                    return;
                }
                if (mediaButton != null && mediaButton.getMode() == MediaRecorderButton.Mode.VIDEO) {
                    showVideoPreview(true);
                }
            });
            locationShareLauncher = registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK && vm != null) {
                    vm.refreshHistory();
                }
            });
            premiumGiftPaymentLauncher = registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == Activity.RESULT_OK) {
                    Toast.makeText(getContext(), R.string.premium_gift_payment_success, Toast.LENGTH_SHORT).show();
                }
            });
        } catch (Throwable t) {
            android.util.Log.e("ChatFragment", "Failed to register activity results", t);
        }
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        binding = FragmentChatBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        // Show crash log if exists
        showCrashLogIfExists();
        try {
            initFragment(view, savedInstanceState);
        } catch (Throwable t) {
            android.util.Log.e("ChatFragment", "onViewCreated failed", t);
            Toast.makeText(getContext(), "Chat init error: " + t.getMessage(), Toast.LENGTH_LONG).show();
            showCrashDialog(t);
        }
    }
    
    private void showCrashLogIfExists() {
        try {
            java.io.File crashFile = new java.io.File(requireContext().getFilesDir(), "crash.log");
            if (crashFile.exists() && crashFile.length() > 0) {
                java.io.BufferedReader reader = new java.io.BufferedReader(new java.io.FileReader(crashFile));
                StringBuilder sb = new StringBuilder();
                String line;
                while ((line = reader.readLine()) != null) {
                    sb.append(line).append("\n");
                }
                reader.close();
                if (sb.length() > 0) {
                    String log = sb.toString();
                    android.util.Log.e("ChatFragment", "CRASH LOG:\n" + log);
                    // Delete after reading
                    crashFile.delete();
                    // Show in dialog
                    new AlertDialog.Builder(requireContext())
                        .setTitle("Previous Crash")
                        .setMessage(log.length() > 2000 ? log.substring(0, 2000) + "..." : log)
                        .setPositiveButton("OK", null)
                        .show();
                }
            }
        } catch (Exception ignored) {}
    }
    
    private void showCrashDialog(Throwable t) {
        try {
            java.io.StringWriter sw = new java.io.StringWriter();
            t.printStackTrace(new java.io.PrintWriter(sw));
            String trace = sw.toString();
            new AlertDialog.Builder(requireContext())
                .setTitle("Error")
                .setMessage(trace.length() > 2000 ? trace.substring(0, 2000) : trace)
                .setPositiveButton("OK", null)
                .show();
        } catch (Exception ignored) {}
    }
    
    private void initFragment(@NonNull View view, @Nullable Bundle savedInstanceState) {
        // Ensure database is initialized before anything else
        try {
            com.xipher.wrapper.data.db.AppDatabase.get(requireContext());
        } catch (IllegalStateException e) {
            try {
                String key = com.xipher.wrapper.security.DbKeyStore.getOrCreateMainKey(requireContext());
                com.xipher.wrapper.data.db.DatabaseManager.getInstance().init(requireContext().getApplicationContext(), key);
            } catch (Exception ignored) {}
        }
        
        ChatThemeManager.applyChatBackground(
            binding.messagesContainer, 
            binding.chatBackgroundImage, 
            binding.chatBackgroundDim, 
            requireContext()
        );
        // ActivityResultLaunchers are now registered in onCreate()

        chatId = getArguments() != null ? getArguments().getString(ARG_CHAT_ID) : null;
        String title = getArguments() != null ? getArguments().getString(ARG_CHAT_TITLE) : "";
        isGroup = getArguments() != null && getArguments().getBoolean(ARG_IS_GROUP, false);
        isChannel = getArguments() != null && getArguments().getBoolean(ARG_IS_CHANNEL, false);
        isSaved = getArguments() != null && getArguments().getBoolean(ARG_IS_SAVED, false);
        binding.chatTitle.setText(title);
        binding.chatSubtitle.setVisibility(View.GONE);
        setBaseSubtitle(null, false);
        if (chatId == null || isGroup || isChannel || isSaved) {
            binding.btnCall.setVisibility(View.GONE);
        } else {
            binding.btnCall.setVisibility(View.VISIBLE);
            binding.btnCall.setOnClickListener(v -> {
                CallActivity.startOutgoing(requireContext(), chatId, title);
            });
        }

        authRepo = AppServices.authRepository(requireContext());
        boolean showBack = getActivity() instanceof ChatDetailActivity;
        binding.btnBack.setVisibility(showBack ? View.VISIBLE : View.GONE);
        binding.btnBack.setOnClickListener(v -> requireActivity().onBackPressed());
        
        // Setup More button with popup menu
        binding.btnMore.setOnClickListener(v -> showChatOptionsMenu(v));
        
        // Avatar is visible for all chat types
        binding.chatHeaderAvatar.setVisibility(View.VISIBLE);
        binding.chatHeaderAvatar.setOnClickListener(v -> {
            if (isChannel) {
                openChannelInfo();
            } else if (isGroup) {
                openGroupInfo();
            } else {
                openProfile();
            }
        });
        
        // Header title click - open group/channel info
        binding.chatTitle.setOnClickListener(v -> {
            if (isChannel) {
                openChannelInfo();
            } else if (isGroup) {
                openGroupInfo();
            } else {
                openProfile();
            }
        });

        mediaButton = binding.btnMedia;
        videoNotePreview = binding.videoNotePreview;
        cameraRecorder = new CameraRecorderHelper(requireContext());
        cameraRecorder.setListener(new CameraRecorderHelper.Listener() {
            @Override
            public void onRecordingStarted() {
                isVideoRecording = true;
                recordingLocked = false;
                if (videoNotePreview != null) {
                    videoNotePreview.setRecordingActive(true);
                }
                updateSendMicState();
                updateRecordingHints();
            }

            @Override
            public void onRecordingProgress(long durationMs, float progress) {
                if (videoNotePreview != null) {
                    videoNotePreview.setRecordingProgress(progress);
                }
            }

            @Override
            public void onRecordingFinalized(@NonNull File file, long durationMs) {
                isVideoRecording = false;
                recordingLocked = false;
                if (videoNotePreview != null) {
                    videoNotePreview.setRecordingActive(false);
                    videoNotePreview.resetProgress();
                }
                if (file != null && file.exists()) {
                    if (file.length() > MAX_FILE_BYTES) {
                        Toast.makeText(getContext(), R.string.file_too_large, Toast.LENGTH_SHORT).show();
                    } else {
                        vm.sendFile(Uri.fromFile(file), file.getName(), file.length(), false, selectedTtlSeconds);
                    }
                }
                updateSendMicState();
                updateRecordingHints();
                if (mediaButton != null) mediaButton.resetState();
                if (mediaButton != null && mediaButton.getMode() != MediaRecorderButton.Mode.VIDEO) {
                    hideVideoPreview(true);
                }
            }

            @Override
            public void onRecordingCanceled() {
                isVideoRecording = false;
                recordingLocked = false;
                if (videoNotePreview != null) {
                    videoNotePreview.setRecordingActive(false);
                    videoNotePreview.resetProgress();
                }
                updateSendMicState();
                updateRecordingHints();
                if (mediaButton != null) mediaButton.resetState();
                if (mediaButton != null && mediaButton.getMode() != MediaRecorderButton.Mode.VIDEO) {
                    hideVideoPreview(true);
                }
            }

            @Override
            public void onRecordingError(@NonNull String message) {
                isVideoRecording = false;
                recordingLocked = false;
                if (videoNotePreview != null) {
                    videoNotePreview.setRecordingActive(false);
                    videoNotePreview.resetProgress();
                }
                Toast.makeText(getContext(), message, Toast.LENGTH_SHORT).show();
                updateSendMicState();
                updateRecordingHints();
                if (mediaButton != null) mediaButton.resetState();
                if (mediaButton != null && mediaButton.getMode() != MediaRecorderButton.Mode.VIDEO) {
                    hideVideoPreview(true);
                }
            }
        });

        if (mediaButton != null) {
            mediaButton.setOnMediaActionListener(new MediaRecorderButton.OnMediaActionListener() {
                @Override
                public void onModeChanged(@NonNull MediaRecorderButton.Mode mode) {
                    if (mode == MediaRecorderButton.Mode.VIDEO) {
                        mediaButton.setContentDescription(getString(R.string.video_note_record));
                        if (hasVideoPermissions()) {
                            showVideoPreview(true);
                        }
                    } else {
                        mediaButton.setContentDescription(getString(R.string.voice_record));
                        if (!isVideoRecording) {
                            hideVideoPreview(true);
                        }
                    }
                }

                @Override
                public void onAudioRecordStart() {
                    requestAudioAndStartRecording();
                }

                @Override
                public void onVideoRecordStart() {
                    requestVideoAndStartRecording();
                }

                @Override
                public void onStopRecording(@NonNull MediaRecorderButton.Mode mode) {
                    if (mode == MediaRecorderButton.Mode.AUDIO) {
                        stopAudioRecording(true);
                    } else {
                        stopVideoRecording(true);
                    }
                }

                @Override
                public void onCancelRecording(@NonNull MediaRecorderButton.Mode mode) {
                    if (mode == MediaRecorderButton.Mode.AUDIO) {
                        stopAudioRecording(false);
                    } else {
                        stopVideoRecording(false);
                    }
                }

                @Override
                public void onLockRecording(@NonNull MediaRecorderButton.Mode mode) {
                    recordingLocked = true;
                    updateRecordingHints();
                    if (mode == MediaRecorderButton.Mode.AUDIO) {
                        binding.recordingLabel.setText(R.string.recording_locked);
                    }
                }
            });
        }

        vm = new ViewModelProvider(this).get(ChatViewModel.class);
        vm.init(chatId, isGroup, isChannel, isSaved);

        adapter = new MessageAdapter(new ArrayList<>());
        adapter.setSelfId(vm.getSelfId());
        adapter.setShowSenderNames(isGroup);
        adapter.setShowAvatarImages(!isChannel);
        adapter.setAvatarUrls(selfAvatarUrl, peerAvatarUrl);
        adapter.setAvatarMap(avatarByUserId);
        adapter.setListener(new MessageAdapter.ActionListener() {
            @Override
            public void onVoiceClick(MessageEntity message) {
                if (selectionMode) {
                    toggleSelection(message);
                    return;
                }
                toggleVoicePlayback(message);
            }

            @Override
            public void onFileClick(MessageEntity message) {
                if (selectionMode) {
                    toggleSelection(message);
                    return;
                }
                openFile(message);
            }

            @Override
            public void onImageClick(MessageEntity message) {
                if (selectionMode) {
                    toggleSelection(message);
                    return;
                }
                openImage(message);
            }

            @Override
            public void onLocationClick(MessageEntity message) {
                if (selectionMode) {
                    toggleSelection(message);
                    return;
                }
                openLocation(message);
            }

            @Override
            public void onMessageClick(MessageEntity message) {
                if (selectionMode) {
                    toggleSelection(message);
                }
            }

            @Override
            public boolean onMessageLongClick(MessageEntity message) {
                if (!selectionMode) {
                    enterSelectionMode();
                }
                toggleSelection(message);
                return true;
            }

            @Override
            public void onMessageExpired(MessageEntity message) {
                if (vm != null) {
                    vm.expireMessage(message);
                }
            }

            @Override
            public void onTransferStart(MessageEntity message) {
                if (selectionMode) {
                    toggleSelection(message);
                    return;
                }
                if (message == null) return;
                String key = getMessageKey(message);
                boolean isMe = vm != null && message.senderId != null && message.senderId.equals(vm.getSelfId());
                if (isMe) {
                    if (isImageMessage(message)) {
                        openImage(message);
                    } else {
                        openFile(message);
                    }
                    return;
                }
                if (key != null && downloadedFiles.containsKey(key)) {
                    if (isImageMessage(message)) {
                        openImage(message);
                    } else {
                        openFile(message);
                    }
                    return;
                }
                startDownload(message);
            }

            @Override
            public void onTransferCancel(MessageEntity message) {
                if (selectionMode) {
                    toggleSelection(message);
                    return;
                }
                if (message == null) return;
                boolean isMe = vm != null && message.senderId != null && message.senderId.equals(vm.getSelfId());
                String key = getMessageKey(message);
                if (isMe) {
                    if (vm != null && key != null) {
                        vm.cancelUpload(key);
                    }
                } else {
                    cancelDownload(key);
                }
            }

            @Override
            public void onChecklistToggle(MessageEntity message, String checklistId, String itemId, boolean done) {
                if (selectionMode) {
                    toggleSelection(message);
                    return;
                }
                if (message == null || checklistId == null || itemId == null) return;
                if (isChannel && (!channelSubscribed || !channelCanPost)) {
                    Toast.makeText(getContext(), binding.channelReadOnly.getText(), Toast.LENGTH_SHORT).show();
                    return;
                }
                vm.toggleChecklistItem(message, checklistId, itemId, done);
            }

            @Override
            public void onChecklistAddItem(MessageEntity message, String checklistId) {
                if (selectionMode) {
                    toggleSelection(message);
                    return;
                }
                if (message == null || checklistId == null) return;
                if (isChannel && (!channelSubscribed || !channelCanPost)) {
                    Toast.makeText(getContext(), binding.channelReadOnly.getText(), Toast.LENGTH_SHORT).show();
                    return;
                }
                openChecklistAddItemDialog(message, checklistId);
            }

            @Override
            public void onReplyClick(MessageEntity message) {
                if (selectionMode) {
                    toggleSelection(message);
                    return;
                }
                setReplyTo(message);
            }
        });
        LinearLayoutManager lm = new LinearLayoutManager(getContext());
        lm.setStackFromEnd(true);
        binding.recyclerMessages.setLayoutManager(lm);
        binding.recyclerMessages.setAdapter(adapter);
        
        // Setup swipe-to-reply (Telegram-style)
        setupSwipeToReply();

        binding.btnSend.setOnClickListener(v -> send());
        binding.btnAttach.setOnClickListener(v -> showAttachmentSheet());
        binding.btnSpoiler.setOnClickListener(v -> {
            spoilerModeEnabled = !spoilerModeEnabled;
            updateSpoilerButton();
        });
        binding.btnTtl.setOnClickListener(v -> showTtlSheet());
        binding.btnEmoji.setOnClickListener(v -> toggleEmojiPanel());
        binding.btnRecordCancel.setOnClickListener(v -> cancelAudioRecording());
        binding.btnSelectionClose.setOnClickListener(v -> exitSelectionMode());
        binding.btnSelectionForward.setOnClickListener(v -> forwardSelectedMessages());
        binding.btnSelectionDelete.setOnClickListener(v -> confirmDeleteSelectedMessages());
        binding.btnReplyClose.setOnClickListener(v -> clearReply());
        binding.etMessage.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                updateSendMicState();
                if (vm == null) return;
                if (isSaved || (isChannel && (!channelSubscribed || !channelCanPost))) {
                    vm.stopTyping();
                    return;
                }
                vm.onTypingInput(s != null ? s.toString() : "");
            }

            @Override
            public void afterTextChanged(Editable s) {}
        });
        binding.etMessage.setOnFocusChangeListener((v1, hasFocus) -> {
            if (hasFocus && emojiVisible) {
                hideEmojiPanel();
            }
            if (!hasFocus && vm != null) {
                vm.stopTyping();
            }
        });
        setupEmojiPanel();
        updateSendMicState();
        updateSpoilerButton();
        updateTtlButton();
        updateChannelUiState();

        vm.getMessages().observe(getViewLifecycleOwner(), messages -> {
            try {
                if (adapter == null || binding == null) return;
                adapter.update(messages);
                updateSelectionUi();
                binding.recyclerMessages.scrollToPosition(Math.max(0, adapter.getItemCount() - 1));
                binding.emptyState.setVisibility(adapter.getItemCount() == 0 ? View.VISIBLE : View.GONE);
                if (isGroup) {
                    loadGroupAvatars(messages);
                }
            } catch (Throwable t) {
                android.util.Log.e("ChatFragment", "Messages observer failed", t);
            }
        });

        vm.getErrors().observe(getViewLifecycleOwner(), msg -> {
            if (!TextUtils.isEmpty(msg) && getContext() != null) {
                Toast.makeText(getContext(), msg, Toast.LENGTH_SHORT).show();
            }
        });

        vm.getTypingInfo().observe(getViewLifecycleOwner(), info -> {
            String text = buildTypingText(info);
            if (TextUtils.isEmpty(text)) {
                hideTypingIndicator();
            } else {
                showTypingIndicator(text);
            }
        });

        vm.getTransferUpdates().observe(getViewLifecycleOwner(), update -> {
            try {
                if (update == null || update.messageKey == null || adapter == null) return;
                adapter.setTransferState(update.messageKey, update.state, update.progress);
                if (update.clearWhenDone && update.state == ChatViewModel.TRANSFER_DONE) {
                    adapter.clearTransferState(update.messageKey);
                }
            } catch (Throwable t) {
                android.util.Log.e("ChatFragment", "Transfer observer failed", t);
            }
        });

        try {
            vm.refreshHistory();
        } catch (Throwable t) {
            android.util.Log.e("ChatFragment", "refreshHistory failed", t);
        }
        try {
            vm.startSocket();
        } catch (Throwable t) {
            android.util.Log.e("ChatFragment", "startSocket failed", t);
        }

        loadHeaderProfile(title);
        loadSelfProfile();
        loadChannelInfo();
        loadGroupTopics();
    }

    private void loadHeaderProfile(String fallbackTitle) {
        if (chatId == null || isGroup || isChannel) {
            updateHeaderAvatar(fallbackTitle, null);
            return;
        }
        String token = authRepo != null ? authRepo.getToken() : null;
        if (token == null || token.isEmpty()) {
            updateHeaderAvatar(fallbackTitle, null);
            return;
        }
        io.execute(() -> {
            try {
                com.xipher.wrapper.data.api.ApiClient client = new com.xipher.wrapper.data.api.ApiClient(token);
                com.google.gson.JsonObject res = client.getUserProfile(token, chatId);
                if (res != null && res.has("success") && res.get("success").getAsBoolean() && res.has("user")) {
                    com.google.gson.JsonObject user = res.getAsJsonObject("user");
                    String displayName = user.has("display_name") ? user.get("display_name").getAsString() : fallbackTitle;
                    String avatarUrl = user.has("avatar_url") ? user.get("avatar_url").getAsString() : null;
                    boolean isOnline = user.has("is_online") && !user.get("is_online").isJsonNull() && user.get("is_online").getAsBoolean();
                    if (!isOnline && user.has("online") && !user.get("online").isJsonNull()) {
                        isOnline = user.get("online").getAsBoolean();
                    }
                    String lastSeen = null;
                    if (user.has("last_seen") && !user.get("last_seen").isJsonNull()) {
                        lastSeen = user.get("last_seen").getAsString();
                    } else if (user.has("lastSeen") && !user.get("lastSeen").isJsonNull()) {
                        lastSeen = user.get("lastSeen").getAsString();
                    }
                    if (getActivity() != null && !getActivity().isFinishing()) {
                        final boolean finalIsOnline = isOnline;
                        final String finalLastSeen = lastSeen;
                        getActivity().runOnUiThread(() -> {
                            if (binding == null || getContext() == null) return;
                            binding.chatTitle.setText(displayName != null ? displayName : fallbackTitle);
                            updateHeaderAvatar(displayName, avatarUrl);
                            setPeerAvatarUrl(avatarUrl);
                            updatePeerStatus(finalIsOnline, finalLastSeen);
                        });
                    }
                    return;
                }
            } catch (Exception ignored) {
            }
            if (getActivity() != null && !getActivity().isFinishing()) {
                getActivity().runOnUiThread(() -> {
                    if (binding == null || getContext() == null) return;
                    updateHeaderAvatar(fallbackTitle, null);
                    setPeerAvatarUrl(null);
                    clearPeerStatus();
                });
            }
        });
    }

    private void updatePeerStatus(boolean isOnline, String lastSeen) {
        if (binding == null || isGroup || isChannel) return;
        String statusText;
        if (isOnline) {
            statusText = getString(R.string.status_online);
        } else if (lastSeen != null && !lastSeen.isEmpty()) {
            statusText = getString(R.string.status_last_seen, lastSeen);
        } else {
            statusText = getString(R.string.status_hidden);
        }
        setBaseSubtitle(statusText, true);
    }

    private void clearPeerStatus() {
        if (binding == null) return;
        setBaseSubtitle(null, false);
    }

    private void setBaseSubtitle(String text, boolean visible) {
        baseSubtitleText = text;
        baseSubtitleVisible = visible && !TextUtils.isEmpty(text);
        if (!typingVisible) {
            applyBaseSubtitle();
        }
    }

    private void applyBaseSubtitle() {
        if (binding == null) return;
        if (baseSubtitleVisible && !TextUtils.isEmpty(baseSubtitleText)) {
            binding.chatSubtitle.setText(baseSubtitleText);
            binding.chatSubtitle.setVisibility(View.VISIBLE);
        } else {
            binding.chatSubtitle.setVisibility(View.GONE);
        }
    }

    private String buildTypingText(ChatViewModel.TypingInfo info) {
        if (info == null || info.count <= 0) return null;
        if (info.names != null && info.names.size() == 1) {
            String name = info.names.get(0);
            if (!TextUtils.isEmpty(name)) {
                return getString(R.string.typing_with_name, name);
            }
        }
        if (info.count > 1) {
            return getResources().getQuantityString(R.plurals.typing_many, info.count, info.count);
        }
        return getString(R.string.typing);
    }

    private void showTypingIndicator(String baseText) {
        if (binding == null || TextUtils.isEmpty(baseText)) {
            hideTypingIndicator();
            return;
        }
        typingVisible = true;
        typingBaseText = baseText;
        typingDotsStep = 0;
        if (typingDotsRunnable == null) {
            typingDotsRunnable = () -> {
                typingDotsStep = (typingDotsStep + 1) % 4;
                updateTypingText();
                mainHandler.postDelayed(typingDotsRunnable, TYPING_DOTS_INTERVAL_MS);
            };
        }
        updateTypingText();
        mainHandler.removeCallbacks(typingDotsRunnable);
        mainHandler.postDelayed(typingDotsRunnable, TYPING_DOTS_INTERVAL_MS);
    }

    private void hideTypingIndicator() {
        typingVisible = false;
        typingBaseText = null;
        typingDotsStep = 0;
        if (typingDotsRunnable != null) {
            mainHandler.removeCallbacks(typingDotsRunnable);
        }
        applyBaseSubtitle();
    }

    private void updateTypingText() {
        if (binding == null || TextUtils.isEmpty(typingBaseText)) return;
        binding.chatSubtitle.setText(typingBaseText + buildTypingDots(typingDotsStep));
        binding.chatSubtitle.setVisibility(View.VISIBLE);
    }

    private String buildTypingDots(int count) {
        if (count <= 0) return "";
        if (count == 1) return ".";
        if (count == 2) return "..";
        return "...";
    }

    private void updateHeaderAvatar(String displayName, String avatarUrl) {
        if (binding == null || getContext() == null) return;
        String letter = "?";
        if (displayName != null && !displayName.isEmpty()) {
            letter = displayName.substring(0, 1).toUpperCase(Locale.getDefault());
        }
        binding.chatHeaderAvatarLetter.setText(letter);
        if (avatarUrl != null && !avatarUrl.isEmpty()) {
            String url = avatarUrl.startsWith("http") ? avatarUrl : (BuildConfig.API_BASE + avatarUrl);
            binding.chatHeaderAvatarImage.setVisibility(View.VISIBLE);
            binding.chatHeaderAvatarLetter.setVisibility(View.GONE);
            try {
                com.bumptech.glide.Glide.with(requireContext())
                        .load(url)
                        .circleCrop()
                        .into(binding.chatHeaderAvatarImage);
            } catch (Exception ignored) {
                // Fragment may be detached
            }
        } else {
            binding.chatHeaderAvatarImage.setVisibility(View.GONE);
            binding.chatHeaderAvatarLetter.setVisibility(View.VISIBLE);
        }
    }

    private void loadSelfProfile() {
        if (authRepo == null) return;
        String token = authRepo.getToken();
        String userId = authRepo.getUserId();
        if (token == null || token.isEmpty() || userId == null || userId.isEmpty()) return;
        io.execute(() -> {
            try {
                com.xipher.wrapper.data.api.ApiClient client = new com.xipher.wrapper.data.api.ApiClient(token);
                com.google.gson.JsonObject res = client.getUserProfile(token, userId);
                if (res != null && res.has("success") && res.get("success").getAsBoolean() && res.has("user")) {
                    com.google.gson.JsonObject user = res.getAsJsonObject("user");
                    String avatarUrl = user.has("avatar_url") ? user.get("avatar_url").getAsString() : null;
                    if (getActivity() != null && !getActivity().isFinishing()) {
                        getActivity().runOnUiThread(() -> {
                            if (getContext() == null) return;
                            setSelfAvatarUrl(avatarUrl);
                        });
                    }
                }
            } catch (Exception ignored) {
            }
        });
    }

    private void setSelfAvatarUrl(String avatarUrl) {
        selfAvatarUrl = avatarUrl;
        updateMessageAvatars();
    }

    private void setPeerAvatarUrl(String avatarUrl) {
        peerAvatarUrl = avatarUrl;
        updateMessageAvatars();
    }

    private void updateMessageAvatars() {
        if (adapter == null) return;
        adapter.setAvatarUrls(selfAvatarUrl, peerAvatarUrl);
    }

    private void loadGroupAvatars(List<MessageEntity> messages) {
        if (!isGroup || messages == null || authRepo == null) return;
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) return;
        List<String> userIds = new ArrayList<>();
        for (MessageEntity m : messages) {
            if (m == null || m.senderId == null) continue;
            if (m.senderId.equals(vm.getSelfId())) continue;
            synchronized (avatarRequests) {
                if (avatarByUserId.containsKey(m.senderId) || avatarRequests.contains(m.senderId)) {
                    continue;
                }
                avatarRequests.add(m.senderId);
            }
            userIds.add(m.senderId);
        }
        if (userIds.isEmpty()) return;
        io.execute(() -> {
            try {
                com.xipher.wrapper.data.api.ApiClient client = new com.xipher.wrapper.data.api.ApiClient(token);
                for (String uid : userIds) {
                    try {
                        com.google.gson.JsonObject res = client.getUserProfile(token, uid);
                        String avatarUrl = null;
                        boolean loaded = false;
                        if (res != null && res.has("success") && res.get("success").getAsBoolean() && res.has("user")) {
                            com.google.gson.JsonObject user = res.getAsJsonObject("user");
                            avatarUrl = user.has("avatar_url") ? user.get("avatar_url").getAsString() : null;
                            loaded = true;
                        }
                        String finalAvatarUrl = avatarUrl;
                        boolean finalLoaded = loaded;
                        if (getActivity() != null && !getActivity().isFinishing()) {
                            getActivity().runOnUiThread(() -> {
                                if (getContext() == null) return;
                                synchronized (avatarRequests) {
                                    avatarRequests.remove(uid);
                                }
                                if (finalLoaded) {
                                    avatarByUserId.put(uid, finalAvatarUrl != null ? finalAvatarUrl : "");
                                }
                                if (adapter != null) {
                                    adapter.setAvatarMap(avatarByUserId);
                                }
                            });
                        }
                    } catch (Exception ignored) {
                        synchronized (avatarRequests) {
                            avatarRequests.remove(uid);
                        }
                    }
                }
            } catch (Exception ignored) {
            }
        });
    }

    private void enterSelectionMode() {
        selectionMode = true;
        binding.selectionBar.setVisibility(View.VISIBLE);
        binding.chatHeaderNormal.setVisibility(View.GONE);
    }

    private void exitSelectionMode() {
        selectionMode = false;
        if (adapter != null) {
            adapter.clearSelection();
        }
        binding.selectionBar.setVisibility(View.GONE);
        binding.chatHeaderNormal.setVisibility(View.VISIBLE);
    }

    private void toggleSelection(MessageEntity message) {
        if (adapter == null || message == null) return;
        adapter.toggleSelection(message);
        updateSelectionUi();
    }

    private void updateSelectionUi() {
        if (!selectionMode || adapter == null) return;
        int count = adapter.getSelectedCount();
        if (count <= 0) {
            exitSelectionMode();
            return;
        }
        binding.selectionCount.setText(getString(R.string.selected_count, count));
    }

    private void confirmDeleteSelectedMessages() {
        if (adapter == null || !adapter.hasSelection()) return;
        new AlertDialog.Builder(requireContext())
                .setTitle(R.string.delete_messages_title)
                .setMessage(R.string.delete_messages_confirm)
                .setPositiveButton(R.string.delete_action, (dialog, which) -> {
                    vm.deleteMessages(adapter.getSelectedMessages());
                    exitSelectionMode();
                })
                .setNegativeButton(R.string.cancel, null)
                .show();
    }

    private void forwardSelectedMessages() {
        if (adapter == null || !adapter.hasSelection()) return;
        List<MessageEntity> selected = new ArrayList<>(adapter.getSelectedMessages());
        if (selected.isEmpty()) return;
        Context context = getContext();
        if (context == null) return;
        io.execute(() -> {
            List<ChatEntity> chats = AppDatabase.get(context).chatDao().getAll();
            if (getActivity() == null) return;
            getActivity().runOnUiThread(() -> {
                if (chats == null || chats.isEmpty()) {
                    Toast.makeText(getContext(), R.string.no_chats_to_forward, Toast.LENGTH_SHORT).show();
                    return;
                }
                String[] titles = new String[chats.size()];
                for (int i = 0; i < chats.size(); i++) {
                    titles[i] = formatChatTitle(chats.get(i));
                }
                new AlertDialog.Builder(requireContext())
                        .setTitle(R.string.forward_to)
                        .setItems(titles, (dialog, which) -> {
                            ChatEntity target = chats.get(which);
                            vm.forwardMessages(selected, target);
                            exitSelectionMode();
                        })
                        .setNegativeButton(R.string.cancel, null)
                        .show();
            });
        });
    }

    private String formatChatTitle(ChatEntity chat) {
        if (chat == null) return "";
        String title = chat.title != null ? chat.title : "";
        if (chat.isChannel) return getString(R.string.chat_type_channel) + ": " + title;
        if (chat.isGroup) return getString(R.string.chat_type_group) + ": " + title;
        if (chat.isSaved) return getString(R.string.chat_type_saved);
        return title;
    }

    private void openProfile() {
        if (chatId == null) return;
        String userId = chatId;
        if (isSaved && authRepo != null && authRepo.getUserId() != null) {
            userId = authRepo.getUserId();
        }
        Intent intent = new Intent(requireContext(), com.xipher.wrapper.ui.ProfileActivity.class);
        intent.putExtra(com.xipher.wrapper.ui.ProfileActivity.EXTRA_USER_ID, userId);
        intent.putExtra(com.xipher.wrapper.ui.ProfileActivity.EXTRA_USERNAME, binding.chatTitle.getText() != null ? binding.chatTitle.getText().toString() : "");
        startActivity(intent);
    }

    private void openChannelInfo() {
        if (chatId == null) return;
        Intent intent = new Intent(requireContext(), com.xipher.wrapper.ui.ChannelInfoActivity.class);
        intent.putExtra(com.xipher.wrapper.ui.ChannelInfoActivity.EXTRA_CHANNEL_ID, chatId);
        intent.putExtra(com.xipher.wrapper.ui.ChannelInfoActivity.EXTRA_CHANNEL_TITLE,
                binding.chatTitle.getText() != null ? binding.chatTitle.getText().toString() : "");
        startActivity(intent);
    }

    private void openGroupInfo() {
        if (chatId == null) return;
        Intent intent = new Intent(requireContext(), com.xipher.wrapper.ui.GroupInfoActivity.class);
        intent.putExtra(com.xipher.wrapper.ui.GroupInfoActivity.EXTRA_GROUP_ID, chatId);
        intent.putExtra(com.xipher.wrapper.ui.GroupInfoActivity.EXTRA_GROUP_TITLE,
                binding.chatTitle.getText() != null ? binding.chatTitle.getText().toString() : "");
        startActivity(intent);
    }

    private void showChatOptionsMenu(View anchor) {
        PopupMenu popup = new PopupMenu(requireContext(), anchor);
        popup.getMenuInflater().inflate(R.menu.menu_chat, popup.getMenu());
        
        // Update mute item title based on current state
        // TODO: track mute state and update menu item
        
        popup.setOnMenuItemClickListener(item -> {
            int id = item.getItemId();
            if (id == R.id.menu_search) {
                // Open search in chat
                Toast.makeText(requireContext(), "Поиск в чате", Toast.LENGTH_SHORT).show();
                return true;
            } else if (id == R.id.menu_mute) {
                toggleMuteChat();
                return true;
            } else if (id == R.id.menu_clear_history) {
                showClearHistoryConfirmation();
                return true;
            } else if (id == R.id.menu_delete_chat) {
                showDeleteChatConfirmation();
                return true;
            }
            return false;
        });
        popup.show();
    }

    private void toggleMuteChat() {
        // TODO: Implement mute/unmute via API
        Toast.makeText(requireContext(), R.string.chat_muted, Toast.LENGTH_SHORT).show();
    }

    private void showClearHistoryConfirmation() {
        new AlertDialog.Builder(requireContext())
                .setMessage(R.string.confirm_clear_history)
                .setPositiveButton(android.R.string.ok, (dialog, which) -> clearChatHistory())
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private void clearChatHistory() {
        if (chatId == null) return;
        io.execute(() -> {
            try {
                AppDatabase db = AppDatabase.get(requireContext());
                db.messageDao().clearChat(chatId);
                if (getActivity() != null && !getActivity().isFinishing()) {
                    getActivity().runOnUiThread(() -> {
                        if (binding == null) return;
                        Toast.makeText(requireContext(), R.string.history_cleared, Toast.LENGTH_SHORT).show();
                    });
                }
            } catch (Exception e) {
                android.util.Log.e("ChatFragment", "Failed to clear history", e);
            }
        });
    }

    private void showDeleteChatConfirmation() {
        new AlertDialog.Builder(requireContext())
                .setMessage(R.string.confirm_delete_chat)
                .setPositiveButton(android.R.string.ok, (dialog, which) -> deleteChat())
                .setNegativeButton(android.R.string.cancel, null)
                .show();
    }

    private void deleteChat() {
        if (chatId == null) return;
        io.execute(() -> {
            try {
                AppDatabase db = AppDatabase.get(requireContext());
                db.messageDao().clearChat(chatId);
                db.chatDao().deleteById(chatId);
                if (getActivity() != null && !getActivity().isFinishing()) {
                    getActivity().runOnUiThread(() -> {
                        Toast.makeText(requireContext(), R.string.chat_deleted, Toast.LENGTH_SHORT).show();
                        requireActivity().finish();
                    });
                }
            } catch (Exception e) {
                android.util.Log.e("ChatFragment", "Failed to delete chat", e);
            }
        });
    }

    private void loadChannelInfo() {
        if (!isChannel || chatId == null || authRepo == null) return;
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) return;
        io.execute(() -> {
            try {
                com.xipher.wrapper.data.api.ApiClient client = new com.xipher.wrapper.data.api.ApiClient(token);
                com.google.gson.JsonObject res = client.getChannelInfo(token, chatId);
                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    boolean isAdmin = res.has("is_admin") && res.get("is_admin").getAsBoolean();
                    boolean isSubscribed = res.has("is_subscribed") && res.get("is_subscribed").getAsBoolean();
                    int permissions = res.has("permissions") ? res.get("permissions").getAsInt() : 0;
                    String role = res.has("user_role") ? res.get("user_role").getAsString() : "";
                    boolean hasSubscribers = res.has("subscribers_count");
                    boolean hasMembers = res.has("total_members");
                    int subscribers = hasSubscribers ? res.get("subscribers_count").getAsInt() : 0;
                    int totalMembers = hasMembers ? res.get("total_members").getAsInt() : 0;
                    int count = subscribers > 0 ? subscribers : totalMembers;
                    boolean hasCount = hasSubscribers || hasMembers;
                    boolean canPost = isAdmin || "owner".equalsIgnoreCase(role) || "admin".equalsIgnoreCase(role)
                            || (permissions & 0x2) == 0x2;
                    if (getActivity() != null && !getActivity().isFinishing()) {
                        getActivity().runOnUiThread(() -> {
                            if (binding == null || getContext() == null) return;
                            applyChannelPermissions(canPost, isSubscribed);
                            updateChannelSubtitle(count, hasCount);
                        });
                    }
                }
            } catch (Exception ignored) {
            }
        });
    }

    /**
     * Load forum topics for group chats (Telegram-style)
     */
    private void loadGroupTopics() {
        if (!isGroup || chatId == null || authRepo == null) return;
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) return;
        
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.getGroupTopics(token, chatId);
                
                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    boolean forumMode = res.has("forum_mode") && res.get("forum_mode").getAsBoolean();
                    String role = res.has("user_role") ? res.get("user_role").getAsString() : "member";
                    
                    List<TopicDto> topics = new ArrayList<>();
                    if (res.has("topics") && res.get("topics").isJsonArray()) {
                        JsonArray topicsArray = res.getAsJsonArray("topics");
                        for (int i = 0; i < topicsArray.size(); i++) {
                            TopicDto topic = topicGson.fromJson(topicsArray.get(i), TopicDto.class);
                            if (topic != null) {
                                topics.add(topic);
                            }
                        }
                    }
                    
                    if (getActivity() != null && !getActivity().isFinishing()) {
                        getActivity().runOnUiThread(() -> {
                            if (binding == null || getContext() == null) return;
                            isForumMode = forumMode;
                            groupUserRole = role;
                            currentTopics = topics;
                            
                            if (forumMode && !topics.isEmpty()) {
                                showTopicTabs(topics, role);
                            } else {
                                hideTopicTabs();
                            }
                        });
                    }
                }
            } catch (Exception e) {
                android.util.Log.e("ChatFragment", "Failed to load group topics", e);
            }
        });
    }
    
    /**
     * Show horizontal topic tabs above messages
     */
    private void showTopicTabs(List<TopicDto> topics, String userRole) {
        if (binding == null || topics == null || topics.isEmpty()) return;
        
        // Show the tabs container
        binding.topicTabsScroll.setVisibility(View.VISIBLE);
        
        // Clear existing tabs (except add button)
        LinearLayout container = binding.topicTabsContainer;
        for (int i = container.getChildCount() - 1; i >= 0; i--) {
            View child = container.getChildAt(i);
            if (child.getId() != R.id.topicAddButton) {
                container.removeViewAt(i);
            }
        }
        
        // Sort topics: General first, then pinned, then by last message
        List<TopicDto> sortedTopics = new ArrayList<>(topics);
        sortedTopics.sort((a, b) -> {
            if (a.is_general) return -1;
            if (b.is_general) return 1;
            if (a.pinned_order > 0 && b.pinned_order == 0) return -1;
            if (a.pinned_order == 0 && b.pinned_order > 0) return 1;
            if (a.last_message_at != null && b.last_message_at != null) {
                return b.last_message_at.compareTo(a.last_message_at);
            }
            return 0;
        });
        
        // Add topic tabs
        int addButtonIndex = container.indexOfChild(binding.topicAddButton);
        if (addButtonIndex < 0) addButtonIndex = container.getChildCount();
        
        for (int i = 0; i < sortedTopics.size(); i++) {
            TopicDto topic = sortedTopics.get(i);
            if (topic.is_hidden) continue;
            
            View tabView = createTopicTabView(topic);
            container.addView(tabView, i);
        }
        
        // Show/hide add button based on role
        boolean canCreate = "creator".equals(userRole) || "admin".equals(userRole);
        binding.topicAddButton.setVisibility(canCreate ? View.VISIBLE : View.GONE);
        binding.topicAddButton.setOnClickListener(v -> showCreateTopicDialog());
    }
    
    /**
     * Create a single topic tab view
     */
    private View createTopicTabView(TopicDto topic) {
        View tabView = LayoutInflater.from(getContext()).inflate(R.layout.item_topic_tab, binding.topicTabsContainer, false);
        
        LinearLayout content = tabView.findViewById(R.id.topicTabContent);
        TextView iconView = tabView.findViewById(R.id.topicIcon);
        TextView nameView = tabView.findViewById(R.id.topicName);
        TextView badgeView = tabView.findViewById(R.id.topicBadge);
        
        // Set emoji icon
        iconView.setText(topic.getIconEmoji());
        
        // Set topic name
        nameView.setText(topic.name);
        
        // Set unread badge
        if (topic.unread_count > 0) {
            badgeView.setVisibility(View.VISIBLE);
            badgeView.setText(topic.unread_count > 99 ? "99+" : String.valueOf(topic.unread_count));
        } else {
            badgeView.setVisibility(View.GONE);
        }
        
        // Set icon background color
        android.graphics.drawable.GradientDrawable iconBg = new android.graphics.drawable.GradientDrawable();
        iconBg.setShape(android.graphics.drawable.GradientDrawable.OVAL);
        try {
            iconBg.setColor(android.graphics.Color.parseColor(topic.getIconColor()));
        } catch (Exception e) {
            iconBg.setColor(0xFF6FB1FC);
        }
        iconView.setBackground(iconBg);
        
        // Initial state (not selected)
        boolean isSelected = currentTopic != null && currentTopic.id != null && currentTopic.id.equals(topic.id);
        updateTopicTabState(content, nameView, isSelected);
        
        // Store topic id in tag
        tabView.setTag(topic.id);
        
        // Click listener
        content.setOnClickListener(v -> selectTopic(topic));
        
        return tabView;
    }
    
    /**
     * Update visual state of a topic tab
     */
    private void updateTopicTabState(LinearLayout content, TextView nameView, boolean isSelected) {
        content.setSelected(isSelected);
        
        android.graphics.drawable.GradientDrawable bg = new android.graphics.drawable.GradientDrawable();
        bg.setShape(android.graphics.drawable.GradientDrawable.RECTANGLE);
        bg.setCornerRadius(40f);
        
        if (isSelected) {
            nameView.setTextColor(android.graphics.Color.WHITE);
            bg.setColor(0xFF229ED9); // Telegram blue
        } else {
            try {
                nameView.setTextColor(getResources().getColor(R.color.x_text_secondary, null));
                bg.setColor(getResources().getColor(R.color.x_bg_secondary, null));
            } catch (Exception e) {
                nameView.setTextColor(0xFFAAAAAA);
                bg.setColor(0xFF2A2A2A);
            }
        }
        
        content.setBackground(bg);
    }
    
    /**
     * Select a topic and load its messages
     */
    private void selectTopic(TopicDto topic) {
        TopicDto oldTopic = currentTopic;
        currentTopic = topic;
        
        // Update all tabs
        LinearLayout container = binding.topicTabsContainer;
        for (int i = 0; i < container.getChildCount(); i++) {
            View child = container.getChildAt(i);
            if (child.getId() == R.id.topicAddButton) continue;
            
            String tabTopicId = (String) child.getTag();
            LinearLayout content = child.findViewById(R.id.topicTabContent);
            TextView nameView = child.findViewById(R.id.topicName);
            
            boolean isSelected = tabTopicId != null && tabTopicId.equals(topic.id);
            updateTopicTabState(content, nameView, isSelected);
        }
        
        // Load topic messages
        loadTopicMessages(topic.id);
        
        // Update header to show topic name
        if (binding != null) {
            String title = binding.chatTitle.getText().toString();
            setBaseSubtitle(topic.getIconEmoji() + " " + topic.name, true);
        }
    }
    
    /**
     * Load messages for a specific topic
     */
    private void loadTopicMessages(String topicId) {
        if (topicId == null || authRepo == null) return;
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) return;
        
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.getTopicMessages(token, topicId, 50);
                
                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    List<MessageEntity> messages = new ArrayList<>();
                    
                    if (res.has("messages") && res.get("messages").isJsonArray()) {
                        JsonArray messagesArray = res.getAsJsonArray("messages");
                        for (int i = 0; i < messagesArray.size(); i++) {
                            JsonObject msgObj = messagesArray.get(i).getAsJsonObject();
                            MessageEntity msg = parseMessageFromJson(msgObj);
                            if (msg != null) {
                                messages.add(msg);
                            }
                        }
                    }
                    
                    if (getActivity() != null && !getActivity().isFinishing()) {
                        getActivity().runOnUiThread(() -> {
                            if (binding == null || adapter == null) return;
                            adapter.update(messages);
                            binding.recyclerMessages.scrollToPosition(Math.max(0, messages.size() - 1));
                            binding.emptyState.setVisibility(messages.isEmpty() ? View.VISIBLE : View.GONE);
                        });
                    }
                }
            } catch (Exception e) {
                android.util.Log.e("ChatFragment", "Failed to load topic messages", e);
            }
        });
    }
    
    /**
     * Parse a message from JSON response
     */
    private MessageEntity parseMessageFromJson(JsonObject obj) {
        try {
            MessageEntity msg = new MessageEntity();
            msg.id = obj.has("id") ? obj.get("id").getAsString() : UUID.randomUUID().toString();
            msg.senderId = obj.has("sender_id") ? obj.get("sender_id").getAsString() : null;
            msg.senderName = obj.has("sender_name") ? obj.get("sender_name").getAsString() : null;
            msg.content = obj.has("content") ? obj.get("content").getAsString() : "";
            msg.messageType = obj.has("message_type") ? obj.get("message_type").getAsString() : "text";
            msg.filePath = obj.has("file_path") ? obj.get("file_path").getAsString() : null;
            msg.fileName = obj.has("file_name") ? obj.get("file_name").getAsString() : null;
            msg.createdAt = obj.has("created_at") ? obj.get("created_at").getAsString() : "";
            return msg;
        } catch (Exception e) {
            return null;
        }
    }
    
    /**
     * Hide topic tabs
     */
    private void hideTopicTabs() {
        if (binding != null && binding.topicTabsScroll != null) {
            binding.topicTabsScroll.setVisibility(View.GONE);
        }
        currentTopic = null;
    }
    
    /**
     * Show dialog to create a new topic
     */
    private void showCreateTopicDialog() {
        if (getContext() == null) return;
        
        AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
        builder.setTitle(R.string.create_topic_title);
        
        LinearLayout layout = new LinearLayout(getContext());
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setPadding(48, 32, 48, 16);
        
        android.widget.EditText nameInput = new android.widget.EditText(getContext());
        nameInput.setHint(R.string.topic_name_hint);
        nameInput.setSingleLine(true);
        layout.addView(nameInput);
        
        builder.setView(layout);
        
        builder.setPositiveButton(R.string.create_title, (dialog, which) -> {
            String name = nameInput.getText().toString().trim();
            if (!name.isEmpty()) {
                createTopic(name, "💬", TopicDto.TOPIC_COLORS[0]);
            }
        });
        
        builder.setNegativeButton(R.string.cancel, null);
        builder.show();
    }
    
    /**
     * Create a new topic
     */
    private void createTopic(String name, String emoji, String color) {
        if (chatId == null || authRepo == null) return;
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) return;
        
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.createGroupTopic(token, chatId, name, emoji, color, false);
                
                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    if (getActivity() != null && !getActivity().isFinishing()) {
                        getActivity().runOnUiThread(() -> {
                            Toast.makeText(getContext(), R.string.topic_created, Toast.LENGTH_SHORT).show();
                            // Reload topics
                            loadGroupTopics();
                        });
                    }
                } else {
                    String error = res != null && res.has("error") ? res.get("error").getAsString() : "Error";
                    if (getActivity() != null && !getActivity().isFinishing()) {
                        getActivity().runOnUiThread(() -> {
                            Toast.makeText(getContext(), error, Toast.LENGTH_SHORT).show();
                        });
                    }
                }
            } catch (Exception e) {
                android.util.Log.e("ChatFragment", "Failed to create topic", e);
                if (getActivity() != null && !getActivity().isFinishing()) {
                    getActivity().runOnUiThread(() -> {
                        Toast.makeText(getContext(), R.string.topic_error_create, Toast.LENGTH_SHORT).show();
                    });
                }
            }
        });
    }

    private void updateChannelSubtitle(int subscribers, boolean hasCount) {
        if (!isChannel || binding == null) return;
        if (!hasCount) {
            setBaseSubtitle(null, false);
            return;
        }
        String text = getResources().getQuantityString(R.plurals.channel_subscribers, subscribers, subscribers);
        setBaseSubtitle(text, true);
    }

    private void applyChannelPermissions(boolean canPost, boolean subscribed) {
        channelCanPost = canPost;
        channelSubscribed = subscribed;
        updateChannelUiState();
    }

    private void updateChannelUiState() {
        if (!isChannel || binding == null) return;
        if (!channelSubscribed) {
            binding.channelReadOnly.setText(getString(R.string.channel_not_subscribed));
            binding.channelReadOnly.setVisibility(View.VISIBLE);
            binding.inputBar.setVisibility(View.GONE);
            binding.emojiPanel.setVisibility(View.GONE);
            binding.recordingBar.setVisibility(View.GONE);
            if (vm != null) {
                vm.stopTyping();
            }
            return;
        }
        if (!channelCanPost) {
            binding.channelReadOnly.setText(getString(R.string.channel_read_only));
            binding.channelReadOnly.setVisibility(View.VISIBLE);
            binding.inputBar.setVisibility(View.GONE);
            binding.emojiPanel.setVisibility(View.GONE);
            binding.recordingBar.setVisibility(View.GONE);
            if (vm != null) {
                vm.stopTyping();
            }
            return;
        }
        binding.channelReadOnly.setVisibility(View.GONE);
        binding.inputBar.setVisibility(View.VISIBLE);
        updateSendMicState();
    }

    private void send() {
        if (isChannel && (!channelSubscribed || !channelCanPost)) {
            Toast.makeText(getContext(), binding.channelReadOnly.getText(), Toast.LENGTH_SHORT).show();
            return;
        }
        String text = binding.etMessage.getText().toString().trim();
        if (text.isEmpty()) return;
        binding.etMessage.setText("");
        
        // Get reply ID if replying
        String replyToId = replyToMessage != null ? replyToMessage.id : null;
        
        // If in forum mode and topic is selected, send to topic
        if (isForumMode && currentTopic != null && currentTopic.id != null) {
            sendTopicMessage(currentTopic.id, text);
        } else {
            vm.sendMessage(text, spoilerModeEnabled, selectedTtlSeconds, replyToId);
        }
        
        // Clear reply after sending
        clearReply();
        
        vm.stopTyping();
        hideEmojiPanel();
        updateSendMicState();
    }
    
    /**
     * Send a message to a topic
     */
    private void sendTopicMessage(String topicId, String content) {
        if (topicId == null || authRepo == null) return;
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) return;
        
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.sendTopicMessage(token, topicId, content, "text");
                
                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    // Reload topic messages
                    loadTopicMessages(topicId);
                } else {
                    String error = res != null && res.has("error") ? res.get("error").getAsString() : "Error";
                    if (getActivity() != null && !getActivity().isFinishing()) {
                        getActivity().runOnUiThread(() -> {
                            Toast.makeText(getContext(), error, Toast.LENGTH_SHORT).show();
                        });
                    }
                }
            } catch (Exception e) {
                android.util.Log.e("ChatFragment", "Failed to send topic message", e);
            }
        });
    }

    @Override
    public void onResume() {
        super.onResume();
        if (binding != null) {
            ChatThemeManager.applyChatBackground(
                binding.messagesContainer, 
                binding.chatBackgroundImage, 
                binding.chatBackgroundDim, 
                requireContext()
            );
        }
        startAutoRefresh();
        startTtlPrune();
    }

    @Override
    public void onPause() {
        super.onPause();
        if (vm != null) {
            vm.stopTyping();
        }
        stopAutoRefresh();
        stopTtlPrune();
        stopAudioRecording(false);
        stopVideoRecording(false);
        hideVideoPreview(false);
        recordingLocked = false;
        updateRecordingHints();
        if (cameraRecorder != null) {
            cameraRecorder.unbind();
        }
        if (mediaButton != null) mediaButton.resetState();
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        stopAutoRefresh();
        stopTtlPrune();
        hideTypingIndicator();
        stopAudioRecording(false);
        stopVideoRecording(false);
        stopPlayback();
        if (vm != null) {
            vm.stopTyping();
            vm.stopSocket();
        }
        if (cameraRecorder != null) {
            cameraRecorder.release();
        }
        io.shutdownNow();
    }

    private void pickFile() {
        if (isChannel && (!channelSubscribed || !channelCanPost)) {
            Toast.makeText(getContext(), binding.channelReadOnly.getText(), Toast.LENGTH_SHORT).show();
            return;
        }
        hideEmojiPanel();
        if (filePicker != null) {
            filePicker.launch(new String[]{"*/*"});
        }
    }

    private void pickImage() {
        if (isChannel && (!channelSubscribed || !channelCanPost)) {
            Toast.makeText(getContext(), binding.channelReadOnly.getText(), Toast.LENGTH_SHORT).show();
            return;
        }
        hideEmojiPanel();
        if (imagePicker != null) {
            imagePicker.launch(new String[]{"image/*"});
        }
    }

    private void showAttachmentSheet() {
        if (getContext() == null) return;
        BottomSheetDialog sheet = new BottomSheetDialog(getContext());
        View view = getLayoutInflater().inflate(R.layout.bottom_sheet_attachments, null);
        View gallery = view.findViewById(R.id.attachGallery);
        View files = view.findViewById(R.id.attachFiles);
        View location = view.findViewById(R.id.attachLocation);
        View checklist = view.findViewById(R.id.attachChecklist);
        View giftRow = view.findViewById(R.id.attachGiftRow);
        View premiumGift = view.findViewById(R.id.attachPremiumGift);
        gallery.setOnClickListener(v -> {
            sheet.dismiss();
            pickImage();
        });
        files.setOnClickListener(v -> {
            sheet.dismiss();
            pickFile();
        });
        location.setOnClickListener(v -> {
            sheet.dismiss();
            openLocationShare();
        });
        checklist.setOnClickListener(v -> {
            sheet.dismiss();
            openChecklistComposer();
        });
        if (premiumGift != null) {
            boolean canGift = canGiftPremium();
            if (giftRow != null) {
                giftRow.setVisibility(canGift ? View.VISIBLE : View.GONE);
            }
            premiumGift.setVisibility(canGift ? View.VISIBLE : View.GONE);
            premiumGift.setOnClickListener(v -> {
                sheet.dismiss();
                openPremiumGiftSheet();
            });
        }
        sheet.setContentView(view);
        sheet.show();
    }

    private boolean canGiftPremium() {
        return chatId != null && !isGroup && !isChannel && !isSaved;
    }

    private void openPremiumGiftSheet() {
        if (getContext() == null) return;
        if (!canGiftPremium()) {
            Toast.makeText(getContext(), R.string.premium_gift_error_unavailable, Toast.LENGTH_SHORT).show();
            return;
        }
        BottomSheetDialog sheet = new BottomSheetDialog(getContext());
        View view = getLayoutInflater().inflate(R.layout.bottom_sheet_premium_gift, null);

        TextView recipientValue = view.findViewById(R.id.premiumGiftRecipientValue);
        String displayName = binding != null && binding.chatTitle.getText() != null
                ? binding.chatTitle.getText().toString()
                : "";
        if (displayName == null || displayName.isEmpty()) {
            displayName = chatId != null ? chatId : "";
        }
        if (recipientValue != null) {
            recipientValue.setText(displayName);
        }

        View planMonth = view.findViewById(R.id.premiumGiftPlanMonth);
        View planHalf = view.findViewById(R.id.premiumGiftPlanHalf);
        View planYear = view.findViewById(R.id.premiumGiftPlanYear);
        selectedGiftPlan = GIFT_PLAN_HALF;
        updateGiftPlanSelection(planMonth, planHalf, planYear);
        if (planMonth != null) {
            planMonth.setOnClickListener(v -> {
                selectedGiftPlan = GIFT_PLAN_MONTH;
                updateGiftPlanSelection(planMonth, planHalf, planYear);
            });
        }
        if (planHalf != null) {
            planHalf.setOnClickListener(v -> {
                selectedGiftPlan = GIFT_PLAN_HALF;
                updateGiftPlanSelection(planMonth, planHalf, planYear);
            });
        }
        if (planYear != null) {
            planYear.setOnClickListener(v -> {
                selectedGiftPlan = GIFT_PLAN_YEAR;
                updateGiftPlanSelection(planMonth, planHalf, planYear);
            });
        }

        RadioGroup providerGroup = view.findViewById(R.id.premiumGiftPaymentProviderGroup);
        RadioButton providerYooMoney = view.findViewById(R.id.premiumGiftPaymentProviderYooMoney);
        RadioButton providerStripe = view.findViewById(R.id.premiumGiftPaymentProviderStripe);
        RadioGroup methodGroup = view.findViewById(R.id.premiumGiftPaymentMethodGroup);
        RadioButton methodCard = view.findViewById(R.id.premiumGiftPaymentCard);

        if (providerYooMoney != null) {
            providerYooMoney.setChecked(true);
        } else if (providerStripe != null) {
            providerStripe.setChecked(true);
        }
        if (methodCard != null) {
            methodCard.setChecked(true);
        }
        if (providerGroup != null) {
            providerGroup.setOnCheckedChangeListener((group, checkedId) ->
                    updateGiftPaymentMethodVisibility(methodGroup, checkedId));
            updateGiftPaymentMethodVisibility(methodGroup, providerGroup.getCheckedRadioButtonId());
        } else {
            updateGiftPaymentMethodVisibility(methodGroup, View.NO_ID);
        }

        Button payButton = view.findViewById(R.id.premiumGiftPayButton);
        if (payButton != null) {
            payButton.setOnClickListener(v -> {
                String provider = resolveGiftProvider(providerGroup);
                String paymentType = resolveGiftPaymentType(methodGroup);
                startPremiumGiftPayment(sheet, payButton, provider, paymentType);
            });
        }

        sheet.setContentView(view);
        sheet.show();
    }

    private void updateGiftPlanSelection(View planMonth, View planHalf, View planYear) {
        if (planMonth != null) {
            planMonth.setActivated(GIFT_PLAN_MONTH.equals(selectedGiftPlan));
        }
        if (planHalf != null) {
            planHalf.setActivated(GIFT_PLAN_HALF.equals(selectedGiftPlan));
        }
        if (planYear != null) {
            planYear.setActivated(GIFT_PLAN_YEAR.equals(selectedGiftPlan));
        }
    }

    private void updateGiftPaymentMethodVisibility(View methodGroup, int providerId) {
        if (methodGroup == null) return;
        boolean stripe = providerId == R.id.premiumGiftPaymentProviderStripe;
        methodGroup.setVisibility(stripe ? View.GONE : View.VISIBLE);
    }

    private String resolveGiftProvider(RadioGroup providerGroup) {
        if (providerGroup != null
                && providerGroup.getCheckedRadioButtonId() == R.id.premiumGiftPaymentProviderStripe) {
            return "stripe";
        }
        return "yoomoney";
    }

    private String resolveGiftPaymentType(RadioGroup methodGroup) {
        if (methodGroup != null
                && methodGroup.getCheckedRadioButtonId() == R.id.premiumGiftPaymentWallet) {
            return "PC";
        }
        return "AC";
    }

    private String normalizeGiftPlan(String plan) {
        if (GIFT_PLAN_MONTH.equals(plan) || GIFT_PLAN_HALF.equals(plan) || GIFT_PLAN_YEAR.equals(plan)) {
            return plan;
        }
        return GIFT_PLAN_HALF;
    }

    private void startPremiumGiftPayment(BottomSheetDialog sheet, Button payButton,
                                         String provider, String paymentType) {
        if (authRepo == null) {
            Toast.makeText(getContext(), R.string.auth_missing_token, Toast.LENGTH_SHORT).show();
            return;
        }
        String token = authRepo.getToken();
        if (token == null || token.isEmpty()) {
            Toast.makeText(getContext(), R.string.auth_missing_token, Toast.LENGTH_SHORT).show();
            return;
        }
        String receiverId = chatId;
        if (receiverId == null || receiverId.isEmpty()) {
            Toast.makeText(getContext(), R.string.premium_gift_error_unavailable, Toast.LENGTH_SHORT).show();
            return;
        }
        String senderId = authRepo.getUserId();
        if (senderId != null && senderId.equals(receiverId)) {
            Toast.makeText(getContext(), R.string.premium_gift_error_self, Toast.LENGTH_SHORT).show();
            return;
        }
        String plan = normalizeGiftPlan(selectedGiftPlan);
        if (payButton != null) {
            payButton.setEnabled(false);
        }
        io.execute(() -> {
            GiftPaymentData payment = null;
            String errorMessage = null;
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.createPremiumGiftPayment(token, plan, provider, receiverId);
                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    JsonObject data = res.has("data") && res.get("data").isJsonObject()
                            ? res.getAsJsonObject("data")
                            : null;
                    if (data != null) {
                        String parsedProvider = readJsonString(data, "provider");
                        if (parsedProvider.isEmpty()) {
                            parsedProvider = provider;
                        }
                        payment = new GiftPaymentData(
                                readJsonString(data, "form_action"),
                                readJsonString(data, "receiver"),
                                readJsonString(data, "label"),
                                readJsonString(data, "sum"),
                                readJsonString(data, "success_url"),
                                parsedProvider,
                                readJsonString(data, "checkout_url")
                        );
                    }
                } else if (res != null && res.has("message")) {
                    errorMessage = res.get("message").getAsString();
                }
            } catch (Exception e) {
                errorMessage = e.getMessage();
            }
            GiftPaymentData finalPayment = payment;
            String finalError = errorMessage;
            mainHandler.post(() -> {
                if (payButton != null) {
                    payButton.setEnabled(true);
                }
                if (finalPayment == null) {
                    Toast.makeText(getContext(),
                            finalError != null && !finalError.isEmpty()
                                    ? finalError
                                    : getString(R.string.premium_payment_toast_error),
                            Toast.LENGTH_SHORT).show();
                    return;
                }
                if (sheet != null) {
                    sheet.dismiss();
                }
                openPremiumGiftPayment(finalPayment, paymentType);
            });
        });
    }

    private void openPremiumGiftPayment(GiftPaymentData payment, String paymentType) {
        if (getContext() == null) return;
        String provider = payment.provider != null ? payment.provider : "";
        if ("stripe".equalsIgnoreCase(provider)) {
            if (payment.checkoutUrl == null || payment.checkoutUrl.isEmpty()) {
                Toast.makeText(getContext(), R.string.premium_payment_toast_error, Toast.LENGTH_SHORT).show();
                return;
            }
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(payment.checkoutUrl));
            startActivity(intent);
            Toast.makeText(getContext(), R.string.premium_gift_payment_toast_opened, Toast.LENGTH_SHORT).show();
            return;
        }
        if (payment.label == null || payment.label.isEmpty()
                || payment.receiver == null || payment.receiver.isEmpty()) {
            Toast.makeText(getContext(), R.string.premium_payment_toast_error, Toast.LENGTH_SHORT).show();
            return;
        }
        Intent intent = new Intent(getContext(), PremiumPaymentActivity.class);
        intent.putExtra(PremiumPaymentActivity.EXTRA_FORM_ACTION, payment.formAction);
        intent.putExtra(PremiumPaymentActivity.EXTRA_RECEIVER, payment.receiver);
        intent.putExtra(PremiumPaymentActivity.EXTRA_LABEL, payment.label);
        intent.putExtra(PremiumPaymentActivity.EXTRA_SUM, payment.sum);
        intent.putExtra(PremiumPaymentActivity.EXTRA_SUCCESS_URL, payment.successUrl);
        intent.putExtra(PremiumPaymentActivity.EXTRA_PAYMENT_TYPE, paymentType);
        if (premiumGiftPaymentLauncher != null) {
            premiumGiftPaymentLauncher.launch(intent);
        } else {
            startActivity(intent);
        }
        Toast.makeText(getContext(), R.string.premium_gift_payment_toast_opened, Toast.LENGTH_SHORT).show();
    }

    private String readJsonString(JsonObject data, String key) {
        if (data == null || key == null) return "";
        if (!data.has(key) || data.get(key).isJsonNull()) return "";
        try {
            return data.get(key).getAsString();
        } catch (Exception ignored) {
            return "";
        }
    }

    private void showTtlSheet() {
        if (getContext() == null) return;
        BottomSheetDialog sheet = new BottomSheetDialog(getContext());
        View view = getLayoutInflater().inflate(R.layout.bottom_sheet_message_ttl, null);
        View optionOff = view.findViewById(R.id.ttlOptionOff);
        View option10s = view.findViewById(R.id.ttlOption10s);
        View option1h = view.findViewById(R.id.ttlOption1h);
        View option1d = view.findViewById(R.id.ttlOption1d);

        optionOff.setOnClickListener(v -> {
            selectedTtlSeconds = 0L;
            updateTtlButton();
            sheet.dismiss();
        });
        option10s.setOnClickListener(v -> {
            selectedTtlSeconds = 10L;
            updateTtlButton();
            sheet.dismiss();
        });
        option1h.setOnClickListener(v -> {
            selectedTtlSeconds = 3600L;
            updateTtlButton();
            sheet.dismiss();
        });
        option1d.setOnClickListener(v -> {
            selectedTtlSeconds = 86400L;
            updateTtlButton();
            sheet.dismiss();
        });

        sheet.setContentView(view);
        sheet.show();
    }

    private void openLocationShare() {
        if (chatId == null) return;
        if (isChannel && (!channelSubscribed || !channelCanPost)) {
            Toast.makeText(getContext(), binding.channelReadOnly.getText(), Toast.LENGTH_SHORT).show();
            return;
        }
        Intent intent = new Intent(requireContext(), com.xipher.wrapper.ui.LocationShareActivity.class);
        intent.putExtra(com.xipher.wrapper.ui.LocationShareActivity.EXTRA_CHAT_ID, chatId);
        intent.putExtra(com.xipher.wrapper.ui.LocationShareActivity.EXTRA_IS_GROUP, isGroup);
        intent.putExtra(com.xipher.wrapper.ui.LocationShareActivity.EXTRA_IS_CHANNEL, isChannel);
        if (locationShareLauncher != null) {
            locationShareLauncher.launch(intent);
        } else {
            startActivity(intent);
        }
    }

    private void openChecklistComposer() {
        if (getContext() == null) return;
        if (isChannel && (!channelSubscribed || !channelCanPost)) {
            Toast.makeText(getContext(), binding.channelReadOnly.getText(), Toast.LENGTH_SHORT).show();
            return;
        }
        hideEmojiPanel();
        BottomSheetDialog sheet = new BottomSheetDialog(getContext());
        View view = getLayoutInflater().inflate(R.layout.bottom_sheet_checklist, null);
        android.widget.EditText titleInput = view.findViewById(R.id.checklistTitleInput);
        LinearLayout itemsContainer = view.findViewById(R.id.checklistItemsContainer);
        TextView addItem = view.findViewById(R.id.checklistAddItem);
        SwitchMaterial allowAdd = view.findViewById(R.id.checklistAllowAdd);
        SwitchMaterial allowCheck = view.findViewById(R.id.checklistAllowCheck);
        android.widget.Button sendButton = view.findViewById(R.id.checklistSendButton);

        addChecklistEditorRow(itemsContainer, null);
        addChecklistEditorRow(itemsContainer, null);

        addItem.setOnClickListener(v -> addChecklistEditorRow(itemsContainer, null));
        sendButton.setOnClickListener(v -> {
            String title = titleInput.getText() != null ? titleInput.getText().toString().trim() : "";
            if (title.isEmpty()) {
                Toast.makeText(getContext(), R.string.checklist_error_title, Toast.LENGTH_SHORT).show();
                return;
            }
            List<String> texts = collectChecklistItems(itemsContainer);
            if (texts.isEmpty()) {
                Toast.makeText(getContext(), R.string.checklist_error_items, Toast.LENGTH_SHORT).show();
                return;
            }
            ChecklistPayload payload = new ChecklistPayload();
            payload.id = UUID.randomUUID().toString();
            payload.title = title;
            payload.othersCanAdd = allowAdd.isChecked();
            payload.othersCanMark = allowCheck.isChecked();
            payload.items = new ArrayList<>();
            for (String text : texts) {
                payload.items.add(new ChecklistItem(UUID.randomUUID().toString(), text, false));
            }
            vm.sendChecklist(payload, selectedTtlSeconds);
            sheet.dismiss();
        });

        sheet.setContentView(view);
        sheet.show();
    }

    private void addChecklistEditorRow(LinearLayout container, @Nullable String text) {
        if (container == null) return;
        View row = getLayoutInflater().inflate(R.layout.item_checklist_editor_row, container, false);
        android.widget.EditText input = row.findViewById(R.id.checklistItemText);
        android.widget.ImageButton remove = row.findViewById(R.id.checklistItemRemove);
        if (text != null) {
            input.setText(text);
        }
        remove.setOnClickListener(v -> container.removeView(row));
        container.addView(row);
    }

    private List<String> collectChecklistItems(LinearLayout container) {
        List<String> items = new ArrayList<>();
        if (container == null) return items;
        for (int i = 0; i < container.getChildCount(); i++) {
            View row = container.getChildAt(i);
            if (row == null) continue;
            android.widget.EditText input = row.findViewById(R.id.checklistItemText);
            if (input == null || input.getText() == null) continue;
            String text = input.getText().toString().trim();
            if (!text.isEmpty()) {
                items.add(text);
            }
        }
        return items;
    }

    private void openChecklistAddItemDialog(MessageEntity message, String checklistId) {
        if (getContext() == null) return;
        android.widget.EditText input = new android.widget.EditText(getContext());
        input.setHint(R.string.checklist_add_item_hint);
        input.setSingleLine(true);
        new AlertDialog.Builder(requireContext())
                .setTitle(R.string.checklist_add_item_title)
                .setView(input)
                .setPositiveButton(R.string.checklist_add_action, (dialog, which) -> {
                    String text = input.getText() != null ? input.getText().toString().trim() : "";
                    if (text.isEmpty()) {
                        Toast.makeText(getContext(), R.string.checklist_error_item_empty, Toast.LENGTH_SHORT).show();
                        return;
                    }
                    vm.addChecklistItem(message, checklistId, text);
                })
                .setNegativeButton(R.string.cancel, null)
                .show();
    }

    private void handlePickedFile(Uri uri) {
        handlePickedUri(uri, false);
    }

    private void handlePickedImage(Uri uri) {
        handlePickedUri(uri, spoilerModeEnabled);
    }

    private void handlePickedUri(Uri uri, boolean spoiler) {
        if (uri == null) return;
        try {
            requireContext().getContentResolver().takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (Exception ignored) {
        }
        long size = -1;
        String name = "file";
        Cursor cursor = null;
        try {
            cursor = requireContext().getContentResolver().query(uri, null, null, null, null);
            if (cursor != null && cursor.moveToFirst()) {
                int nameIdx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                int sizeIdx = cursor.getColumnIndex(OpenableColumns.SIZE);
                if (nameIdx >= 0) name = cursor.getString(nameIdx);
                if (sizeIdx >= 0) size = cursor.getLong(sizeIdx);
            }
        } catch (Exception ignored) {
        } finally {
            if (cursor != null) cursor.close();
        }
        if (size > MAX_FILE_BYTES) {
            Toast.makeText(getContext(), R.string.file_too_large, Toast.LENGTH_SHORT).show();
            return;
        }
        vm.sendFile(uri, name, size, spoiler, selectedTtlSeconds);
    }

    private void requestAudioAndStartRecording() {
        if (isChannel && (!channelSubscribed || !channelCanPost)) {
            Toast.makeText(getContext(), binding.channelReadOnly.getText(), Toast.LENGTH_SHORT).show();
            if (mediaButton != null) mediaButton.resetState();
            return;
        }
        if (ContextCompat.checkSelfPermission(requireContext(), Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            startAudioRecording();
            return;
        }
        if (audioPermission != null) {
            audioPermission.launch(Manifest.permission.RECORD_AUDIO);
        }
        if (mediaButton != null) mediaButton.resetState();
    }

    private boolean hasVideoPermissions() {
        return ContextCompat.checkSelfPermission(requireContext(), Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED
                && ContextCompat.checkSelfPermission(requireContext(), Manifest.permission.RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED;
    }

    private void requestVideoAndStartRecording() {
        if (isChannel && (!channelSubscribed || !channelCanPost)) {
            Toast.makeText(getContext(), binding.channelReadOnly.getText(), Toast.LENGTH_SHORT).show();
            if (mediaButton != null) mediaButton.resetState();
            return;
        }
        if (hasVideoPermissions()) {
            startVideoRecording();
            return;
        }
        if (videoPermissions != null) {
            videoPermissions.launch(new String[]{Manifest.permission.CAMERA, Manifest.permission.RECORD_AUDIO});
        }
        if (mediaButton != null) mediaButton.resetState();
    }

    private void startAudioRecording() {
        if (isAudioRecording) return;
        try {
            hideEmojiPanel();
            recordFile = new File(requireContext().getCacheDir(), "voice_" + System.currentTimeMillis() + ".m4a");
            recorder = new MediaRecorder();
            recorder.setAudioSource(MediaRecorder.AudioSource.MIC);
            recorder.setOutputFormat(MediaRecorder.OutputFormat.MPEG_4);
            recorder.setAudioEncoder(MediaRecorder.AudioEncoder.AAC);
            recorder.setOutputFile(recordFile.getAbsolutePath());
            recorder.prepare();
            recorder.start();
            isAudioRecording = true;
            recordingLocked = false;
            recordStart = System.currentTimeMillis();
            binding.recordingBar.setVisibility(View.VISIBLE);
            binding.recordingLabel.setText(R.string.recording_label);
            updateSendMicState();
            updateRecordingHints();
            startTimer();
        } catch (Exception e) {
            stopAudioRecording(false);
            if (mediaButton != null) mediaButton.resetState();
            Toast.makeText(getContext(), R.string.recording_start_failed, Toast.LENGTH_SHORT).show();
        }
    }

    private void startVideoRecording() {
        if (isVideoRecording) return;
        hideEmojiPanel();
        recordingLocked = false;
        updateRecordingHints();
        showVideoPreview(true);
        if (cameraRecorder != null && videoNotePreview != null) {
            cameraRecorder.bind(getViewLifecycleOwner(), videoNotePreview.getPreviewView());
            cameraRecorder.startRecording();
        }
    }

    private void cancelAudioRecording() {
        stopAudioRecording(false);
    }

    private void stopAudioRecording(boolean send) {
        if (!isAudioRecording) return;
        try {
            recorder.stop();
        } catch (Exception ignored) {
        }
        if (recorder != null) {
            recorder.release();
            recorder = null;
        }
        isAudioRecording = false;
        recordingLocked = false;
        binding.recordingBar.setVisibility(View.GONE);
        binding.recordingLabel.setText(R.string.recording_label);
        stopTimer();
        updateSendMicState();
        updateRecordingHints();

        if (recordFile != null && recordFile.exists()) {
            if (send) {
                if (recordFile.length() > MAX_VOICE_BYTES) {
                    Toast.makeText(getContext(), R.string.voice_too_large, Toast.LENGTH_SHORT).show();
                } else {
                    vm.sendVoice(Uri.fromFile(recordFile), "audio/mp4", selectedTtlSeconds);
                }
            } else {
                //noinspection ResultOfMethodCallIgnored
                recordFile.delete();
            }
        }
        recordFile = null;
        if (mediaButton != null) mediaButton.resetState();
    }

    private void stopVideoRecording(boolean send) {
        if (cameraRecorder != null) {
            cameraRecorder.stopRecording(send);
        }
        if (!send && mediaButton != null && mediaButton.getMode() != MediaRecorderButton.Mode.VIDEO) {
            hideVideoPreview(true);
        }
    }

    private void showVideoPreview(boolean animate) {
        if (videoNotePreview == null) return;
        if (videoNotePreview.getVisibility() == View.VISIBLE) {
            if (cameraRecorder != null) {
                cameraRecorder.bind(getViewLifecycleOwner(), videoNotePreview.getPreviewView());
            }
            return;
        }
        if (!animate) {
            videoNotePreview.show(false);
            if (cameraRecorder != null) {
                cameraRecorder.bind(getViewLifecycleOwner(), videoNotePreview.getPreviewView());
            }
            return;
        }
        videoNotePreview.show(false);
        videoNotePreview.animate().cancel();
        videoNotePreview.setAlpha(0f);
        videoNotePreview.setScaleX(0.2f);
        videoNotePreview.setScaleY(0.2f);
        videoNotePreview.post(() -> {
            if (videoNotePreview.getVisibility() != View.VISIBLE) return;
            float[] offset = resolvePreviewOffsetToButton();
            videoNotePreview.setTranslationX(offset[0]);
            videoNotePreview.setTranslationY(offset[1]);
            videoNotePreview.animate()
                    .translationX(0f)
                    .translationY(0f)
                    .alpha(1f)
                    .scaleX(1f)
                    .scaleY(1f)
                    .setDuration(200)
                    .start();
        });
        if (cameraRecorder != null) {
            cameraRecorder.bind(getViewLifecycleOwner(), videoNotePreview.getPreviewView());
        }
    }

    private void hideVideoPreview(boolean animate) {
        if (videoNotePreview == null || isVideoRecording) return;
        if (!animate) {
            videoNotePreview.hide(false);
            if (cameraRecorder != null) {
                cameraRecorder.unbind();
            }
            return;
        }
        if (videoNotePreview.getVisibility() != View.VISIBLE) return;
        videoNotePreview.animate().cancel();
        float[] offset = resolvePreviewOffsetToButton();
        videoNotePreview.animate()
                .translationX(offset[0])
                .translationY(offset[1])
                .alpha(0f)
                .scaleX(0.2f)
                .scaleY(0.2f)
                .setDuration(160)
                .withEndAction(() -> {
                    videoNotePreview.hide(false);
                    videoNotePreview.setTranslationX(0f);
                    videoNotePreview.setTranslationY(0f);
                    videoNotePreview.setScaleX(1f);
                    videoNotePreview.setScaleY(1f);
                    videoNotePreview.setAlpha(1f);
                    if (cameraRecorder != null) {
                        cameraRecorder.unbind();
                    }
                })
                .start();
    }

    private float[] resolvePreviewOffsetToButton() {
        float[] out = new float[]{0f, 0f};
        if (videoNotePreview == null || mediaButton == null) return out;
        if (videoNotePreview.getWidth() == 0 || mediaButton.getWidth() == 0) return out;
        int[] previewLoc = new int[2];
        int[] buttonLoc = new int[2];
        videoNotePreview.getLocationOnScreen(previewLoc);
        mediaButton.getLocationOnScreen(buttonLoc);
        float previewCx = previewLoc[0] + (videoNotePreview.getWidth() / 2f);
        float previewCy = previewLoc[1] + (videoNotePreview.getHeight() / 2f);
        float buttonCx = buttonLoc[0] + (mediaButton.getWidth() / 2f);
        float buttonCy = buttonLoc[1] + (mediaButton.getHeight() / 2f);
        out[0] = buttonCx - previewCx;
        out[1] = buttonCy - previewCy;
        return out;
    }

    private void startTimer() {
        timerHandler.removeCallbacks(timerRunnable);
        timerRunnable = () -> {
            long elapsed = System.currentTimeMillis() - recordStart;
            long seconds = elapsed / 1000;
            long minutes = seconds / 60;
            long rem = seconds % 60;
            binding.recordingTimer.setText(String.format(Locale.getDefault(), "%02d:%02d", minutes, rem));
            timerHandler.postDelayed(timerRunnable, 1000);
        };
        timerHandler.post(timerRunnable);
    }

    private void stopTimer() {
        timerHandler.removeCallbacks(timerRunnable);
        binding.recordingTimer.setText(R.string.recording_timer_default);
    }

    private Runnable timerRunnable = () -> {};

    private void updateSendMicState() {
        if (isChannel && (!channelSubscribed || !channelCanPost)) {
            binding.btnSend.setVisibility(View.GONE);
            if (mediaButton != null) mediaButton.setVisibility(View.GONE);
            return;
        }
        boolean hasText = binding.etMessage.getText() != null && binding.etMessage.getText().toString().trim().length() > 0;
        if (isAudioRecording || isVideoRecording) {
            binding.btnSend.setVisibility(View.GONE);
            if (mediaButton != null) mediaButton.setVisibility(View.VISIBLE);
            return;
        }
        if (hasText) {
            binding.btnSend.setVisibility(View.VISIBLE);
            if (mediaButton != null) mediaButton.setVisibility(View.GONE);
            if (mediaButton != null && mediaButton.getMode() == MediaRecorderButton.Mode.VIDEO) {
                hideVideoPreview(true);
            }
        } else {
            binding.btnSend.setVisibility(View.GONE);
            if (mediaButton != null) mediaButton.setVisibility(View.VISIBLE);
        }
        updateRecordingHints();
    }

    private void updateSpoilerButton() {
        if (binding == null) return;
        Context context = getContext();
        if (context == null) return;
        int tint = ContextCompat.getColor(context, spoilerModeEnabled ? R.color.x_accent_end : R.color.x_text_secondary);
        binding.btnSpoiler.setColorFilter(tint);
        binding.btnSpoiler.setSelected(spoilerModeEnabled);
    }

    private void updateTtlButton() {
        if (binding == null) return;
        Context context = getContext();
        if (context == null) return;
        int tint = ContextCompat.getColor(context, selectedTtlSeconds > 0L ? R.color.x_accent_end : R.color.x_text_secondary);
        binding.btnTtl.setColorFilter(tint);
        binding.btnTtl.setSelected(selectedTtlSeconds > 0L);
    }

    private void updateRecordingHints() {
        if (binding == null) return;
        boolean recording = isAudioRecording || isVideoRecording;
        if (!recording) {
            binding.recordingHintCancel.setVisibility(View.GONE);
            binding.recordingHintLock.setVisibility(View.GONE);
            binding.recordingHintLocked.setVisibility(View.GONE);
            return;
        }
        if (recordingLocked) {
            binding.recordingHintCancel.setVisibility(View.GONE);
            binding.recordingHintLock.setVisibility(View.GONE);
            binding.recordingHintLocked.setVisibility(View.VISIBLE);
            return;
        }
        binding.recordingHintCancel.setVisibility(View.VISIBLE);
        binding.recordingHintLock.setVisibility(View.VISIBLE);
        binding.recordingHintLocked.setVisibility(View.GONE);
    }

    private void toggleEmojiPanel() {
        if (emojiVisible) {
            hideEmojiPanel();
        } else {
            emojiVisible = true;
            binding.emojiPanel.setVisibility(View.VISIBLE);
            hideKeyboard();
        }
    }

    private void hideEmojiPanel() {
        emojiVisible = false;
        binding.emojiPanel.setVisibility(View.GONE);
    }

    private void hideKeyboard() {
        InputMethodManager imm = (InputMethodManager) requireContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) {
            imm.hideSoftInputFromWindow(binding.etMessage.getWindowToken(), 0);
        }
    }

    private void setupEmojiPanel() {
        binding.emojiPanel.removeAllViews();
        for (String emoji : emojis) {
            EmojiTextView tv = new EmojiTextView(requireContext());
            tv.setText(emoji);
            tv.setTextSize(22f);
            tv.setGravity(Gravity.CENTER);
            tv.setBackgroundResource(R.drawable.btn_icon);
            int pad = (int) (6 * getResources().getDisplayMetrics().density);
            tv.setPadding(pad, pad, pad, pad);
            tv.setOnClickListener(v -> insertEmoji(emoji));
            binding.emojiPanel.addView(tv);
        }
    }

    private void insertEmoji(String emoji) {
        int start = Math.max(binding.etMessage.getSelectionStart(), 0);
        int end = Math.max(binding.etMessage.getSelectionEnd(), 0);
        binding.etMessage.getText().replace(Math.min(start, end), Math.max(start, end), emoji, 0, emoji.length());
    }

    private void toggleVoicePlayback(MessageEntity message) {
        if (message == null || message.filePath == null || message.filePath.isEmpty()) {
            Toast.makeText(getContext(), R.string.file_missing, Toast.LENGTH_SHORT).show();
            return;
        }
        if (message.id != null && message.id.equals(playingId)) {
            stopPlayback();
            return;
        }
        stopPlayback();
        String url = BuildConfig.API_BASE + message.filePath;
        try {
            mediaPlayer = new MediaPlayer();
            mediaPlayer.setDataSource(url);
            mediaPlayer.setOnPreparedListener(mp -> {
                playingId = message.id;
                adapter.setPlaybackState(playingId, 0, mp.getDuration());
                mp.start();
                startPlaybackTicker();
            });
            mediaPlayer.setOnCompletionListener(mp -> stopPlayback());
            mediaPlayer.setOnErrorListener((mp, what, extra) -> {
                stopPlayback();
                return true;
            });
            mediaPlayer.prepareAsync();
        } catch (Exception e) {
            stopPlayback();
            Toast.makeText(getContext(), R.string.playback_failed, Toast.LENGTH_SHORT).show();
        }
    }

    private void startPlaybackTicker() {
        playbackHandler.removeCallbacks(playbackRunnable);
        playbackRunnable = new Runnable() {
            @Override
            public void run() {
                if (mediaPlayer == null || playingId == null) return;
                try {
                    int position = mediaPlayer.getCurrentPosition();
                    int duration = mediaPlayer.getDuration();
                    if (adapter != null) {
                        adapter.setPlaybackState(playingId, position, duration);
                    }
                } catch (Exception ignored) {
                }
                playbackHandler.postDelayed(this, 250);
            }
        };
        playbackHandler.post(playbackRunnable);
    }

    private void stopPlayback() {
        playbackHandler.removeCallbacks(playbackRunnable);
        if (mediaPlayer != null) {
            try {
                mediaPlayer.stop();
            } catch (Exception ignored) {
            }
            mediaPlayer.release();
            mediaPlayer = null;
        }
        playingId = null;
        if (adapter != null) {
            adapter.clearPlaybackState();
        }
    }

    private void startAutoRefresh() {
        stopAutoRefresh();
        autoRefreshRunnable = new Runnable() {
            @Override
            public void run() {
                if (vm != null) {
                    vm.pollNewMessages();
                }
                autoRefreshHandler.postDelayed(this, AUTO_REFRESH_MS);
            }
        };
        autoRefreshHandler.postDelayed(autoRefreshRunnable, AUTO_REFRESH_MS);
    }

    private void stopAutoRefresh() {
        autoRefreshHandler.removeCallbacks(autoRefreshRunnable);
    }

    private void startTtlPrune() {
        stopTtlPrune();
        ttlPruneRunnable = new Runnable() {
            @Override
            public void run() {
                if (vm != null) {
                    vm.pruneExpiredMessages();
                }
                ttlPruneHandler.postDelayed(this, TTL_PRUNE_INTERVAL_MS);
            }
        };
        ttlPruneHandler.postDelayed(ttlPruneRunnable, TTL_PRUNE_INTERVAL_MS);
    }

    private void stopTtlPrune() {
        ttlPruneHandler.removeCallbacks(ttlPruneRunnable);
    }

    private File getDownloadedFile(MessageEntity message) {
        if (message == null || message.id == null) return null;
        File cached = downloadedFiles.get(message.id);
        if (cached != null && cached.exists()) return cached;
        // Check common download locations
        if (message.fileName != null && !message.fileName.isEmpty()) {
            File downloads = requireContext().getExternalFilesDir(android.os.Environment.DIRECTORY_DOWNLOADS);
            if (downloads != null) {
                File f = new File(downloads, message.fileName);
                if (f.exists()) {
                    downloadedFiles.put(message.id, f);
                    return f;
                }
            }
        }
        return null;
    }

    private Uri getLocalFileUri(File file) {
        if (file == null) return null;
        return FileProvider.getUriForFile(requireContext(),
                requireContext().getPackageName() + ".fileprovider", file);
    }

    private String getMessageKey(MessageEntity message) {
        if (message == null) return null;
        return message.id != null ? message.id : message.tempId;
    }

    private void startDownload(MessageEntity message) {
        if (message == null || message.filePath == null) return;
        String url = message.filePath.startsWith("http") ? message.filePath : BuildConfig.API_BASE + message.filePath;
        String key = getMessageKey(message);
        if (key == null) return;
        // TODO: implement actual download with progress tracking
        // For now, just open in browser
        try {
            Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
            startActivity(intent);
        } catch (Exception e) {
            Toast.makeText(getContext(), R.string.file_unavailable, Toast.LENGTH_SHORT).show();
        }
    }

    private void cancelDownload(String key) {
        // TODO: implement download cancellation
        // For now, just a stub
    }

    private boolean isImageMessage(MessageEntity message) {
        if (message == null) return false;
        if ("image".equals(message.messageType)) return true;
        String name = message.fileName != null ? message.fileName : "";
        String path = message.filePath != null ? message.filePath : "";
        String lower = (name + " " + path).toLowerCase(Locale.getDefault());
        return lower.contains(".png") || lower.contains(".jpg") || lower.contains(".jpeg")
                || lower.contains(".gif") || lower.contains(".webp");
    }

    private void openImage(MessageEntity message) {
        if (message == null || message.filePath == null || message.filePath.isEmpty()) {
            Toast.makeText(getContext(), R.string.image_unavailable, Toast.LENGTH_SHORT).show();
            return;
        }
        File local = getDownloadedFile(message);
        String url;
        if (local != null && local.exists()) {
            url = getLocalFileUri(local).toString();
        } else if (message.filePath.startsWith("http")
                || message.filePath.startsWith("content:")
                || message.filePath.startsWith("file:")) {
            url = message.filePath;
        } else {
            url = BuildConfig.API_BASE + message.filePath;
        }
        Intent intent = new Intent(requireContext(), com.xipher.wrapper.ui.ImagePreviewActivity.class);
        intent.putExtra(com.xipher.wrapper.ui.ImagePreviewActivity.EXTRA_IMAGE_URL, url);
        startActivity(intent);
    }

    private void openFile(MessageEntity message) {
        if (message == null) {
            Toast.makeText(getContext(), R.string.file_unavailable, Toast.LENGTH_SHORT).show();
            return;
        }
        File local = getDownloadedFile(message);
        Uri uri = null;
        if (local != null && local.exists()) {
            uri = getLocalFileUri(local);
        } else if (message.filePath != null && !message.filePath.isEmpty()) {
            String raw = message.filePath.startsWith("http")
                    || message.filePath.startsWith("content:")
                    || message.filePath.startsWith("file:")
                    ? message.filePath
                    : BuildConfig.API_BASE + message.filePath;
            uri = Uri.parse(raw);
        }
        if (uri == null) {
            Toast.makeText(getContext(), R.string.file_unavailable, Toast.LENGTH_SHORT).show();
            return;
        }
        Intent intent = new Intent(Intent.ACTION_VIEW, uri);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
        try {
            startActivity(intent);
        } catch (Exception e) {
            Toast.makeText(getContext(), R.string.no_app_to_open, Toast.LENGTH_SHORT).show();
        }
    }

    private void openLocation(MessageEntity message) {
        if (message == null || message.content == null || message.content.isEmpty()) {
            Toast.makeText(getContext(), R.string.location_unavailable, Toast.LENGTH_SHORT).show();
            return;
        }
        com.xipher.wrapper.util.LocationUtils.LocationPayload payload =
                com.xipher.wrapper.util.LocationUtils.parse(message.content);
        if (payload == null) {
            Toast.makeText(getContext(), R.string.location_unavailable, Toast.LENGTH_SHORT).show();
            return;
        }
        Intent intent = new Intent(requireContext(), com.xipher.wrapper.ui.MapActivity.class);
        intent.putExtra(com.xipher.wrapper.ui.MapActivity.EXTRA_CHAT_ID, chatId);
        intent.putExtra(com.xipher.wrapper.ui.MapActivity.EXTRA_IS_GROUP, isGroup);
        intent.putExtra(com.xipher.wrapper.ui.MapActivity.EXTRA_IS_CHANNEL, isChannel);
        intent.putExtra(com.xipher.wrapper.ui.MapActivity.EXTRA_LAT, payload.lat);
        intent.putExtra(com.xipher.wrapper.ui.MapActivity.EXTRA_LON, payload.lon);
        intent.putExtra(com.xipher.wrapper.ui.MapActivity.EXTRA_LIVE_ID, payload.liveId);
        intent.putExtra(com.xipher.wrapper.ui.MapActivity.EXTRA_EXPIRES_AT, payload.expiresAt);
        startActivity(intent);
    }

    private static final class GiftPaymentData {
        final String formAction;
        final String receiver;
        final String label;
        final String sum;
        final String successUrl;
        final String provider;
        final String checkoutUrl;

        GiftPaymentData(String formAction, String receiver, String label, String sum,
                        String successUrl, String provider, String checkoutUrl) {
            this.formAction = formAction;
            this.receiver = receiver;
            this.label = label;
            this.sum = sum;
            this.successUrl = successUrl;
            this.provider = provider;
            this.checkoutUrl = checkoutUrl;
        }
    }

    // ========== Reply functionality ==========
    
    private void setReplyTo(MessageEntity message) {
        if (message == null || binding == null) return;
        replyToMessage = message;
        
        // Show reply preview panel
        binding.replyPreviewPanel.setVisibility(View.VISIBLE);
        
        // Set sender name
        String senderName = message.senderName;
        if (TextUtils.isEmpty(senderName)) {
            senderName = getString(R.string.unknown_sender);
        }
        binding.replyPreviewSender.setText(senderName);
        
        // Set content preview
        String content = getReplyPreviewText(message);
        binding.replyPreviewText.setText(content);
        
        // Show thumbnail for media messages
        if (isImageMessage(message) && !TextUtils.isEmpty(message.filePath)) {
            binding.replyPreviewThumbnail.setVisibility(View.VISIBLE);
            String url = message.filePath.startsWith("http") 
                    ? message.filePath 
                    : BuildConfig.API_BASE + message.filePath;
            com.bumptech.glide.Glide.with(binding.replyPreviewThumbnail)
                    .load(url)
                    .centerCrop()
                    .into(binding.replyPreviewThumbnail);
        } else {
            binding.replyPreviewThumbnail.setVisibility(View.GONE);
        }
        
        // Focus input
        binding.etMessage.requestFocus();
    }
    
    private void clearReply() {
        replyToMessage = null;
        if (binding != null) {
            binding.replyPreviewPanel.setVisibility(View.GONE);
            binding.replyPreviewThumbnail.setVisibility(View.GONE);
        }
    }
    
    private String getReplyPreviewText(MessageEntity message) {
        if (message == null) return "";
        
        String type = message.messageType;
        if (type == null) type = "text";
        
        switch (type) {
            case "voice":
                return getString(R.string.reply_voice_message);
            case "video_note":
                return getString(R.string.reply_video_message);
            case "image":
            case "photo":
                return getString(R.string.reply_photo);
            case "file":
                return !TextUtils.isEmpty(message.fileName) 
                        ? message.fileName 
                        : getString(R.string.reply_file);
            case "location":
            case "live_location":
                return getString(R.string.reply_location);
            case "checklist":
                return getString(R.string.reply_checklist);
            default:
                String text = message.content;
                if (TextUtils.isEmpty(text)) return "";
                // Truncate long messages
                if (text.length() > 100) {
                    text = text.substring(0, 100) + "...";
                }
                return text;
        }
    }

    private void setupSwipeToReply() {
        ItemTouchHelper helper = new ItemTouchHelper(new SwipeToReplyCallback(requireContext(), position -> {
            if (adapter != null && position >= 0 && position < adapter.getItemCount()) {
                MessageEntity msg = adapter.getItem(position);
                if (msg != null) setReplyTo(msg);
            }
        }));
        helper.attachToRecyclerView(binding.recyclerMessages);
    }
}
