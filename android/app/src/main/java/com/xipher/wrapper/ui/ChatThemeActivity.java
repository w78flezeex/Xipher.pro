package com.xipher.wrapper.ui;

import android.Manifest;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.bottomsheet.BottomSheetDialog;
import com.google.android.material.slider.Slider;
import com.xipher.wrapper.R;
import com.xipher.wrapper.databinding.ActivityChatThemeBinding;
import com.xipher.wrapper.theme.ChatThemeManager;

public class ChatThemeActivity extends AppCompatActivity {
    private ActivityChatThemeBinding binding;
    private String selectedTheme;
    private ThemeAdapter lightAdapter;
    private ThemeAdapter darkAdapter;
    private ThemeAdapter gradientAdapter;

    private final ActivityResultLauncher<Intent> galleryLauncher = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(),
            result -> {
                if (result.getResultCode() == RESULT_OK && result.getData() != null) {
                    Uri imageUri = result.getData().getData();
                    if (imageUri != null) {
                        handleSelectedImage(imageUri);
                    }
                }
            });

    private final ActivityResultLauncher<String> permissionLauncher = registerForActivityResult(
            new ActivityResultContracts.RequestPermission(),
            isGranted -> {
                if (isGranted) {
                    openGallery();
                } else {
                    Toast.makeText(this, R.string.permission_gallery_denied, Toast.LENGTH_SHORT).show();
                }
            });

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityChatThemeBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        selectedTheme = ChatThemeManager.getTheme(this);

        setupToolbar();
        setupPreview();
        setupThemeGrids();
        setupCustomWallpaper();
        setupWallpaperSettings();
        
        BottomNavHelper.setup(this, binding.bottomNav, R.id.nav_settings);
    }

    private void setupToolbar() {
        binding.btnBack.setOnClickListener(v -> finish());
    }

    private void setupPreview() {
        updatePreview();
    }

    private void updatePreview() {
        if (ChatThemeManager.THEME_CUSTOM.equals(selectedTheme) && ChatThemeManager.hasCustomWallpaper(this)) {
            Bitmap wallpaper = ChatThemeManager.getCustomWallpaper(this);
            if (wallpaper != null) {
                binding.previewBackground.setImageBitmap(wallpaper);
                binding.previewBackground.setScaleType(ImageView.ScaleType.CENTER_CROP);
            }
        } else {
            binding.previewBackground.setImageResource(ChatThemeManager.getBackgroundResForTheme(selectedTheme));
            binding.previewBackground.setScaleType(ImageView.ScaleType.CENTER_CROP);
        }

        // Apply dim overlay
        int dim = ChatThemeManager.getWallpaperDim(this);
        binding.previewDimOverlay.setAlpha(dim / 100f);

        // Update custom wallpaper button state
        boolean hasCustom = ChatThemeManager.hasCustomWallpaper(this);
        binding.btnDeleteWallpaper.setVisibility(hasCustom ? View.VISIBLE : View.GONE);
        binding.customWallpaperPreview.setVisibility(hasCustom ? View.VISIBLE : View.GONE);
        
        if (hasCustom) {
            Bitmap thumb = ChatThemeManager.getCustomWallpaper(this);
            if (thumb != null) {
                binding.customWallpaperPreview.setImageBitmap(thumb);
            }
            binding.customWallpaperCheck.setVisibility(
                ChatThemeManager.THEME_CUSTOM.equals(selectedTheme) ? View.VISIBLE : View.GONE);
        }

        // Update adapters
        if (lightAdapter != null) lightAdapter.setSelectedTheme(selectedTheme);
        if (darkAdapter != null) darkAdapter.setSelectedTheme(selectedTheme);
        if (gradientAdapter != null) gradientAdapter.setSelectedTheme(selectedTheme);
    }

    private void setupThemeGrids() {
        // Light themes
        binding.recyclerLightThemes.setLayoutManager(new GridLayoutManager(this, 3));
        lightAdapter = new ThemeAdapter(ChatThemeManager.getLightThemes(), this::onThemeSelected);
        lightAdapter.setSelectedTheme(selectedTheme);
        binding.recyclerLightThemes.setAdapter(lightAdapter);

        // Dark themes
        binding.recyclerDarkThemes.setLayoutManager(new GridLayoutManager(this, 3));
        darkAdapter = new ThemeAdapter(ChatThemeManager.getDarkThemes(), this::onThemeSelected);
        darkAdapter.setSelectedTheme(selectedTheme);
        binding.recyclerDarkThemes.setAdapter(darkAdapter);

        // Gradient themes
        binding.recyclerGradientThemes.setLayoutManager(new GridLayoutManager(this, 3));
        gradientAdapter = new ThemeAdapter(ChatThemeManager.getGradientThemes(), this::onThemeSelected);
        gradientAdapter.setSelectedTheme(selectedTheme);
        binding.recyclerGradientThemes.setAdapter(gradientAdapter);
    }

    private void setupCustomWallpaper() {
        binding.btnUploadWallpaper.setOnClickListener(v -> checkPermissionAndOpenGallery());
        
        binding.customWallpaperCard.setOnClickListener(v -> {
            if (ChatThemeManager.hasCustomWallpaper(this)) {
                selectedTheme = ChatThemeManager.THEME_CUSTOM;
                ChatThemeManager.setTheme(this, selectedTheme);
                updatePreview();
            }
        });

        binding.btnDeleteWallpaper.setOnClickListener(v -> {
            ChatThemeManager.deleteCustomWallpaper(this);
            selectedTheme = ChatThemeManager.THEME_DEFAULT;
            updatePreview();
            Toast.makeText(this, R.string.wallpaper_deleted, Toast.LENGTH_SHORT).show();
        });
    }

    private void setupWallpaperSettings() {
        // Dim slider
        binding.sliderDim.setValue(ChatThemeManager.getWallpaperDim(this));
        binding.sliderDim.addOnChangeListener((slider, value, fromUser) -> {
            if (fromUser) {
                ChatThemeManager.setWallpaperDim(this, (int) value);
                binding.previewDimOverlay.setAlpha(value / 100f);
            }
        });

        // Blur slider
        binding.sliderBlur.setValue(ChatThemeManager.getWallpaperBlur(this));
        binding.sliderBlur.addOnChangeListener((slider, value, fromUser) -> {
            if (fromUser) {
                ChatThemeManager.setWallpaperBlur(this, (int) value);
                // Note: Real blur would require RenderScript or similar
            }
        });
    }

    private void checkPermissionAndOpenGallery() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_MEDIA_IMAGES) 
                    == PackageManager.PERMISSION_GRANTED) {
                openGallery();
            } else {
                permissionLauncher.launch(Manifest.permission.READ_MEDIA_IMAGES);
            }
        } else {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.READ_EXTERNAL_STORAGE) 
                    == PackageManager.PERMISSION_GRANTED) {
                openGallery();
            } else {
                permissionLauncher.launch(Manifest.permission.READ_EXTERNAL_STORAGE);
            }
        }
    }

    private void openGallery() {
        Intent intent = new Intent(Intent.ACTION_PICK);
        intent.setType("image/*");
        galleryLauncher.launch(intent);
    }

    private void handleSelectedImage(Uri imageUri) {
        boolean success = ChatThemeManager.saveCustomWallpaper(this, imageUri);
        if (success) {
            selectedTheme = ChatThemeManager.THEME_CUSTOM;
            updatePreview();
            Toast.makeText(this, R.string.wallpaper_set, Toast.LENGTH_SHORT).show();
        } else {
            Toast.makeText(this, R.string.wallpaper_error, Toast.LENGTH_SHORT).show();
        }
    }

    private void onThemeSelected(ChatThemeManager.ThemeInfo theme) {
        selectedTheme = theme.id;
        ChatThemeManager.setTheme(this, selectedTheme);
        updatePreview();
    }

    // ============ Theme Adapter ============

    private static class ThemeAdapter extends RecyclerView.Adapter<ThemeAdapter.ThemeViewHolder> {
        private final ChatThemeManager.ThemeInfo[] themes;
        private final OnThemeClickListener listener;
        private String selectedTheme = "";

        interface OnThemeClickListener {
            void onThemeClick(ChatThemeManager.ThemeInfo theme);
        }

        ThemeAdapter(ChatThemeManager.ThemeInfo[] themes, OnThemeClickListener listener) {
            this.themes = themes;
            this.listener = listener;
        }

        void setSelectedTheme(String themeId) {
            this.selectedTheme = themeId;
            notifyDataSetChanged();
        }

        @NonNull
        @Override
        public ThemeViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            View view = LayoutInflater.from(parent.getContext())
                    .inflate(R.layout.item_theme_preview, parent, false);
            return new ThemeViewHolder(view);
        }

        @Override
        public void onBindViewHolder(@NonNull ThemeViewHolder holder, int position) {
            ChatThemeManager.ThemeInfo theme = themes[position];
            holder.bind(theme, selectedTheme.equals(theme.id), listener);
        }

        @Override
        public int getItemCount() {
            return themes.length;
        }

        static class ThemeViewHolder extends RecyclerView.ViewHolder {
            private final ImageView preview;
            private final TextView name;
            private final View checkMark;
            private final View selectedBorder;

            ThemeViewHolder(@NonNull View itemView) {
                super(itemView);
                preview = itemView.findViewById(R.id.themePreviewImage);
                name = itemView.findViewById(R.id.themePreviewName);
                checkMark = itemView.findViewById(R.id.themeCheckMark);
                selectedBorder = itemView.findViewById(R.id.themeSelectedBorder);
            }

            void bind(ChatThemeManager.ThemeInfo theme, boolean isSelected, OnThemeClickListener listener) {
                preview.setImageResource(theme.previewRes);
                name.setText(theme.nameRes);
                
                checkMark.setVisibility(isSelected ? View.VISIBLE : View.GONE);
                selectedBorder.setVisibility(isSelected ? View.VISIBLE : View.GONE);

                itemView.setOnClickListener(v -> listener.onThemeClick(theme));
            }
        }
    }
}
