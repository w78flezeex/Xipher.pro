package com.xipher.wrapper.util;

import android.graphics.BlurMaskFilter;
import android.graphics.RenderEffect;
import android.graphics.Shader;
import android.os.Build;
import android.view.View;
import android.widget.TextView;

public final class BlurUtils {
    public static final int TEXT_BLUR_RADIUS = 12;
    public static final int IMAGE_BLUR_RADIUS = 16;
    public static final float IMAGE_BLUR_SAMPLING = 0.2f;
    private static final BlurTransformation IMAGE_BLUR = new BlurTransformation(IMAGE_BLUR_RADIUS, IMAGE_BLUR_SAMPLING);

    private BlurUtils() {}

    public static BlurTransformation imageBlur() {
        return IMAGE_BLUR;
    }

    public static void applyTextBlur(TextView view, boolean blur) {
        if (view == null) return;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            view.setRenderEffect(blur
                    ? RenderEffect.createBlurEffect(TEXT_BLUR_RADIUS, TEXT_BLUR_RADIUS, Shader.TileMode.CLAMP)
                    : null);
            return;
        }
        if (blur) {
            view.setLayerType(View.LAYER_TYPE_SOFTWARE, view.getPaint());
            view.getPaint().setMaskFilter(new BlurMaskFilter(TEXT_BLUR_RADIUS, BlurMaskFilter.Blur.NORMAL));
        } else {
            view.getPaint().setMaskFilter(null);
            view.setLayerType(View.LAYER_TYPE_NONE, null);
        }
    }
}
