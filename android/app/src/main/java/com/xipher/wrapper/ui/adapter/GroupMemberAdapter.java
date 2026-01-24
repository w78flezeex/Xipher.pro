package com.xipher.wrapper.ui.adapter;

import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.xipher.wrapper.R;
import com.xipher.wrapper.data.model.GroupMemberDto;
import com.xipher.wrapper.databinding.ItemGroupMemberBinding;

import java.util.List;

public class GroupMemberAdapter extends RecyclerView.Adapter<GroupMemberAdapter.MemberViewHolder> {

    private final List<GroupMemberDto> members;
    private final OnMemberClickListener listener;

    public interface OnMemberClickListener {
        void onMemberClick(GroupMemberDto member);
        void onMemberLongClick(GroupMemberDto member);
    }

    public GroupMemberAdapter(List<GroupMemberDto> members, OnMemberClickListener listener) {
        this.members = members;
        this.listener = listener;
    }

    @NonNull
    @Override
    public MemberViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        ItemGroupMemberBinding binding = ItemGroupMemberBinding.inflate(
                LayoutInflater.from(parent.getContext()), parent, false);
        return new MemberViewHolder(binding);
    }

    @Override
    public void onBindViewHolder(@NonNull MemberViewHolder holder, int position) {
        holder.bind(members.get(position));
    }

    @Override
    public int getItemCount() {
        return members.size();
    }

    class MemberViewHolder extends RecyclerView.ViewHolder {
        private final ItemGroupMemberBinding binding;

        MemberViewHolder(ItemGroupMemberBinding binding) {
            super(binding.getRoot());
            this.binding = binding;
        }

        void bind(GroupMemberDto member) {
            // Имя пользователя
            binding.username.setText(member.getUsername());

            // Аватар (первая буква)
            if (!TextUtils.isEmpty(member.getUsername())) {
                binding.avatarLetter.setText(String.valueOf(member.getUsername().charAt(0)).toUpperCase());
            }

            // Роль
            String roleText;
            int roleVisibility = View.VISIBLE;
            switch (member.getRole()) {
                case "owner":
                    roleText = "Владелец";
                    binding.roleIcon.setVisibility(View.VISIBLE);
                    break;
                case "admin":
                    roleText = "Администратор";
                    binding.roleIcon.setVisibility(View.VISIBLE);
                    break;
                default:
                    roleText = "";
                    roleVisibility = View.GONE;
                    binding.roleIcon.setVisibility(View.GONE);
                    break;
            }
            binding.roleLabel.setText(roleText);
            binding.roleLabel.setVisibility(roleVisibility);

            // Статус (если замьючен)
            if (member.isMuted()) {
                binding.statusText.setText("Заглушен");
                binding.statusText.setVisibility(View.VISIBLE);
                binding.mutedIcon.setVisibility(View.VISIBLE);
            } else if (member.isBanned()) {
                binding.statusText.setText("Заблокирован");
                binding.statusText.setVisibility(View.VISIBLE);
                binding.mutedIcon.setVisibility(View.GONE);
            } else {
                binding.statusText.setVisibility(View.GONE);
                binding.mutedIcon.setVisibility(View.GONE);
            }

            // Клики
            binding.getRoot().setOnClickListener(v -> {
                if (listener != null) {
                    listener.onMemberClick(member);
                }
            });

            binding.getRoot().setOnLongClickListener(v -> {
                if (listener != null) {
                    listener.onMemberLongClick(member);
                }
                return true;
            });
        }
    }
}
