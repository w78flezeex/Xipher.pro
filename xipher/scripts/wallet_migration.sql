-- Xipher Wallet Database Schema
-- Run this migration to add wallet support

-- User wallet addresses
CREATE TABLE IF NOT EXISTS user_wallets (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    ethereum_address VARCHAR(42),
    polygon_address VARCHAR(42),
    solana_address VARCHAR(64),
    ton_address VARCHAR(64),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE,
    UNIQUE(user_id)
);

-- Create index for faster lookups
CREATE INDEX IF NOT EXISTS idx_user_wallets_user_id ON user_wallets(user_id);
CREATE INDEX IF NOT EXISTS idx_user_wallets_ethereum ON user_wallets(ethereum_address) WHERE ethereum_address IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_user_wallets_polygon ON user_wallets(polygon_address) WHERE polygon_address IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_user_wallets_solana ON user_wallets(solana_address) WHERE solana_address IS NOT NULL;
CREATE INDEX IF NOT EXISTS idx_user_wallets_ton ON user_wallets(ton_address) WHERE ton_address IS NOT NULL;

-- Transaction history (for tracking relayed transactions)
CREATE TABLE IF NOT EXISTS wallet_transactions (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    chain VARCHAR(20) NOT NULL,  -- 'ethereum', 'polygon', 'solana', 'ton'
    tx_hash VARCHAR(128) NOT NULL,
    tx_type VARCHAR(20) DEFAULT 'send',  -- 'send', 'receive', 'swap'
    amount_native DECIMAL(38, 18),
    amount_usd DECIMAL(18, 2),
    token_symbol VARCHAR(20),
    token_contract VARCHAR(64),
    from_address VARCHAR(64),
    to_address VARCHAR(64),
    status VARCHAR(20) DEFAULT 'pending',  -- 'pending', 'confirmed', 'failed'
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    confirmed_at TIMESTAMP WITH TIME ZONE
);

-- Indexes for transaction queries
CREATE INDEX IF NOT EXISTS idx_wallet_tx_user_id ON wallet_transactions(user_id);
CREATE INDEX IF NOT EXISTS idx_wallet_tx_chain ON wallet_transactions(chain);
CREATE INDEX IF NOT EXISTS idx_wallet_tx_created_at ON wallet_transactions(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_wallet_tx_hash ON wallet_transactions(tx_hash);

-- In-chat money transfers (peer-to-peer in messenger)
CREATE TABLE IF NOT EXISTS chat_money_transfers (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    sender_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    recipient_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    message_id UUID,  -- Reference to the message containing the transfer
    amount_usd DECIMAL(18, 2) NOT NULL,
    chain VARCHAR(20) NOT NULL,
    tx_hash VARCHAR(128),
    status VARCHAR(20) DEFAULT 'pending',  -- 'pending', 'claimed', 'expired', 'refunded'
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    claimed_at TIMESTAMP WITH TIME ZONE,
    expires_at TIMESTAMP WITH TIME ZONE DEFAULT NOW() + INTERVAL '7 days'
);

-- Indexes for money transfers
CREATE INDEX IF NOT EXISTS idx_money_transfer_sender ON chat_money_transfers(sender_id);
CREATE INDEX IF NOT EXISTS idx_money_transfer_recipient ON chat_money_transfers(recipient_id);
CREATE INDEX IF NOT EXISTS idx_money_transfer_status ON chat_money_transfers(status);
CREATE INDEX IF NOT EXISTS idx_money_transfer_message ON chat_money_transfers(message_id);

-- Price cache for quick balance lookups
CREATE TABLE IF NOT EXISTS crypto_prices (
    symbol VARCHAR(20) PRIMARY KEY,
    price_usd DECIMAL(18, 8) NOT NULL,
    change_24h DECIMAL(8, 4),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Insert default prices
INSERT INTO crypto_prices (symbol, price_usd, change_24h) VALUES
    ('ETH', 3200.00, 0),
    ('MATIC', 0.85, 0),
    ('SOL', 150.00, 0),
    ('TON', 5.50, 0),
    ('USDT', 1.00, 0),
    ('USDC', 1.00, 0)
ON CONFLICT (symbol) DO NOTHING;

-- Function to update prices
CREATE OR REPLACE FUNCTION update_crypto_price(
    p_symbol VARCHAR(20),
    p_price DECIMAL(18, 8),
    p_change DECIMAL(8, 4)
) RETURNS VOID AS $$
BEGIN
    INSERT INTO crypto_prices (symbol, price_usd, change_24h, updated_at)
    VALUES (p_symbol, p_price, p_change, NOW())
    ON CONFLICT (symbol) DO UPDATE SET
        price_usd = EXCLUDED.price_usd,
        change_24h = EXCLUDED.change_24h,
        updated_at = NOW();
END;
$$ LANGUAGE plpgsql;

-- Grant permissions
GRANT SELECT, INSERT, UPDATE ON user_wallets TO xipher;
GRANT SELECT, INSERT, UPDATE ON wallet_transactions TO xipher;
GRANT SELECT, INSERT, UPDATE ON chat_money_transfers TO xipher;
GRANT SELECT, INSERT, UPDATE ON crypto_prices TO xipher;
GRANT USAGE ON SEQUENCE user_wallets_id_seq TO xipher;
GRANT USAGE ON SEQUENCE wallet_transactions_id_seq TO xipher;
GRANT USAGE ON SEQUENCE chat_money_transfers_id_seq TO xipher;

-- Add wallet_address column to users if needed
ALTER TABLE users ADD COLUMN IF NOT EXISTS wallet_address VARCHAR(64);
