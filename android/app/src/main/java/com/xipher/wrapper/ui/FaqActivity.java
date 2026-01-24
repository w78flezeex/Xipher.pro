package com.xipher.wrapper.ui;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.xipher.wrapper.databinding.ActivityFaqBinding;
import com.xipher.wrapper.R;

public class FaqActivity extends AppCompatActivity {
    private ActivityFaqBinding binding;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityFaqBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        binding.btnBack.setOnClickListener(v -> finish());
        BottomNavHelper.setup(this, binding.bottomNav, R.id.nav_settings);
    }
}
