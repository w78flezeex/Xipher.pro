package com.xipher.wrapper.ui;

import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.model.TopicDto;
import com.xipher.wrapper.di.AppServices;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Helper class for managing forum topic tabs UI
 * Similar to web version's topics.js
 */
public class TopicTabsHelper {
    private static final String TAG = "TopicTabsHelper";

    private final Context context;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final Gson gson = new Gson();

    private LinearLayout tabsContainer;
    private HorizontalScrollView scrollView;
    private FrameLayout addButton;

    private List<TopicDto> currentTopics = new ArrayList<>();
    private String currentGroupId;
    private String selectedTopicId;
    private String userRole = "member";

    private TopicTabListener listener;

    public interface TopicTabListener {
        void onTopicSelected(TopicDto topic);
        void onCreateTopicClicked();
    }

    public TopicTabsHelper(@NonNull Context context) {
        this.context = context;
    }

    public void setListener(TopicTabListener listener) {
        this.listener = listener;
    }

    public void setUserRole(String role) {
        this.userRole = role != null ? role : "member";
        updateAddButtonVisibility();
    }

    /**
     * Attach to a ViewGroup and create topic tabs UI
     */
    public void attachToContainer(@NonNull ViewGroup parent) {
        // Inflate the topic tabs view
        View tabsView = LayoutInflater.from(context).inflate(R.layout.view_topic_tabs, parent, false);
        scrollView = (HorizontalScrollView) tabsView;
        tabsContainer = tabsView.findViewById(R.id.topicTabsContainer);
        addButton = tabsView.findViewById(R.id.topicAddButton);

        if (addButton != null) {
            addButton.setOnClickListener(v -> {
                if (listener != null) {
                    listener.onCreateTopicClicked();
                }
            });
        }

        // Insert at position 0 or after header
        parent.addView(tabsView, 0);
    }

    /**
     * Show topic tabs for a forum group
     */
    public void showTopics(String groupId, List<TopicDto> topics) {
        this.currentGroupId = groupId;
        this.currentTopics = topics != null ? topics : new ArrayList<>();
        this.selectedTopicId = null;

        if (scrollView == null || tabsContainer == null) {
            Log.w(TAG, "Tabs container not attached");
            return;
        }

        // Show the scroll view
        scrollView.setVisibility(View.VISIBLE);

        // Clear existing tabs (except add button)
        for (int i = tabsContainer.getChildCount() - 1; i >= 0; i--) {
            View child = tabsContainer.getChildAt(i);
            if (child.getId() != R.id.topicAddButton) {
                tabsContainer.removeViewAt(i);
            }
        }

        // Sort topics: General first, then pinned, then by last message
        List<TopicDto> sortedTopics = new ArrayList<>(currentTopics);
        sortedTopics.sort((a, b) -> {
            if (a.is_general) return -1;
            if (b.is_general) return 1;
            if (a.pinned_order > 0 && b.pinned_order == 0) return -1;
            if (a.pinned_order == 0 && b.pinned_order > 0) return 1;
            // By last message time (descending)
            if (a.last_message_at != null && b.last_message_at != null) {
                return b.last_message_at.compareTo(a.last_message_at);
            }
            return 0;
        });

        // Add topic tabs
        int addButtonIndex = tabsContainer.indexOfChild(addButton);
        if (addButtonIndex < 0) addButtonIndex = tabsContainer.getChildCount();

        for (int i = 0; i < sortedTopics.size(); i++) {
            TopicDto topic = sortedTopics.get(i);
            if (topic.is_hidden) continue;

            View tabView = createTopicTab(topic);
            tabsContainer.addView(tabView, i);
        }

        updateAddButtonVisibility();
    }

    /**
     * Create a single topic tab view
     */
    private View createTopicTab(TopicDto topic) {
        View tabView = LayoutInflater.from(context).inflate(R.layout.item_topic_tab, tabsContainer, false);

        LinearLayout content = tabView.findViewById(R.id.topicTabContent);
        TextView iconView = tabView.findViewById(R.id.topicIcon);
        TextView nameView = tabView.findViewById(R.id.topicName);
        TextView badgeView = tabView.findViewById(R.id.topicBadge);

        // Set emoji icon
        iconView.setText(topic.getIconEmoji());

        // Set topic name
        nameView.setText(topic.name);

        // Set unread badge
        if (topic.unread_count > 0) {
            badgeView.setVisibility(View.VISIBLE);
            badgeView.setText(topic.unread_count > 99 ? "99+" : String.valueOf(topic.unread_count));
        } else {
            badgeView.setVisibility(View.GONE);
        }

        // Set icon background color
        GradientDrawable iconBg = new GradientDrawable();
        iconBg.setShape(GradientDrawable.OVAL);
        try {
            iconBg.setColor(Color.parseColor(topic.getIconColor()));
        } catch (Exception e) {
            iconBg.setColor(0xFF6FB1FC);
        }
        iconView.setBackground(iconBg);

        // Initial state (not selected)
        updateTabState(content, nameView, false);

        // Store topic id in tag
        tabView.setTag(topic.id);

        // Click listener
        content.setOnClickListener(v -> {
            selectTopic(topic);
        });

        return tabView;
    }

    /**
     * Select a topic and update UI
     */
    public void selectTopic(TopicDto topic) {
        String oldId = selectedTopicId;
        selectedTopicId = topic.id;

        // Update all tabs
        for (int i = 0; i < tabsContainer.getChildCount(); i++) {
            View child = tabsContainer.getChildAt(i);
            if (child.getId() == R.id.topicAddButton) continue;

            String tabTopicId = (String) child.getTag();
            LinearLayout content = child.findViewById(R.id.topicTabContent);
            TextView nameView = child.findViewById(R.id.topicName);

            boolean isSelected = tabTopicId != null && tabTopicId.equals(selectedTopicId);
            updateTabState(content, nameView, isSelected);
        }

        // Notify listener
        if (listener != null) {
            listener.onTopicSelected(topic);
        }
    }

    /**
     * Update visual state of a tab
     */
    private void updateTabState(LinearLayout content, TextView nameView, boolean isSelected) {
        content.setSelected(isSelected);

        GradientDrawable bg = new GradientDrawable();
        bg.setShape(GradientDrawable.RECTANGLE);
        bg.setCornerRadius(40f);

        if (isSelected) {
            nameView.setTextColor(Color.WHITE);
            bg.setColor(0xFF229ED9); // Telegram blue
        } else {
            try {
                nameView.setTextColor(context.getResources().getColor(R.color.x_text_secondary, null));
                bg.setColor(context.getResources().getColor(R.color.x_bg_secondary, null));
            } catch (Exception e) {
                nameView.setTextColor(0xFFAAAAAA);
                bg.setColor(0xFF2A2A2A);
            }
        }

        content.setBackground(bg);
    }

    /**
     * Hide topic tabs
     */
    public void hide() {
        if (scrollView != null) {
            scrollView.setVisibility(View.GONE);
        }
        selectedTopicId = null;
        currentTopics.clear();
        currentGroupId = null;
    }

    /**
     * Check if tabs are visible
     */
    public boolean isVisible() {
        return scrollView != null && scrollView.getVisibility() == View.VISIBLE;
    }

    /**
     * Get current selected topic ID
     */
    @Nullable
    public String getSelectedTopicId() {
        return selectedTopicId;
    }

    /**
     * Get current group ID
     */
    @Nullable
    public String getCurrentGroupId() {
        return currentGroupId;
    }

    /**
     * Get current topics list
     */
    public List<TopicDto> getCurrentTopics() {
        return currentTopics;
    }

    private void updateAddButtonVisibility() {
        if (addButton != null) {
            boolean canCreate = "creator".equals(userRole) || "admin".equals(userRole);
            addButton.setVisibility(canCreate ? View.VISIBLE : View.GONE);
        }
    }

    /**
     * Load topics from server for a group
     */
    public void loadTopicsFromServer(String token, String groupId, TopicsLoadCallback callback) {
        io.execute(() -> {
            try {
                ApiClient api = new ApiClient(token);
                JsonObject response = api.getGroupTopics(token, groupId);

                if (response.has("success") && response.get("success").getAsBoolean()) {
                    JsonArray topicsArray = response.has("topics") ? response.getAsJsonArray("topics") : null;
                    List<TopicDto> topics = new ArrayList<>();

                    if (topicsArray != null) {
                        for (int i = 0; i < topicsArray.size(); i++) {
                            TopicDto topic = gson.fromJson(topicsArray.get(i), TopicDto.class);
                            topics.add(topic);
                        }
                    }

                    boolean forumMode = response.has("forum_mode") && response.get("forum_mode").getAsBoolean();

                    if (callback != null) {
                        callback.onTopicsLoaded(topics, forumMode);
                    }
                } else {
                    String error = response.has("error") ? response.get("error").getAsString() : "Unknown error";
                    if (callback != null) {
                        callback.onError(error);
                    }
                }
            } catch (Exception e) {
                Log.e(TAG, "Failed to load topics", e);
                if (callback != null) {
                    callback.onError(e.getMessage());
                }
            }
        });
    }

    public interface TopicsLoadCallback {
        void onTopicsLoaded(List<TopicDto> topics, boolean forumMode);
        void onError(String error);
    }
}
