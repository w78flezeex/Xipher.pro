package com.xipher.wrapper.data.db;

import android.content.Context;

import com.xipher.wrapper.data.db.entity.ChatEntity;
import com.xipher.wrapper.data.db.entity.MessageEntity;
import com.xipher.wrapper.security.DbKeyStore;
import com.xipher.wrapper.security.PanicModePrefs;
import com.xipher.wrapper.security.PanicModeWorker;
import com.xipher.wrapper.security.SecurityContext;

import net.sqlcipher.database.SQLiteDatabase;
import net.sqlcipher.database.SupportFactory;

import java.io.File;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public final class DatabaseManager {
    public static final String REAL_DB_NAME = "real_user.db";
    public static final String DECOY_DB_NAME = "decoy_user.db";

    private static volatile DatabaseManager INSTANCE;

    private final Object lock = new Object();
    private AppDatabase activeDb;
    private byte[] activePassphrase;
    private boolean panicMode;
    private String activeDbName;

    public static DatabaseManager getInstance() {
        if (INSTANCE == null) {
            synchronized (DatabaseManager.class) {
                if (INSTANCE == null) {
                    INSTANCE = new DatabaseManager();
                }
            }
        }
        return INSTANCE;
    }

    public AppDatabase getDatabase(Context context) {
        AppDatabase db = activeDb;
        if (db == null) {
            throw new IllegalStateException("DatabaseManager.init must be called before database access");
        }
        return db;
    }

    public boolean isPanicMode() {
        return panicMode;
    }

    public String getActiveDbName() {
        return activeDbName;
    }

    public void init(Context context, String password) throws InvalidPasswordException {
        initInternal(context, password, false);
    }

    public void initOrCreateDecoy(Context context, String password) throws InvalidPasswordException {
        initInternal(context, password, true);
    }

    public void clearSensitiveState() {
        synchronized (lock) {
            clearSensitiveStateLocked();
        }
        SecurityContext.get().setPanicMode(false);
    }

    private void initInternal(Context context, String password, boolean allowDecoyCreate)
            throws InvalidPasswordException {
        if (context == null) throw new IllegalArgumentException("Context is required");
        char[] passphrase = password != null ? password.toCharArray() : new char[0];
        try {
            try {
                DbHolder real = openDatabase(context, REAL_DB_NAME, passphrase, true);
                setActive(context, real, false);
                return;
            } catch (Exception ignore) {
                // fall through to decoy attempt
            }

            if (!PanicModePrefs.isEnabled(context) && !allowDecoyCreate) {
                throw new IllegalStateException("Panic mode disabled");
            }
            DbHolder decoy = openDatabase(context, DECOY_DB_NAME, passphrase, allowDecoyCreate);
            if (allowDecoyCreate && decoy.created) {
                seedDecoy(decoy.db);
            }
            setActive(context, decoy, true);
        } catch (Exception e) {
            throw new InvalidPasswordException("Invalid password", e);
        } finally {
            wipeChars(passphrase);
        }
    }

    public boolean realDbExists(Context context) {
        return dbExists(context, REAL_DB_NAME);
    }

    public boolean decoyDbExists(Context context) {
        return dbExists(context, DECOY_DB_NAME);
    }

    public void setMainPin(Context context, String currentPin, String newPin) throws InvalidPasswordException {
        rekeyOrCreate(context, REAL_DB_NAME, currentPin, newPin, false);
    }

    public void setMainPinFromAutoKey(Context context, String newPin) throws InvalidPasswordException {
        if (rekeyActiveIfOpen(REAL_DB_NAME, newPin)) {
            return;
        }
        String current = DbKeyStore.getMainKey(context);
        if (dbExists(context, REAL_DB_NAME) && (current == null || current.isEmpty())) {
            throw new InvalidPasswordException("Missing auto key", null);
        }
        rekeyOrCreate(context, REAL_DB_NAME, current, newPin, false);
    }

    public void setPanicPin(Context context, String currentPin, String newPin) throws InvalidPasswordException {
        rekeyOrCreate(context, DECOY_DB_NAME, currentPin, newPin, true);
    }

    public void generateDecoyFromActive(Context context, String panicPin, boolean force)
            throws InvalidPasswordException {
        generateDecoyFromActive(context, panicPin, force, null, null);
    }

    public void generateDecoyFromActive(Context context, String panicPin, boolean force, String selfId, String selfName)
            throws InvalidPasswordException {
        if (context == null) throw new IllegalArgumentException("Context is required");
        char[] passphrase = panicPin != null ? panicPin.toCharArray() : new char[0];
        AppDatabase source = null;
        synchronized (lock) {
            if (activeDb != null && !panicMode && REAL_DB_NAME.equals(activeDbName)) {
                source = activeDb;
            }
        }
        DbHolder decoy = null;
        try {
            decoy = openDatabase(context, DECOY_DB_NAME, passphrase, true);
            boolean shouldGenerate = force || decoy.db.chatDao().getAll().isEmpty();
            if (shouldGenerate) {
                DecoyDataGenerator.generate(context.getApplicationContext(), source, decoy.db, selfId, selfName);
            }
        } catch (Exception e) {
            throw new InvalidPasswordException("Invalid password", e);
        } finally {
            if (decoy != null) {
                safeClose(decoy.db);
                wipeBytes(decoy.passphrase);
            }
            wipeChars(passphrase);
        }
    }

    public void rekeyMainToAutoKey(Context context, String currentPin) throws InvalidPasswordException {
        String autoKey = DbKeyStore.getOrCreateMainKey(context);
        rekeyOrCreate(context, REAL_DB_NAME, currentPin, autoKey, false);
    }

    private boolean rekeyActiveIfOpen(String dbName, String newPin) throws InvalidPasswordException {
        if (newPin == null) {
            throw new InvalidPasswordException("Invalid password", null);
        }
        char[] newChars = newPin.toCharArray();
        try {
            synchronized (lock) {
                if (activeDb == null || activeDbName == null || !activeDbName.equals(dbName)) {
                    return false;
                }
                if (panicMode && REAL_DB_NAME.equals(dbName)) {
                    return false;
                }
                rekeyDatabase(activeDb, newChars);
                byte[] newBytes = SQLiteDatabase.getBytes(newChars);
                if (activePassphrase != null) {
                    wipeBytes(activePassphrase);
                }
                activePassphrase = newBytes;
                return true;
            }
        } catch (Exception e) {
            throw new InvalidPasswordException("Invalid password", e);
        } finally {
            wipeChars(newChars);
        }
    }

    private void rekeyOrCreate(Context context,
                               String dbName,
                               String currentPin,
                               String newPin,
                               boolean seedIfCreated) throws InvalidPasswordException {
        if (context == null) throw new IllegalArgumentException("Context is required");
        char[] currentChars = currentPin != null ? currentPin.toCharArray() : new char[0];
        char[] newChars = newPin != null ? newPin.toCharArray() : new char[0];
        boolean exists = dbExists(context, dbName);
        try {
            if (exists) {
                if (tryRekeyActive(dbName, currentChars, newChars)) {
                    return;
                }
                DbHolder holder = openDatabase(context, dbName, currentChars, false);
                try {
                    rekeyDatabase(holder.db, newChars);
                } finally {
                    safeClose(holder.db);
                    wipeBytes(holder.passphrase);
                }
                return;
            }
            DbHolder holder = openDatabase(context, dbName, newChars, true);
            try {
                if (seedIfCreated && holder.created) {
                    seedDecoy(holder.db);
                }
            } finally {
                safeClose(holder.db);
                wipeBytes(holder.passphrase);
            }
        } catch (InvalidPasswordException e) {
            throw e;
        } catch (Exception e) {
            throw new InvalidPasswordException("Invalid password", e);
        } finally {
            wipeChars(currentChars);
            wipeChars(newChars);
        }
    }

    private boolean tryRekeyActive(String dbName, char[] currentPin, char[] newPin)
            throws InvalidPasswordException {
        synchronized (lock) {
            if (activeDb == null || activeDbName == null || !activeDbName.equals(dbName)) {
                return false;
            }
            byte[] currentBytes = SQLiteDatabase.getBytes(currentPin);
            try {
                if (activePassphrase == null || !Arrays.equals(activePassphrase, currentBytes)) {
                    throw new InvalidPasswordException("Invalid password", null);
                }
                rekeyDatabase(activeDb, newPin);
                byte[] newBytes = SQLiteDatabase.getBytes(newPin);
                wipeBytes(activePassphrase);
                activePassphrase = newBytes;
                return true;
            } catch (InvalidPasswordException e) {
                throw e;
            } catch (Exception e) {
                throw new InvalidPasswordException("Invalid password", e);
            } finally {
                wipeBytes(currentBytes);
            }
        }
    }

    private void rekeyDatabase(AppDatabase db, char[] newPin) throws Exception {
        if (db == null) throw new IllegalStateException("Database not open");
        androidx.sqlite.db.SupportSQLiteDatabase supportDb = db.getOpenHelper().getWritableDatabase();
        if (supportDb instanceof net.sqlcipher.database.SQLiteDatabase) {
            ((net.sqlcipher.database.SQLiteDatabase) supportDb).changePassword(newPin);
        } else {
            String newPinString = new String(newPin);
            supportDb.execSQL("PRAGMA rekey = '" + newPinString + "'");
        }
    }

    private boolean dbExists(Context context, String dbName) {
        Context appContext = context.getApplicationContext();
        File dbFile = appContext.getDatabasePath(dbName);
        return dbFile.exists();
    }

    private DbHolder openDatabase(Context context, String dbName, char[] passphrase, boolean allowCreate)
            throws Exception {
        Context appContext = context.getApplicationContext();
        File dbFile = appContext.getDatabasePath(dbName);
        File parent = dbFile.getParentFile();
        if (parent != null && !parent.exists()) {
            parent.mkdirs();
        }
        boolean existed = dbFile.exists();
        if (!allowCreate && !existed) {
            throw new FileNotFoundException(dbName);
        }
        SQLiteDatabase.loadLibs(appContext);
        byte[] passphraseBytes = SQLiteDatabase.getBytes(passphrase);
        SupportFactory factory = new SupportFactory(passphraseBytes);
        AppDatabase db = AppDatabase.build(appContext, dbName, factory);
        try {
            db.getOpenHelper().getWritableDatabase();
        } catch (Exception e) {
            safeClose(db);
            wipeBytes(passphraseBytes);
            throw e;
        }
        return new DbHolder(dbName, db, passphraseBytes, !existed);
    }

    private void setActive(Context context, DbHolder holder, boolean isPanic) {
        synchronized (lock) {
            clearSensitiveStateLocked();
            activeDb = holder.db;
            activePassphrase = holder.passphrase;
            activeDbName = holder.dbName;
            panicMode = isPanic;
        }
        SecurityContext.get().setPanicMode(isPanic);
        if (isPanic) {
            PanicModeWorker.enqueue(context.getApplicationContext());
        }
    }

    private void clearSensitiveStateLocked() {
        if (activeDb != null) {
            activeDb.close();
            activeDb = null;
        }
        if (activePassphrase != null) {
            wipeBytes(activePassphrase);
            activePassphrase = null;
        }
        activeDbName = null;
        panicMode = false;
    }

    private void seedDecoy(AppDatabase db) {
        if (db == null) return;
        if (!db.chatDao().getAll().isEmpty()) return;
        long now = System.currentTimeMillis();

        ChatEntity welcome = new ChatEntity();
        welcome.id = "decoy_welcome";
        welcome.title = "Welcome";
        welcome.username = "Xipher Support";
        welcome.lastMessage = "Welcome to Xipher. Tap here for tips.";
        welcome.unread = 0;
        welcome.isChannel = false;
        welcome.isGroup = false;
        welcome.isSaved = false;
        welcome.isPrivate = true;
        welcome.updatedAt = now - 60000L;

        ChatEntity support = new ChatEntity();
        support.id = "decoy_support";
        support.title = "Support";
        support.username = "Support";
        support.lastMessage = "We are here if you need anything.";
        support.unread = 0;
        support.isChannel = false;
        support.isGroup = false;
        support.isSaved = false;
        support.isPrivate = true;
        support.updatedAt = now - 30000L;

        MessageEntity welcomeMsg = new MessageEntity();
        welcomeMsg.id = "decoy_welcome_1";
        welcomeMsg.chatId = welcome.id;
        welcomeMsg.senderId = "support_bot";
        welcomeMsg.senderName = "Xipher";
        welcomeMsg.content = "Welcome to Xipher. Your chats are ready to go.";
        welcomeMsg.messageType = "text";
        welcomeMsg.createdAtMillis = now - 60000L;
        welcomeMsg.createdAt = String.valueOf(welcomeMsg.createdAtMillis);
        welcomeMsg.status = "read";

        MessageEntity supportMsg = new MessageEntity();
        supportMsg.id = "decoy_support_1";
        supportMsg.chatId = support.id;
        supportMsg.senderId = "support";
        supportMsg.senderName = "Support";
        supportMsg.content = "Hi! Let us know if you have any questions.";
        supportMsg.messageType = "text";
        supportMsg.createdAtMillis = now - 30000L;
        supportMsg.createdAt = String.valueOf(supportMsg.createdAtMillis);
        supportMsg.status = "read";

        List<ChatEntity> chats = new ArrayList<>();
        chats.add(welcome);
        chats.add(support);
        List<MessageEntity> messages = new ArrayList<>();
        messages.add(welcomeMsg);
        messages.add(supportMsg);

        db.runInTransaction(() -> {
            db.chatDao().upsertAll(chats);
            db.messageDao().upsertAll(messages);
        });
    }

    private static void safeClose(AppDatabase db) {
        if (db != null) {
            db.close();
        }
    }

    private static void wipeChars(char[] chars) {
        if (chars != null) {
            Arrays.fill(chars, '\0');
        }
    }

    private static void wipeBytes(byte[] bytes) {
        if (bytes != null) {
            Arrays.fill(bytes, (byte) 0);
        }
    }

    private static final class DbHolder {
        private final String dbName;
        private final AppDatabase db;
        private final byte[] passphrase;
        private final boolean created;

        private DbHolder(String dbName, AppDatabase db, byte[] passphrase, boolean created) {
            this.dbName = dbName;
            this.db = db;
            this.passphrase = passphrase;
            this.created = created;
        }
    }

    public static final class InvalidPasswordException extends Exception {
        public InvalidPasswordException(String message, Throwable cause) {
            super(message, cause);
        }
    }
}
