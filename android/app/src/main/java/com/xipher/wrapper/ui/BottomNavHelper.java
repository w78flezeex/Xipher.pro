package com.xipher.wrapper.ui;

import android.content.Intent;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import com.google.android.material.bottomnavigation.BottomNavigationView;
import com.xipher.wrapper.di.AppServices;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.R;

public final class BottomNavHelper {
    private BottomNavHelper() {}

    public static void setup(@NonNull AppCompatActivity activity,
                             BottomNavigationView nav,
                             int selectedItemId) {
        if (nav == null) return;
        
        // Apply window insets for gesture navigation
        ViewCompat.setOnApplyWindowInsetsListener(nav, (v, windowInsets) -> {
            Insets insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars());
            ViewGroup.MarginLayoutParams params = (ViewGroup.MarginLayoutParams) v.getLayoutParams();
            params.bottomMargin = 0;
            v.setLayoutParams(params);
            v.setPadding(v.getPaddingLeft(), v.getPaddingTop(), v.getPaddingRight(), insets.bottom);
            return windowInsets;
        });
        
        // Set listener to null first to prevent triggering during setSelectedItemId
        nav.setOnItemSelectedListener(null);
        nav.setOnItemReselectedListener(null);
        
        // Now set the selected item without triggering navigation
        nav.setSelectedItemId(selectedItemId);
        
        // Now set up the listeners
        nav.setOnItemSelectedListener(item -> {
            int id = item.getItemId();
            if (id == selectedItemId) {
                return true;
            }
            Intent intent = null;
            if (id == R.id.nav_chats) {
                intent = new Intent(activity, ChatActivity.class);
            } else if (id == R.id.nav_calls) {
                intent = new Intent(activity, CallsActivity.class);
            } else if (id == R.id.nav_settings) {
                intent = new Intent(activity, SettingsActivity.class);
            } else if (id == R.id.nav_profile) {
                intent = new Intent(activity, ProfileActivity.class);
                try {
                    AuthRepository repo = AppServices.authRepository(activity);
                    if (repo != null && repo.getUserId() != null) {
                        intent.putExtra(ProfileActivity.EXTRA_USER_ID, repo.getUserId());
                    }
                } catch (Exception ignored) {}
            }
            if (intent != null) {
                intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
                activity.startActivity(intent);
                activity.overridePendingTransition(0, 0);
            }
            return true;
        });
        nav.setOnItemReselectedListener(item -> openRootIfNeeded(activity, item.getItemId()));
    }

    private static boolean openRootIfNeeded(@NonNull AppCompatActivity activity, int id) {
        if (id == R.id.nav_chats && !(activity instanceof ChatActivity)) {
            activity.startActivity(new Intent(activity, ChatActivity.class)
                    .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP));
            activity.overridePendingTransition(0, 0);
            return true;
        }
        if (id == R.id.nav_calls && !(activity instanceof CallsActivity)) {
            activity.startActivity(new Intent(activity, CallsActivity.class)
                    .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP));
            activity.overridePendingTransition(0, 0);
            return true;
        }
        if (id == R.id.nav_settings && !(activity instanceof SettingsActivity)) {
            activity.startActivity(new Intent(activity, SettingsActivity.class)
                    .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP));
            activity.overridePendingTransition(0, 0);
            return true;
        }
        if (id == R.id.nav_profile && !(activity instanceof ProfileActivity)) {
            Intent intent = new Intent(activity, ProfileActivity.class)
                    .addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            try {
                AuthRepository repo = AppServices.authRepository(activity);
                if (repo != null && repo.getUserId() != null) {
                    intent.putExtra(ProfileActivity.EXTRA_USER_ID, repo.getUserId());
                }
            } catch (Exception ignored) {}
            activity.startActivity(intent);
            activity.overridePendingTransition(0, 0);
            return true;
        }
        return true;
    }
}
