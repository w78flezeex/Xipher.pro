package com.xipher.wrapper.ui;

import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;

import com.xipher.wrapper.databinding.ActivityChatDetailBinding;
import com.xipher.wrapper.ui.fragment.ChatFragment;
import com.xipher.wrapper.R;

public class ChatDetailActivity extends AppCompatActivity {
    public static final String EXTRA_CHAT_ID = "chat_id";
    public static final String EXTRA_CHAT_TITLE = "chat_title";
    public static final String EXTRA_IS_GROUP = "is_group";
    public static final String EXTRA_IS_CHANNEL = "is_channel";
    public static final String EXTRA_IS_SAVED = "is_saved";
    public static final String EXTRA_UNREAD_COUNT = "unread_count";

    private ActivityChatDetailBinding binding;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        // Ensure database is initialized
        try {
            com.xipher.wrapper.data.db.AppDatabase.get(this);
        } catch (IllegalStateException e) {
            try {
                String key = com.xipher.wrapper.security.DbKeyStore.getOrCreateMainKey(this);
                com.xipher.wrapper.data.db.DatabaseManager.getInstance().init(getApplicationContext(), key);
            } catch (Exception ignored) {}
        }
        
        try {
            binding = ActivityChatDetailBinding.inflate(getLayoutInflater());
            setContentView(binding.getRoot());

            if (savedInstanceState == null) {
                Intent intent = getIntent();
                String chatId = intent.getStringExtra(EXTRA_CHAT_ID);
                String title = intent.getStringExtra(EXTRA_CHAT_TITLE);
                boolean isGroup = intent.getBooleanExtra(EXTRA_IS_GROUP, false);
                boolean isChannel = intent.getBooleanExtra(EXTRA_IS_CHANNEL, false);
                boolean isSaved = intent.getBooleanExtra(EXTRA_IS_SAVED, false);
                int unreadCount = intent.getIntExtra(EXTRA_UNREAD_COUNT, 0);
                android.util.Log.d("ChatDetailActivity", "Opening chat: " + chatId + " title: " + title);
                ChatFragment fragment = ChatFragment.newInstance(chatId, title != null ? title : "", isGroup, isChannel, isSaved);
                // Use commit() instead of commitNow() to allow proper fragment lifecycle
                getSupportFragmentManager()
                        .beginTransaction()
                        .replace(R.id.chatContainer, fragment)
                        .commit();
            }
        } catch (Throwable t) {
            android.util.Log.e("ChatDetailActivity", "onCreate failed", t);
            android.widget.Toast.makeText(this, "Error: " + t.getMessage(), android.widget.Toast.LENGTH_LONG).show();
        }
    }
    
    @Override
    protected void onDestroy() {
        android.util.Log.e("ChatDetailActivity", "onDestroy called - Activity being destroyed!");
        super.onDestroy();
    }
    
    @Override
    protected void onResume() {
        super.onResume();
        // Check for crash log
        try {
            java.io.File crashFile = new java.io.File(getFilesDir(), "crash.log");
            if (crashFile.exists() && crashFile.length() > 0) {
                java.io.BufferedReader reader = new java.io.BufferedReader(new java.io.FileReader(crashFile));
                StringBuilder sb = new StringBuilder();
                String line;
                int lineCount = 0;
                while ((line = reader.readLine()) != null && lineCount < 10) {
                    sb.append(line).append("\n");
                    lineCount++;
                }
                reader.close();
                if (sb.length() > 0) {
                    android.util.Log.e("ChatDetailActivity", "CRASH LOG FOUND:\n" + sb.toString());
                }
            }
        } catch (Exception ignored) {}
    }
}
