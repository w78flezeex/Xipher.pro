package com.xipher.wrapper.ui.adapter;

import android.graphics.Color;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.xipher.wrapper.R;
import com.xipher.wrapper.data.model.ChannelMemberDto;

import java.util.List;

/**
 * Адаптер для списка участников канала
 */
public class ChannelMemberAdapter extends RecyclerView.Adapter<ChannelMemberAdapter.MemberViewHolder> {

    private static final int[] AVATAR_COLORS = {
            0xFFE17076, // красный
            0xFFEDA86C, // оранжевый
            0xFFA695E7, // фиолетовый
            0xFF7BC862, // зеленый
            0xFF6EC9CB, // голубой
            0xFF65AADD, // синий
            0xFFEE7AAE  // розовый
    };

    private final List<ChannelMemberDto> members;
    private final OnMemberClickListener listener;

    public interface OnMemberClickListener {
        void onMemberClick(ChannelMemberDto member);
        void onMemberLongClick(ChannelMemberDto member);
    }

    public ChannelMemberAdapter(List<ChannelMemberDto> members, OnMemberClickListener listener) {
        this.members = members;
        this.listener = listener;
    }

    @NonNull
    @Override
    public MemberViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext())
                .inflate(R.layout.item_channel_member, parent, false);
        return new MemberViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull MemberViewHolder holder, int position) {
        ChannelMemberDto member = members.get(position);
        holder.bind(member);
    }

    @Override
    public int getItemCount() {
        return members.size();
    }

    private int getAvatarColor(String id) {
        int hash = id != null ? id.hashCode() : 0;
        return AVATAR_COLORS[Math.abs(hash) % AVATAR_COLORS.length];
    }

    class MemberViewHolder extends RecyclerView.ViewHolder {
        private final TextView avatarLetter;
        private final TextView username;
        private final TextView roleLabel;
        private final TextView status;
        private final View bannedIcon;

        MemberViewHolder(@NonNull View itemView) {
            super(itemView);
            avatarLetter = itemView.findViewById(R.id.avatarPlaceholder);
            username = itemView.findViewById(R.id.memberName);
            roleLabel = itemView.findViewById(R.id.roleBadge);
            status = itemView.findViewById(R.id.status);
            bannedIcon = itemView.findViewById(R.id.bannedIcon);

            itemView.setOnClickListener(v -> {
                int pos = getAdapterPosition();
                if (pos != RecyclerView.NO_POSITION && listener != null) {
                    listener.onMemberClick(members.get(pos));
                }
            });

            itemView.setOnLongClickListener(v -> {
                int pos = getAdapterPosition();
                if (pos != RecyclerView.NO_POSITION && listener != null) {
                    listener.onMemberLongClick(members.get(pos));
                    return true;
                }
                return false;
            });
        }

        void bind(ChannelMemberDto member) {
            // Аватар
            String name = member.getUsername();
            if (name != null && !name.isEmpty()) {
                avatarLetter.setText(String.valueOf(name.charAt(0)).toUpperCase());
            } else {
                avatarLetter.setText("?");
            }

            // Цвет аватара
            int color = getAvatarColor(member.getUserId());
            if (avatarLetter != null && avatarLetter.getBackground() != null) {
                avatarLetter.getBackground().setTint(color);
            }

            // Имя пользователя
            username.setText(name != null ? name : "Unknown");

            // Роль
            if (member.isOwner()) {
                roleLabel.setVisibility(View.VISIBLE);
                roleLabel.setText("👑 " + itemView.getContext().getString(R.string.role_owner));
                roleLabel.setTextColor(0xFFE17076);
            } else if (member.isAdmin()) {
                roleLabel.setVisibility(View.VISIBLE);
                String title = member.getAdminTitle();
                if (title != null && !title.isEmpty()) {
                    roleLabel.setText("⭐ " + title);
                } else {
                    roleLabel.setText("⭐ " + itemView.getContext().getString(R.string.role_admin));
                }
                roleLabel.setTextColor(0xFF65AADD);
            } else {
                roleLabel.setVisibility(View.GONE);
            }

            // Статус (права)
            if (member.isAdmin() && !member.isOwner()) {
                String permsStr = member.getPermissionsDisplayString();
                if (!permsStr.isEmpty()) {
                    status.setVisibility(View.VISIBLE);
                    status.setText(permsStr);
                } else {
                    status.setVisibility(View.GONE);
                }
            } else {
                status.setVisibility(View.GONE);
            }

            // Заблокирован
            if (bannedIcon != null) {
                bannedIcon.setVisibility(member.isBanned() ? View.VISIBLE : View.GONE);
            }

            // Затемнение для заблокированных
            itemView.setAlpha(member.isBanned() ? 0.5f : 1.0f);
        }
    }
}
