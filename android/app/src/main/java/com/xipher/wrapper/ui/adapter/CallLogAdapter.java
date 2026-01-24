package com.xipher.wrapper.ui.adapter;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import com.xipher.wrapper.R;
import com.xipher.wrapper.data.db.entity.CallLogEntity;

import java.text.DateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

public class CallLogAdapter extends RecyclerView.Adapter<CallLogAdapter.Holder> {
    private final List<CallLogEntity> items = new ArrayList<>();

    public void setItems(List<CallLogEntity> data) {
        items.clear();
        if (data != null) items.addAll(data);
        notifyDataSetChanged();
    }

    @NonNull
    @Override
    public Holder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View v = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_call_log, parent, false);
        return new Holder(v);
    }

    @Override
    public void onBindViewHolder(@NonNull Holder holder, int position) {
        holder.bind(items.get(position));
    }

    @Override
    public int getItemCount() {
        return items.size();
    }

    static class Holder extends RecyclerView.ViewHolder {
        private final ImageView icon;
        private final TextView name;
        private final TextView meta;
        private final TextView time;

        Holder(@NonNull View itemView) {
            super(itemView);
            icon = itemView.findViewById(R.id.callIcon);
            name = itemView.findViewById(R.id.callName);
            meta = itemView.findViewById(R.id.callMeta);
            time = itemView.findViewById(R.id.callTime);
        }

        void bind(CallLogEntity log) {
            Context ctx = itemView.getContext();
            String displayName = log.peerName != null && !log.peerName.isEmpty()
                    ? log.peerName
                    : ctx.getString(R.string.profile_title);
            name.setText(displayName);

            boolean incoming = "incoming".equals(log.direction);
            int iconRes = incoming ? R.drawable.ic_call_in : R.drawable.ic_call_out;
            icon.setImageResource(iconRes);

            String metaText = buildMeta(ctx, log, incoming);
            meta.setText(metaText);

            if (isMissed(log)) {
                meta.setTextColor(ctx.getColor(R.color.x_call_reject));
            } else {
                meta.setTextColor(ctx.getColor(R.color.x_text_secondary));
            }

            long ts = log.startedAt > 0 ? log.startedAt : log.endedAt;
            if (ts <= 0) ts = System.currentTimeMillis();
            DateFormat df = DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT, Locale.getDefault());
            time.setText(df.format(new Date(ts)));
        }

        private boolean isMissed(CallLogEntity log) {
            return "missed".equals(log.status) || "rejected".equals(log.status) || "cancelled".equals(log.status);
        }

        private String buildMeta(Context ctx, CallLogEntity log, boolean incoming) {
            String status = log.status != null ? log.status : "";
            if ("missed".equals(status)) return ctx.getString(R.string.call_status_missed);
            if ("rejected".equals(status)) return ctx.getString(R.string.call_status_rejected);
            if ("cancelled".equals(status)) return ctx.getString(R.string.call_status_cancelled);
            String dir = incoming ? ctx.getString(R.string.call_incoming) : ctx.getString(R.string.call_outgoing);
            if (log.durationSec > 0) {
                return dir + " - " + formatDuration(log.durationSec);
            }
            return dir;
        }

        private String formatDuration(long seconds) {
            long mins = seconds / 60;
            long secs = seconds % 60;
            return String.format(Locale.getDefault(), "%d:%02d", mins, secs);
        }
    }
}
