package com.xipher.wrapper.ui.adapter;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.bumptech.glide.Glide;
import com.xipher.wrapper.BuildConfig;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.model.FriendDto;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

public class FriendSelectAdapter extends RecyclerView.Adapter<FriendSelectAdapter.VH> {

    public interface SelectionListener {
        void onSelectionChanged(int count);
    }

    private final List<FriendDto> items = new ArrayList<>();
    private final Set<String> selectedIds = new HashSet<>();
    private SelectionListener listener;

    public void setListener(SelectionListener listener) {
        this.listener = listener;
    }

    public void setItems(List<FriendDto> data) {
        items.clear();
        if (data != null) {
            items.addAll(data);
        }
        selectedIds.clear();
        notifyDataSetChanged();
        notifySelection();
    }

    public List<String> getSelectedIds() {
        return new ArrayList<>(selectedIds);
    }

    @NonNull
    @Override
    public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View v = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_friend_select, parent, false);
        return new VH(v);
    }

    @Override
    public void onBindViewHolder(@NonNull VH holder, int position) {
        FriendDto friend = items.get(position);
        holder.bind(friend, selectedIds.contains(friend.id));
        holder.itemView.setOnClickListener(v -> toggle(friend));
        holder.check.setOnClickListener(v -> toggle(friend));
    }

    @Override
    public int getItemCount() {
        return items.size();
    }

    private void toggle(FriendDto friend) {
        if (friend == null || friend.id == null) return;
        if (selectedIds.contains(friend.id)) {
            selectedIds.remove(friend.id);
        } else {
            selectedIds.add(friend.id);
        }
        notifyDataSetChanged();
        notifySelection();
    }

    private void notifySelection() {
        if (listener != null) {
            listener.onSelectionChanged(selectedIds.size());
        }
    }

    static class VH extends RecyclerView.ViewHolder {
        final ImageView avatarImage;
        final TextView avatarLetter;
        final TextView name;
        final CheckBox check;

        VH(@NonNull View itemView) {
            super(itemView);
            avatarImage = itemView.findViewById(R.id.friendAvatarImage);
            avatarLetter = itemView.findViewById(R.id.friendAvatarLetter);
            name = itemView.findViewById(R.id.friendName);
            check = itemView.findViewById(R.id.friendCheck);
        }

        void bind(FriendDto friend, boolean checked) {
            String title = friend != null && friend.username != null
                    ? friend.username
                    : itemView.getContext().getString(R.string.user_placeholder);
            name.setText(title);
            String letter = title.isEmpty()
                    ? itemView.getContext().getString(R.string.avatar_placeholder)
                    : title.substring(0, 1).toUpperCase(Locale.getDefault());
            avatarLetter.setText(letter);
            check.setChecked(checked);
            if (friend != null && friend.avatar_url != null && !friend.avatar_url.isEmpty()) {
                String url = friend.avatar_url.startsWith("http") ? friend.avatar_url : (BuildConfig.API_BASE + friend.avatar_url);
                avatarImage.setVisibility(View.VISIBLE);
                avatarLetter.setVisibility(View.GONE);
                Glide.with(avatarImage.getContext())
                        .load(url)
                        .circleCrop()
                        .into(avatarImage);
            } else {
                avatarImage.setVisibility(View.GONE);
                avatarLetter.setVisibility(View.VISIBLE);
            }
        }
    }
}
