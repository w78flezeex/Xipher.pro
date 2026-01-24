package com.xipher.wrapper.ui.adapter;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.DiffUtil;
import androidx.recyclerview.widget.ListAdapter;
import androidx.recyclerview.widget.RecyclerView;

import com.bumptech.glide.Glide;
import com.bumptech.glide.load.MultiTransformation;
import com.bumptech.glide.load.resource.bitmap.CircleCrop;
import com.xipher.wrapper.BuildConfig;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.db.entity.ChatEntity;
import com.xipher.wrapper.data.prefs.ChatListPrefs;
import com.xipher.wrapper.util.BlurUtils;
import com.xipher.wrapper.util.PremiumGiftCodec;
import com.xipher.wrapper.util.SpoilerCodec;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class ChatListAdapter extends ListAdapter<ChatListItem, RecyclerView.ViewHolder> {

    private static final int VIEW_TYPE_CHAT = 0;
    private static final int VIEW_TYPE_DIVIDER = 1;

    public interface OnChatClick {
        void onClick(ChatEntity chat);
    }

    public interface OnChatLongClick {
        void onLongClick(ChatEntity chat);
    }

    private final OnChatClick listener;
    private OnChatLongClick longClickListener;
    private Set<String> pinnedIds = new HashSet<>();
    private Set<String> mutedIds = new HashSet<>();
    private boolean showUnreadDivider = true;

    public ChatListAdapter(OnChatClick l) {
        super(DIFF);
        this.listener = l;
    }

    public void setOnLongClickListener(OnChatLongClick l) {
        this.longClickListener = l;
    }

    public void setPinnedIds(Set<String> ids) {
        Set<String> next = ids != null ? new HashSet<>(ids) : new HashSet<>();
        if (next.equals(pinnedIds)) return;
        pinnedIds = next;
        notifyDataSetChanged();
    }

    public void setMutedIds(Set<String> ids) {
        Set<String> next = ids != null ? new HashSet<>(ids) : new HashSet<>();
        if (next.equals(mutedIds)) return;
        mutedIds = next;
        notifyDataSetChanged();
    }
    
    public void setShowUnreadDivider(boolean show) {
        this.showUnreadDivider = show;
    }
    
    /**
     * Submit list of ChatEntity and automatically add unread divider
     */
    public void submitChatList(List<ChatEntity> chats) {
        if (chats == null) {
            super.submitList(null);
            return;
        }
        
        List<ChatListItem> items = new ArrayList<>();
        boolean dividerAdded = false;
        boolean hasUnread = false;
        boolean hasRead = false;
        
        // First pass: check if we have both unread and read chats (excluding pinned)
        for (ChatEntity chat : chats) {
            String key = ChatListPrefs.buildKey(chat);
            boolean isPinned = pinnedIds.contains(key);
            // Skip pinned chats for divider logic - they're always at top
            if (isPinned) continue;
            
            if (chat.unread > 0) {
                hasUnread = true;
            } else {
                hasRead = true;
            }
        }
        
        boolean shouldShowDivider = showUnreadDivider && hasUnread && hasRead;
        
        // Second pass: build items list with divider
        boolean inUnreadSection = true;
        for (ChatEntity chat : chats) {
            String key = ChatListPrefs.buildKey(chat);
            boolean isPinned = pinnedIds.contains(key);
            
            // For pinned chats, just add them without divider logic
            if (isPinned) {
                items.add(ChatListItem.chat(chat));
                continue;
            }
            
            // Insert divider before first read chat after unread chats
            if (shouldShowDivider && !dividerAdded && inUnreadSection && chat.unread == 0) {
                items.add(ChatListItem.unreadDivider());
                dividerAdded = true;
                inUnreadSection = false;
            }
            
            items.add(ChatListItem.chat(chat));
            
            if (chat.unread == 0) {
                inUnreadSection = false;
            }
        }
        
        super.submitList(items);
    }

    @Override
    public int getItemViewType(int position) {
        ChatListItem item = getItem(position);
        return item.isDivider() ? VIEW_TYPE_DIVIDER : VIEW_TYPE_CHAT;
    }

    @NonNull
    @Override
    public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        if (viewType == VIEW_TYPE_DIVIDER) {
            View v = LayoutInflater.from(parent.getContext())
                    .inflate(R.layout.item_chat_unread_divider, parent, false);
            return new DividerVH(v);
        } else {
            View v = LayoutInflater.from(parent.getContext())
                    .inflate(R.layout.item_chat, parent, false);
            return new ChatVH(v);
        }
    }

    @Override
    public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int position) {
        ChatListItem item = getItem(position);
        
        if (holder instanceof DividerVH) {
            // Divider doesn't need binding - static text
            return;
        }
        
        if (holder instanceof ChatVH && item.isChat()) {
            ChatEntity c = item.getChat();
            ((ChatVH) holder).bind(c);
            holder.itemView.setOnClickListener(v -> {
                if (listener != null) listener.onClick(c);
            });
            holder.itemView.setOnLongClickListener(v -> {
                if (longClickListener != null) {
                    longClickListener.onLongClick(c);
                    return true;
                }
                return false;
            });
        }
    }

    static class DividerVH extends RecyclerView.ViewHolder {
        DividerVH(@NonNull View itemView) {
            super(itemView);
        }
    }

    class ChatVH extends RecyclerView.ViewHolder {
        TextView avatar;
        ImageView avatarImage;
        TextView title;
        TextView type;
        TextView last;
        TextView unread;
        ImageView pinned;
        ImageView muted;

        ChatVH(@NonNull View itemView) {
            super(itemView);
            avatar = itemView.findViewById(R.id.chatAvatar);
            avatarImage = itemView.findViewById(R.id.chatAvatarImage);
            title = itemView.findViewById(R.id.chatTitle);
            type = itemView.findViewById(R.id.chatType);
            last = itemView.findViewById(R.id.chatLast);
            unread = itemView.findViewById(R.id.chatUnread);
            pinned = itemView.findViewById(R.id.chatPinned);
            muted = itemView.findViewById(R.id.chatMuted);
        }

        void bind(ChatEntity c) {
            boolean privacyEnabled = ChatListPrefs.isChatListPrivacyEnabled(itemView.getContext());
            String letter = "?";
            if (c.title != null && !c.title.isEmpty()) {
                letter = c.title.substring(0, 1).toUpperCase();
            }
            avatar.setText(letter);
            if (c.avatarUrl != null && !c.avatarUrl.isEmpty()) {
                String url = c.avatarUrl.startsWith("http") ? c.avatarUrl : (BuildConfig.API_BASE + c.avatarUrl);
                avatarImage.setVisibility(View.VISIBLE);
                avatar.setVisibility(View.GONE);
                if (privacyEnabled) {
                    Glide.with(avatarImage.getContext())
                            .load(url)
                            .transform(new MultiTransformation<>(new CircleCrop(), BlurUtils.imageBlur()))
                            .into(avatarImage);
                } else {
                    Glide.with(avatarImage.getContext())
                            .load(url)
                            .circleCrop()
                            .into(avatarImage);
                }
            } else {
                avatarImage.setVisibility(View.GONE);
                avatar.setVisibility(View.VISIBLE);
            }
            title.setText(c.title != null ? c.title : itemView.getContext().getString(R.string.chat_title_unknown));
            if (c.isGroup) {
                type.setText(itemView.getContext().getString(R.string.chat_type_group));
                type.setVisibility(View.VISIBLE);
            } else if (c.isChannel) {
                type.setText(itemView.getContext().getString(R.string.chat_type_channel));
                type.setVisibility(View.VISIBLE);
            } else if (c.isSaved) {
                type.setText(itemView.getContext().getString(R.string.chat_type_saved));
                type.setVisibility(View.VISIBLE);
            } else {
                type.setVisibility(View.GONE);
            }
            String rawPreview = c.lastMessage != null ? c.lastMessage : "";
            String preview;
            if (SpoilerCodec.isSpoilerContent(rawPreview)) {
                preview = itemView.getContext().getString(R.string.chat_preview_spoiler);
            } else if (PremiumGiftCodec.isPremiumGiftContent(rawPreview)) {
                preview = itemView.getContext().getString(R.string.premium_gift_preview);
            } else {
                preview = SpoilerCodec.strip(rawPreview);
            }
            last.setText(preview);
            BlurUtils.applyTextBlur(last, privacyEnabled);
            BlurUtils.applyTextBlur(avatar, privacyEnabled);
            if (c.unread > 0) {
                unread.setVisibility(View.VISIBLE);
                unread.setText(String.valueOf(c.unread));
            } else {
                unread.setVisibility(View.GONE);
            }

            String key = ChatListPrefs.buildKey(c);
            if (pinned != null) {
                pinned.setVisibility(pinnedIds.contains(key) ? View.VISIBLE : View.GONE);
            }
            if (muted != null) {
                muted.setVisibility(mutedIds.contains(key) ? View.VISIBLE : View.GONE);
            }
        }
    }

    private static final DiffUtil.ItemCallback<ChatListItem> DIFF = new DiffUtil.ItemCallback<>() {
        @Override
        public boolean areItemsTheSame(@NonNull ChatListItem oldItem, @NonNull ChatListItem newItem) {
            return oldItem.getId().equals(newItem.getId());
        }

        @Override
        public boolean areContentsTheSame(@NonNull ChatListItem oldItem, @NonNull ChatListItem newItem) {
            return oldItem.equals(newItem);
        }
    };
}
