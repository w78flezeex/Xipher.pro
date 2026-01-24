# Xipher ProGuard Rules - Security Hardened
# OWASP Mobile Top 10 2024 Compliant

# Keep only what's strictly necessary for WebView JS interface
-keepclassmembers class com.xipher.wrapper.WebAppInterface {
    @android.webkit.JavascriptInterface <methods>;
}

# Keep UpdateManager for reflection
-keep class com.xipher.wrapper.UpdateManager { *; }

# Keep security-critical classes properly obfuscated but functional
-keep class com.xipher.wrapper.security.** { *; }

# ViewModel
-keep class * extends androidx.lifecycle.ViewModel { *; }
-keep class com.xipher.wrapper.ui.vm.** { *; }

# UI classes
-keep class com.xipher.wrapper.ui.** { *; }
-keep class com.xipher.wrapper.ui.adapter.** { *; }
-keep class com.xipher.wrapper.ui.fragment.** { *; }

# Room database entities and DAOs
-keep class * extends androidx.room.RoomDatabase
-keep @androidx.room.Entity class *
-keepclassmembers @androidx.room.Entity class * { *; }
-keep @androidx.room.Dao interface *
-keep class com.xipher.wrapper.data.db.** { *; }
-keep class com.xipher.wrapper.data.db.entity.** { *; }
-keep class com.xipher.wrapper.data.db.dao.** { *; }
-keep class com.xipher.wrapper.data.db.AppDatabase { *; }
-keep class com.xipher.wrapper.data.db.DatabaseManager { *; }
-keepclassmembers class com.xipher.wrapper.data.db.entity.** { *; }

# Firebase
-keep class com.google.firebase.** { *; }
-dontwarn com.google.firebase.**

# RuStore Push
-keep class ru.rustore.** { *; }
-dontwarn ru.rustore.**

# OkHttp
-keepnames class okhttp3.internal.publicsuffix.PublicSuffixDatabase
-dontwarn okhttp3.**
-dontwarn okio.**

# WebRTC
-keep class org.webrtc.** { *; }
-dontwarn org.webrtc.**

# Yandex MapKit
-keep class com.yandex.** { *; }
-dontwarn com.yandex.**

# SQLCipher
-keep class net.zetetic.database.** { *; }
-dontwarn net.zetetic.**

# ============================================
# WALLET SECURITY RULES (MAXIMUM OBFUSCATION)
# ============================================

# Keep WalletSecurity methods needed for validation
-keep class com.xipher.wrapper.wallet.WalletSecurity {
    public static *** getInstance(...);
    public *** validateAddress(...);
    public *** validateAmount(...);
    public *** validateSeedPhrase(...);
    public *** validatePassword(...);
    public *** checkRateLimit(...);
    public *** escapeHtml(...);
    public *** sanitizeSql(...);
}

# Keep WalletManager public interface but obfuscate internals
-keep class com.xipher.wrapper.wallet.WalletManager {
    public static *** getInstance(...);
    public *** createWallet(...);
    public *** importWallet(...);
    public *** sendTransaction(...);
    public *** hasWallet();
    public *** getAddress(...);
    public *** getBalances();
    public *** fetchAllBalances(...);
}

# Keep WalletActivity
-keep class com.xipher.wrapper.wallet.WalletActivity { *; }

# Keep inner classes for callbacks
-keep class com.xipher.wrapper.wallet.WalletManager$WalletCallback { *; }
-keep class com.xipher.wrapper.wallet.WalletManager$WalletBalance { *; }
-keep class com.xipher.wrapper.wallet.WalletSecurity$ValidationResult { *; }

# Obfuscate encryption internals (DO NOT KEEP - just don't break)
-keepclassmembers class com.xipher.wrapper.wallet.WalletSecurity {
    private <fields>;
}
-keepclassmembers class com.xipher.wrapper.wallet.WalletManager {
    private <fields>;
}

# Encryption classes (keep methods but obfuscate names)
-dontwarn javax.crypto.**
-dontwarn java.security.**

# Glide
-keep public class * implements com.bumptech.glide.module.GlideModule
-keep class * extends com.bumptech.glide.module.AppGlideModule { <init>(...); }
-keep public enum com.bumptech.glide.load.ImageHeaderParser$** {
    **[] $VALUES;
    public *;
}

# Remove debug logs in release
-assumenosideeffects class android.util.Log {
    public static int d(...);
    public static int v(...);
    public static int i(...);
}

# Security: Obfuscate everything else
-repackageclasses 'x'
-allowaccessmodification
-flattenpackagehierarchy

# Gson - keep model classes
-keep class com.xipher.wrapper.data.model.** { *; }
-keep class com.xipher.wrapper.data.api.** { *; }
-keepclassmembers class * {
    @com.google.gson.annotations.SerializedName <fields>;
}

# Keep generic signatures for Gson
-keepattributes Signature
-keepattributes *Annotation*
-keepattributes EnclosingMethod
-keepattributes InnerClasses

# Gson specific - FULL KEEP
-keep class com.google.gson.** { *; }
-keep class com.google.gson.stream.** { *; }
-keepclassmembers class com.google.gson.** { *; }
-keep class sun.misc.Unsafe { *; }
-dontwarn sun.misc.**
-keep class * implements com.google.gson.TypeAdapterFactory
-keep class * implements com.google.gson.JsonSerializer
-keep class * implements com.google.gson.JsonDeserializer
-keepclassmembers,allowobfuscation class * {
    @com.google.gson.annotations.SerializedName <fields>;
}

# Prevent R8 from removing fields used by Gson
-keepclassmembers class com.xipher.wrapper.data.model.** {
    <fields>;
    <init>();
    <init>(...);
}

# Keep all data classes with default constructors
-keep class com.xipher.wrapper.data.model.AuthResponse { *; }
-keep class com.xipher.wrapper.data.model.AuthResponse$* { *; }
-keep class com.xipher.wrapper.data.model.BasicResponse { *; }
-keep class com.xipher.wrapper.data.model.ChatDto { *; }
-keep class com.xipher.wrapper.data.model.MessageDto { *; }
-keep class com.xipher.wrapper.data.model.FriendDto { *; }
-keep class com.xipher.wrapper.data.model.GroupDto { *; }
-keep class com.xipher.wrapper.data.model.ChannelDto { *; }
-keep class com.xipher.wrapper.data.model.GroupMemberDto { *; }
-keep class com.xipher.wrapper.data.model.GroupMemberDto$* { *; }

# Gson TypeToken fix
-keep class * extends com.google.gson.reflect.TypeToken { *; }
