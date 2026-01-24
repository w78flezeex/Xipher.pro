-- Migration: add delivery status + read receipts preference

ALTER TABLE messages
    ADD COLUMN IF NOT EXISTS is_delivered BOOLEAN DEFAULT FALSE;

ALTER TABLE user_privacy_settings
    ADD COLUMN IF NOT EXISTS send_read_receipts BOOLEAN NOT NULL DEFAULT TRUE;

UPDATE user_privacy_settings
    SET send_read_receipts = TRUE
    WHERE send_read_receipts IS NULL;
