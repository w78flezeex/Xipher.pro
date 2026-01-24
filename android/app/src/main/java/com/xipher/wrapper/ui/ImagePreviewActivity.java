package com.xipher.wrapper.ui;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.bumptech.glide.Glide;
import com.xipher.wrapper.databinding.ActivityImagePreviewBinding;

public class ImagePreviewActivity extends AppCompatActivity {
    public static final String EXTRA_IMAGE_URL = "image_url";

    private ActivityImagePreviewBinding binding;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityImagePreviewBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        binding.btnClose.setOnClickListener(v -> finish());

        String url = getIntent() != null ? getIntent().getStringExtra(EXTRA_IMAGE_URL) : null;
        if (url != null && !url.isEmpty()) {
            Glide.with(this).load(url).into(binding.previewImage);
        }
    }
}
