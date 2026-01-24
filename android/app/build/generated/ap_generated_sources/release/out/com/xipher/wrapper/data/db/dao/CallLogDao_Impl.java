package com.xipher.wrapper.data.db.dao;

import androidx.annotation.NonNull;
import androidx.room.EntityInsertAdapter;
import androidx.room.RoomDatabase;
import androidx.room.util.DBUtil;
import androidx.room.util.SQLiteStatementUtil;
import androidx.sqlite.SQLiteStatement;
import com.xipher.wrapper.data.db.entity.CallLogEntity;
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
public final class CallLogDao_Impl implements CallLogDao {
  private final RoomDatabase __db;

  private final EntityInsertAdapter<CallLogEntity> __insertAdapterOfCallLogEntity;

  public CallLogDao_Impl(@NonNull final RoomDatabase __db) {
    this.__db = __db;
    this.__insertAdapterOfCallLogEntity = new EntityInsertAdapter<CallLogEntity>() {
      @Override
      @NonNull
      protected String createQuery() {
        return "INSERT OR ABORT INTO `call_logs` (`id`,`peerId`,`peerName`,`direction`,`status`,`startedAt`,`endedAt`,`durationSec`) VALUES (nullif(?, 0),?,?,?,?,?,?,?)";
      }

      @Override
      protected void bind(@NonNull final SQLiteStatement statement, final CallLogEntity entity) {
        statement.bindLong(1, entity.id);
        if (entity.peerId == null) {
          statement.bindNull(2);
        } else {
          statement.bindText(2, entity.peerId);
        }
        if (entity.peerName == null) {
          statement.bindNull(3);
        } else {
          statement.bindText(3, entity.peerName);
        }
        if (entity.direction == null) {
          statement.bindNull(4);
        } else {
          statement.bindText(4, entity.direction);
        }
        if (entity.status == null) {
          statement.bindNull(5);
        } else {
          statement.bindText(5, entity.status);
        }
        statement.bindLong(6, entity.startedAt);
        statement.bindLong(7, entity.endedAt);
        statement.bindLong(8, entity.durationSec);
      }
    };
  }

  @Override
  public void insert(final CallLogEntity log) {
    DBUtil.performBlocking(__db, false, true, (_connection) -> {
      __insertAdapterOfCallLogEntity.insert(_connection, log);
      return null;
    });
  }

  @Override
  public List<CallLogEntity> getAll() {
    final String _sql = "SELECT * FROM call_logs ORDER BY startedAt DESC";
    return DBUtil.performBlocking(__db, true, false, (_connection) -> {
      final SQLiteStatement _stmt = _connection.prepare(_sql);
      try {
        final int _columnIndexOfId = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "id");
        final int _columnIndexOfPeerId = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "peerId");
        final int _columnIndexOfPeerName = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "peerName");
        final int _columnIndexOfDirection = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "direction");
        final int _columnIndexOfStatus = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "status");
        final int _columnIndexOfStartedAt = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "startedAt");
        final int _columnIndexOfEndedAt = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "endedAt");
        final int _columnIndexOfDurationSec = SQLiteStatementUtil.getColumnIndexOrThrow(_stmt, "durationSec");
        final List<CallLogEntity> _result = new ArrayList<CallLogEntity>();
        while (_stmt.step()) {
          final CallLogEntity _item;
          _item = new CallLogEntity();
          _item.id = _stmt.getLong(_columnIndexOfId);
          if (_stmt.isNull(_columnIndexOfPeerId)) {
            _item.peerId = null;
          } else {
            _item.peerId = _stmt.getText(_columnIndexOfPeerId);
          }
          if (_stmt.isNull(_columnIndexOfPeerName)) {
            _item.peerName = null;
          } else {
            _item.peerName = _stmt.getText(_columnIndexOfPeerName);
          }
          if (_stmt.isNull(_columnIndexOfDirection)) {
            _item.direction = null;
          } else {
            _item.direction = _stmt.getText(_columnIndexOfDirection);
          }
          if (_stmt.isNull(_columnIndexOfStatus)) {
            _item.status = null;
          } else {
            _item.status = _stmt.getText(_columnIndexOfStatus);
          }
          _item.startedAt = _stmt.getLong(_columnIndexOfStartedAt);
          _item.endedAt = _stmt.getLong(_columnIndexOfEndedAt);
          _item.durationSec = _stmt.getLong(_columnIndexOfDurationSec);
          _result.add(_item);
        }
        return _result;
      } finally {
        _stmt.close();
      }
    });
  }

  @Override
  public void clear() {
    final String _sql = "DELETE FROM call_logs";
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
