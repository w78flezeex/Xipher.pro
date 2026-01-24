package com.xipher.wrapper.ui.videonote;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.util.Rational;
import android.view.Surface;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.camera.core.AspectRatio;
import androidx.camera.core.CameraSelector;
import androidx.camera.core.Preview;
import androidx.camera.core.UseCaseGroup;
import androidx.camera.core.ViewPort;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.camera.video.FallbackStrategy;
import androidx.camera.video.FileOutputOptions;
import androidx.camera.video.PendingRecording;
import androidx.camera.video.Quality;
import androidx.camera.video.QualitySelector;
import androidx.camera.video.Recorder;
import androidx.camera.video.Recording;
import androidx.camera.video.VideoCapture;
import androidx.camera.video.VideoRecordEvent;
import androidx.camera.view.PreviewView;
import androidx.core.content.ContextCompat;
import androidx.lifecycle.LifecycleOwner;

import com.google.common.util.concurrent.ListenableFuture;

import java.io.File;
import java.util.concurrent.Executor;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

public class CameraRecorderHelper {

    public interface Listener {
        void onRecordingStarted();
        void onRecordingProgress(long durationMs, float progress);
        void onRecordingFinalized(@NonNull File file, long durationMs);
        void onRecordingCanceled();
        void onRecordingError(@NonNull String message);
    }

    public static final long MAX_DURATION_MS = 60_000L;
    private static final int TARGET_VIDEO_BITRATE = 2_000_000;

    private final Context appContext;
    private final Executor mainExecutor;
    private ExecutorService cameraExecutor;
    private ProcessCameraProvider cameraProvider;
    private ListenableFuture<ProcessCameraProvider> cameraProviderFuture;
    private VideoCapture<Recorder> videoCapture;
    private Recording recording;
    private File currentOutputFile;
    private LifecycleOwner lifecycleOwner;
    private PreviewView previewView;
    private Listener listener;

    private boolean cameraStarted;
    private boolean cameraBound;
    private boolean pendingStart;
    private boolean stopRequested;
    private boolean sendOnFinalize;
    private long recordedDurationMs;
    private CameraSelector cameraSelector = CameraSelector.DEFAULT_FRONT_CAMERA;

    public CameraRecorderHelper(@NonNull Context context) {
        appContext = context.getApplicationContext();
        mainExecutor = ContextCompat.getMainExecutor(appContext);
    }

    public void setListener(@Nullable Listener listener) {
        this.listener = listener;
    }

    public void bind(@NonNull LifecycleOwner owner, @NonNull PreviewView previewView) {
        lifecycleOwner = owner;
        this.previewView = previewView;
        startCamera();
    }

    public void unbind() {
        stopRecording(false);
        pendingStart = false;
        releaseCamera();
        lifecycleOwner = null;
        previewView = null;
    }

    public void release() {
        unbind();
        if (cameraExecutor != null) {
            cameraExecutor.shutdown();
            cameraExecutor = null;
        }
    }

    public boolean isRecording() {
        return recording != null;
    }

    public void setCameraSelector(@NonNull CameraSelector selector) {
        cameraSelector = selector;
        if (cameraBound) {
            bindUseCases();
        }
    }

    public void startRecording() {
        if (recording != null) return;
        if (!cameraBound || videoCapture == null) {
            pendingStart = true;
            startCamera();
            return;
        }
        startRecordingInternal();
    }

    private void startRecordingInternal() {
        if (recording != null) return;
        stopRequested = false;
        sendOnFinalize = true;
        recordedDurationMs = 0L;
        currentOutputFile = new File(appContext.getCacheDir(),
                "videonote_" + System.currentTimeMillis() + ".mp4");

        FileOutputOptions outputOptions = new FileOutputOptions.Builder(currentOutputFile).build();
        PendingRecording pending = videoCapture.getOutput()
                .prepareRecording(appContext, outputOptions);
        if (ContextCompat.checkSelfPermission(appContext, Manifest.permission.RECORD_AUDIO)
                == PackageManager.PERMISSION_GRANTED) {
            try {
                pending = pending.withAudioEnabled();
            } catch (Exception ignored) {
            }
        }
        pendingStart = false;
        recording = pending.start(mainExecutor, this::handleRecordEvent);
    }

    public void stopRecording(boolean send) {
        if (recording == null) {
            if (pendingStart) {
                pendingStart = false;
                stopRequested = false;
                if (listener != null) listener.onRecordingCanceled();
            }
            return;
        }
        if (stopRequested) return;
        stopRequested = true;
        sendOnFinalize = send;
        pendingStart = false;
        try {
            recording.stop();
        } catch (Exception e) {
            if (listener != null) listener.onRecordingError("Stop failed: " + e.getMessage());
        }
    }

    private void startCamera() {
        if (cameraStarted || lifecycleOwner == null || previewView == null) return;
        cameraStarted = true;
        cameraProviderFuture = ProcessCameraProvider.getInstance(appContext);
        cameraProviderFuture.addListener(() -> {
            try {
                ProcessCameraProvider provider = cameraProviderFuture.get();
                if (lifecycleOwner == null || previewView == null) {
                    cameraStarted = false;
                    return;
                }
                cameraProvider = provider;
                bindUseCases();
            } catch (Exception e) {
                cameraStarted = false;
                if (listener != null) listener.onRecordingError("Camera init failed: " + e.getMessage());
            }
        }, mainExecutor);
    }

    private void bindUseCases() {
        if (cameraProvider == null || lifecycleOwner == null || previewView == null) return;
        int rotation = previewView.getDisplay() != null ? previewView.getDisplay().getRotation() : Surface.ROTATION_0;
        Preview preview = new Preview.Builder()
                .setTargetRotation(rotation)
                .build();
        preview.setSurfaceProvider(previewView.getSurfaceProvider());

        Recorder recorder = new Recorder.Builder()
                .setExecutor(ensureCameraExecutor())
                .setQualitySelector(QualitySelector.from(
                        Quality.SD,
                        FallbackStrategy.lowerQualityOrHigherThan(Quality.SD)))
                .setAspectRatio(AspectRatio.RATIO_4_3)
                .setTargetVideoEncodingBitRate(TARGET_VIDEO_BITRATE)
                .build();

        videoCapture = VideoCapture.withOutput(recorder);
        videoCapture.setTargetRotation(rotation);

        ViewPort viewPort = new ViewPort.Builder(new Rational(1, 1), rotation)
                .setScaleType(ViewPort.FILL_CENTER)
                .build();

        UseCaseGroup useCaseGroup = new UseCaseGroup.Builder()
                .setViewPort(viewPort)
                .addUseCase(preview)
                .addUseCase(videoCapture)
                .build();

        try {
            cameraProvider.unbindAll();
            cameraProvider.bindToLifecycle(lifecycleOwner, resolveCameraSelector(), useCaseGroup);
            cameraBound = true;
            if (pendingStart) {
                startRecordingInternal();
            }
        } catch (Exception e) {
            cameraBound = false;
            pendingStart = false;
            if (listener != null) listener.onRecordingError("Camera bind failed: " + e.getMessage());
        }
    }

    private CameraSelector resolveCameraSelector() {
        if (cameraProvider == null) return cameraSelector;
        try {
            if (cameraProvider.hasCamera(cameraSelector)) return cameraSelector;
            if (cameraProvider.hasCamera(CameraSelector.DEFAULT_BACK_CAMERA)) {
                return CameraSelector.DEFAULT_BACK_CAMERA;
            }
        } catch (Exception ignored) {
        }
        return CameraSelector.DEFAULT_FRONT_CAMERA;
    }

    private void releaseCamera() {
        cameraBound = false;
        cameraStarted = false;
        if (cameraProvider != null) {
            try {
                cameraProvider.unbindAll();
            } catch (Exception ignored) {
            }
        }
        cameraProvider = null;
        cameraProviderFuture = null;
        videoCapture = null;
    }

    private void handleRecordEvent(@NonNull VideoRecordEvent event) {
        if (event instanceof VideoRecordEvent.Start) {
            if (listener != null) listener.onRecordingStarted();
        } else if (event instanceof VideoRecordEvent.Status) {
            long durationNs = event.getRecordingStats().getRecordedDurationNanos();
            recordedDurationMs = TimeUnit.NANOSECONDS.toMillis(durationNs);
            float progress = Math.min(1f, recordedDurationMs / (float) MAX_DURATION_MS);
            if (listener != null) listener.onRecordingProgress(recordedDurationMs, progress);
            if (!stopRequested && recordedDurationMs >= MAX_DURATION_MS) {
                stopRecording(true);
            }
        } else if (event instanceof VideoRecordEvent.Finalize) {
            VideoRecordEvent.Finalize finalizeEvent = (VideoRecordEvent.Finalize) event;
            File outputFile = currentOutputFile;
            currentOutputFile = null;
            recording = null;
            boolean success = finalizeEvent.getError() == VideoRecordEvent.Finalize.ERROR_NONE;
            if (success && sendOnFinalize && outputFile != null && outputFile.exists()) {
                if (listener != null) listener.onRecordingFinalized(outputFile, recordedDurationMs);
            } else {
                if (outputFile != null && outputFile.exists()) {
                    //noinspection ResultOfMethodCallIgnored
                    outputFile.delete();
                }
                if (listener != null) {
                    if (success) {
                        listener.onRecordingCanceled();
                    } else {
                        listener.onRecordingError("Record failed: " + finalizeEvent.getError());
                    }
                }
            }
            recordedDurationMs = 0L;
        }
    }

    private ExecutorService ensureCameraExecutor() {
        if (cameraExecutor == null || cameraExecutor.isShutdown()) {
            cameraExecutor = Executors.newSingleThreadExecutor();
        }
        return cameraExecutor;
    }
}
