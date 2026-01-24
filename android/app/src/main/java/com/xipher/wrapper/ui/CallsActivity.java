package com.xipher.wrapper.ui;

import android.os.Bundle;

import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;

import com.xipher.wrapper.R;
import com.xipher.wrapper.data.db.AppDatabase;
import com.xipher.wrapper.data.db.entity.CallLogEntity;
import com.xipher.wrapper.databinding.ActivityCallsBinding;
import com.xipher.wrapper.ui.adapter.CallLogAdapter;

import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class CallsActivity extends AppCompatActivity {
    private ActivityCallsBinding binding;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private CallLogAdapter adapter;

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        binding = ActivityCallsBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());

        adapter = new CallLogAdapter();
        binding.recyclerCalls.setLayoutManager(new LinearLayoutManager(this));
        binding.recyclerCalls.setAdapter(adapter);

        BottomNavHelper.setup(this, binding.bottomNav, R.id.nav_calls);
        loadCalls();
    }

    @Override
    protected void onResume() {
        super.onResume();
        loadCalls();
    }

    private void loadCalls() {
        io.execute(() -> {
            List<CallLogEntity> logs;
            try {
                logs = AppDatabase.get(this).callLogDao().getAll();
            } catch (IllegalStateException e) {
                logs = new java.util.ArrayList<>();
            }
            List<CallLogEntity> finalLogs = logs;
            runOnUiThread(() -> {
                adapter.setItems(finalLogs);
                binding.emptyCalls.setVisibility(finalLogs == null || finalLogs.isEmpty() ? android.view.View.VISIBLE : android.view.View.GONE);
            });
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        io.shutdownNow();
    }
}
