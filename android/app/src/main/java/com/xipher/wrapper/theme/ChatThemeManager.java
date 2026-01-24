package com.xipher.wrapper.theme;

import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.Nullable;

import com.xipher.wrapper.R;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public final class ChatThemeManager {
    private static final String PREFS = "xipher_prefs";
    private static final String KEY_CHAT_THEME = "chat_theme";
    private static final String KEY_CUSTOM_WALLPAPER = "custom_wallpaper_path";
    private static final String KEY_WALLPAPER_BLUR = "wallpaper_blur";
    private static final String KEY_WALLPAPER_DIM = "wallpaper_dim";
    private static final String WALLPAPER_FILENAME = "chat_wallpaper.jpg";

    // Built-in themes
    public static final String THEME_DEFAULT = "default";
    public static final String THEME_CLASSIC = "classic";
    public static final String THEME_OCEAN = "ocean";
    public static final String THEME_SUNSET = "sunset";
    public static final String THEME_FOREST = "forest";
    public static final String THEME_MIDNIGHT = "midnight";
    public static final String THEME_VIOLET = "violet";
    public static final String THEME_AURORA = "aurora";
    public static final String THEME_NEBULA = "nebula";
    public static final String THEME_GRADIENT_PINK = "gradient_pink";
    public static final String THEME_GRADIENT_BLUE = "gradient_blue";
    public static final String THEME_CUSTOM = "custom";

    private ChatThemeManager() {}

    // ============ Theme Management ============

    public static void setTheme(Context context, String themeId) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        prefs.edit().putString(KEY_CHAT_THEME, themeId).apply();
    }

    public static String getTheme(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        return prefs.getString(KEY_CHAT_THEME, THEME_DEFAULT);
    }

    public static boolean isCustomTheme(Context context) {
        return THEME_CUSTOM.equals(getTheme(context));
    }

    // ============ Custom Wallpaper ============

    public static boolean saveCustomWallpaper(Context context, Uri imageUri) {
        try {
            InputStream inputStream = context.getContentResolver().openInputStream(imageUri);
            if (inputStream == null) return false;

            Bitmap bitmap = BitmapFactory.decodeStream(inputStream);
            inputStream.close();

            if (bitmap == null) return false;

            // Resize to reasonable size for performance
            int maxSize = 1920;
            if (bitmap.getWidth() > maxSize || bitmap.getHeight() > maxSize) {
                float scale = Math.min((float) maxSize / bitmap.getWidth(), 
                                       (float) maxSize / bitmap.getHeight());
                int newWidth = Math.round(bitmap.getWidth() * scale);
                int newHeight = Math.round(bitmap.getHeight() * scale);
                bitmap = Bitmap.createScaledBitmap(bitmap, newWidth, newHeight, true);
            }

            File file = new File(context.getFilesDir(), WALLPAPER_FILENAME);
            FileOutputStream fos = new FileOutputStream(file);
            bitmap.compress(Bitmap.CompressFormat.JPEG, 90, fos);
            fos.close();

            SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
            prefs.edit()
                .putString(KEY_CUSTOM_WALLPAPER, file.getAbsolutePath())
                .putString(KEY_CHAT_THEME, THEME_CUSTOM)
                .apply();

            return true;
        } catch (Exception e) {
            e.printStackTrace();
            return false;
        }
    }

    @Nullable
    public static Bitmap getCustomWallpaper(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String path = prefs.getString(KEY_CUSTOM_WALLPAPER, null);
        if (path == null) return null;

        File file = new File(path);
        if (!file.exists()) return null;

        return BitmapFactory.decodeFile(path);
    }

    public static boolean hasCustomWallpaper(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String path = prefs.getString(KEY_CUSTOM_WALLPAPER, null);
        if (path == null) return false;
        return new File(path).exists();
    }

    public static void deleteCustomWallpaper(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        String path = prefs.getString(KEY_CUSTOM_WALLPAPER, null);
        if (path != null) {
            new File(path).delete();
        }
        prefs.edit().remove(KEY_CUSTOM_WALLPAPER).apply();
        if (THEME_CUSTOM.equals(getTheme(context))) {
            setTheme(context, THEME_DEFAULT);
        }
    }

    // ============ Wallpaper Effects ============

    public static void setWallpaperBlur(Context context, int blurLevel) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        prefs.edit().putInt(KEY_WALLPAPER_BLUR, blurLevel).apply();
    }

    public static int getWallpaperBlur(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        return prefs.getInt(KEY_WALLPAPER_BLUR, 0);
    }

    public static void setWallpaperDim(Context context, int dimPercent) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        prefs.edit().putInt(KEY_WALLPAPER_DIM, dimPercent).apply();
    }

    public static int getWallpaperDim(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        return prefs.getInt(KEY_WALLPAPER_DIM, 0);
    }

    // ============ Background Resources ============

    public static int getBackgroundRes(Context context) {
        return getBackgroundResForTheme(getTheme(context));
    }

    public static int getBackgroundResForTheme(String themeId) {
        switch (themeId) {
            case THEME_CLASSIC:
                return R.drawable.bg_chat_classic;
            case THEME_OCEAN:
                return R.drawable.bg_chat_ocean;
            case THEME_SUNSET:
                return R.drawable.bg_chat_sunset;
            case THEME_FOREST:
                return R.drawable.bg_chat_forest;
            case THEME_MIDNIGHT:
                return R.drawable.bg_chat_midnight;
            case THEME_VIOLET:
                return R.drawable.bg_chat_violet;
            case THEME_AURORA:
                return R.drawable.bg_chat_aurora;
            case THEME_NEBULA:
                return R.drawable.bg_chat_nebula;
            case THEME_GRADIENT_PINK:
                return R.drawable.bg_chat_gradient_pink;
            case THEME_GRADIENT_BLUE:
                return R.drawable.bg_chat_gradient_blue;
            default:
                return R.drawable.bg_app;
        }
    }

    public static int getPreviewResForTheme(String themeId) {
        switch (themeId) {
            case THEME_CLASSIC:
                return R.drawable.bg_theme_preview_classic;
            case THEME_OCEAN:
                return R.drawable.bg_theme_preview_ocean;
            case THEME_SUNSET:
                return R.drawable.bg_theme_preview_sunset;
            case THEME_FOREST:
                return R.drawable.bg_theme_preview_forest;
            case THEME_MIDNIGHT:
                return R.drawable.bg_theme_preview_midnight;
            case THEME_VIOLET:
                return R.drawable.bg_theme_preview_violet;
            case THEME_AURORA:
                return R.drawable.bg_theme_preview_aurora;
            case THEME_NEBULA:
                return R.drawable.bg_theme_preview_nebula;
            case THEME_GRADIENT_PINK:
                return R.drawable.bg_theme_preview_gradient_pink;
            case THEME_GRADIENT_BLUE:
                return R.drawable.bg_theme_preview_gradient_blue;
            default:
                return R.drawable.bg_theme_preview_default;
        }
    }

    // ============ Apply to View ============

    public static void applyTo(View root, Context context) {
        if (root == null || context == null) return;

        String theme = getTheme(context);
        if (THEME_CUSTOM.equals(theme) && hasCustomWallpaper(context)) {
            Bitmap wallpaper = getCustomWallpaper(context);
            if (wallpaper != null) {
                Drawable drawable = new BitmapDrawable(context.getResources(), wallpaper);
                root.setBackground(drawable);
                return;
            }
        }
        root.setBackgroundResource(getBackgroundRes(context));
    }

    /**
     * Apply chat theme to chat fragment views.
     * @param container The messages container (FrameLayout)
     * @param backgroundImage The ImageView for custom wallpaper
     * @param dimOverlay The View for dim overlay
     * @param context The context
     */
    public static void applyChatBackground(View container, ImageView backgroundImage, View dimOverlay, Context context) {
        if (container == null || context == null) return;

        String theme = getTheme(context);
        int dim = getWallpaperDim(context);

        if (THEME_CUSTOM.equals(theme) && hasCustomWallpaper(context)) {
            // Custom wallpaper mode
            Bitmap wallpaper = getCustomWallpaper(context);
            if (wallpaper != null && backgroundImage != null) {
                container.setBackgroundColor(0xFF000000);
                backgroundImage.setImageBitmap(wallpaper);
                backgroundImage.setScaleType(ImageView.ScaleType.CENTER_CROP);
                backgroundImage.setVisibility(View.VISIBLE);
                
                if (dimOverlay != null) {
                    dimOverlay.setVisibility(View.VISIBLE);
                    dimOverlay.setAlpha(dim / 100f);
                }
                return;
            }
        }

        // Built-in theme mode
        if (backgroundImage != null) {
            backgroundImage.setVisibility(View.GONE);
        }
        if (dimOverlay != null) {
            dimOverlay.setVisibility(View.GONE);
        }
        container.setBackgroundResource(getBackgroundRes(context));
    }

    public static void applyToImageView(ImageView imageView, Context context) {
        if (imageView == null || context == null) return;

        String theme = getTheme(context);
        if (THEME_CUSTOM.equals(theme) && hasCustomWallpaper(context)) {
            Bitmap wallpaper = getCustomWallpaper(context);
            if (wallpaper != null) {
                imageView.setImageBitmap(wallpaper);
                imageView.setScaleType(ImageView.ScaleType.CENTER_CROP);
                return;
            }
        }
        imageView.setImageResource(getBackgroundRes(context));
        imageView.setScaleType(ImageView.ScaleType.CENTER_CROP);
    }

    // ============ Theme Info ============

    public static class ThemeInfo {
        public final String id;
        public final int nameRes;
        public final int previewRes;
        public final boolean isDark;

        public ThemeInfo(String id, int nameRes, int previewRes, boolean isDark) {
            this.id = id;
            this.nameRes = nameRes;
            this.previewRes = previewRes;
            this.isDark = isDark;
        }
    }

    public static ThemeInfo[] getLightThemes() {
        return new ThemeInfo[] {
            new ThemeInfo(THEME_DEFAULT, R.string.theme_default, R.drawable.bg_theme_preview_default, false),
            new ThemeInfo(THEME_CLASSIC, R.string.theme_classic, R.drawable.bg_theme_preview_classic, false),
            new ThemeInfo(THEME_VIOLET, R.string.theme_violet, R.drawable.bg_theme_preview_violet, false),
            new ThemeInfo(THEME_OCEAN, R.string.theme_ocean, R.drawable.bg_theme_preview_ocean, false),
            new ThemeInfo(THEME_SUNSET, R.string.theme_sunset, R.drawable.bg_theme_preview_sunset, false),
            new ThemeInfo(THEME_FOREST, R.string.theme_forest, R.drawable.bg_theme_preview_forest, false),
        };
    }

    public static ThemeInfo[] getDarkThemes() {
        return new ThemeInfo[] {
            new ThemeInfo(THEME_MIDNIGHT, R.string.theme_midnight, R.drawable.bg_theme_preview_midnight, true),
            new ThemeInfo(THEME_NEBULA, R.string.theme_nebula, R.drawable.bg_theme_preview_nebula, true),
            new ThemeInfo(THEME_AURORA, R.string.theme_aurora, R.drawable.bg_theme_preview_aurora, true),
        };
    }

    public static ThemeInfo[] getGradientThemes() {
        return new ThemeInfo[] {
            new ThemeInfo(THEME_GRADIENT_PINK, R.string.theme_gradient_pink, R.drawable.bg_theme_preview_gradient_pink, false),
            new ThemeInfo(THEME_GRADIENT_BLUE, R.string.theme_gradient_blue, R.drawable.bg_theme_preview_gradient_blue, true),
        };
    }
}
