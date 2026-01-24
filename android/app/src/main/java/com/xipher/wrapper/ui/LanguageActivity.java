package com.xipher.wrapper.ui;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.app.AppCompatDelegate;
import androidx.core.os.LocaleListCompat;

import com.xipher.wrapper.databinding.ActivityLanguageBinding;
import com.xipher.wrapper.R;

import java.util.Locale;

public class LanguageActivity extends AppCompatActivity {
    private ActivityLanguageBinding binding;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityLanguageBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        binding.btnBack.setOnClickListener(v -> finish());

        selectCurrent();
        binding.radioGroup.setOnCheckedChangeListener((group, checkedId) -> {
            if (checkedId == binding.rbRu.getId()) {
                applyLanguage("ru");
            } else if (checkedId == binding.rbEn.getId()) {
                applyLanguage("en");
            }
        });

        BottomNavHelper.setup(this, binding.bottomNav, R.id.nav_settings);
    }

    private void selectCurrent() {
        LocaleListCompat locales = AppCompatDelegate.getApplicationLocales();
        String lang;
        if (locales == null || locales.isEmpty()) {
            lang = Locale.getDefault().getLanguage();
        } else {
            Locale locale = locales.get(0);
            lang = locale != null ? locale.getLanguage() : Locale.getDefault().getLanguage();
        }
        if (lang != null && lang.startsWith("ru")) {
            binding.rbRu.setChecked(true);
        } else {
            binding.rbEn.setChecked(true);
        }
    }

    private void applyLanguage(String lang) {
        AppCompatDelegate.setApplicationLocales(LocaleListCompat.forLanguageTags(lang));
        recreate();
    }
}
