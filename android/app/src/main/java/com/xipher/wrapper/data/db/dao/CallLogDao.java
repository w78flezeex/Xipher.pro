package com.xipher.wrapper.data.db.dao;

import androidx.room.Dao;
import androidx.room.Insert;
import androidx.room.Query;

import com.xipher.wrapper.data.db.entity.CallLogEntity;

import java.util.List;

@Dao
public interface CallLogDao {
    @Query("SELECT * FROM call_logs ORDER BY startedAt DESC")
    List<CallLogEntity> getAll();

    @Insert
    void insert(CallLogEntity log);

    @Query("DELETE FROM call_logs")
    void clear();
}
