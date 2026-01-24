package com.xipher.wrapper.ui.fragment;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.FragmentChannelOverviewBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.ui.ChannelManagementActivity;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Фрагмент с обзором канала (информация, действия)
 */
public class ChannelOverviewFragment extends Fragment {
    private static final String ARG_CHANNEL_ID = "channel_id";
    private static final String ARG_CHANNEL_TITLE = "channel_title";
    private static final String ARG_CHANNEL_DESCRIPTION = "channel_description";
    private static final String ARG_CHANNEL_USERNAME = "channel_username";
    private static final String ARG_IS_PRIVATE = "is_private";
    private static final String ARG_SHOW_AUTHOR = "show_author";
    private static final String ARG_MY_ROLE = "my_role";
    private static final String ARG_IS_ADMIN = "is_admin";
    private static final String ARG_SUBSCRIBER_COUNT = "subscriber_count";

    private FragmentChannelOverviewBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    private String channelId;
    private String channelTitle;
    private String channelDescription;
    private String channelUsername;
    private boolean isPrivate;
    private boolean showAuthor;
    private String myRole = "";
    private boolean isAdmin = false;
    private int subscriberCount = 0;

    private String lastInviteUrl = "";

    public static ChannelOverviewFragment newInstance(
            String channelId, String title, String description, String username,
            boolean isPrivate, boolean showAuthor, String myRole, boolean isAdmin, int subCount) {
        ChannelOverviewFragment fragment = new ChannelOverviewFragment();
        Bundle args = new Bundle();
        args.putString(ARG_CHANNEL_ID, channelId);
        args.putString(ARG_CHANNEL_TITLE, title);
        args.putString(ARG_CHANNEL_DESCRIPTION, description);
        args.putString(ARG_CHANNEL_USERNAME, username);
        args.putBoolean(ARG_IS_PRIVATE, isPrivate);
        args.putBoolean(ARG_SHOW_AUTHOR, showAuthor);
        args.putString(ARG_MY_ROLE, myRole);
        args.putBoolean(ARG_IS_ADMIN, isAdmin);
        args.putInt(ARG_SUBSCRIBER_COUNT, subCount);
        fragment.setArguments(args);
        return fragment;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        if (getArguments() != null) {
            channelId = getArguments().getString(ARG_CHANNEL_ID);
            channelTitle = getArguments().getString(ARG_CHANNEL_TITLE);
            channelDescription = getArguments().getString(ARG_CHANNEL_DESCRIPTION);
            channelUsername = getArguments().getString(ARG_CHANNEL_USERNAME);
            isPrivate = getArguments().getBoolean(ARG_IS_PRIVATE);
            showAuthor = getArguments().getBoolean(ARG_SHOW_AUTHOR);
            myRole = getArguments().getString(ARG_MY_ROLE, "");
            isAdmin = getArguments().getBoolean(ARG_IS_ADMIN);
            subscriberCount = getArguments().getInt(ARG_SUBSCRIBER_COUNT);
        }
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        binding = FragmentChannelOverviewBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        authRepo = AppServices.authRepository(requireContext());
        setupUI();
        updateDisplay();
    }

    private void setupUI() {
        // Действия
        binding.actionNotifications.setOnClickListener(v -> {
            Toast.makeText(requireContext(), R.string.channel_notifications_settings, Toast.LENGTH_SHORT).show();
        });

        binding.actionSearch.setOnClickListener(v -> {
            Toast.makeText(requireContext(), R.string.channel_search, Toast.LENGTH_SHORT).show();
        });

        binding.actionLink.setOnClickListener(v -> copyInviteLink());

        binding.actionLeave.setOnClickListener(v -> showLeaveDialog());

        // Клик по ссылке
        binding.linkRow.setOnClickListener(v -> copyLink());
    }

    private void updateDisplay() {
        if (binding == null) return;

        binding.channelName.setText(channelTitle);
        
        // Описание
        if (!TextUtils.isEmpty(channelDescription)) {
            binding.descriptionText.setText(channelDescription);
            binding.descriptionSection.setVisibility(View.VISIBLE);
        } else {
            binding.descriptionSection.setVisibility(View.GONE);
        }

        // Ссылка
        if (!TextUtils.isEmpty(channelUsername)) {
            binding.linkText.setText("@" + channelUsername);
            binding.linkSection.setVisibility(View.VISIBLE);
        } else if (isPrivate) {
            binding.linkText.setText(R.string.channel_private);
            binding.linkSection.setVisibility(View.VISIBLE);
        } else {
            binding.linkSection.setVisibility(View.GONE);
        }

        // Кнопка выхода только для не-админов
        boolean canLeave = !isOwner();
        binding.actionLeave.setVisibility(canLeave ? View.VISIBLE : View.GONE);
    }

    private boolean isOwner() {
        return "owner".equals(myRole) || "creator".equals(myRole);
    }

    private void copyLink() {
        if (!TextUtils.isEmpty(channelUsername)) {
            String url = "https://xipher.ru/@" + channelUsername;
            if (getActivity() instanceof ChannelManagementActivity) {
                ((ChannelManagementActivity) getActivity()).copyToClipboard(url);
            }
        }
    }

    private void copyInviteLink() {
        if (!TextUtils.isEmpty(lastInviteUrl)) {
            if (getActivity() instanceof ChannelManagementActivity) {
                ((ChannelManagementActivity) getActivity()).copyToClipboard(lastInviteUrl);
            }
            return;
        }

        // Создаём пригласительную ссылку
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(channelId)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.createChannelInvite(token, channelId, 0);

                if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                    String url = res.has("invite_url") ? res.get("invite_url").getAsString() : "";
                    if (TextUtils.isEmpty(url) && res.has("invite_token")) {
                        url = "https://xipher.ru/join/" + res.get("invite_token").getAsString();
                    }
                    
                    String finalUrl = url;
                    requireActivity().runOnUiThread(() -> {
                        lastInviteUrl = finalUrl;
                        if (!TextUtils.isEmpty(finalUrl) && getActivity() instanceof ChannelManagementActivity) {
                            ((ChannelManagementActivity) getActivity()).copyToClipboard(finalUrl);
                        } else {
                            Toast.makeText(requireContext(), R.string.channel_invite_failed, Toast.LENGTH_SHORT).show();
                        }
                    });
                } else {
                    requireActivity().runOnUiThread(() ->
                            Toast.makeText(requireContext(), R.string.channel_invite_failed, Toast.LENGTH_SHORT).show()
                    );
                }
            } catch (Exception e) {
                requireActivity().runOnUiThread(() ->
                        Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
    }

    private void showLeaveDialog() {
        new MaterialAlertDialogBuilder(requireContext())
                .setTitle(R.string.channel_leave_title)
                .setMessage(getString(R.string.channel_leave_message, channelTitle))
                .setPositiveButton(R.string.channel_leave, (dialog, which) -> leaveChannel())
                .setNegativeButton(R.string.cancel, null)
                .show();
    }

    private void leaveChannel() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(channelId)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                boolean success = client.leaveChannel(token, channelId);

                requireActivity().runOnUiThread(() -> {
                    if (success) {
                        Toast.makeText(requireContext(), R.string.channel_left, Toast.LENGTH_SHORT).show();
                        requireActivity().finish();
                    } else {
                        Toast.makeText(requireContext(), R.string.channel_leave_failed, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                requireActivity().runOnUiThread(() ->
                        Toast.makeText(requireContext(), R.string.network_error, Toast.LENGTH_SHORT).show()
                );
            }
        });
    }

    public void updateData(String channelId, String title, String description, String username,
                           boolean isPrivate, boolean showAuthor, String myRole, boolean isAdmin, int subCount) {
        this.channelId = channelId;
        this.channelTitle = title;
        this.channelDescription = description;
        this.channelUsername = username;
        this.isPrivate = isPrivate;
        this.showAuthor = showAuthor;
        this.myRole = myRole;
        this.isAdmin = isAdmin;
        this.subscriberCount = subCount;

        if (binding != null) {
            updateDisplay();
        }
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        binding = null;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        io.shutdownNow();
    }
}
