package com.xipher.wrapper.ui.videonote;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Outline;
import android.graphics.Paint;
import android.graphics.RectF;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewOutlineProvider;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.camera.view.PreviewView;
import androidx.core.content.ContextCompat;

import com.xipher.wrapper.R;

public class CircularCameraView extends FrameLayout {

    private static final int DEFAULT_RING_STROKE_DP = 5;

    private final RectF ringRect = new RectF();
    private final Paint ringPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private PreviewView previewView;
    private float ringStrokePx;
    private float ringInsetPx;
    private float progress = 0f;
    private boolean recordingActive = false;

    public CircularCameraView(@NonNull Context context) {
        super(context);
        init();
    }

    public CircularCameraView(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public CircularCameraView(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        setWillNotDraw(false);
        setClipToOutline(true);
        setOutlineProvider(new ViewOutlineProvider() {
            @Override
            public void getOutline(View view, Outline outline) {
                outline.setOval(0, 0, view.getWidth(), view.getHeight());
            }
        });

        previewView = new PreviewView(getContext());
        previewView.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        previewView.setScaleType(PreviewView.ScaleType.FILL_CENTER);
        previewView.setImplementationMode(PreviewView.ImplementationMode.COMPATIBLE);
        previewView.setClickable(false);
        addView(previewView);

        ringStrokePx = dp(DEFAULT_RING_STROKE_DP);
        ringInsetPx = ringStrokePx / 2f + dp(2);

        ringPaint.setStyle(Paint.Style.STROKE);
        ringPaint.setStrokeCap(Paint.Cap.ROUND);
        ringPaint.setStrokeWidth(ringStrokePx);
        ringPaint.setColor(ContextCompat.getColor(getContext(), R.color.x_accent_start));
    }

    @NonNull
    public PreviewView getPreviewView() {
        return previewView;
    }

    public void setRecordingActive(boolean active) {
        recordingActive = active;
        if (!active) {
            progress = 0f;
        }
        invalidate();
    }

    public void setRecordingProgress(float progress) {
        this.progress = Math.max(0f, Math.min(1f, progress));
        invalidate();
    }

    public void resetProgress() {
        progress = 0f;
        recordingActive = false;
        invalidate();
    }

    public void show(boolean animate) {
        if (getVisibility() == View.VISIBLE) return;
        setVisibility(View.VISIBLE);
        if (animate) {
            setAlpha(0f);
            setScaleX(0.9f);
            setScaleY(0.9f);
            animate().alpha(1f).scaleX(1f).scaleY(1f).setDuration(160).start();
        } else {
            setAlpha(1f);
            setScaleX(1f);
            setScaleY(1f);
        }
    }

    public void hide(boolean animate) {
        if (getVisibility() != View.VISIBLE) return;
        if (animate) {
            animate().alpha(0f).scaleX(0.9f).scaleY(0.9f).setDuration(140)
                    .withEndAction(() -> {
                        setVisibility(View.GONE);
                        setAlpha(1f);
                        setScaleX(1f);
                        setScaleY(1f);
                    })
                    .start();
        } else {
            setVisibility(View.GONE);
        }
    }

    @Override
    protected void dispatchDraw(Canvas canvas) {
        super.dispatchDraw(canvas);
        if (!recordingActive && progress <= 0f) return;
        float sweep = 360f * Math.max(progress, 0.01f);
        canvas.drawArc(ringRect, -90f, sweep, false, ringPaint);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        ringRect.set(ringInsetPx, ringInsetPx, w - ringInsetPx, h - ringInsetPx);
    }

    private float dp(int value) {
        return TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, value, getResources().getDisplayMetrics());
    }
}
