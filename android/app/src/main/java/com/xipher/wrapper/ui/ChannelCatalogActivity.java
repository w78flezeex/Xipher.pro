package com.xipher.wrapper.ui;

import android.content.Intent;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextUtils;
import android.text.TextWatcher;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.bumptech.glide.Glide;
import com.google.android.material.chip.Chip;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.ActivityChannelCatalogBinding;
import com.xipher.wrapper.databinding.ItemCatalogChannelBinding;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.ui.fragment.VerificationBottomSheet;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class ChannelCatalogActivity extends AppCompatActivity {

    private ActivityChannelCatalogBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private ChannelAdapter adapter;
    private String currentCategory = "";
    private String currentSearch = "";

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityChannelCatalogBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        authRepo = AppServices.authRepository(this);

        setupUI();
        loadChannels();
    }

    private void setupUI() {
        binding.btnBack.setOnClickListener(v -> finish());
        binding.btnVerification.setOnClickListener(v -> {
            VerificationBottomSheet sheet = VerificationBottomSheet.newInstance();
            sheet.show(getSupportFragmentManager(), "verification");
        });

        // RecyclerView
        adapter = new ChannelAdapter();
        binding.rvChannels.setLayoutManager(new LinearLayoutManager(this));
        binding.rvChannels.setAdapter(adapter);

        // Search
        binding.etSearch.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}

            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {}

            @Override
            public void afterTextChanged(Editable s) {
                currentSearch = s.toString().trim();
                loadChannels();
            }
        });

        // Category chips
        setupCategoryChips();
    }

    private void setupCategoryChips() {
        View.OnClickListener chipListener = v -> {
            // Uncheck all
            uncheckAllChips();
            ((Chip) v).setChecked(true);
            
            int id = v.getId();
            if (id == R.id.chipAll) currentCategory = "";
            else if (id == R.id.chipNews) currentCategory = "news";
            else if (id == R.id.chipEntertainment) currentCategory = "entertainment";
            else if (id == R.id.chipTech) currentCategory = "tech";
            else if (id == R.id.chipEducation) currentCategory = "education";
            else if (id == R.id.chipMusic) currentCategory = "music";
            else if (id == R.id.chipGaming) currentCategory = "gaming";
            else if (id == R.id.chipSports) currentCategory = "sports";
            else if (id == R.id.chipOther) currentCategory = "other";
            
            loadChannels();
        };

        binding.chipAll.setOnClickListener(chipListener);
        binding.chipNews.setOnClickListener(chipListener);
        binding.chipEntertainment.setOnClickListener(chipListener);
        binding.chipTech.setOnClickListener(chipListener);
        binding.chipEducation.setOnClickListener(chipListener);
        binding.chipMusic.setOnClickListener(chipListener);
        binding.chipGaming.setOnClickListener(chipListener);
        binding.chipSports.setOnClickListener(chipListener);
        binding.chipOther.setOnClickListener(chipListener);
    }

    private void uncheckAllChips() {
        binding.chipAll.setChecked(false);
        binding.chipNews.setChecked(false);
        binding.chipEntertainment.setChecked(false);
        binding.chipTech.setChecked(false);
        binding.chipEducation.setChecked(false);
        binding.chipMusic.setChecked(false);
        binding.chipGaming.setChecked(false);
        binding.chipSports.setChecked(false);
        binding.chipOther.setChecked(false);
    }

    private void loadChannels() {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) return;

        binding.progressBar.setVisibility(View.VISIBLE);
        binding.emptyState.setVisibility(View.GONE);

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.getPublicChannels(token, currentCategory, currentSearch);
                
                runOnUiThread(() -> {
                    binding.progressBar.setVisibility(View.GONE);
                    
                    if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                        // API returns "items" array with both groups and channels
                        JsonArray itemsArray = res.has("items") ? res.getAsJsonArray("items") : new JsonArray();
                        List<ChannelItem> items = new ArrayList<>();
                        
                        for (int i = 0; i < itemsArray.size(); i++) {
                            JsonObject ch = itemsArray.get(i).getAsJsonObject();
                            // Filter to only show channels (not groups)
                            String type = ch.has("type") ? ch.get("type").getAsString() : "";
                            if (!"channel".equals(type)) continue;
                            
                            ChannelItem item = new ChannelItem();
                            item.id = ch.has("id") ? ch.get("id").getAsString() : "";
                            item.name = ch.has("name") ? ch.get("name").getAsString() : "";
                            item.username = ch.has("username") ? ch.get("username").getAsString() : "";
                            item.description = ch.has("description") ? ch.get("description").getAsString() : "";
                            item.category = ch.has("category") ? ch.get("category").getAsString() : "";
                            item.subscribersCount = ch.has("members_count") ? ch.get("members_count").getAsInt() : 0;
                            item.isVerified = ch.has("verified") && ch.get("verified").getAsBoolean();
                            item.avatarUrl = ch.has("avatar_url") ? ch.get("avatar_url").getAsString() : "";
                            items.add(item);
                        }
                        
                        adapter.setItems(items);
                        
                        if (items.isEmpty()) {
                            binding.emptyState.setVisibility(View.VISIBLE);
                            binding.tvEmptyTitle.setText(TextUtils.isEmpty(currentSearch) 
                                ? R.string.catalog_empty 
                                : R.string.catalog_empty_search);
                        }
                    } else {
                        Toast.makeText(this, R.string.error_network, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                runOnUiThread(() -> {
                    binding.progressBar.setVisibility(View.GONE);
                    Toast.makeText(this, R.string.error_network, Toast.LENGTH_SHORT).show();
                });
            }
        });
    }

    private void openChannel(ChannelItem item) {
        Intent intent = new Intent(this, ChatDetailActivity.class);
        intent.putExtra(ChatDetailActivity.EXTRA_CHAT_ID, item.id);
        intent.putExtra(ChatDetailActivity.EXTRA_CHAT_TITLE, item.name);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_CHANNEL, true);
        intent.putExtra(ChatDetailActivity.EXTRA_IS_GROUP, false);
        startActivity(intent);
    }

    private void joinChannel(ChannelItem item) {
        String token = authRepo.getToken();
        if (TextUtils.isEmpty(token)) return;

        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject res = client.joinChannel(token, item.id);
                runOnUiThread(() -> {
                    if (res != null && res.has("success") && res.get("success").getAsBoolean()) {
                        Toast.makeText(this, R.string.channel_joined, Toast.LENGTH_SHORT).show();
                        openChannel(item);
                    } else {
                        String msg = res != null && res.has("message") ? res.get("message").getAsString() : getString(R.string.error_network);
                        Toast.makeText(this, msg, Toast.LENGTH_SHORT).show();
                    }
                });
            } catch (Exception e) {
                runOnUiThread(() -> Toast.makeText(this, R.string.error_network, Toast.LENGTH_SHORT).show());
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        io.shutdownNow();
    }

    // Data class
    static class ChannelItem {
        String id;
        String name;
        String username;
        String description;
        String category;
        int subscribersCount;
        boolean isVerified;
        String avatarUrl;
    }

    // Adapter
    class ChannelAdapter extends RecyclerView.Adapter<ChannelAdapter.ViewHolder> {
        private List<ChannelItem> items = new ArrayList<>();

        void setItems(List<ChannelItem> newItems) {
            items = newItems;
            notifyDataSetChanged();
        }

        @NonNull
        @Override
        public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            ItemCatalogChannelBinding b = ItemCatalogChannelBinding.inflate(
                LayoutInflater.from(parent.getContext()), parent, false);
            return new ViewHolder(b);
        }

        @Override
        public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
            holder.bind(items.get(position));
        }

        @Override
        public int getItemCount() {
            return items.size();
        }

        class ViewHolder extends RecyclerView.ViewHolder {
            private final ItemCatalogChannelBinding b;

            ViewHolder(ItemCatalogChannelBinding binding) {
                super(binding.getRoot());
                this.b = binding;
            }

            void bind(ChannelItem item) {
                b.tvName.setText(item.name);
                b.tvUsername.setText("@" + item.username);
                b.tvSubscribers.setText(formatSubscribers(item.subscribersCount));
                b.verifiedBadge.setVisibility(item.isVerified ? View.VISIBLE : View.GONE);

                // Category
                if (!TextUtils.isEmpty(item.category)) {
                    b.tvCategory.setVisibility(View.VISIBLE);
                    b.tvCategory.setText(getCategoryName(item.category));
                } else {
                    b.tvCategory.setVisibility(View.GONE);
                }

                // Description
                if (!TextUtils.isEmpty(item.description)) {
                    b.tvDescription.setVisibility(View.VISIBLE);
                    b.tvDescription.setText(item.description);
                } else {
                    b.tvDescription.setVisibility(View.GONE);
                }

                // Avatar
                String avatarUrl = item.avatarUrl;
                if (!TextUtils.isEmpty(avatarUrl)) {
                    // Make absolute URL if relative
                    if (avatarUrl.startsWith("/")) {
                        avatarUrl = "https://xipher.pro" + avatarUrl;
                    }
                    b.ivAvatar.setVisibility(View.VISIBLE);
                    b.tvAvatarLetter.setVisibility(View.GONE);
                    Glide.with(b.ivAvatar)
                        .load(avatarUrl)
                        .circleCrop()
                        .into(b.ivAvatar);
                } else {
                    b.ivAvatar.setVisibility(View.GONE);
                    b.tvAvatarLetter.setVisibility(View.VISIBLE);
                    b.tvAvatarLetter.setText(item.name.isEmpty() ? "?" : item.name.substring(0, 1).toUpperCase());
                }

                // Click handlers
                itemView.setOnClickListener(v -> openChannel(item));
                b.btnJoin.setOnClickListener(v -> joinChannel(item));
            }

            private String formatSubscribers(int count) {
                if (count >= 1000000) {
                    return String.format("%.1fM", count / 1000000.0);
                } else if (count >= 1000) {
                    return String.format("%.1fK", count / 1000.0);
                }
                return String.valueOf(count);
            }

            private String getCategoryName(String category) {
                switch (category.toLowerCase()) {
                    case "news": return getString(R.string.catalog_category_news);
                    case "entertainment": return getString(R.string.catalog_category_entertainment);
                    case "tech": return getString(R.string.catalog_category_tech);
                    case "education": return getString(R.string.catalog_category_education);
                    case "music": return getString(R.string.catalog_category_music);
                    case "gaming": return getString(R.string.catalog_category_gaming);
                    case "sports": return getString(R.string.catalog_category_sports);
                    default: return getString(R.string.catalog_category_other);
                }
            }
        }
    }
}
