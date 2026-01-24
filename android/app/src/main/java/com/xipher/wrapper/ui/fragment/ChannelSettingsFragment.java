package com.xipher.wrapper.ui.fragment;

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
import com.xipher.wrapper.databinding.FragmentChannelSettingsBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.ui.ChannelManagementActivity;

import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Фрагмент настроек канала (для админов)
 */
public class ChannelSettingsFragment extends Fragment {
    private static final String ARG_CHANNEL_ID = "channel_id";
    private static final String ARG_CHANNEL_TITLE = "channel_title";
    private static final String ARG_CHANNEL_DESCRIPTION = "channel_description";
    private static final String ARG_CHANNEL_USERNAME = "channel_username";
    private static final String ARG_IS_PRIVATE = "is_private";
    private static final String ARG_SHOW_AUTHOR = "show_author";
    private static final String ARG_MY_ROLE = "my_role";

    private FragmentChannelSettingsBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();

    private String channelId;
    private String channelTitle;
    private String channelDescription;
    private String channelUsername;
    private boolean isPrivate;
    private boolean showAuthor;
    private String myRole = "";

    private String originalName = "";
    private String originalDescription = "";
    private String originalUsername = "";
    private boolean originalPrivate = false;
    private boolean originalShowAuthor = true;

    public static ChannelSettingsFragment newInstance(
            String channelId, String title, String description, String username,
            boolean isPrivate, boolean showAuthor, String myRole) {
        ChannelSettingsFragment fragment = new ChannelSettingsFragment();
        Bundle args = new Bundle();
        args.putString(ARG_CHANNEL_ID, channelId);
        args.putString(ARG_CHANNEL_TITLE, title);
        args.putString(ARG_CHANNEL_DESCRIPTION, description);
        args.putString(ARG_CHANNEL_USERNAME, username);
        args.putBoolean(ARG_IS_PRIVATE, isPrivate);
        args.putBoolean(ARG_SHOW_AUTHOR, showAuthor);
        args.putString(ARG_MY_ROLE, myRole);
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

            originalName = channelTitle;
            originalDescription = channelDescription;
            originalUsername = channelUsername;
            originalPrivate = isPrivate;
            originalShowAuthor = showAuthor;
        }
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        binding = FragmentChannelSettingsBinding.inflate(inflater, container, false);
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
        // Сохранение
        binding.btnSave.setOnClickListener(v -> saveChanges());

        // Удаление канала
        binding.actionDelete.setOnClickListener(v -> showDeleteDialog());

        // Верификация
        binding.btnVerification.setOnClickListener(v -> openVerificationSheet());

        // Показываем удаление только владельцу
        boolean isOwner = "owner".equals(myRole) || "creator".equals(myRole);
        binding.dangerSection.setVisibility(isOwner ? View.VISIBLE : View.GONE);
        // Показываем верификацию только владельцу
        binding.verificationSection.setVisibility(isOwner ? View.VISIBLE : View.GONE);
    }

    private void openVerificationSheet() {
        VerificationBottomSheet sheet = VerificationBottomSheet.newInstance();
        sheet.show(getChildFragmentManager(), "verification");
    }

    private void updateDisplay() {
        if (binding == null) return;

        binding.etName.setText(channelTitle);
        binding.etDescription.setText(channelDescription);
        binding.etUsername.setText(channelUsername);
        binding.switchPrivate.setChecked(isPrivate);
        binding.switchShowAuthor.setChecked(showAuthor);
    }

    private void saveChanges() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(channelId)) return;

        String newName = binding.etName.getText() != null ? binding.etName.getText().toString().trim() : "";
        String newDescription = binding.etDescription.getText() != null ? binding.etDescription.getText().toString().trim() : "";
        String newUsername = binding.etUsername.getText() != null ? binding.etUsername.getText().toString().trim() : "";
        boolean newPrivate = binding.switchPrivate.isChecked();
        boolean newShowAuthor = binding.switchShowAuthor.isChecked();

        if (TextUtils.isEmpty(newName) || newName.length() < 3) {
            Toast.makeText(requireContext(), R.string.channel_name_min_length, Toast.LENGTH_SHORT).show();
            return;
        }

        binding.btnSave.setEnabled(false);

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                final boolean[] hasChanges = {false};

                // Обновляем название
                if (!newName.equals(originalName)) {
                    client.updateChannelName(token, channelId, newName);
                    originalName = newName;
                    hasChanges[0] = true;
                }

                // Обновляем описание
                if (!newDescription.equals(originalDescription)) {
                    client.updateChannelDescription(token, channelId, newDescription);
                    originalDescription = newDescription;
                    hasChanges[0] = true;
                }

                // Обновляем username
                if (!newUsername.equals(originalUsername)) {
                    client.setChannelCustomLink(token, channelId, newUsername);
                    originalUsername = newUsername;
                    hasChanges[0] = true;
                }

                // Обновляем приватность
                if (newPrivate != originalPrivate) {
                    client.setChannelPrivacy(token, channelId, newPrivate);
                    originalPrivate = newPrivate;
                    hasChanges[0] = true;
                }

                // Обновляем показ автора
                if (newShowAuthor != originalShowAuthor) {
                    client.setChannelShowAuthor(token, channelId, newShowAuthor);
                    originalShowAuthor = newShowAuthor;
                    hasChanges[0] = true;
                }

                requireActivity().runOnUiThread(() -> {
                    binding.btnSave.setEnabled(true);
                    if (hasChanges[0]) {
                        Toast.makeText(requireContext(), R.string.channel_settings_saved, Toast.LENGTH_SHORT).show();
                        // Обновляем данные в родительской активности
                        if (getActivity() instanceof ChannelManagementActivity) {
                            ((ChannelManagementActivity) getActivity()).refreshData();
                        }
                    }
                });
            } catch (Exception e) {
                requireActivity().runOnUiThread(() -> {
                    binding.btnSave.setEnabled(true);
                    Toast.makeText(requireContext(), R.string.channel_settings_save_failed, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }

    private void showDeleteDialog() {
        new MaterialAlertDialogBuilder(requireContext())
                .setTitle(R.string.channel_delete_title)
                .setMessage(getString(R.string.channel_delete_message, channelTitle))
                .setPositiveButton(R.string.channel_delete, (dialog, which) -> deleteChannel())
                .setNegativeButton(R.string.cancel, null)
                .show();
    }

    private void deleteChannel() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token) || TextUtils.isEmpty(channelId)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                boolean success = client.deleteChannel(token, channelId);

                requireActivity().runOnUiThread(() -> {
                    if (success) {
                        Toast.makeText(requireContext(), R.string.channel_deleted, Toast.LENGTH_SHORT).show();
                        requireActivity().finish();
                    } else {
                        Toast.makeText(requireContext(), R.string.channel_delete_failed, Toast.LENGTH_SHORT).show();
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
                           boolean isPrivate, boolean showAuthor, String myRole) {
        this.channelId = channelId;
        this.channelTitle = title;
        this.channelDescription = description;
        this.channelUsername = username;
        this.isPrivate = isPrivate;
        this.showAuthor = showAuthor;
        this.myRole = myRole;

        originalName = title;
        originalDescription = description;
        originalUsername = username;
        originalPrivate = isPrivate;
        originalShowAuthor = showAuthor;

        if (binding != null) {
            updateDisplay();
            boolean isOwner = "owner".equals(myRole) || "creator".equals(myRole);
            binding.dangerSection.setVisibility(isOwner ? View.VISIBLE : View.GONE);
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
