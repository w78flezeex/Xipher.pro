package com.xipher.wrapper.data.db.dao;

import androidx.annotation.NonNull;
import androidx.room.EntityInsertAdapter;
import androidx.room.RoomDatabase;
import androidx.room.util.DBUtil;
import androidx.room.util.SQLiteStatementUtil;
import androidx.sqlite.SQLiteStatement;
import com.xipher.wrapper.data.db.entity.ChatEntity;
import java.lang.Class;
import java.lang.Override;
import java.lang.String;
import java.lang.SuppressWarnings;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import javax.annotation.processing.Generated;

@Generated("androidx.room.RoomProcessor")
@SuppressWarnings({"unchecked", "deprecation", "removal"})
public final class ChatDao_Impl implements ChatDao {
  private final RoomDatabase __db;

  private final EntityInsertAdapter<ChatEntity> __insertAdapterOfChatEntity;

  public ChatDao_Impl(@NonNull final RoomDatabase __db) {
    this.__db = __db;
    this.__insertAdapterOfChatEntity = new EntityInsertAdapter<ChatEntity>() {
      @Override
      @NonNull
      protected String createQuery() {
        return "INSERT OR REPLACE INTO `chats` (`id`,`title`,`username`,`avatarUrl`,`lastMessage`,`unread`,`isChannel`,`isGroup`,`isSaved`,`isPrivate`,`updatedAt`) VALUES (?,?,?,?,?,?,?,?,?,?,?)";
      }

      @Override
      protected void bind(@NonNull final SQLiteStatement statement, final ChatEntity entity) {
        if (entity.id == null) {
          statement.bindNull(1);
        } else {
          statement.bindText(1, entity.id);
        }
        if (entity.title == null) {
          statement.bindNull(2);
        } else {
          statement.bindText(2, entity.title);
        }
        if (entity.username == null) {
          statement.bindNull(3);
        } else {
          statement.bindText(3, entity.username);
        }
        if (entity.avatarUrl == null) {
          statement.bindNull(4);
        } else {
          statement.bindText(4, entity.avatarUrl);
        }
        if (entity.lastMessage == null) {
          statement.bindNull(5);
        } else {
          statement.bindText(5, entity.lastMessage);
        }
        statement.bindLong(6, entity.unread);
        final int _tmp = entity.isChannel ? 1 : 0;
        statement.bindLong(7, _tmp);
        final int _tmp_1 = entity.isGroup ? 1 : 0;
        statement.bindLong(8, _tmp_1);
        final int _tmp_2 = entity.isSaved ? 1 : 0;
        statement.bindLong(9, _tmp_2);
        final int _tmp_3 = entity.isPrivate ? 1 : 0;
        statement.bindLong(10, _tmp_3);
        statement.bindLong(11, entity.updatedAt);
      }
    };
  }

  @Override
  public void upsertAll(final List<ChatEntity> chats) {
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      __insertAdapterOfChatEntity.insert(_connection, chats);
      return null;
    });
  }

  @Override
  public List<ChatEntity> getAll() {
    final String _sql = "SELECT * FROM chats ORDER BY updatedAt DESC";
    return DBUtil.performBlocking(__db, true, false, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        final int _columnIndexOfId = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "id");
        final int _columnIndexOfTitle = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "title");
        final int _columnIndexOfUsername = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "username");
        final int _columnIndexOfAvatarUrl = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "avatarUrl");
        final int _columnIndexOfLastMessage = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "lastMessage");
        final int _columnIndexOfUnread = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "unread");
        final int _columnIndexOfIsChannel = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "isChannel");
        final int _columnIndexOfIsGroup = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "isGroup");
        final int _columnIndexOfIsSaved = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "isSaved");
        final int _columnIndexOfIsPrivate = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "isPrivate");
        final int _columnIndexOfUpdatedAt = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "updatedAt");
        final List<ChatEntity> _result = new ArrayList<ChatEntity>();
        while (_stmt.step()) {
          final ChatEntity _item;
          _item = new ChatEntity();
          if (_stmt.isNull(_columnIndexOfId)) {
            _item.id = null;
          } else {
            _item.id = _stmt.getText(_columnIndexOfId);
          }
          if (_stmt.isNull(_columnIndexOfTitle)) {
            _item.title = null;
          } else {
            _item.title = _stmt.getText(_columnIndexOfTitle);
          }
          if (_stmt.isNull(_columnIndexOfUsername)) {
            _item.username = null;
          } else {
            _item.username = _stmt.getText(_columnIndexOfUsername);
          }
          if (_stmt.isNull(_columnIndexOfAvatarUrl)) {
            _item.avatarUrl = null;
          } else {
            _item.avatarUrl = _stmt.getText(_columnIndexOfAvatarUrl);
          }
          if (_stmt.isNull(_columnIndexOfLastMessage)) {
            _item.lastMessage = null;
          } else {
            _item.lastMessage = _stmt.getText(_columnIndexOfLastMessage);
          }
          _item.unread = (int) (_stmt.getLong(_columnIndexOfUnread));
          final int _tmp;
          _tmp = (int) (_stmt.getLong(_columnIndexOfIsChannel));
          _item.isChannel = _tmp != 0;
          final int _tmp_1;
          _tmp_1 = (int) (_stmt.getLong(_columnIndexOfIsGroup));
          _item.isGroup = _tmp_1 != 0;
          final int _tmp_2;
          _tmp_2 = (int) (_stmt.getLong(_columnIndexOfIsSaved));
          _item.isSaved = _tmp_2 != 0;
          final int _tmp_3;
          _tmp_3 = (int) (_stmt.getLong(_columnIndexOfIsPrivate));
          _item.isPrivate = _tmp_3 != 0;
          _item.updatedAt = _stmt.getLong(_columnIndexOfUpdatedAt);
          _result.add(_item);
        }
        return _result;
      } finally {
        _stmt.close();
      }
    });
  }

  @Override
  public ChatEntity getById(final String chatId) {
    final String _sql = "SELECT * FROM chats WHERE id = ? LIMIT 1";
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
        final int _columnIndexOfTitle = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "title");
        final int _columnIndexOfUsername = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "username");
        final int _columnIndexOfAvatarUrl = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "avatarUrl");
        final int _columnIndexOfLastMessage = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "lastMessage");
        final int _columnIndexOfUnread = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "unread");
        final int _columnIndexOfIsChannel = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "isChannel");
        final int _columnIndexOfIsGroup = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "isGroup");
        final int _columnIndexOfIsSaved = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "isSaved");
        final int _columnIndexOfIsPrivate = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "isPrivate");
        final int _columnIndexOfUpdatedAt = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "updatedAt");
        final ChatEntity _result;
        if (_stmt.step()) {
          _result = new ChatEntity();
          if (_stmt.isNull(_columnIndexOfId)) {
            _result.id = null;
          } else {
            _result.id = _stmt.getText(_columnIndexOfId);
          }
          if (_stmt.isNull(_columnIndexOfTitle)) {
            _result.title = null;
          } else {
            _result.title = _stmt.getText(_columnIndexOfTitle);
          }
          if (_stmt.isNull(_columnIndexOfUsername)) {
            _result.username = null;
          } else {
            _result.username = _stmt.getText(_columnIndexOfUsername);
          }
          if (_stmt.isNull(_columnIndexOfAvatarUrl)) {
            _result.avatarUrl = null;
          } else {
            _result.avatarUrl = _stmt.getText(_columnIndexOfAvatarUrl);
          }
          if (_stmt.isNull(_columnIndexOfLastMessage)) {
            _result.lastMessage = null;
          } else {
            _result.lastMessage = _stmt.getText(_columnIndexOfLastMessage);
          }
          _result.unread = (int) (_stmt.getLong(_columnIndexOfUnread));
          final int _tmp;
          _tmp = (int) (_stmt.getLong(_columnIndexOfIsChannel));
          _result.isChannel = _tmp != 0;
          final int _tmp_1;
          _tmp_1 = (int) (_stmt.getLong(_columnIndexOfIsGroup));
          _result.isGroup = _tmp_1 != 0;
          final int _tmp_2;
          _tmp_2 = (int) (_stmt.getLong(_columnIndexOfIsSaved));
          _result.isSaved = _tmp_2 != 0;
          final int _tmp_3;
          _tmp_3 = (int) (_stmt.getLong(_columnIndexOfIsPrivate));
          _result.isPrivate = _tmp_3 != 0;
          _result.updatedAt = _stmt.getLong(_columnIndexOfUpdatedAt);
        } else {
          _result = null;
        }
        return _result;
      } finally {
        _stmt.close();
      }
    });
  }

  @Override
  public void updatePreview(final String chatId, final String lastMessage, final long updatedAt) {
    final String _sql = "UPDATE chats SET lastMessage = ?, updatedAt = ? WHERE id = ?";
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        int _argIndex = 1;
        if (lastMessage == null) {
          _stmt.bindNull(_argIndex);
        } else {
          _stmt.bindText(_argIndex, lastMessage);
        }
        _argIndex = 2;
        _stmt.bindLong(_argIndex, updatedAt);
        _argIndex = 3;
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
  public void updateUpdatedAt(final String chatId, final long updatedAt) {
    final String _sql = "UPDATE chats SET updatedAt = ? WHERE id = ?";
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        int _argIndex = 1;
        _stmt.bindLong(_argIndex, updatedAt);
        _argIndex = 2;
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
  public void deleteById(final String chatId) {
    final String _sql = "DELETE FROM chats WHERE id = ?";
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
  public void clear() {
    final String _sql = "DELETE FROM chats";
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
