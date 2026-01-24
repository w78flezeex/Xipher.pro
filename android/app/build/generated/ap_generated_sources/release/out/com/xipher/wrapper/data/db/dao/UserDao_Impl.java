package com.xipher.wrapper.data.db.dao;

import androidx.annotation.NonNull;
import androidx.room.EntityInsertAdapter;
import androidx.room.RoomDatabase;
import androidx.room.util.DBUtil;
import androidx.room.util.SQLiteStatementUtil;
import androidx.sqlite.SQLiteStatement;
import com.xipher.wrapper.data.db.entity.UserEntity;
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
public final class UserDao_Impl implements UserDao {
  private final RoomDatabase __db;

  private final EntityInsertAdapter<UserEntity> __insertAdapterOfUserEntity;

  public UserDao_Impl(@NonNull final RoomDatabase __db) {
    this.__db = __db;
    this.__insertAdapterOfUserEntity = new EntityInsertAdapter<UserEntity>() {
      @Override
      @NonNull
      protected String createQuery() {
        return "INSERT OR REPLACE INTO `users` (`id`,`username`,`avatarUrl`,`isFriend`,`lastActivity`) VALUES (?,?,?,?,?)";
      }

      @Override
      protected void bind(@NonNull final SQLiteStatement statement, final UserEntity entity) {
        if (entity.id == null) {
          statement.bindNull(1);
        } else {
          statement.bindText(1, entity.id);
        }
        if (entity.username == null) {
          statement.bindNull(2);
        } else {
          statement.bindText(2, entity.username);
        }
        if (entity.avatarUrl == null) {
          statement.bindNull(3);
        } else {
          statement.bindText(3, entity.avatarUrl);
        }
        final int _tmp = entity.isFriend ? 1 : 0;
        statement.bindLong(4, _tmp);
        statement.bindLong(5, entity.lastActivity);
      }
    };
  }

  @Override
  public void upsert(final UserEntity user) {
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      __insertAdapterOfUserEntity.insert(_connection, user);
      return null;
    });
  }

  @Override
  public UserEntity getById(final String id) {
    final String _sql = "SELECT * FROM users WHERE id = ? LIMIT 1";
    return DBUtil.performBlocking(__db, true, false, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        int _argIndex = 1;
        if (id == null) {
          _stmt.bindNull(_argIndex);
        } else {
          _stmt.bindText(_argIndex, id);
        }
        final int _columnIndexOfId = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "id");
        final int _columnIndexOfUsername = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "username");
        final int _columnIndexOfAvatarUrl = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "avatarUrl");
        final int _columnIndexOfIsFriend = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "isFriend");
        final int _columnIndexOfLastActivity = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "lastActivity");
        final UserEntity _result;
        if (_stmt.step()) {
          _result = new UserEntity();
          if (_stmt.isNull(_columnIndexOfId)) {
            _result.id = null;
          } else {
            _result.id = _stmt.getText(_columnIndexOfId);
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
          final int _tmp;
          _tmp = (int) (_stmt.getLong(_columnIndexOfIsFriend));
          _result.isFriend = _tmp != 0;
          _result.lastActivity = _stmt.getLong(_columnIndexOfLastActivity);
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
  public List<UserEntity> getAll() {
    final String _sql = "SELECT * FROM users";
    return DBUtil.performBlocking(__db, true, false, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        final int _columnIndexOfId = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "id");
        final int _columnIndexOfUsername = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "username");
        final int _columnIndexOfAvatarUrl = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "avatarUrl");
        final int _columnIndexOfIsFriend = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "isFriend");
        final int _columnIndexOfLastActivity = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "lastActivity");
        final List<UserEntity> _result = new ArrayList<UserEntity>();
        while (_stmt.step()) {
          final UserEntity _item;
          _item = new UserEntity();
          if (_stmt.isNull(_columnIndexOfId)) {
            _item.id = null;
          } else {
            _item.id = _stmt.getText(_columnIndexOfId);
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
          final int _tmp;
          _tmp = (int) (_stmt.getLong(_columnIndexOfIsFriend));
          _item.isFriend = _tmp != 0;
          _item.lastActivity = _stmt.getLong(_columnIndexOfLastActivity);
          _result.add(_item);
        }
        return _result;
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
