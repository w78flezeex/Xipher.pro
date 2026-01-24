package com.xipher.wrapper.ui.adapter;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.AnimationUtils;
import android.content.res.ColorStateList;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.util.TypedValue;

import androidx.annotation.NonNull;
import androidx.appcompat.widget.AppCompatCheckBox;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.ColorUtils;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.widget.CompoundButtonCompat;
import androidx.recyclerview.widget.DiffUtil;
import androidx.recyclerview.widget.RecyclerView;

import com.bumptech.glide.Glide;
import com.xipher.wrapper.BuildConfig;
import com.xipher.wrapper.R;
import com.xipher.wrapper.data.db.entity.MessageEntity;
import com.xipher.wrapper.data.model.ChecklistItem;
import com.xipher.wrapper.data.model.ChecklistPayload;
import com.xipher.wrapper.data.model.PremiumGiftPayload;
import com.xipher.wrapper.ui.videonote.VideoNoteView;
import com.xipher.wrapper.util.LocationUtils;
import com.xipher.wrapper.util.ChecklistCodec;
import com.xipher.wrapper.util.BlurUtils;
import com.xipher.wrapper.util.PremiumGiftCodec;
import com.xipher.wrapper.util.SpoilerCodec;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

public class MessageAdapter extends RecyclerView.Adapter<MessageAdapter.VH> {

    private static final Object PAYLOAD_STATUS = new Object();
    private static final Object PAYLOAD_TRANSFER = new Object();
    public static final int TRANSFER_IDLE = 0;
    public static final int TRANSFER_LOADING = 1;
    public static final int TRANSFER_DONE = 2;
    private final List<MessageEntity> items;
    private final Map<String, TransferState> transferStates = new HashMap<>();
    private String selfId;
    private String selfAvatarUrl;
    private String peerAvatarUrl;
    private boolean showAvatarImages;
    private Map<String, String> avatarByUserId = new HashMap<>();
    private final Set<String> selectedIds = new HashSet<>();
    private final Set<String> revealedSpoilerIds = new HashSet<>();
    private String playingId;
    private int playingPositionMs;
    private int playingDurationMs;
    private ActionListener listener;
    private boolean showSenderNames;

    public interface ActionListener {
        void onVoiceClick(MessageEntity message);
        void onFileClick(MessageEntity message);
        void onImageClick(MessageEntity message);
        void onLocationClick(MessageEntity message);
        void onMessageClick(MessageEntity message);
        boolean onMessageLongClick(MessageEntity message);
        void onMessageExpired(MessageEntity message);
        void onTransferStart(MessageEntity message);
        void onTransferCancel(MessageEntity message);
        void onChecklistToggle(MessageEntity message, String checklistId, String itemId, boolean done);
        void onChecklistAddItem(MessageEntity message, String checklistId);
        default void onReplyClick(MessageEntity message) {}
    }

    public MessageAdapter(List<MessageEntity> items) {
        this.items = items;
    }

    public void setSelfId(String id) {
        this.selfId = id;
    }

    public void setAvatarUrls(String selfUrl, String peerUrl) {
        this.selfAvatarUrl = selfUrl;
        this.peerAvatarUrl = peerUrl;
        notifyDataSetChanged();
    }

    public void setShowAvatarImages(boolean show) {
        this.showAvatarImages = show;
        notifyDataSetChanged();
    }

    public void setAvatarMap(Map<String, String> map) {
        this.avatarByUserId = map != null ? map : new HashMap<>();
        notifyDataSetChanged();
    }

    public void update(List<MessageEntity> data) {
        List<MessageEntity> next = data != null ? new ArrayList<>(data) : new ArrayList<>();
        // Remove null elements to prevent NPE
        next.removeIf(m -> m == null);
        List<MessageEntity> prev = new ArrayList<>(items);
        DiffUtil.DiffResult diff = DiffUtil.calculateDiff(new MessageDiff(prev, next));
        items.clear();
        items.addAll(next);
        Set<String> existing = new HashSet<>();
        for (MessageEntity m : items) {
            if (m == null) continue;
            String key = getMessageKey(m);
            if (key != null) {
                existing.add(key);
            }
        }
        selectedIds.retainAll(existing);
        revealedSpoilerIds.retainAll(existing);
        diff.dispatchUpdatesTo(this);
    }

    public void setTransferState(String messageKey, int state, int progress) {
        if (messageKey == null || messageKey.isEmpty()) return;
        TransferState info = transferStates.get(messageKey);
        if (info == null) {
            info = new TransferState();
            transferStates.put(messageKey, info);
        }
        info.state = state;
        info.progress = Math.max(0, Math.min(100, progress));
        int index = findPositionByKey(messageKey);
        if (index >= 0) {
            notifyItemChanged(index, PAYLOAD_TRANSFER);
        }
    }

    public void clearTransferState(String messageKey) {
        if (messageKey == null || messageKey.isEmpty()) return;
        transferStates.remove(messageKey);
        int index = findPositionByKey(messageKey);
        if (index >= 0) {
            notifyItemChanged(index, PAYLOAD_TRANSFER);
        }
    }

    public int getSelectedCount() {
        return selectedIds.size();
    }

    public boolean hasSelection() {
        return !selectedIds.isEmpty();
    }

    public void clearSelection() {
        if (selectedIds.isEmpty()) return;
        selectedIds.clear();
        notifyDataSetChanged();
    }

    public void toggleSelection(MessageEntity message) {
        if (message == null || message.id == null) return;
        if (selectedIds.contains(message.id)) {
            selectedIds.remove(message.id);
        } else {
            selectedIds.add(message.id);
        }
        int index = findPositionById(message.id);
        if (index >= 0) {
            notifyItemChanged(index);
        } else {
            notifyDataSetChanged();
        }
    }

    public List<MessageEntity> getSelectedMessages() {
        List<MessageEntity> result = new ArrayList<>();
        for (MessageEntity m : items) {
            if (m != null && m.id != null && selectedIds.contains(m.id)) {
                result.add(m);
            }
        }
        return result;
    }

    public void setPlayingId(String id) {
        setPlaybackState(id, 0, 0);
    }

    public void setPlaybackState(String id, int positionMs, int durationMs) {
        String prevId = playingId;
        playingId = id;
        playingPositionMs = positionMs;
        playingDurationMs = durationMs;
        notifyPlaybackChanged(prevId, playingId);
    }

    public void clearPlaybackState() {
        String prevId = playingId;
        playingId = null;
        playingPositionMs = 0;
        playingDurationMs = 0;
        notifyPlaybackChanged(prevId, null);
    }

    public void setListener(ActionListener listener) {
        this.listener = listener;
    }

    public void setShowSenderNames(boolean show) {
        this.showSenderNames = show;
        notifyDataSetChanged();
    }

    @NonNull
    @Override
    public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View v = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_message, parent, false);
        return new VH(v);
    }

    @Override
    public void onBindViewHolder(@NonNull VH holder, int position, @NonNull List<Object> payloads) {
        if (!payloads.isEmpty()) {
            for (Object payload : payloads) {
                if (payload == PAYLOAD_STATUS) {
                    MessageEntity m = items.get(position);
                    if (m == null) return;
                    boolean isMe = m.senderId != null && m.senderId.equals(selfId);
                    holder.bindStatus(m, isMe);
                    holder.bindTtl(m, isMe);
                    return;
                }
                if (payload == PAYLOAD_TRANSFER) {
                    MessageEntity m = items.get(position);
                    if (m == null) return;
                    boolean isMe = m.senderId != null && m.senderId.equals(selfId);
                    TransferState transferState = getTransferState(getMessageKey(m));
                    holder.bindTransferState(m, isMe, transferState);
                    return;
                }
            }
        }
        super.onBindViewHolder(holder, position, payloads);
    }

    @Override
    public void onBindViewHolder(@NonNull VH holder, int position) {
        MessageEntity m = items.get(position);
        if (m == null) {
            holder.itemView.setVisibility(View.GONE);
            return;
        }
        boolean selected = m.id != null && selectedIds.contains(m.id);
        boolean isMe = m.senderId != null && m.senderId.equals(selfId);
        holder.bind(m, isMe, playingId, playingPositionMs, playingDurationMs,
                showSenderNames, listener, showAvatarImages, selfAvatarUrl, peerAvatarUrl, avatarByUserId, selected,
                getTransferState(getMessageKey(m)));
    }

    @Override
    public int getItemCount() {
        return items.size();
    }

    public MessageEntity getItem(int position) {
        if (position >= 0 && position < items.size()) {
            return items.get(position);
        }
        return null;
    }

    @Override
    public void onViewRecycled(@NonNull VH holder) {
        holder.releaseVideo();
        holder.clearTtl();
        holder.clearGiftViews();
        super.onViewRecycled(holder);
    }

    private void notifyPlaybackChanged(String prevId, String newId) {
        if (prevId != null && !prevId.equals(newId)) {
            int prevIndex = findPositionById(prevId);
            if (prevIndex >= 0) {
                notifyItemChanged(prevIndex);
            }
        }
        if (newId != null) {
            int index = findPositionById(newId);
            if (index >= 0) {
                notifyItemChanged(index);
                return;
            }
        }
        notifyDataSetChanged();
    }

    private int findPositionByKey(String key) {
        if (key == null) return -1;
        for (int i = 0; i < items.size(); i++) {
            MessageEntity m = items.get(i);
            if (m != null && key.equals(getMessageKey(m))) return i;
        }
        return -1;
    }

    private int findPositionById(String id) {
        if (id == null) return -1;
        for (int i = 0; i < items.size(); i++) {
            MessageEntity m = items.get(i);
            if (m != null && id.equals(m.id)) return i;
        }
        return -1;
    }

    private static String getMessageKey(MessageEntity m) {
        if (m == null) return null;
        if (m.id != null && !m.id.isEmpty()) return m.id;
        if (m.tempId != null && !m.tempId.isEmpty()) return m.tempId;
        return null;
    }

    private TransferState getTransferState(String key) {
        if (key == null) return null;
        return transferStates.get(key);
    }

    private static boolean safeEq(Object a, Object b) {
        return a == null ? b == null : a.equals(b);
    }

    private static boolean sameContentExceptStatus(MessageEntity a, MessageEntity b) {
        if (a == b) return true;
        if (a == null || b == null) return false;
        return safeEq(a.senderId, b.senderId)
                && safeEq(a.senderName, b.senderName)
                && safeEq(a.content, b.content)
                && safeEq(a.messageType, b.messageType)
                && safeEq(a.filePath, b.filePath)
                && safeEq(a.fileName, b.fileName)
                && safeEq(a.fileSize, b.fileSize)
                && safeEq(a.replyToMessageId, b.replyToMessageId)
                && safeEq(a.createdAt, b.createdAt)
                && a.createdAtMillis == b.createdAtMillis
                && a.ttlSeconds == b.ttlSeconds
                && a.readTimestamp == b.readTimestamp;
    }

    private static boolean sameContent(MessageEntity a, MessageEntity b) {
        return sameContentExceptStatus(a, b) && safeEq(a.status, b.status);
    }

    private static class MessageDiff extends DiffUtil.Callback {
        private final List<MessageEntity> oldList;
        private final List<MessageEntity> newList;

        MessageDiff(List<MessageEntity> oldList, List<MessageEntity> newList) {
            this.oldList = oldList != null ? oldList : new ArrayList<>();
            this.newList = newList != null ? newList : new ArrayList<>();
        }

        @Override
        public int getOldListSize() {
            return oldList.size();
        }

        @Override
        public int getNewListSize() {
            return newList.size();
        }

        @Override
        public boolean areItemsTheSame(int oldItemPosition, int newItemPosition) {
            MessageEntity oldItem = oldList.get(oldItemPosition);
            MessageEntity newItem = newList.get(newItemPosition);
            String oldKey = getMessageKey(oldItem);
            String newKey = getMessageKey(newItem);
            if (oldKey != null && newKey != null) {
                return oldKey.equals(newKey);
            }
            return oldItemPosition == newItemPosition;
        }

        @Override
        public boolean areContentsTheSame(int oldItemPosition, int newItemPosition) {
            return sameContent(oldList.get(oldItemPosition), newList.get(newItemPosition));
        }

        @Override
        public Object getChangePayload(int oldItemPosition, int newItemPosition) {
            MessageEntity oldItem = oldList.get(oldItemPosition);
            MessageEntity newItem = newList.get(newItemPosition);
            if (oldItem == null || newItem == null) return null;
            if (sameContentExceptStatus(oldItem, newItem) && !safeEq(oldItem.status, newItem.status)) {
                return PAYLOAD_STATUS;
            }
            return null;
        }
    }

    class VH extends RecyclerView.ViewHolder {
        ActionListener currentListener;
        View rowIn, rowOut;
        TextView avatarIn;
        View avatarOut; // FrameLayout in layout, not used for display
        android.widget.ImageView avatarInImage, avatarOutImage;
        TextView senderIn, textIn, textOut, timeIn, timeOut, ttlIn, ttlOut;
        android.widget.ImageView statusOut;
        View voiceIn, voiceOut, fileIn, fileOut;
        View locationIn, locationOut;
        View checklistIn, checklistOut;
        LinearLayout bubbleIn, bubbleOut;
        View giftIn, giftOut;
        TextView giftTitleIn, giftTitleOut, giftSubtitleIn, giftSubtitleOut;
        TextView giftDurationIn, giftDurationOut, giftPriceIn, giftPriceOut, giftDotIn, giftDotOut;
        View giftIconWrapIn, giftIconWrapOut, giftShineIn, giftShineOut;
        View tailIn, tailOut; // Hidden placeholders (0dp)
        ImageButton voicePlayIn, voicePlayOut;
        android.widget.ProgressBar voiceProgressIn, voiceProgressOut;
        TextView voiceDurationIn, voiceDurationOut;
        TextView fileNameIn, fileNameOut, fileSizeIn, fileSizeOut;
        View imageInContainer, imageOutContainer;
        android.widget.ImageView imageIn, imageOut;
        android.widget.ImageView imageInSpoiler, imageOutSpoiler;
        TextView imageInSpoilerLabel, imageOutSpoilerLabel;
        VideoNoteView videoNoteIn, videoNoteOut;
        android.widget.ImageView locationMapIn, locationMapOut;
        TextView locationTitleIn, locationTitleOut, locationSubtitleIn, locationSubtitleOut;
        TextView checklistTitleIn, checklistTitleOut, checklistProgressIn, checklistProgressOut;
        TextView checklistAddIn, checklistAddOut;
        LinearLayout checklistItemsIn, checklistItemsOut;
        TransferViews imageTransferIn, imageTransferOut;
        TransferViews fileTransferIn, fileTransferOut;
        View replyBlockIn, replyBlockOut;
        TextView replySenderIn, replySenderOut;
        TextView replyContentIn, replyContentOut;
        int bubbleInPaddingLeft;
        int bubbleInPaddingTop;
        int bubbleInPaddingRight;
        int bubbleInPaddingBottom;
        int bubbleOutPaddingLeft;
        int bubbleOutPaddingTop;
        int bubbleOutPaddingRight;
        int bubbleOutPaddingBottom;
        int videoNotePadding;
        Runnable ttlRunnable;
        String ttlKey;
        long ttlExpiresAt;

        VH(@NonNull View itemView) {
            super(itemView);
            rowIn = itemView.findViewById(R.id.rowIn);
            rowOut = itemView.findViewById(R.id.rowOut);
            avatarIn = itemView.findViewById(R.id.avatarIn);
            avatarOut = itemView.findViewById(R.id.avatarOut);
            avatarInImage = itemView.findViewById(R.id.avatarInImage);
            avatarOutImage = itemView.findViewById(R.id.avatarOutImage);
            senderIn = itemView.findViewById(R.id.msgSenderIn);
            textIn = itemView.findViewById(R.id.msgTextIn);
            textOut = itemView.findViewById(R.id.msgTextOut);
            timeIn = itemView.findViewById(R.id.msgTimeIn);
            timeOut = itemView.findViewById(R.id.msgTimeOut);
            ttlIn = itemView.findViewById(R.id.msgTtlIn);
            ttlOut = itemView.findViewById(R.id.msgTtlOut);
            statusOut = itemView.findViewById(R.id.msgStatusOut);
            voiceIn = itemView.findViewById(R.id.msgVoiceIn);
            voiceOut = itemView.findViewById(R.id.msgVoiceOut);
            fileIn = itemView.findViewById(R.id.msgFileIn);
            fileOut = itemView.findViewById(R.id.msgFileOut);
            locationIn = itemView.findViewById(R.id.msgLocationIn);
            locationOut = itemView.findViewById(R.id.msgLocationOut);
            checklistIn = itemView.findViewById(R.id.msgChecklistIn);
            checklistOut = itemView.findViewById(R.id.msgChecklistOut);
            bubbleIn = itemView.findViewById(R.id.bubbleIn);
            bubbleOut = itemView.findViewById(R.id.bubbleOut);
            giftIn = itemView.findViewById(R.id.msgGiftIn);
            giftOut = itemView.findViewById(R.id.msgGiftOut);
            tailIn = itemView.findViewById(R.id.tailIn);
            tailOut = itemView.findViewById(R.id.tailOut);
            voicePlayIn = itemView.findViewById(R.id.btnVoicePlayIn);
            voicePlayOut = itemView.findViewById(R.id.btnVoicePlayOut);
            voiceProgressIn = itemView.findViewById(R.id.msgVoiceProgressIn);
            voiceProgressOut = itemView.findViewById(R.id.msgVoiceProgressOut);
            voiceDurationIn = itemView.findViewById(R.id.msgVoiceDurationIn);
            voiceDurationOut = itemView.findViewById(R.id.msgVoiceDurationOut);
            fileNameIn = itemView.findViewById(R.id.msgFileNameIn);
            fileNameOut = itemView.findViewById(R.id.msgFileNameOut);
            fileSizeIn = itemView.findViewById(R.id.msgFileSizeIn);
            fileSizeOut = itemView.findViewById(R.id.msgFileSizeOut);
            imageInContainer = itemView.findViewById(R.id.msgImageInContainer);
            imageOutContainer = itemView.findViewById(R.id.msgImageOutContainer);
            imageIn = itemView.findViewById(R.id.msgImageIn);
            imageOut = itemView.findViewById(R.id.msgImageOut);
            imageInSpoiler = itemView.findViewById(R.id.msgImageInSpoiler);
            imageOutSpoiler = itemView.findViewById(R.id.msgImageOutSpoiler);
            imageInSpoilerLabel = itemView.findViewById(R.id.msgImageInSpoilerLabel);
            imageOutSpoilerLabel = itemView.findViewById(R.id.msgImageOutSpoilerLabel);
            imageTransferIn = TransferViews.from(itemView.findViewById(R.id.msgImageTransferIn), true);
            imageTransferOut = TransferViews.from(itemView.findViewById(R.id.msgImageTransferOut), true);
            fileTransferIn = TransferViews.from(itemView.findViewById(R.id.msgFileTransferIn), false);
            fileTransferOut = TransferViews.from(itemView.findViewById(R.id.msgFileTransferOut), false);
            videoNoteIn = itemView.findViewById(R.id.msgVideoNoteIn);
            videoNoteOut = itemView.findViewById(R.id.msgVideoNoteOut);
            locationMapIn = itemView.findViewById(R.id.msgLocationMapIn);
            locationMapOut = itemView.findViewById(R.id.msgLocationMapOut);
            locationTitleIn = itemView.findViewById(R.id.msgLocationTitleIn);
            locationTitleOut = itemView.findViewById(R.id.msgLocationTitleOut);
            locationSubtitleIn = itemView.findViewById(R.id.msgLocationSubtitleIn);
            locationSubtitleOut = itemView.findViewById(R.id.msgLocationSubtitleOut);
            checklistTitleIn = itemView.findViewById(R.id.msgChecklistTitleIn);
            checklistTitleOut = itemView.findViewById(R.id.msgChecklistTitleOut);
            checklistProgressIn = itemView.findViewById(R.id.msgChecklistProgressIn);
            checklistProgressOut = itemView.findViewById(R.id.msgChecklistProgressOut);
            checklistItemsIn = itemView.findViewById(R.id.msgChecklistItemsIn);
            checklistItemsOut = itemView.findViewById(R.id.msgChecklistItemsOut);
            checklistAddIn = itemView.findViewById(R.id.msgChecklistAddIn);
            checklistAddOut = itemView.findViewById(R.id.msgChecklistAddOut);
            replyBlockIn = itemView.findViewById(R.id.replyBlockIn);
            replyBlockOut = itemView.findViewById(R.id.replyBlockOut);
            replySenderIn = itemView.findViewById(R.id.replySenderIn);
            replySenderOut = itemView.findViewById(R.id.replySenderOut);
            replyContentIn = itemView.findViewById(R.id.replyContentIn);
            replyContentOut = itemView.findViewById(R.id.replyContentOut);
            if (giftIn != null) {
                giftTitleIn = giftIn.findViewById(R.id.giftTitle);
                giftSubtitleIn = giftIn.findViewById(R.id.giftSubtitle);
                giftDurationIn = giftIn.findViewById(R.id.giftDuration);
                giftPriceIn = giftIn.findViewById(R.id.giftPrice);
                giftDotIn = giftIn.findViewById(R.id.giftDot);
                giftIconWrapIn = giftIn.findViewById(R.id.giftIconWrap);
                giftShineIn = giftIn.findViewById(R.id.giftShine);
            }
            if (giftOut != null) {
                giftTitleOut = giftOut.findViewById(R.id.giftTitle);
                giftSubtitleOut = giftOut.findViewById(R.id.giftSubtitle);
                giftDurationOut = giftOut.findViewById(R.id.giftDuration);
                giftPriceOut = giftOut.findViewById(R.id.giftPrice);
                giftDotOut = giftOut.findViewById(R.id.giftDot);
                giftIconWrapOut = giftOut.findViewById(R.id.giftIconWrap);
                giftShineOut = giftOut.findViewById(R.id.giftShine);
            }
            if (bubbleIn != null) {
                bubbleInPaddingLeft = bubbleIn.getPaddingLeft();
                bubbleInPaddingTop = bubbleIn.getPaddingTop();
                bubbleInPaddingRight = bubbleIn.getPaddingRight();
                bubbleInPaddingBottom = bubbleIn.getPaddingBottom();
            }
            if (bubbleOut != null) {
                bubbleOutPaddingLeft = bubbleOut.getPaddingLeft();
                bubbleOutPaddingTop = bubbleOut.getPaddingTop();
                bubbleOutPaddingRight = bubbleOut.getPaddingRight();
                bubbleOutPaddingBottom = bubbleOut.getPaddingBottom();
            }
            videoNotePadding = dp(itemView, 2);
        }

        void bind(MessageEntity m, boolean isMe, String playingId, int playingPositionMs, int playingDurationMs,
                  boolean showSenderNames, ActionListener listener, boolean showAvatarImages,
                  String selfAvatarUrl, String peerAvatarUrl, Map<String, String> avatarByUserId, boolean isSelected,
                  TransferState transferState) {
            currentListener = listener;
            clearTtl();
            if (m == null) {
                itemView.setVisibility(View.GONE);
                return;
            }
            itemView.setVisibility(View.VISIBLE);
            rowIn.setVisibility(isMe ? View.GONE : View.VISIBLE);
            rowOut.setVisibility(isMe ? View.VISIBLE : View.GONE);
            if (isSelected) {
                itemView.setBackgroundResource(R.drawable.bg_message_selected);
            } else {
                itemView.setBackground(null);
            }
            itemView.setOnClickListener(v -> {
                if (listener != null) listener.onMessageClick(m);
            });
            itemView.setOnLongClickListener(v -> listener != null && listener.onMessageLongClick(m));

            String letter = "?";
            if (m.senderName != null && !m.senderName.isEmpty()) {
                letter = m.senderName.substring(0, 1).toUpperCase(Locale.getDefault());
            } else if (m.senderId != null && !m.senderId.isEmpty()) {
                letter = m.senderId.substring(0, 1).toUpperCase(Locale.getDefault());
            }
            avatarIn.setText(letter);
            // avatarOut is a hidden FrameLayout (0dp size), no setText needed
            String inUrl = null;
            String outUrl = null;
            if (showAvatarImages) {
                outUrl = selfAvatarUrl;
                if (!isMe && avatarByUserId != null && m.senderId != null) {
                    inUrl = avatarByUserId.get(m.senderId);
                }
                if (!isMe && (inUrl == null || inUrl.isEmpty())) {
                    inUrl = peerAvatarUrl;
                }
            }
            applyAvatar(avatarInImage, avatarIn, inUrl);
            // avatarOut is hidden (0dp), skip applyAvatar for it

            String rawContent = m != null && m.content != null ? m.content : "";
            boolean isSpoiler = SpoilerCodec.isSpoilerContent(rawContent);
            String content = SpoilerCodec.strip(rawContent);
            String time = formatTime(m.createdAt);

            boolean isVoice = "voice".equals(m.messageType);
            boolean isImage = isImageMessage(m);
            boolean isVideoNote = isVideoNoteMessage(m);
            String videoUrl = isVideoNote ? buildUrl(m.filePath) : null;
            if (videoUrl == null || videoUrl.isEmpty()) {
                isVideoNote = false;
            }
            boolean isFile = "file".equals(m.messageType) && !isImage && !isVideoNote;
            boolean isLocation = "location".equals(m.messageType) || "live_location".equals(m.messageType);
            PremiumGiftPayload giftPayload = isSpoiler ? null : PremiumGiftCodec.parseGift(content);
            boolean isGift = giftPayload != null;
            ChecklistPayload checklistPayload = (!isSpoiler && !isGift) ? ChecklistCodec.parseChecklist(content) : null;
            boolean isChecklist = !isSpoiler && !isGift && checklistPayload != null;
            boolean isChecklistUpdate = !isSpoiler && !isGift && ChecklistCodec.isChecklistUpdateContent(content);
            boolean isText = !isVoice && !isFile && !isImage && !isLocation && !isChecklist && !isChecklistUpdate && !isVideoNote && !isGift;

            if (showSenderNames && !isMe && m.senderName != null && !m.senderName.isEmpty()) {
                senderIn.setText(m.senderName);
                senderIn.setVisibility(View.VISIBLE);
            } else {
                senderIn.setVisibility(View.GONE);
            }

            // Reply block
            boolean hasReply = m.replyContent != null && !m.replyContent.isEmpty();
            if (replyBlockIn != null) {
                if (hasReply && !isMe) {
                    replyBlockIn.setVisibility(View.VISIBLE);
                    if (replySenderIn != null) {
                        replySenderIn.setText(m.replySenderName != null ? m.replySenderName : "");
                    }
                    if (replyContentIn != null) {
                        replyContentIn.setText(m.replyContent);
                    }
                } else {
                    replyBlockIn.setVisibility(View.GONE);
                }
            }
            if (replyBlockOut != null) {
                if (hasReply && isMe) {
                    replyBlockOut.setVisibility(View.VISIBLE);
                    if (replySenderOut != null) {
                        replySenderOut.setText(m.replySenderName != null ? m.replySenderName : "");
                    }
                    if (replyContentOut != null) {
                        replyContentOut.setText(m.replyContent);
                    }
                } else {
                    replyBlockOut.setVisibility(View.GONE);
                }
            }

            textIn.setVisibility(isText ? View.VISIBLE : View.GONE);
            textOut.setVisibility(isText ? View.VISIBLE : View.GONE);
            voiceIn.setVisibility(isVoice ? View.VISIBLE : View.GONE);
            voiceOut.setVisibility(isVoice ? View.VISIBLE : View.GONE);
            fileIn.setVisibility(isFile ? View.VISIBLE : View.GONE);
            fileOut.setVisibility(isFile ? View.VISIBLE : View.GONE);
            if (imageInContainer != null) {
                imageInContainer.setVisibility(isImage ? View.VISIBLE : View.GONE);
            }
            if (imageOutContainer != null) {
                imageOutContainer.setVisibility(isImage ? View.VISIBLE : View.GONE);
            }
            if (videoNoteIn != null) {
                videoNoteIn.setVisibility(isVideoNote && !isMe ? View.VISIBLE : View.GONE);
            }
            if (videoNoteOut != null) {
                videoNoteOut.setVisibility(isVideoNote && isMe ? View.VISIBLE : View.GONE);
            }
            locationIn.setVisibility(isLocation ? View.VISIBLE : View.GONE);
            locationOut.setVisibility(isLocation ? View.VISIBLE : View.GONE);
            checklistIn.setVisibility(isChecklist && !isMe ? View.VISIBLE : View.GONE);
            checklistOut.setVisibility(isChecklist && isMe ? View.VISIBLE : View.GONE);
            if (giftIn != null) {
                giftIn.setVisibility(isGift && !isMe ? View.VISIBLE : View.GONE);
            }
            if (giftOut != null) {
                giftOut.setVisibility(isGift && isMe ? View.VISIBLE : View.GONE);
            }
            if (tailIn != null) {
                tailIn.setVisibility(isGift && !isMe ? View.GONE : View.VISIBLE);
            }
            if (tailOut != null) {
                tailOut.setVisibility(isGift && isMe ? View.GONE : View.VISIBLE);
            }

            updateBubbleStyle(isVideoNote, isGift, isMe);

            if (isGift) {
                bindGift(m, giftPayload, isMe);
            } else {
                clearGiftViews();
            }

            String messageKey = getMessageKey(m);
            boolean spoilerRevealed = isSpoilerRevealed(messageKey, m);

            if (isText) {
                textIn.setText(content);
                textOut.setText(content);
                TextView activeText = isMe ? textOut : textIn;
                TextView inactiveText = isMe ? textIn : textOut;
                applySpoilerText(activeText, isSpoiler && !spoilerRevealed, messageKey, m);
                clearSpoilerText(inactiveText);
            } else {
                clearSpoilerText(textIn);
                clearSpoilerText(textOut);
            }

            if (isChecklist) {
                bindChecklist(m, checklistPayload, isMe, listener);
            } else {
                clearChecklistViews();
            }

            if (isImage) {
                String url = buildUrl(m.filePath);
                android.widget.ImageView target = isMe ? imageOut : imageIn;
                android.widget.ImageView overlay = isMe ? imageOutSpoiler : imageInSpoiler;
                TextView overlayLabel = isMe ? imageOutSpoilerLabel : imageInSpoilerLabel;
                if (target != null) {
                    target.setVisibility(View.VISIBLE);
                    Glide.with(target.getContext()).load(url).into(target);
                }
                applyImageSpoiler(target, overlay, overlayLabel, isSpoiler && !spoilerRevealed, url, m, listener, messageKey);
            } else {
                clearImageSpoiler(imageInSpoiler, imageInSpoilerLabel);
                clearImageSpoiler(imageOutSpoiler, imageOutSpoilerLabel);
                if (imageIn != null) imageIn.setOnClickListener(null);
                if (imageOut != null) imageOut.setOnClickListener(null);
            }

            bindTransferState(m, isMe, transferState);

            if (isVideoNote) {
                if (isMe && videoNoteOut != null) {
                    videoNoteOut.bind(videoUrl);
                } else if (!isMe && videoNoteIn != null) {
                    videoNoteIn.bind(videoUrl);
                }
            } else {
                if (videoNoteIn != null) videoNoteIn.release();
                if (videoNoteOut != null) videoNoteOut.release();
            }

            if (isLocation) {
                LocationUtils.LocationPayload payload = LocationUtils.parse(content);
                boolean isLive = "live_location".equals(m.messageType);
                String title = isLive
                        ? itemView.getContext().getString(R.string.location_live_label)
                        : itemView.getContext().getString(R.string.location_label);
                String subtitle = buildLocationSubtitle(isLive, payload);
                if (isMe) {
                    locationTitleOut.setText(title);
                    locationSubtitleOut.setText(subtitle);
                } else {
                    locationTitleIn.setText(title);
                    locationSubtitleIn.setText(subtitle);
                }
                if (payload != null) {
                    String mapUrl = LocationUtils.buildStaticMapUrl(payload.lat, payload.lon, 420, 220);
                    if (isMe) {
                        Glide.with(locationMapOut.getContext()).load(mapUrl).into(locationMapOut);
                        locationOut.setOnClickListener(v -> {
                            if (listener != null) listener.onLocationClick(m);
                        });
                    } else {
                        Glide.with(locationMapIn.getContext()).load(mapUrl).into(locationMapIn);
                        locationIn.setOnClickListener(v -> {
                            if (listener != null) listener.onLocationClick(m);
                        });
                    }
                } else {
                    if (isMe) {
                        locationMapOut.setImageDrawable(null);
                        locationOut.setOnClickListener(null);
                    } else {
                        locationMapIn.setImageDrawable(null);
                        locationIn.setOnClickListener(null);
                    }
                }
            }

            if (isVoice) {
                boolean playing = m.id != null && m.id.equals(playingId);
                int durationMs = playing ? Math.max(playingDurationMs, 0) : 0;
                int positionMs = playing ? Math.max(playingPositionMs, 0) : 0;
                int progress = durationMs > 0 ? Math.min(1000, (int) ((positionMs / (float) durationMs) * 1000f)) : 0;
                if (isMe) {
                    voiceDurationOut.setText(formatDuration(durationMs > 0 ? durationMs : 0));
                    voiceProgressOut.setProgress(progress);
                    voicePlayOut.setImageResource(playing ? R.drawable.ic_pause : R.drawable.ic_play);
                    voicePlayOut.setOnClickListener(v -> {
                        if (listener != null) listener.onVoiceClick(m);
                    });
                } else {
                    voiceDurationIn.setText(formatDuration(durationMs > 0 ? durationMs : 0));
                    voiceProgressIn.setProgress(progress);
                    voicePlayIn.setImageResource(playing ? R.drawable.ic_pause : R.drawable.ic_play);
                    voicePlayIn.setOnClickListener(v -> {
                        if (listener != null) listener.onVoiceClick(m);
                    });
                }
            }

            if (isFile) {
                String name = m.fileName != null && !m.fileName.isEmpty() ? m.fileName : content;
                String size = formatFileSize(m.fileSize);
                if (isMe) {
                    fileNameOut.setText(name);
                    fileSizeOut.setText(size);
                    fileOut.setOnClickListener(v -> {
                        if (listener != null) listener.onFileClick(m);
                    });
                } else {
                    fileNameIn.setText(name);
                    fileSizeIn.setText(size);
                    fileIn.setOnClickListener(v -> {
                        if (listener != null) listener.onFileClick(m);
                    });
                }
            }

            timeIn.setText(time);
            timeOut.setText(time);
            bindStatus(m, isMe);
            bindTtl(m, isMe);
        }

        void bindTransferState(MessageEntity m, boolean isMe, TransferState transferState) {
            boolean isImage = isImageMessage(m);
            boolean isVideoNote = isVideoNoteMessage(m);
            boolean isFile = "file".equals(m != null ? m.messageType : null) && !isImage && !isVideoNote;
            int state = transferState != null ? transferState.state : (isMe ? TRANSFER_DONE : TRANSFER_IDLE);
            int progress = transferState != null ? transferState.progress : 0;
            applyTransferState(isMe ? imageTransferOut : imageTransferIn, isImage, state, progress);
            applyTransferState(isMe ? fileTransferOut : fileTransferIn, isFile, state, progress);
            bindTransferClick(isMe ? imageTransferOut : imageTransferIn, isImage, m, state);
            bindTransferClick(isMe ? fileTransferOut : fileTransferIn, isFile, m, state);
        }

        void bindGift(MessageEntity m, PremiumGiftPayload payload, boolean isMe) {
            TextView title = isMe ? giftTitleOut : giftTitleIn;
            TextView subtitle = isMe ? giftSubtitleOut : giftSubtitleIn;
            TextView durationView = isMe ? giftDurationOut : giftDurationIn;
            TextView priceView = isMe ? giftPriceOut : giftPriceIn;
            TextView dotView = isMe ? giftDotOut : giftDotIn;
            View iconWrap = isMe ? giftIconWrapOut : giftIconWrapIn;
            View shine = isMe ? giftShineOut : giftShineIn;
            if (isMe) {
                clearGiftView(giftIn, giftIconWrapIn, giftShineIn);
            } else {
                clearGiftView(giftOut, giftIconWrapOut, giftShineOut);
            }

            if (title != null) {
                int titleRes = isMe ? R.string.premium_gift_sent_title : R.string.premium_gift_received_title;
                title.setText(itemView.getContext().getString(titleRes));
            }

            String user = payload != null ? (isMe ? payload.to : payload.from) : null;
            if ((user == null || user.isEmpty()) && !isMe) {
                user = m != null ? m.senderName : null;
            }
            if (user == null || user.isEmpty()) {
                user = m != null ? m.senderId : null;
            }
            user = normalizeGiftUser(user);
            if (subtitle != null) {
                if (user != null && !user.isEmpty()) {
                    int subtitleRes = isMe ? R.string.premium_gift_to : R.string.premium_gift_from;
                    subtitle.setText(itemView.getContext().getString(subtitleRes, user));
                    subtitle.setVisibility(View.VISIBLE);
                } else {
                    subtitle.setVisibility(View.GONE);
                }
            }

            int months = resolveGiftMonths(payload);
            int price = resolveGiftPrice(payload, months);
            String duration = formatGiftDuration(months);
            String priceLabel = price > 0
                    ? String.format(Locale.getDefault(), "%d \u20BD", price)
                    : "";

            if (durationView != null) {
                durationView.setText(duration);
                durationView.setVisibility(duration.isEmpty() ? View.GONE : View.VISIBLE);
            }
            if (priceView != null) {
                priceView.setText(priceLabel);
                priceView.setVisibility(priceLabel.isEmpty() ? View.GONE : View.VISIBLE);
            }
            if (dotView != null) {
                dotView.setVisibility(duration.isEmpty() || priceLabel.isEmpty() ? View.GONE : View.VISIBLE);
            }

            applyGiftAnimation(iconWrap, shine);
        }

        void clearGiftViews() {
            clearGiftView(giftIn, giftIconWrapIn, giftShineIn);
            clearGiftView(giftOut, giftIconWrapOut, giftShineOut);
        }

        private void clearGiftView(View root, View iconWrap, View shine) {
            if (root != null) root.setVisibility(View.GONE);
            if (iconWrap != null) iconWrap.clearAnimation();
            if (shine != null) shine.clearAnimation();
        }

        private void applyGiftAnimation(View iconWrap, View shine) {
            if (iconWrap != null) {
                iconWrap.clearAnimation();
                iconWrap.startAnimation(AnimationUtils.loadAnimation(iconWrap.getContext(), R.anim.gift_float));
            }
            if (shine != null) {
                shine.clearAnimation();
                shine.startAnimation(AnimationUtils.loadAnimation(shine.getContext(), R.anim.gift_shine));
            }
        }

        private int resolveGiftMonths(PremiumGiftPayload payload) {
            if (payload == null) return 0;
            int months = payload.months;
            if (months > 0) return months;
            String plan = payload.plan != null ? payload.plan.toLowerCase(Locale.getDefault()) : "";
            if (plan.contains("12") || plan.contains("year")) return 12;
            if (plan.contains("6") || plan.contains("half")) return 6;
            if (plan.contains("1") || plan.contains("month")) return 1;
            return 0;
        }

        private int resolveGiftPrice(PremiumGiftPayload payload, int months) {
            if (payload != null && payload.price > 0) return payload.price;
            if (months == 1) return 99;
            if (months == 6) return 299;
            if (months == 12) return 499;
            return 0;
        }

        private String formatGiftDuration(int months) {
            if (months <= 0) return "";
            if (months == 12) {
                return itemView.getContext().getString(R.string.premium_gift_year);
            }
            return itemView.getContext().getResources()
                    .getQuantityString(R.plurals.premium_gift_months, months, months);
        }

        private String normalizeGiftUser(String user) {
            if (user == null) return null;
            String trimmed = user.trim();
            if (trimmed.isEmpty()) return null;
            return trimmed.startsWith("@") ? trimmed : "@" + trimmed;
        }

        private void applyTransferState(TransferViews views, boolean shouldShow, int state, int progress) {
            if (views == null || views.root == null) return;
            views.root.setTag(state);
            if (!shouldShow || state == TRANSFER_DONE) {
                views.root.setVisibility(View.GONE);
                if (views.dim != null) views.dim.setVisibility(View.GONE);
                return;
            }
            views.root.setVisibility(View.VISIBLE);
            if (views.dim != null) {
                views.dim.setVisibility(shouldShow && views.showDim ? View.VISIBLE : View.GONE);
            }
            if (views.progress != null) {
                if (state == TRANSFER_LOADING) {
                    views.progress.setVisibility(View.VISIBLE);
                    views.progress.setProgress(progress, true);
                } else {
                    views.progress.setVisibility(View.GONE);
                    views.progress.setProgress(0);
                }
            }
            if (views.icon != null) {
                int iconRes = state == TRANSFER_IDLE ? R.drawable.ic_download : R.drawable.ic_close;
                views.icon.setImageResource(iconRes);
            }
        }

        private void bindTransferClick(TransferViews views, boolean shouldShow, MessageEntity m, int state) {
            if (views == null || views.root == null) return;
            if (!shouldShow || state == TRANSFER_DONE) {
                views.root.setOnClickListener(null);
                return;
            }
            views.root.setOnClickListener(v -> {
                if (currentListener == null) return;
                Object tag = views.root.getTag();
                int currentState = tag instanceof Integer ? (Integer) tag : TRANSFER_DONE;
                if (currentState == TRANSFER_LOADING) {
                    currentListener.onTransferCancel(m);
                } else {
                    currentListener.onTransferStart(m);
                }
            });
        }

        void bindStatus(MessageEntity m, boolean isMe) {
            if (statusOut == null) return;
            if (!isMe) {
                statusOut.setVisibility(View.GONE);
                return;
            }
            String status = m != null ? m.status : null;
            if (status == null || status.isEmpty()) {
                statusOut.setVisibility(View.GONE);
                return;
            }
            int iconRes = "sent".equals(status) ? R.drawable.ic_msg_sent : R.drawable.ic_msg_delivered;
            statusOut.setImageResource(iconRes);
            int baseGray = ContextCompat.getColor(itemView.getContext(), R.color.x_text_secondary);
            int tint;
            if ("read".equals(status)) {
                tint = ContextCompat.getColor(itemView.getContext(), R.color.x_accent_end);
            } else if ("sent".equals(status)) {
                tint = ColorUtils.setAlphaComponent(baseGray, 170);
            } else {
                tint = baseGray;
            }
            if (statusOut.getDrawable() != null) {
                statusOut.setImageDrawable(DrawableCompat.wrap(statusOut.getDrawable()).mutate());
                DrawableCompat.setTint(statusOut.getDrawable(), tint);
            }
            statusOut.setVisibility(View.VISIBLE);
        }

        void bindTtl(MessageEntity m, boolean isMe) {
            clearTtl();
            itemView.setVisibility(View.VISIBLE);
            TextView active = isMe ? ttlOut : ttlIn;
            TextView inactive = isMe ? ttlIn : ttlOut;
            if (inactive != null) {
                inactive.setVisibility(View.GONE);
            }
            if (active == null) return;
            long ttlSeconds = m != null ? m.ttlSeconds : 0L;
            long readTimestamp = m != null ? m.readTimestamp : 0L;
            boolean isRead = m != null && ("read".equals(m.status) || readTimestamp > 0L);
            if (ttlSeconds <= 0L || readTimestamp <= 0L || !isRead) {
                active.setVisibility(View.GONE);
                return;
            }
            ttlKey = getMessageKey(m);
            ttlExpiresAt = readTimestamp + (ttlSeconds * 1000L);
            scheduleTtlTick(active, m, ttlKey);
        }

        void clearTtl() {
            if (ttlRunnable != null) {
                itemView.removeCallbacks(ttlRunnable);
                ttlRunnable = null;
            }
            ttlKey = null;
            ttlExpiresAt = 0L;
            if (ttlIn != null) ttlIn.setVisibility(View.GONE);
            if (ttlOut != null) ttlOut.setVisibility(View.GONE);
        }

        private void scheduleTtlTick(TextView view, MessageEntity message, String key) {
            if (view == null) return;
            if (ttlRunnable != null) {
                itemView.removeCallbacks(ttlRunnable);
            }
            ttlRunnable = new Runnable() {
                @Override
                public void run() {
                    if (!safeEq(key, ttlKey)) return;
                    long remaining = ttlExpiresAt - System.currentTimeMillis();
                    if (remaining <= 0L) {
                        view.setVisibility(View.GONE);
                        itemView.setVisibility(View.GONE);
                        if (currentListener != null) {
                            currentListener.onMessageExpired(message);
                        }
                        clearTtl();
                        return;
                    }
                    view.setVisibility(View.VISIBLE);
                    String label = itemView.getContext().getString(R.string.ttl_label);
                    view.setText(label + " " + formatTtlRemaining(remaining));
                    itemView.postDelayed(this, 1000L);
                }
            };
            ttlRunnable.run();
        }

        private String formatTtlRemaining(long remainingMs) {
            long totalSeconds = Math.max(0L, (remainingMs + 999L) / 1000L);
            long hours = totalSeconds / 3600L;
            long minutes = (totalSeconds % 3600L) / 60L;
            long seconds = totalSeconds % 60L;
            if (hours > 0L) {
                return String.format(Locale.getDefault(), "%d:%02d:%02d", hours, minutes, seconds);
            }
            return String.format(Locale.getDefault(), "%d:%02d", minutes, seconds);
        }

        void releaseVideo() {
            if (videoNoteIn != null) {
                videoNoteIn.release();
            }
            if (videoNoteOut != null) {
                videoNoteOut.release();
            }
        }

        private void applyAvatar(android.widget.ImageView imageView, TextView letterView, String avatarUrl) {
            if (imageView == null || letterView == null) return;
            if (avatarUrl != null && !avatarUrl.isEmpty()) {
                String url = avatarUrl.startsWith("http") ? avatarUrl : (BuildConfig.API_BASE + avatarUrl);
                imageView.setVisibility(View.VISIBLE);
                letterView.setVisibility(View.GONE);
                Glide.with(imageView.getContext())
                        .load(url)
                        .circleCrop()
                        .into(imageView);
            } else {
                imageView.setVisibility(View.GONE);
                letterView.setVisibility(View.VISIBLE);
            }
        }

        private String formatTime(String iso) {
            if (iso == null) {
                return "";
            }
            try {
                // naive parse; adjust if backend returns another format
                SimpleDateFormat parser = new SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss", Locale.getDefault());
                Date d = parser.parse(iso);
                if (d == null) return "";
                return new SimpleDateFormat("HH:mm", Locale.getDefault()).format(d);
            } catch (Exception e) {
                return "";
            }
        }

        private String formatFileSize(Long size) {
            if (size == null || size <= 0) return "";
            double kb = size / 1024.0;
            if (kb < 1024) {
                return String.format(Locale.getDefault(), "%.1f KB", kb);
            }
            double mb = kb / 1024.0;
            if (mb < 1024) {
                return String.format(Locale.getDefault(), "%.1f MB", mb);
            }
            double gb = mb / 1024.0;
            return String.format(Locale.getDefault(), "%.1f GB", gb);
        }

        private String formatDuration(int ms) {
            if (ms <= 0) return "0:00";
            int totalSec = ms / 1000;
            int min = totalSec / 60;
            int sec = totalSec % 60;
            return String.format(Locale.getDefault(), "%d:%02d", min, sec);
        }

        private String buildLocationSubtitle(boolean isLive, LocationUtils.LocationPayload payload) {
            if (!isLive) {
                return itemView.getContext().getString(R.string.location_open_map);
            }
            if (payload == null || payload.expiresAt <= 0L) {
                return itemView.getContext().getString(R.string.location_live_label);
            }
            long remaining = payload.expiresAt - System.currentTimeMillis();
            if (remaining <= 0L) {
                return itemView.getContext().getString(R.string.location_live_label);
            }
            long minutes = Math.max(1L, remaining / 60000L);
            String label;
            if (minutes < 60) {
                label = itemView.getContext().getString(R.string.location_time_minutes, minutes);
            } else {
                long hours = minutes / 60;
                label = itemView.getContext().getString(R.string.location_time_hours, hours);
            }
            return itemView.getContext().getString(R.string.location_live_left, label);
        }

        private boolean isImageMessage(MessageEntity m) {
            if (m == null) return false;
            if ("image".equals(m.messageType)) return true;
            String name = m.fileName != null ? m.fileName : "";
            String path = m.filePath != null ? m.filePath : "";
            String lower = (name + " " + path).toLowerCase(Locale.getDefault());
            return lower.contains(".png") || lower.contains(".jpg") || lower.contains(".jpeg")
                    || lower.contains(".gif") || lower.contains(".webp");
        }

        private boolean isVideoNoteMessage(MessageEntity m) {
            if (m == null) return false;
            if (!"file".equals(m.messageType)) return false;
            String name = m.fileName != null ? m.fileName : "";
            String path = m.filePath != null ? m.filePath : "";
            String candidate = !name.isEmpty() ? name : path;
            if (candidate == null) return false;
            String lower = candidate.toLowerCase(Locale.getDefault());
            return lower.contains("videonote_") && lower.endsWith(".mp4");
        }

        private String buildUrl(String filePath) {
            if (filePath == null) return null;
            if (filePath.startsWith("http")
                    || filePath.startsWith("content:")
                    || filePath.startsWith("file:")) {
                return filePath;
            }
            return BuildConfig.API_BASE + filePath;
        }

        private void updateBubbleStyle(boolean isVideoNote, boolean isGift, boolean isMe) {
            LinearLayout bubble = isMe ? bubbleOut : bubbleIn;
            if (bubble == null) return;
            if (isGift) {
                bubble.setBackground(null);
                bubble.setPadding(0, 0, 0, 0);
                bubble.setClipChildren(false);
                bubble.setClipToPadding(false);
                return;
            }
            bubble.setClipChildren(true);
            bubble.setClipToPadding(true);
            if (isVideoNote) {
                bubble.setBackground(null);
                bubble.setPadding(videoNotePadding, videoNotePadding, videoNotePadding, videoNotePadding);
            } else {
                bubble.setBackgroundResource(isMe ? R.drawable.msg_out_bg : R.drawable.msg_in_bg);
                if (isMe) {
                    bubble.setPadding(bubbleOutPaddingLeft, bubbleOutPaddingTop, bubbleOutPaddingRight, bubbleOutPaddingBottom);
                } else {
                    bubble.setPadding(bubbleInPaddingLeft, bubbleInPaddingTop, bubbleInPaddingRight, bubbleInPaddingBottom);
                }
            }
        }

        private int dp(View view, int value) {
            return (int) TypedValue.applyDimension(
                    TypedValue.COMPLEX_UNIT_DIP, value, view.getResources().getDisplayMetrics());
        }

        private boolean isSpoilerRevealed(String key, MessageEntity m) {
            if (key != null && revealedSpoilerIds.contains(key)) return true;
            if (m != null && m.id != null && m.tempId != null && revealedSpoilerIds.contains(m.tempId)) {
                revealedSpoilerIds.add(m.id);
                return true;
            }
            return false;
        }

        private void markSpoilerRevealed(String key, MessageEntity m) {
            if (key != null) {
                revealedSpoilerIds.add(key);
            }
            if (m != null) {
                if (m.id != null) revealedSpoilerIds.add(m.id);
                if (m.tempId != null) revealedSpoilerIds.add(m.tempId);
            }
        }

        private void applySpoilerText(TextView view, boolean hidden, String key, MessageEntity m) {
            if (view == null) return;
            if (hidden) {
                BlurUtils.applyTextBlur(view, true);
                view.setOnClickListener(v -> {
                    markSpoilerRevealed(key, m);
                    BlurUtils.applyTextBlur(view, false);
                    view.setOnClickListener(null);
                });
            } else {
                BlurUtils.applyTextBlur(view, false);
                view.setOnClickListener(null);
            }
        }

        private void clearSpoilerText(TextView view) {
            if (view == null) return;
            BlurUtils.applyTextBlur(view, false);
            view.setOnClickListener(null);
        }

        private void applyImageSpoiler(android.widget.ImageView image, android.widget.ImageView overlay,
                                       TextView overlayLabel, boolean hidden, String url,
                                       MessageEntity m, ActionListener listener, String key) {
            if (image == null) return;
            if (!hidden) {
                clearImageSpoiler(overlay, overlayLabel);
                image.setOnClickListener(v -> {
                    if (listener != null) listener.onImageClick(m);
                });
                return;
            }
            if (overlay != null) {
                overlay.setVisibility(View.VISIBLE);
                overlay.setOnClickListener(v -> revealImageSpoiler(image, overlay, overlayLabel, m, listener, key));
                loadBlurred(overlay, url);
            }
            if (overlayLabel != null) {
                overlayLabel.setVisibility(View.VISIBLE);
            }
            image.setOnClickListener(v -> revealImageSpoiler(image, overlay, overlayLabel, m, listener, key));
        }

        private void revealImageSpoiler(android.widget.ImageView image, android.widget.ImageView overlay,
                                        TextView overlayLabel, MessageEntity m, ActionListener listener, String key) {
            markSpoilerRevealed(key, m);
            if (overlay != null) {
                overlay.setVisibility(View.GONE);
                overlay.setOnClickListener(null);
            }
            if (overlayLabel != null) {
                overlayLabel.setVisibility(View.GONE);
            }
            if (image != null) {
                image.setOnClickListener(v -> {
                    if (listener != null) listener.onImageClick(m);
                });
            }
        }

        private void clearImageSpoiler(android.widget.ImageView overlay, TextView overlayLabel) {
            if (overlay != null) {
                overlay.setVisibility(View.GONE);
                overlay.setOnClickListener(null);
                overlay.setTag(null);
            }
            if (overlayLabel != null) {
                overlayLabel.setVisibility(View.GONE);
            }
        }

        private void loadBlurred(android.widget.ImageView target, String url) {
            if (target == null || url == null || url.isEmpty()) return;
            Object tag = target.getTag();
            if (url.equals(tag)) return;
            target.setTag(url);
            Glide.with(target.getContext())
                    .load(url)
                    .transform(BlurUtils.imageBlur())
                    .into(target);
        }

        private void bindChecklist(MessageEntity m, ChecklistPayload payload, boolean isMe, ActionListener listener) {
            if (payload == null) return;
            List<ChecklistItem> items = payload.items;
            int total = items != null ? items.size() : 0;
            int done = 0;
            if (items != null) {
                for (ChecklistItem item : items) {
                    if (item != null && item.done) done++;
                }
            }
            TextView titleView = isMe ? checklistTitleOut : checklistTitleIn;
            TextView progressView = isMe ? checklistProgressOut : checklistProgressIn;
            LinearLayout itemsContainer = isMe ? checklistItemsOut : checklistItemsIn;
            TextView addView = isMe ? checklistAddOut : checklistAddIn;

            titleView.setText(payload.title != null ? payload.title : "");
            progressView.setText(total > 0 ? done + "/" + total : "");

            itemsContainer.removeAllViews();
            boolean canToggle = payload.othersCanMark || isMe;
            boolean canAdd = payload.othersCanAdd || isMe;
            if (items != null) {
                LayoutInflater inflater = LayoutInflater.from(itemsContainer.getContext());
                int textColor = ContextCompat.getColor(itemView.getContext(),
                        isMe ? R.color.x_on_accent : R.color.x_text_primary);
                int tintColor = ContextCompat.getColor(itemView.getContext(),
                        isMe ? R.color.x_on_accent : R.color.x_accent_end);
                for (ChecklistItem item : items) {
                    if (item == null) continue;
                    View row = inflater.inflate(R.layout.item_checklist_task, itemsContainer, false);
                    AppCompatCheckBox cb = row.findViewById(R.id.checklistItemCheck);
                    cb.setText(item.text != null ? item.text : "");
                    cb.setTextColor(textColor);
                    CompoundButtonCompat.setButtonTintList(cb, ColorStateList.valueOf(tintColor));
                    cb.setEnabled(canToggle);
                    cb.setAlpha(canToggle ? 1f : 0.6f);
                    cb.setOnCheckedChangeListener(null);
                    cb.setChecked(item.done);
                    cb.setOnCheckedChangeListener((buttonView, checked) -> {
                        if (listener != null && payload.id != null && item.id != null) {
                            listener.onChecklistToggle(m, payload.id, item.id, checked);
                        }
                    });
                    itemsContainer.addView(row);
                }
            }

            if (canAdd && payload.id != null) {
                addView.setVisibility(View.VISIBLE);
                addView.setOnClickListener(v -> {
                    if (listener != null) {
                        listener.onChecklistAddItem(m, payload.id);
                    }
                });
            } else {
                addView.setVisibility(View.GONE);
                addView.setOnClickListener(null);
            }
        }

        private void clearChecklistViews() {
            if (checklistItemsIn != null) {
                checklistItemsIn.removeAllViews();
            }
            if (checklistItemsOut != null) {
                checklistItemsOut.removeAllViews();
            }
            if (checklistAddIn != null) {
                checklistAddIn.setOnClickListener(null);
            }
            if (checklistAddOut != null) {
                checklistAddOut.setOnClickListener(null);
            }
        }
    }

    static class TransferState {
        int state = TRANSFER_DONE;
        int progress = 0;
    }

    static class TransferViews {
        final View root;
        final View dim;
        final android.widget.ProgressBar progress;
        final android.widget.ImageView icon;
        final boolean showDim;

        private TransferViews(View root, boolean showDim) {
            this.root = root;
            this.dim = root != null ? root.findViewById(R.id.transferDim) : null;
            this.progress = root != null ? root.findViewById(R.id.transferProgress) : null;
            this.icon = root != null ? root.findViewById(R.id.transferIcon) : null;
            this.showDim = showDim;
        }

        static TransferViews from(View root, boolean showDim) {
            return root == null ? null : new TransferViews(root, showDim);
        }
    }
}
