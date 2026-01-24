package com.xipher.wrapper.data.db;

import android.content.Context;

import androidx.room.Database;
import androidx.room.Room;
import androidx.room.RoomDatabase;
import androidx.sqlite.db.SupportSQLiteOpenHelper;

import com.xipher.wrapper.data.db.dao.CallLogDao;
import com.xipher.wrapper.data.db.dao.ChatDao;
import com.xipher.wrapper.data.db.dao.MessageDao;
import com.xipher.wrapper.data.db.dao.UserDao;
import com.xipher.wrapper.data.db.entity.CallLogEntity;
import com.xipher.wrapper.data.db.entity.ChatEntity;
import com.xipher.wrapper.data.db.entity.MessageEntity;
import com.xipher.wrapper.data.db.entity.UserEntity;

@Database(
        entities = {ChatEntity.class, MessageEntity.class, UserEntity.class, CallLogEntity.class},
        version = 8,
        exportSchema = false
)
public abstract class AppDatabase extends RoomDatabase {
    public abstract ChatDao chatDao();
    public abstract MessageDao messageDao();
    public abstract UserDao userDao();
    public abstract CallLogDao callLogDao();

    public static AppDatabase get(Context context) {
        return DatabaseManager.getInstance().getDatabase(context);
    }

    public static AppDatabase build(Context context, String name, SupportSQLiteOpenHelper.Factory factory) {
        RoomDatabase.Builder<AppDatabase> builder = Room.databaseBuilder(
                context.getApplicationContext(),
                AppDatabase.class,
                name
        );
        if (factory != null) {
            builder.openHelperFactory(factory);
        }
        return builder.fallbackToDestructiveMigration().build();
    }
}
