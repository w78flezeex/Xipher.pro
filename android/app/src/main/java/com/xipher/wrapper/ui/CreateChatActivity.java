package com.xipher.wrapper.ui;

import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.View;
import android.widget.Toast;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.model.FriendDto;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityCreateChatBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.ui.adapter.FriendSelectAdapter;

import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class CreateChatActivity extends AppCompatActivity {
    public static final String EXTRA_IS_GROUP = "is_group";

    private ActivityCreateChatBinding binding;
    private AuthRepository authRepo;
    private FriendSelectAdapter adapter;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private boolean isGroup;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityCreateChatBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);
        isGroup = getIntent() != null && getIntent().getBooleanExtra(EXTRA_IS_GROUP, true);

        binding.screenTitle.setText(isGroup ? R.string.create_group_title : R.string.create_channel_title);
        binding.stepTitle.setText(isGroup ? R.string.create_step_add_members : R.string.create_step_add_subscribers);
        binding.stepSubtitle.setText(isGroup ? R.string.create_step_pick_people : R.string.create_step_can_skip);
        binding.detailsTitle.setText(isGroup ? R.string.create_details_title : R.string.create_channel_details_title);

        binding.btnBack.setOnClickListener(v -> onBackPressed());
        binding.btnPickPhoto.setOnClickListener(v ->
                Toast.makeText(this, R.string.create_photo_not_supported, Toast.LENGTH_SHORT).show()
        );
        binding.inputName.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                String name = s != null ? s.toString().trim() : "";
                if (!name.isEmpty()) {
                    binding.chatPhotoLetter.setText(name.substring(0, 1).toUpperCase(Locale.getDefault()));
                }
            }

            @Override
            public void afterTextChanged(Editable s) {}
        });

        adapter = new FriendSelectAdapter();
        adapter.setListener(count -> {
            binding.selectedCount.setText(getString(R.string.selected_count, count));
            updateNextButtonText(count);
        });
        binding.recyclerFriends.setLayoutManager(new LinearLayoutManager(this));
        binding.recyclerFriends.setAdapter(adapter);

        binding.btnNextMembers.setOnClickListener(v -> {
            int selected = adapter.getSelectedIds().size();
            if (isGroup && selected == 0) {
                Toast.makeText(this, R.string.create_group_pick_member, Toast.LENGTH_SHORT).show();
                return;
            }
            if (!isGroup && selected > 0) {
                Toast.makeText(this, R.string.create_channel_subscribers_manual, Toast.LENGTH_SHORT).show();
            }
            showDetailsStep();
        });

        binding.btnCreateChat.setOnClickListener(v -> createChat());

        loadFriends();
        updateNextButtonText(0);
    }

    private void updateNextButtonText(int selected) {
        if (isGroup) {
            binding.btnNextMembers.setText(R.string.next);
        } else {
            binding.btnNextMembers.setText(selected > 0 ? R.string.next : R.string.skip);
        }
    }

    private void showDetailsStep() {
        binding.stepMembers.setVisibility(View.GONE);
        binding.stepDetails.setVisibility(View.VISIBLE);
    }

    private void loadFriends() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) return;
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                List<FriendDto> friends = client.friends(token);
                runOnUiThread(() -> adapter.setItems(friends));
            } catch (Exception e) {
                runOnUiThread(() -> Toast.makeText(this, R.string.friends_load_failed, Toast.LENGTH_SHORT).show());
            }
        });
    }

    private void createChat() {
        String name = binding.inputName.getText() != null ? binding.inputName.getText().toString().trim() : "";
        if (name.length() < 3) {
            Toast.makeText(this, R.string.create_name_min, Toast.LENGTH_SHORT).show();
            return;
        }
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) {
            Toast.makeText(this, R.string.auth_missing_token, Toast.LENGTH_SHORT).show();
            return;
        }
        List<String> members = adapter.getSelectedIds();
        binding.btnCreateChat.setEnabled(false);
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                com.google.gson.JsonObject res = isGroup ? client.createGroup(token, name) : client.createChannel(token, name);
                boolean ok = res != null && res.has("success") && res.get("success").getAsBoolean();
                if (!ok) {
                    runOnUiThread(() -> {
                        binding.btnCreateChat.setEnabled(true);
                        Toast.makeText(this, R.string.create_failed, Toast.LENGTH_SHORT).show();
                    });
                    return;
                }
                if (isGroup && res.has("group_id")) {
                    String groupId = res.get("group_id").getAsString();
                    for (String id : members) {
                        try {
                            client.addFriendToGroup(token, groupId, id);
                        } catch (Exception ignored) {
                        }
                    }
                }
                runOnUiThread(() -> {
                    Toast.makeText(this, R.string.create_success, Toast.LENGTH_SHORT).show();
                    setResult(RESULT_OK);
                    finish();
                });
            } catch (Exception e) {
                runOnUiThread(() -> {
                    binding.btnCreateChat.setEnabled(true);
                    Toast.makeText(this, R.string.network_error, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        io.shutdownNow();
    }
}
