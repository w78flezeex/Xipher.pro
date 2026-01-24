package com.xipher.wrapper.ui.fragment;

import android.graphics.Color;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.bottomsheet.BottomSheetBehavior;
import com.google.android.material.bottomsheet.BottomSheetDialog;
import com.google.android.material.bottomsheet.BottomSheetDialogFragment;
import com.google.gson.JsonArray;
import com.google.gson.JsonObject;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.api.ApiClient;
import com.xipher.wrapper.data.repo.AuthRepository;
import com.xipher.wrapper.databinding.BottomSheetVerificationBinding;
import com.xipher.wrapper.databinding.ItemVerificationRequirementBinding;
import com.xipher.wrapper.databinding.ItemVerificationRequestBinding;
import com.xipher.wrapper.di.AppServices;

import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class VerificationBottomSheet extends BottomSheetDialogFragment {

    private BottomSheetVerificationBinding binding;
    private AuthRepository authRepo;
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private RequestsAdapter adapter;

    public static VerificationBottomSheet newInstance() {
        return new VerificationBottomSheet();
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setStyle(STYLE_NORMAL, R.style.Theme_Xipher_BottomSheet);
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
                             @Nullable Bundle savedInstanceState) {
        binding = BottomSheetVerificationBinding.inflate(inflater, container, false);
        return binding.getRoot();
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        authRepo = AppServices.authRepository(requireContext());

        setupDialog();
        setupRequirements();
        setupForm();
        loadMyRequests();
    }

    private void setupDialog() {
        if (getDialog() instanceof BottomSheetDialog) {
            BottomSheetDialog dialog = (BottomSheetDialog) getDialog();
            dialog.getBehavior().setState(BottomSheetBehavior.STATE_EXPANDED);
            dialog.getBehavior().setSkipCollapsed(true);
        }

        binding.btnClose.setOnClickListener(v -> dismiss());
    }

    private void setupRequirements() {
        String[][] requirements = {
            {"👥", getString(R.string.verification_req_1_title), getString(R.string.verification_req_1_desc)},
            {"📊", getString(R.string.verification_req_2_title), getString(R.string.verification_req_2_desc)},
            {"⭐", getString(R.string.verification_req_3_title), getString(R.string.verification_req_3_desc)},
            {"📝", getString(R.string.verification_req_4_title), getString(R.string.verification_req_4_desc)},
            {"🔗", getString(R.string.verification_req_5_title), getString(R.string.verification_req_5_desc)},
            {"✅", getString(R.string.verification_req_6_title), getString(R.string.verification_req_6_desc)}
        };

        binding.requirementsContainer.removeAllViews();
        
        for (String[] req : requirements) {
            ItemVerificationRequirementBinding reqBinding = ItemVerificationRequirementBinding.inflate(
                LayoutInflater.from(getContext()), binding.requirementsContainer, false);
            reqBinding.reqIcon.setText(req[0]);
            reqBinding.reqTitle.setText(req[1]);
            reqBinding.reqDescription.setText(req[2]);
            binding.requirementsContainer.addView(reqBinding.getRoot());
        }
    }

    private void setupForm() {
        binding.btnSubmit.setOnClickListener(v -> submitRequest());
    }

    private void submitRequest() {
        String username = binding.etUsername.getText() != null 
            ? binding.etUsername.getText().toString().trim() : "";
        String reason = binding.etReason.getText() != null 
            ? binding.etReason.getText().toString().trim() : "";

        if (TextUtils.isEmpty(username)) {
            showStatus(false, getString(R.string.verification_error_username));
            return;
        }

        binding.btnSubmit.setEnabled(false);
        binding.btnSubmit.setText(R.string.loading);

        String token = authRepo.getToken();
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject result = client.requestVerification(token, username, reason);
                
                boolean success = result != null && result.has("success") 
                    && result.get("success").getAsBoolean();
                String message = result != null && result.has("message") 
                    ? result.get("message").getAsString() 
                    : (success ? getString(R.string.verification_success) : "Ошибка");

                if (getActivity() != null) {
                    getActivity().runOnUiThread(() -> {
                        binding.btnSubmit.setEnabled(true);
                        binding.btnSubmit.setText(R.string.verification_submit);
                        
                        if (success) {
                            binding.etUsername.setText("");
                            binding.etReason.setText("");
                            showStatus(true, getString(R.string.verification_success));
                            loadMyRequests();
                        } else {
                            showStatus(false, message);
                        }
                    });
                }
            } catch (Exception e) {
                if (getActivity() != null) {
                    getActivity().runOnUiThread(() -> {
                        binding.btnSubmit.setEnabled(true);
                        binding.btnSubmit.setText(R.string.verification_submit);
                        showStatus(false, "Ошибка сети");
                    });
                }
            }
        });
    }

    private void showStatus(boolean success, String message) {
        binding.tvStatus.setVisibility(View.VISIBLE);
        binding.tvStatus.setText(message);
        
        if (success) {
            binding.tvStatus.setBackgroundResource(R.drawable.status_success_bg);
            binding.tvStatus.setTextColor(Color.parseColor("#22C55E"));
        } else {
            binding.tvStatus.setBackgroundResource(R.drawable.status_error_bg);
            binding.tvStatus.setTextColor(Color.parseColor("#EF4444"));
        }
    }

    private void loadMyRequests() {
        String token = authRepo.getToken();
        io.execute(() -> {
            try {
                ApiClient client = new ApiClient(token);
                JsonObject result = client.getMyVerificationRequests(token);
                
                boolean success = result != null && result.has("success") 
                    && result.get("success").getAsBoolean();
                
                if (success && result.has("requests")) {
                    JsonArray arr = result.getAsJsonArray("requests");
                    List<VerificationRequest> requests = new ArrayList<>();
                    
                    for (int i = 0; i < arr.size(); i++) {
                        JsonObject obj = arr.get(i).getAsJsonObject();
                        VerificationRequest req = new VerificationRequest();
                        req.id = obj.has("id") ? obj.get("id").getAsString() : "";
                        req.channelName = obj.has("channel_name") ? obj.get("channel_name").getAsString() : "";
                        req.channelUsername = obj.has("channel_username") ? obj.get("channel_username").getAsString() : "";
                        req.status = obj.has("status") ? obj.get("status").getAsString() : "";
                        req.adminComment = obj.has("admin_comment") ? obj.get("admin_comment").getAsString() : "";
                        req.createdAt = obj.has("created_at") ? obj.get("created_at").getAsString() : "";
                        requests.add(req);
                    }
                    
                    if (getActivity() != null) {
                        getActivity().runOnUiThread(() -> {
                            if (!requests.isEmpty()) {
                                binding.myRequestsSection.setVisibility(View.VISIBLE);
                                adapter = new RequestsAdapter(requests);
                                binding.rvMyRequests.setLayoutManager(new LinearLayoutManager(getContext()));
                                binding.rvMyRequests.setAdapter(adapter);
                            }
                        });
                    }
                }
            } catch (Exception e) {
                e.printStackTrace();
            }
        });
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        binding = null;
        io.shutdownNow();
    }

    // Data class
    static class VerificationRequest {
        String id;
        String channelName;
        String channelUsername;
        String status;
        String adminComment;
        String createdAt;
    }

    // Adapter
    class RequestsAdapter extends RecyclerView.Adapter<RequestsAdapter.VH> {
        private final List<VerificationRequest> items;

        RequestsAdapter(List<VerificationRequest> items) {
            this.items = items;
        }

        @NonNull
        @Override
        public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            ItemVerificationRequestBinding b = ItemVerificationRequestBinding.inflate(
                LayoutInflater.from(parent.getContext()), parent, false);
            return new VH(b);
        }

        @Override
        public void onBindViewHolder(@NonNull VH holder, int position) {
            VerificationRequest req = items.get(position);
            holder.bind(req);
        }

        @Override
        public int getItemCount() {
            return items.size();
        }

        class VH extends RecyclerView.ViewHolder {
            private final ItemVerificationRequestBinding b;

            VH(ItemVerificationRequestBinding binding) {
                super(binding.getRoot());
                this.b = binding;
            }

            void bind(VerificationRequest req) {
                b.tvChannelName.setText(req.channelName);
                b.tvChannelUsername.setText("@" + req.channelUsername);
                
                // Format date
                try {
                    SimpleDateFormat inFmt = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss", Locale.getDefault());
                    SimpleDateFormat outFmt = new SimpleDateFormat("dd.MM.yyyy", Locale.getDefault());
                    Date date = inFmt.parse(req.createdAt);
                    if (date != null) {
                        b.tvDate.setText(outFmt.format(date));
                    }
                } catch (Exception e) {
                    b.tvDate.setText(req.createdAt.length() > 10 ? req.createdAt.substring(0, 10) : req.createdAt);
                }

                // Status
                switch (req.status) {
                    case "pending":
                        b.tvStatus.setText(R.string.verification_status_pending);
                        b.tvStatus.setTextColor(Color.parseColor("#F59E0B"));
                        break;
                    case "approved":
                        b.tvStatus.setText(R.string.verification_status_approved);
                        b.tvStatus.setTextColor(Color.parseColor("#22C55E"));
                        break;
                    case "rejected":
                        b.tvStatus.setText(R.string.verification_status_rejected);
                        b.tvStatus.setTextColor(Color.parseColor("#EF4444"));
                        break;
                    default:
                        b.tvStatus.setText(req.status);
                        b.tvStatus.setTextColor(Color.parseColor("#6B7280"));
                }

                // Admin comment
                if (!TextUtils.isEmpty(req.adminComment)) {
                    b.tvAdminComment.setVisibility(View.VISIBLE);
                    b.tvAdminComment.setText(req.adminComment);
                } else {
                    b.tvAdminComment.setVisibility(View.GONE);
                }
            }
        }
    }
}
