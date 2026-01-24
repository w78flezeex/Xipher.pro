package com.xipher.wrapper.ui.adapter;

import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.xipher.wrapper.R;
import com.xipher.wrapper.data.model.TopicDto;

import java.util.ArrayList;
import java.util.List;

/**
 * Adapter for horizontal topic tabs (Telegram-style forum topics)
 */
public class TopicTabsAdapter extends RecyclerView.Adapter<TopicTabsAdapter.TopicTabViewHolder> {

    private List<TopicDto> topics = new ArrayList<>();
    private String selectedTopicId = null;
    private OnTopicSelectedListener listener;

    public interface OnTopicSelectedListener {
        void onTopicSelected(TopicDto topic);
    }

    public TopicTabsAdapter(OnTopicSelectedListener listener) {
        this.listener = listener;
    }

    public void setTopics(List<TopicDto> topics) {
        this.topics = topics != null ? topics : new ArrayList<>();
        notifyDataSetChanged();
    }

    public void setSelectedTopicId(String topicId) {
        String oldId = this.selectedTopicId;
        this.selectedTopicId = topicId;
        
        // Update old and new selected items
        for (int i = 0; i < topics.size(); i++) {
            TopicDto t = topics.get(i);
            if ((oldId != null && oldId.equals(t.id)) || (topicId != null && topicId.equals(t.id))) {
                notifyItemChanged(i);
            }
        }
    }

    public String getSelectedTopicId() {
        return selectedTopicId;
    }

    @NonNull
    @Override
    public TopicTabViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_topic_tab, parent, false);
        return new TopicTabViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull TopicTabViewHolder holder, int position) {
        TopicDto topic = topics.get(position);
        holder.bind(topic, selectedTopicId != null && selectedTopicId.equals(topic.id));
    }

    @Override
    public int getItemCount() {
        return topics.size();
    }

    class TopicTabViewHolder extends RecyclerView.ViewHolder {
        private final FrameLayout root;
        private final LinearLayout content;
        private final TextView iconView;
        private final TextView nameView;
        private final TextView badgeView;

        TopicTabViewHolder(@NonNull View itemView) {
            super(itemView);
            root = itemView.findViewById(R.id.topicTabRoot);
            content = itemView.findViewById(R.id.topicTabContent);
            iconView = itemView.findViewById(R.id.topicIcon);
            nameView = itemView.findViewById(R.id.topicName);
            badgeView = itemView.findViewById(R.id.topicBadge);
        }

        void bind(TopicDto topic, boolean isSelected) {
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
            
            // Update selection state
            content.setSelected(isSelected);
            
            // Set colors based on selection
            if (isSelected) {
                nameView.setTextColor(Color.WHITE);
                // Use accent color as background
                GradientDrawable bg = new GradientDrawable();
                bg.setShape(GradientDrawable.RECTANGLE);
                bg.setCornerRadius(40f);
                try {
                    bg.setColor(Color.parseColor("#229ED9")); // Telegram blue
                } catch (Exception e) {
                    bg.setColor(0xFF229ED9);
                }
                content.setBackground(bg);
            } else {
                nameView.setTextColor(itemView.getContext().getResources().getColor(R.color.x_text_secondary, null));
                // Use secondary background
                GradientDrawable bg = new GradientDrawable();
                bg.setShape(GradientDrawable.RECTANGLE);
                bg.setCornerRadius(40f);
                bg.setColor(itemView.getContext().getResources().getColor(R.color.x_bg_secondary, null));
                content.setBackground(bg);
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
            
            // Click listener
            content.setOnClickListener(v -> {
                if (listener != null) {
                    setSelectedTopicId(topic.id);
                    listener.onTopicSelected(topic);
                }
            });
        }
    }
}
