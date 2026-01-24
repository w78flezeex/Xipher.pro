package com.xipher.wrapper.ui.helper;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.drawable.Drawable;
import android.view.HapticFeedbackConstants;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.core.content.ContextCompat;
import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import com.xipher.wrapper.R;

/**
 * Swipe-to-reply helper for chat messages (Telegram-style)
 */
public class SwipeToReplyCallback extends ItemTouchHelper.Callback {

    private final SwipeListener listener;
    private final Drawable replyIcon;
    private final Paint circlePaint;
    private final int iconSize;
    private final int circleRadius;
    private final float swipeThreshold;
    private boolean hapticTriggered = false;

    public interface SwipeListener {
        void onSwipeReply(int position);
    }

    public SwipeToReplyCallback(Context context, SwipeListener listener) {
        this.listener = listener;
        this.replyIcon = ContextCompat.getDrawable(context, R.drawable.ic_reply);
        if (replyIcon != null) {
            replyIcon.setTint(Color.WHITE);
        }
        
        this.circlePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        this.circlePaint.setColor(ContextCompat.getColor(context, R.color.x_accent_primary));
        
        float density = context.getResources().getDisplayMetrics().density;
        this.iconSize = (int) (24 * density);
        this.circleRadius = (int) (20 * density);
        this.swipeThreshold = 80 * density;
    }

    @Override
    public int getMovementFlags(@NonNull RecyclerView recyclerView, @NonNull RecyclerView.ViewHolder viewHolder) {
        return makeMovementFlags(0, ItemTouchHelper.RIGHT);
    }

    @Override
    public boolean onMove(@NonNull RecyclerView recyclerView, @NonNull RecyclerView.ViewHolder viewHolder, @NonNull RecyclerView.ViewHolder target) {
        return false;
    }

    @Override
    public void onSwiped(@NonNull RecyclerView.ViewHolder viewHolder, int direction) {
        // We handle this in onChildDraw instead
    }

    @Override
    public float getSwipeThreshold(@NonNull RecyclerView.ViewHolder viewHolder) {
        return Float.MAX_VALUE; // Disable auto-swipe completion
    }

    @Override
    public float getSwipeEscapeVelocity(float defaultValue) {
        return Float.MAX_VALUE; // Disable velocity-based swipe
    }

    @Override
    public void clearView(@NonNull RecyclerView recyclerView, @NonNull RecyclerView.ViewHolder viewHolder) {
        super.clearView(recyclerView, viewHolder);
        viewHolder.itemView.setTranslationX(0f);
        hapticTriggered = false;
    }

    @Override
    public void onChildDraw(@NonNull Canvas c, @NonNull RecyclerView recyclerView,
                            @NonNull RecyclerView.ViewHolder viewHolder, float dX, float dY,
                            int actionState, boolean isCurrentlyActive) {
        
        View itemView = viewHolder.itemView;
        
        // Limit swipe distance
        float limitedDX = Math.min(dX, swipeThreshold * 1.5f);
        
        // Calculate progress (0 to 1)
        float progress = Math.min(1f, Math.abs(limitedDX) / swipeThreshold);
        
        // Draw reply icon with circle background
        if (limitedDX > 20) {
            float iconX = itemView.getLeft() + limitedDX / 2;
            float iconY = itemView.getTop() + itemView.getHeight() / 2f;
            
            // Draw circle background
            float circleScale = Math.min(1f, progress * 1.2f);
            c.drawCircle(iconX, iconY, circleRadius * circleScale, circlePaint);
            
            // Draw reply icon
            if (replyIcon != null && circleScale > 0.5f) {
                int halfIcon = iconSize / 2;
                replyIcon.setBounds(
                        (int) (iconX - halfIcon),
                        (int) (iconY - halfIcon),
                        (int) (iconX + halfIcon),
                        (int) (iconY + halfIcon)
                );
                replyIcon.setAlpha((int) (255 * Math.min(1f, (circleScale - 0.5f) * 2)));
                replyIcon.draw(c);
            }
            
            // Haptic feedback when threshold reached
            if (progress >= 1f && !hapticTriggered && isCurrentlyActive) {
                itemView.performHapticFeedback(HapticFeedbackConstants.LONG_PRESS);
                hapticTriggered = true;
            }
        }
        
        // Apply translation to item
        itemView.setTranslationX(limitedDX);
        
        // Trigger reply when released past threshold
        if (!isCurrentlyActive && progress >= 1f) {
            itemView.setTranslationX(0f);
            if (listener != null) {
                listener.onSwipeReply(viewHolder.getAdapterPosition());
            }
            hapticTriggered = false;
        }
    }
}
