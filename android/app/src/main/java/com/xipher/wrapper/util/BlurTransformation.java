package com.xipher.wrapper.util;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;

import androidx.annotation.NonNull;

import com.bumptech.glide.load.Key;
import com.bumptech.glide.load.engine.bitmap_recycle.BitmapPool;
import com.bumptech.glide.load.resource.bitmap.BitmapTransformation;

import java.nio.ByteBuffer;
import java.security.MessageDigest;

public class BlurTransformation extends BitmapTransformation {
    private static final String ID = "com.xipher.wrapper.util.BlurTransformation";
    private static final byte[] ID_BYTES = ID.getBytes(Key.CHARSET);
    private final int radius;
    private final float sampling;

    public BlurTransformation(int radius, float sampling) {
        this.radius = Math.max(1, radius);
        this.sampling = Math.max(0.1f, sampling);
    }

    @Override
    protected Bitmap transform(@NonNull BitmapPool pool, @NonNull Bitmap toTransform, int outWidth, int outHeight) {
        int srcWidth = toTransform.getWidth();
        int srcHeight = toTransform.getHeight();
        int scaledWidth = Math.max(1, Math.round(srcWidth * sampling));
        int scaledHeight = Math.max(1, Math.round(srcHeight * sampling));

        Bitmap scaled = pool.get(scaledWidth, scaledHeight, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(scaled);
        canvas.scale((float) scaledWidth / (float) srcWidth, (float) scaledHeight / (float) srcHeight);
        Paint paint = new Paint(Paint.FILTER_BITMAP_FLAG | Paint.ANTI_ALIAS_FLAG);
        canvas.drawBitmap(toTransform, 0, 0, paint);

        int[] pixels = new int[scaledWidth * scaledHeight];
        scaled.getPixels(pixels, 0, scaledWidth, 0, 0, scaledWidth, scaledHeight);
        int[] blurred = blurPixels(pixels, scaledWidth, scaledHeight, radius);
        scaled.setPixels(blurred, 0, scaledWidth, 0, 0, scaledWidth, scaledHeight);

        Bitmap result = pool.get(srcWidth, srcHeight, Bitmap.Config.ARGB_8888);
        Canvas resultCanvas = new Canvas(result);
        resultCanvas.scale((float) srcWidth / (float) scaledWidth, (float) srcHeight / (float) scaledHeight);
        resultCanvas.drawBitmap(scaled, 0, 0, paint);
        pool.put(scaled);
        return result;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        BlurTransformation that = (BlurTransformation) o;
        return radius == that.radius && Float.compare(that.sampling, sampling) == 0;
    }

    @Override
    public int hashCode() {
        int result = ID.hashCode();
        result = 31 * result + radius;
        result = 31 * result + Float.floatToIntBits(sampling);
        return result;
    }

    @Override
    public void updateDiskCacheKey(@NonNull MessageDigest messageDigest) {
        messageDigest.update(ID_BYTES);
        messageDigest.update(ByteBuffer.allocate(8)
                .putInt(radius)
                .putFloat(sampling)
                .array());
    }

    private static int[] blurPixels(int[] src, int width, int height, int radius) {
        int[] tmp = new int[width * height];
        int[] out = new int[width * height];
        blurHorizontal(src, tmp, width, height, radius);
        blurVertical(tmp, out, width, height, radius);
        return out;
    }

    private static void blurHorizontal(int[] src, int[] dst, int width, int height, int radius) {
        int window = radius * 2 + 1;
        for (int y = 0; y < height; y++) {
            int row = y * width;
            int asum = 0;
            int rsum = 0;
            int gsum = 0;
            int bsum = 0;
            for (int i = -radius; i <= radius; i++) {
                int x = clamp(i, 0, width - 1);
                int c = src[row + x];
                asum += (c >> 24) & 0xFF;
                rsum += (c >> 16) & 0xFF;
                gsum += (c >> 8) & 0xFF;
                bsum += c & 0xFF;
            }
            for (int x = 0; x < width; x++) {
                int a = asum / window;
                int r = rsum / window;
                int g = gsum / window;
                int b = bsum / window;
                dst[row + x] = (a << 24) | (r << 16) | (g << 8) | b;

                int xRemove = clamp(x - radius, 0, width - 1);
                int xAdd = clamp(x + radius + 1, 0, width - 1);
                int cRemove = src[row + xRemove];
                int cAdd = src[row + xAdd];
                asum += ((cAdd >> 24) & 0xFF) - ((cRemove >> 24) & 0xFF);
                rsum += ((cAdd >> 16) & 0xFF) - ((cRemove >> 16) & 0xFF);
                gsum += ((cAdd >> 8) & 0xFF) - ((cRemove >> 8) & 0xFF);
                bsum += (cAdd & 0xFF) - (cRemove & 0xFF);
            }
        }
    }

    private static void blurVertical(int[] src, int[] dst, int width, int height, int radius) {
        int window = radius * 2 + 1;
        for (int x = 0; x < width; x++) {
            int asum = 0;
            int rsum = 0;
            int gsum = 0;
            int bsum = 0;
            for (int i = -radius; i <= radius; i++) {
                int y = clamp(i, 0, height - 1);
                int c = src[y * width + x];
                asum += (c >> 24) & 0xFF;
                rsum += (c >> 16) & 0xFF;
                gsum += (c >> 8) & 0xFF;
                bsum += c & 0xFF;
            }
            for (int y = 0; y < height; y++) {
                int a = asum / window;
                int r = rsum / window;
                int g = gsum / window;
                int b = bsum / window;
                dst[y * width + x] = (a << 24) | (r << 16) | (g << 8) | b;

                int yRemove = clamp(y - radius, 0, height - 1);
                int yAdd = clamp(y + radius + 1, 0, height - 1);
                int cRemove = src[yRemove * width + x];
                int cAdd = src[yAdd * width + x];
                asum += ((cAdd >> 24) & 0xFF) - ((cRemove >> 24) & 0xFF);
                rsum += ((cAdd >> 16) & 0xFF) - ((cRemove >> 16) & 0xFF);
                gsum += ((cAdd >> 8) & 0xFF) - ((cRemove >> 8) & 0xFF);
                bsum += (cAdd & 0xFF) - (cRemove & 0xFF);
            }
        }
    }

    private static int clamp(int value, int min, int max) {
        return value < min ? min : Math.min(value, max);
    }
}
