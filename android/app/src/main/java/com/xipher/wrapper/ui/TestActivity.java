package com.xipher.wrapper.ui;

import android.content.Intent;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;

public class TestActivity extends AppCompatActivity {
    private static final String CRASH_FILE = "crash.log";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.TOP | Gravity.CENTER_HORIZONTAL);
        layout.setBackgroundColor(Color.parseColor("#17212B"));
        int pad = dp(16);
        layout.setPadding(pad, pad, pad, pad);
        
        TextView header = new TextView(this);
        header.setText("Xipher Test");
        header.setTextSize(20);
        header.setTextColor(Color.WHITE);
        header.setGravity(Gravity.CENTER_HORIZONTAL);
        layout.addView(header, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        ));

        File crashFile = new File(getFilesDir(), CRASH_FILE);
        String crashText = readCrashLog(crashFile);

        TextView status = new TextView(this);
        status.setText(crashText == null ? "No crash log found." : "Last crash log:");
        status.setTextSize(14);
        status.setTextColor(Color.WHITE);
        status.setGravity(Gravity.CENTER_HORIZONTAL);
        LinearLayout.LayoutParams statusParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        );
        statusParams.topMargin = dp(12);
        layout.addView(status, statusParams);

        if (crashText != null) {
            LinearLayout buttons = new LinearLayout(this);
            buttons.setOrientation(LinearLayout.HORIZONTAL);
            buttons.setGravity(Gravity.CENTER_HORIZONTAL);
            LinearLayout.LayoutParams buttonsParams = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
            );
            buttonsParams.topMargin = dp(12);
            layout.addView(buttons, buttonsParams);

            Button share = new Button(this);
            share.setText("Share");
            share.setOnClickListener(v -> shareCrashLog(crashText));
            LinearLayout.LayoutParams shareParams = new LinearLayout.LayoutParams(
                    0,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    1f
            );
            shareParams.rightMargin = dp(8);
            buttons.addView(share, shareParams);

            Button clear = new Button(this);
            clear.setText("Clear");
            clear.setOnClickListener(v -> {
                if (crashFile.exists() && !crashFile.delete()) {
                    return;
                }
                recreate();
            });
            LinearLayout.LayoutParams clearParams = new LinearLayout.LayoutParams(
                    0,
                    ViewGroup.LayoutParams.WRAP_CONTENT,
                    1f
            );
            buttons.addView(clear, clearParams);

            ScrollView scroll = new ScrollView(this);
            LinearLayout.LayoutParams scrollParams = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    0,
                    1f
            );
            scrollParams.topMargin = dp(12);

            TextView logView = new TextView(this);
            logView.setText(crashText);
            logView.setTextSize(12);
            logView.setTextColor(Color.WHITE);
            logView.setTextIsSelectable(true);
            logView.setPadding(dp(12), dp(12), dp(12), dp(12));
            scroll.addView(logView, new ViewGroup.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
            ));
            layout.addView(scroll, scrollParams);
        } else {
            TextView tv = new TextView(this);
            tv.setText("Xipher Test - App Works!");
            tv.setTextSize(18);
            tv.setTextColor(Color.WHITE);
            tv.setGravity(Gravity.CENTER_HORIZONTAL);
            LinearLayout.LayoutParams tvParams = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
            );
            tvParams.topMargin = dp(16);
            layout.addView(tv, tvParams);
        }

        setContentView(layout);
    }

    private String readCrashLog(File file) {
        if (file == null || !file.exists()) return null;
        StringBuilder sb = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(new FileReader(file))) {
            String line;
            while ((line = reader.readLine()) != null) {
                sb.append(line).append('\n');
            }
            return sb.toString().trim();
        } catch (IOException e) {
            return "Failed to read crash log: " + e.getMessage();
        }
    }

    private void shareCrashLog(String crashText) {
        if (crashText == null || crashText.isEmpty()) return;
        Intent sendIntent = new Intent(Intent.ACTION_SEND);
        sendIntent.setType("text/plain");
        sendIntent.putExtra(Intent.EXTRA_TEXT, crashText);
        startActivity(Intent.createChooser(sendIntent, "Share crash log"));
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}
