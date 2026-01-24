package com.xipher.wrapper.ui.videonote;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Outline;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewOutlineProvider;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.media3.common.MediaItem;
import androidx.media3.common.Player;
import androidx.media3.exoplayer.ExoPlayer;
import androidx.media3.ui.AspectRatioFrameLayout;
import androidx.media3.ui.PlayerView;

import com.xipher.wrapper.R;

public class VideoNoteView extends FrameLayout {

    private PlayerView playerView;
    private ImageView playOverlay;
    private ExoPlayer player;
    private String videoUrl;

    private final Player.Listener playerListener = new Player.Listener() {
        @Override
        public void onIsPlayingChanged(boolean isPlaying) {
            updateOverlay(isPlaying);
        }

        @Override
        public void onPlaybackStateChanged(int playbackState) {
            updateOverlay(player != null && player.isPlaying());
        }
    };

    public VideoNoteView(@NonNull Context context) {
        super(context);
        init();
    }

    public VideoNoteView(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    public VideoNoteView(@NonNull Context context, @Nullable AttributeSet attrs, int defStyleAttr) {
        super(context, attrs, defStyleAttr);
        init();
    }

    private void init() {
        setClipToOutline(true);
        setOutlineProvider(new ViewOutlineProvider() {
            @Override
            public void getOutline(View view, Outline outline) {
                outline.setOval(0, 0, view.getWidth(), view.getHeight());
            }
        });
        setBackgroundColor(Color.BLACK);

        playerView = new PlayerView(getContext());
        playerView.setLayoutParams(new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        playerView.setUseController(false);
        playerView.setResizeMode(AspectRatioFrameLayout.RESIZE_MODE_ZOOM);
        addView(playerView);

        playOverlay = new ImageView(getContext());
        int size = dp(44);
        LayoutParams overlayParams = new LayoutParams(size, size);
        overlayParams.gravity = Gravity.CENTER;
        playOverlay.setLayoutParams(overlayParams);
        playOverlay.setImageResource(R.drawable.ic_play);
        playOverlay.setBackgroundResource(R.drawable.btn_circle_accent);
        playOverlay.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        int pad = dp(10);
        playOverlay.setPadding(pad, pad, pad, pad);
        addView(playOverlay);

        setClickable(true);
        setOnClickListener(v -> togglePlayback());
    }

    public void bind(@Nullable String url) {
        if (url == null || url.isEmpty()) {
            release();
            return;
        }
        if (url.equals(videoUrl) && player != null) {
            updateOverlay(player.isPlaying());
            return;
        }
        videoUrl = url;
        ensurePlayer();
        player.setMediaItem(MediaItem.fromUri(url));
        player.setRepeatMode(Player.REPEAT_MODE_ONE);
        player.setPlayWhenReady(false);
        player.prepare();
        updateOverlay(false);
    }

    public void release() {
        videoUrl = null;
        if (player != null) {
            player.removeListener(playerListener);
            playerView.setPlayer(null);
            player.release();
            player = null;
        }
        updateOverlay(false);
    }

    private void togglePlayback() {
        if (player == null) return;
        if (player.isPlaying()) {
            player.pause();
        } else {
            player.play();
        }
    }

    private void ensurePlayer() {
        if (player != null) return;
        player = new ExoPlayer.Builder(getContext()).build();
        player.addListener(playerListener);
        playerView.setPlayer(player);
    }

    private void updateOverlay(boolean isPlaying) {
        playOverlay.setVisibility(isPlaying ? View.GONE : View.VISIBLE);
    }

    private int dp(int value) {
        return (int) TypedValue.applyDimension(
                TypedValue.COMPLEX_UNIT_DIP, value, getResources().getDisplayMetrics());
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        release();
    }
}
