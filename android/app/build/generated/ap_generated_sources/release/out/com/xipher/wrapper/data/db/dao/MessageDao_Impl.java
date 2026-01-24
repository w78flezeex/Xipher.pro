package com.xipher.wrapper.data.db.dao;

import androidx.annotation.NonNull;
import androidx.room.EntityInsertAdapter;
import androidx.room.RoomDatabase;
import androidx.room.util.DBUtil;
import androidx.room.util.SQLiteStatementUtil;
import androidx.room.util.StringUtil;
import androidx.sqlite.SQLiteStatement;
import com.xipher.wrapper.data.db.entity.MessageEntity;
import java.lang.Class;
import java.lang.Override;
import java.lang.String;
import java.lang.StringBuilder;
import java.lang.SuppressWarnings;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import javax.annotation.processing.Generated;

@Generated("androidx.room.RoomProcessor")
@SuppressWarnings({"unchecked", "deprecation", "removal"})
public final class MessageDao_Impl implements MessageDao {
  private final RoomDatabase __db;

  private final EntityInsertAdapter<MessageEntity> __insertAdapterOfMessageEntity;

  public MessageDao_Impl(@NonNull final RoomDatabase __db) {
    this.__db = __db;
    this.__insertAdapterOfMessageEntity = new EntityInsertAdapter<MessageEntity>() {
      @Override
      @NonNull
      protected String createQuery() {
        return "INSERT OR REPLACE INTO `messages` (`id`,`tempId`,`chatId`,`senderId`,`senderName`,`content`,`messageType`,`filePath`,`fileName`,`fileSize`,`replyToMessageId`,`replyContent`,`replySenderName`,`createdAt`,`createdAtMillis`,`ttlSeconds`,`readTimestamp`,`status`) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
      }

      @Override
      protected void bind(@NonNull final SQLiteStatement statement, final MessageEntity entity) {
        if (entity.id == null) {
          statement.bindNull(1);
        } else {
          statement.bindText(1, entity.id);
        }
        if (entity.tempId == null) {
          statement.bindNull(2);
        } else {
          statement.bindText(2, entity.tempId);
        }
        if (entity.chatId == null) {
          statement.bindNull(3);
        } else {
          statement.bindText(3, entity.chatId);
        }
        if (entity.senderId == null) {
          statement.bindNull(4);
        } else {
          statement.bindText(4, entity.senderId);
        }
        if (entity.senderName == null) {
          statement.bindNull(5);
        } else {
          statement.bindText(5, entity.senderName);
        }
        if (entity.content == null) {
          statement.bindNull(6);
        } else {
          statement.bindText(6, entity.content);
        }
        if (entity.messageType == null) {
          statement.bindNull(7);
        } else {
          statement.bindText(7, entity.messageType);
        }
        if (entity.filePath == null) {
          statement.bindNull(8);
        } else {
          statement.bindText(8, entity.filePath);
        }
        if (entity.fileName == null) {
          statement.bindNull(9);
        } else {
          statement.bindText(9, entity.fileName);
        }
        if (entity.fileSize == null) {
          statement.bindNull(10);
        } else {
          statement.bindLong(10, entity.fileSize);
        }
        if (entity.replyToMessageId == null) {
          statement.bindNull(11);
        } else {
          statement.bindText(11, entity.replyToMessageId);
        }
        if (entity.replyContent == null) {
          statement.bindNull(12);
        } else {
          statement.bindText(12, entity.replyContent);
        }
        if (entity.replySenderName == null) {
          statement.bindNull(13);
        } else {
          statement.bindText(13, entity.replySenderName);
        }
        if (entity.createdAt == null) {
          statement.bindNull(14);
        } else {
          statement.bindText(14, entity.createdAt);
        }
        statement.bindLong(15, entity.createdAtMillis);
        statement.bindLong(16, entity.ttlSeconds);
        statement.bindLong(17, entity.readTimestamp);
        if (entity.status == null) {
          statement.bindNull(18);
        } else {
          statement.bindText(18, entity.status);
        }
      }
    };
  }

  @Override
  public void upsertAll(final List<MessageEntity> messages) {
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      __insertAdapterOfMessageEntity.insert(_connection, messages);
      return null;
    });
  }

  @Override
  public void upsert(final MessageEntity message) {
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      __insertAdapterOfMessageEntity.insert(_connection, message);
      return null;
    });
  }

  @Override
  public List<MessageEntity> getByChat(final String chatId) {
    final String _sql = "SELECT * FROM messages WHERE chatId = ? ORDER BY createdAtMillis ASC, createdAt ASC";
    return DBUtil.performBlocking(__db, true, false, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        int _argIndex = 1;
        if (chatId == null) {
          _stmt.bindNull(_argIndex);
        } else {
          _stmt.bindText(_argIndex, chatId);
        }
        final int _columnIndexOfId = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "id");
        final int _columnIndexOfTempId = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "tempId");
        final int _columnIndexOfChatId = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "chatId");
        final int _columnIndexOfSenderId = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "senderId");
        final int _columnIndexOfSenderName = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "senderName");
        final int _columnIndexOfContent = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "content");
        final int _columnIndexOfMessageType = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "messageType");
        final int _columnIndexOfFilePath = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "filePath");
        final int _columnIndexOfFileName = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "fileName");
        final int _columnIndexOfFileSize = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "fileSize");
        final int _columnIndexOfReplyToMessageId = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "replyToMessageId");
        final int _columnIndexOfReplyContent = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "replyContent");
        final int _columnIndexOfReplySenderName = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "replySenderName");
        final int _columnIndexOfCreatedAt = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "createdAt");
        final int _columnIndexOfCreatedAtMillis = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "createdAtMillis");
        final int _columnIndexOfTtlSeconds = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "ttlSeconds");
        final int _columnIndexOfReadTimestamp = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "readTimestamp");
        final int _columnIndexOfStatus = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "status");
        final List<MessageEntity> _result = new ArrayList<MessageEntity>();
        while (_stmt.step()) {
          final MessageEntity _item;
          _item = new MessageEntity();
          if (_stmt.isNull(_columnIndexOfId)) {
            _item.id = null;
          } else {
            _item.id = _stmt.getText(_columnIndexOfId);
          }
          if (_stmt.isNull(_columnIndexOfTempId)) {
            _item.tempId = null;
          } else {
            _item.tempId = _stmt.getText(_columnIndexOfTempId);
          }
          if (_stmt.isNull(_columnIndexOfChatId)) {
            _item.chatId = null;
          } else {
            _item.chatId = _stmt.getText(_columnIndexOfChatId);
          }
          if (_stmt.isNull(_columnIndexOfSenderId)) {
            _item.senderId = null;
          } else {
            _item.senderId = _stmt.getText(_columnIndexOfSenderId);
          }
          if (_stmt.isNull(_columnIndexOfSenderName)) {
            _item.senderName = null;
          } else {
            _item.senderName = _stmt.getText(_columnIndexOfSenderName);
          }
          if (_stmt.isNull(_columnIndexOfContent)) {
            _item.content = null;
          } else {
            _item.content = _stmt.getText(_columnIndexOfContent);
          }
          if (_stmt.isNull(_columnIndexOfMessageType)) {
            _item.messageType = null;
          } else {
            _item.messageType = _stmt.getText(_columnIndexOfMessageType);
          }
          if (_stmt.isNull(_columnIndexOfFilePath)) {
            _item.filePath = null;
          } else {
            _item.filePath = _stmt.getText(_columnIndexOfFilePath);
          }
          if (_stmt.isNull(_columnIndexOfFileName)) {
            _item.fileName = null;
          } else {
            _item.fileName = _stmt.getText(_columnIndexOfFileName);
          }
          if (_stmt.isNull(_columnIndexOfFileSize)) {
            _item.fileSize = null;
          } else {
            _item.fileSize = _stmt.getLong(_columnIndexOfFileSize);
          }
          if (_stmt.isNull(_columnIndexOfReplyToMessageId)) {
            _item.replyToMessageId = null;
          } else {
            _item.replyToMessageId = _stmt.getText(_columnIndexOfReplyToMessageId);
          }
          if (_stmt.isNull(_columnIndexOfReplyContent)) {
            _item.replyContent = null;
          } else {
            _item.replyContent = _stmt.getText(_columnIndexOfReplyContent);
          }
          if (_stmt.isNull(_columnIndexOfReplySenderName)) {
            _item.replySenderName = null;
          } else {
            _item.replySenderName = _stmt.getText(_columnIndexOfReplySenderName);
          }
          if (_stmt.isNull(_columnIndexOfCreatedAt)) {
            _item.createdAt = null;
          } else {
            _item.createdAt = _stmt.getText(_columnIndexOfCreatedAt);
          }
          _item.createdAtMillis = _stmt.getLong(_columnIndexOfCreatedAtMillis);
          _item.ttlSeconds = _stmt.getLong(_columnIndexOfTtlSeconds);
          _item.readTimestamp = _stmt.getLong(_columnIndexOfReadTimestamp);
          if (_stmt.isNull(_columnIndexOfStatus)) {
            _item.status = null;
          } else {
            _item.status = _stmt.getText(_columnIndexOfStatus);
          }
          _result.add(_item);
        }
        return _result;
      } finally {
        _stmt.close();
      }
    });
  }

  @Override
  public int countByChat(final String chatId) {
    final String _sql = "SELECT COUNT(*) FROM messages WHERE chatId = ?";
    return DBUtil.performBlocking(__db, true, false, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        int _argIndex = 1;
        if (chatId == null) {
          _stmt.bindNull(_argIndex);
        } else {
          _stmt.bindText(_argIndex, chatId);
        }
        final int _result;
        if (_stmt.step()) {
          _result = (int) (_stmt.getLong(0));
        } else {
          _result = 0;
        }
        return _result;
      } finally {
        _stmt.close();
      }
    });
  }

  @Override
  public void updateStatus(final String messageId, final String status) {
    final String _sql = "UPDATE messages SET status = ? WHERE id = ?";
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        int _argIndex = 1;
        if (status == null) {
          _stmt.bindNull(_argIndex);
        } else {
          _stmt.bindText(_argIndex, status);
        }
        _argIndex = 2;
        if (messageId == null) {
          _stmt.bindNull(_argIndex);
        } else {
          _stmt.bindText(_argIndex, messageId);
        }
        _stmt.step();
        return null;
      } finally {
        _stmt.close();
      }
    });
  }

  @Override
  public void updateStatusAndReadTimestamp(final String messageId, final String status,
      final long readTimestamp) {
    final String _sql = "UPDATE messages SET status = ?, readTimestamp = ? WHERE id = ?";
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        int _argIndex = 1;
        if (status == null) {
          _stmt.bindNull(_argIndex);
        } else {
          _stmt.bindText(_argIndex, status);
        }
        _argIndex = 2;
        _stmt.bindLong(_argIndex, readTimestamp);
        _argIndex = 3;
        if (messageId == null) {
          _stmt.bindNull(_argIndex);
        } else {
          _stmt.bindText(_argIndex, messageId);
        }
        _stmt.step();
        return null;
      } finally {
        _stmt.close();
      }
    });
  }

  @Override
  public void updateReadTimestamp(final String messageId, final long readTimestamp) {
    final String _sql = "UPDATE messages SET readTimestamp = ? WHERE id = ?";
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        int _argIndex = 1;
        _stmt.bindLong(_argIndex, readTimestamp);
        _argIndex = 2;
        if (messageId == null) {
          _stmt.bindNull(_argIndex);
        } else {
          _stmt.bindText(_argIndex, messageId);
        }
        _stmt.step();
        return null;
      } finally {
        _stmt.close();
      }
    });
  }

  @Override
  public void deleteById(final String messageId) {
    final String _sql = "DELETE FROM messages WHERE id = ?";
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        int _argIndex = 1;
        if (messageId == null) {
          _stmt.bindNull(_argIndex);
        } else {
          _stmt.bindText(_argIndex, messageId);
        }
        _stmt.step();
        return null;
      } finally {
        _stmt.close();
      }
    });
  }

  @Override
  public void deleteByIds(final List<String> messageIds) {
    final StringBuilder _stringBuilder = new StringBuilder();
    _stringBuilder.append("DELETE FROM messages WHERE id IN (");
    final int _inputSize = messageIds == null ? 1 : messageIds.size();
    StringUtil.appendPlaceholders(_stringBuilder, _inputSize);
    _stringBuilder.append(")");
    final String _sql = _stringBuilder.toString();
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        int _argIndex = 1;
        if (messageIds == null) {
          _stmt.bindNull(_argIndex);
        } else {
          for (String _item : messageIds) {
            if (_item == null) {
              _stmt.bindNull(_argIndex);
            } else {
              _stmt.bindText(_argIndex, _item);
            }
            _argIndex++;
          }
        }
        _stmt.step();
        return null;
      } finally {
        _stmt.close();
      }
    });
  }

  @Override
  public void clearChat(final String chatId) {
    final String _sql = "DELETE FROM messages WHERE chatId = ?";
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        int _argIndex = 1;
        if (chatId == null) {
          _stmt.bindNull(_argIndex);
        } else {
          _stmt.bindText(_argIndex, chatId);
        }
        _stmt.step();
        return null;
      } finally {
        _stmt.close();
      }
    });
  }

  @Override
  public void clearAll() {
    final String _sql = "DELETE FROM messages";
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        _stmt.step();
        return null;
      } finally {
        _stmt.close();
      }
    });
  }

  @NonNull
  public static List<Class<?>> getRequiredConverters() {
    return Collections.emptyList();
  }
}
