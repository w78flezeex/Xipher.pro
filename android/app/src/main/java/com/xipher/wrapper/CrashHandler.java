package com.xipher.wrapper;

import android.content.Context;
import android.util.Log;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.io.FileWriter;
import java.io.File;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class CrashHandler implements Thread.UncaughtExceptionHandler {
    private static final String TAG = "CrashHandler";
    private final Context context;
    private final Thread.UncaughtExceptionHandler defaultHandler;
    
    public CrashHandler(Context context) {
        this.context = context;
        this.defaultHandler = Thread.getDefaultUncaughtExceptionHandler();
    }
    
    @Override
    public void uncaughtException(Thread t, Throwable e) {
        try {
            StringWriter sw = new StringWriter();
            PrintWriter pw = new PrintWriter(sw);
            e.printStackTrace(pw);
            String stackTrace = sw.toString();
            Log.e(TAG, "CRASH: " + stackTrace);
            
            String timestamp = new SimpleDateFormat("yyyy-MM-dd_HH-mm-ss", Locale.US).format(new Date());
            String header = "Crash at: " + timestamp + "\nThread: " + t.getName() + "\n\n";
            
            // Save to internal storage (always accessible)
            File internalCrash = new File(context.getFilesDir(), "crash.log");
            FileWriter fw1 = new FileWriter(internalCrash);
            fw1.write(header + stackTrace);
            fw1.close();
            
            // Also try external storage
            try {
                File crashFile = new File(context.getExternalFilesDir(null), "crash.log");
                FileWriter fw2 = new FileWriter(crashFile);
                fw2.write(header + stackTrace);
                fw2.close();
            } catch (Exception ignored) {}
            
        } catch (Exception ex) {
            Log.e(TAG, "Failed to write crash log", ex);
        }
        
        if (defaultHandler != null) {
            defaultHandler.uncaughtException(t, e);
        }
    }
}
