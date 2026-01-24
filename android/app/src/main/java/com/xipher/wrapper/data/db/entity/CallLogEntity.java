package com.xipher.wrapper.data.db.entity;

import androidx.room.Entity;
import androidx.room.PrimaryKey;

@Entity(tableName = "call_logs")
public class CallLogEntity {
    @PrimaryKey(autoGenerate = true)
    public long id;
    public String peerId;
    public String peerName;
    public String direction;
    public String status;
    public long startedAt;
    public long endedAt;
    public long durationSec;
}
