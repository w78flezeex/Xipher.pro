package com.xipher.wrapper.ui.videonote;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.MotionEvent;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;

import com.xipher.wrapper.R;

public class MediaRecorderButton extends FrameLayout {

    public enum Mode {
        AUDIO,
        VIDEO
    }

    public interface OnMediaActionListener {
        void onModeChanged(@NonNull Mode mode);
        void onAudioRecordStart();
        void onVideoRecordStart();
        void onStopRecording(@NonNull Mode mode);
        void onCancelRecording(@NonNull Mode mode);
        void onLockRecording(@NonNull Mode mode);
    }

    private static final long HOLD_DELAY_MS = 220L;
    private static final int DEFAULT_LOCK_THRESHOLD_DP = 72;
    private static final int DEFAULT_CANCEL_THRESHOLD_DP = 72;

    private final Handler handler = new Handler(Looper.getMainLooper());
    private final Runnable holdRunnable = this::onHoldTriggered;

    private ImageView micIcon;
    private ImageView cameraIcon;
    private ImageView stopIcon;
    private Mode mode = Mode.AUDIO;
    private OnMediaActionListener listener;

    private float downX;
    private float downY;
    private boolean recordingActive;
    private boolean recordingLocked;
    private boolean lockedInGesture;
    private boolean pressActive;
    private float lockThresholdPx;
    private float cancelThresholdPx;

    public MediaRecorderButton(@NonNull Context context) {
        super(context);
        init();
    }

    public MediaRecorderButton(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public MediaRecorderButton(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        setClickable(true);
        setFocusable(true);
        setBackgroundResource(R.drawable.btn_circle_accent);
        int padding = dp(9);
        setPadding(padding, padding, padding, padding);

        micIcon = buildIcon(R.drawable.ic_mic);
        cameraIcon = buildIcon(R.drawable.ic_camera);
        stopIcon = buildIcon(R.drawable.ic_stop);
        stopIcon.setAlpha(0f);

        addView(micIcon);
        addView(cameraIcon);
        addView(stopIcon);

        cameraIcon.setAlpha(0f);
        lockThresholdPx = dp(DEFAULT_LOCK_THRESHOLD_DP);
        cancelThresholdPx = dp(DEFAULT_CANCEL_THRESHOLD_DP);
    }

    public void setOnMediaActionListener(@Nullable OnMediaActionListener listener) {
        this.listener = listener;
    }

    public void setMode(@NonNull Mode mode, boolean animate) {
        if (recordingActive) return;
        if (this.mode == mode) return;
        this.mode = mode;
        animateModeChange(animate);
        if (listener != null) listener.onModeChanged(mode);
    }

    public Mode getMode() {
        return mode;
    }

    public void resetState() {
        handler.removeCallbacks(holdRunnable);
        pressActive = false;
        recordingActive = false;
        recordingLocked = false;
        lockedInGesture = false;
        stopIcon.animate().cancel();
        stopIcon.setAlpha(0f);
        animateModeChange(false);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (!isEnabled()) return false;
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                pressActive = true;
                lockedInGesture = false;
                downX = event.getX();
                downY = event.getY();
                handler.removeCallbacks(holdRunnable);
                handler.postDelayed(holdRunnable, HOLD_DELAY_MS);
                return true;
            case MotionEvent.ACTION_MOVE:
                if (!recordingActive || recordingLocked) return true;
                float dx = event.getX() - downX;
                float dy = event.getY() - downY;
                if (!recordingLocked && dy < -lockThresholdPx) {
                    recordingLocked = true;
                    lockedInGesture = true;
                    showStopIcon(true);
                    if (listener != null) listener.onLockRecording(mode);
                    return true;
                }
                if (dx < -cancelThresholdPx) {
                    cancelRecording();
                }
                return true;
            case MotionEvent.ACTION_UP:
                handler.removeCallbacks(holdRunnable);
                pressActive = false;
                if (recordingActive) {
                    if (recordingLocked) {
                        if (!lockedInGesture) {
                            stopRecording();
                        }
                        return true;
                    }
                    stopRecording();
                    return true;
                }
                performClick();
                toggleMode();
                return true;
            case MotionEvent.ACTION_CANCEL:
                handler.removeCallbacks(holdRunnable);
                pressActive = false;
                if (recordingActive && !recordingLocked) {
                    cancelRecording();
                }
                return true;
            default:
                return false;
        }
    }

    @Override
    public boolean performClick() {
        return super.performClick();
    }

    private void onHoldTriggered() {
        if (!pressActive || recordingActive) return;
        recordingActive = true;
        recordingLocked = false;
        lockedInGesture = false;
        if (listener != null) {
            if (mode == Mode.AUDIO) {
                listener.onAudioRecordStart();
            } else {
                listener.onVideoRecordStart();
            }
        }
    }

    private void stopRecording() {
        recordingActive = false;
        recordingLocked = false;
        lockedInGesture = false;
        showStopIcon(false);
        if (listener != null) listener.onStopRecording(mode);
    }

    private void cancelRecording() {
        recordingActive = false;
        recordingLocked = false;
        lockedInGesture = false;
        showStopIcon(false);
        if (listener != null) listener.onCancelRecording(mode);
    }

    private void toggleMode() {
        Mode next = mode == Mode.AUDIO ? Mode.VIDEO : Mode.AUDIO;
        setMode(next, true);
    }

    private void animateModeChange(boolean animate) {
        float micTarget = mode == Mode.AUDIO ? 1f : 0f;
        float camTarget = mode == Mode.VIDEO ? 1f : 0f;
        if (animate) {
            micIcon.animate().alpha(micTarget).setDuration(140).start();
            cameraIcon.animate().alpha(camTarget).setDuration(140).start();
        } else {
            micIcon.setAlpha(micTarget);
            cameraIcon.setAlpha(camTarget);
        }
    }

    private void showStopIcon(boolean show) {
        stopIcon.animate().cancel();
        if (show) {
            stopIcon.setAlpha(1f);
        } else {
            stopIcon.setAlpha(0f);
        }
    }

    private ImageView buildIcon(int resId) {
        ImageView icon = new ImageView(getContext());
        LayoutParams lp = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        icon.setLayoutParams(lp);
        icon.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        icon.setImageResource(resId);
        icon.setColorFilter(ContextCompat.getColor(getContext(), R.color.x_on_accent));
        return icon;
    }

    private int dp(int value) {
        return (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, value, getResources().getDisplayMetrics());
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        handler.removeCallbacksAndMessages(null);
    }
}
