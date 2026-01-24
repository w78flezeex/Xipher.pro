-- Xipher Wallet Extended Features Migration
-- P2P Trading, Vouchers/Checks, Staking, NFT

-- =============================================
-- P2P MARKETPLACE
-- =============================================

-- P2P trade offers (buy/sell ads)
CREATE TABLE IF NOT EXISTS p2p_offers (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    offer_type VARCHAR(10) NOT NULL CHECK (offer_type IN ('buy', 'sell')),
    crypto_symbol VARCHAR(20) NOT NULL,  -- USDT, BTC, ETH, TON, etc.
    fiat_currency VARCHAR(10) NOT NULL,  -- RUB, USD, EUR
    price_per_unit DECIMAL(18, 4) NOT NULL,  -- Price per 1 crypto unit
    min_amount DECIMAL(18, 2) NOT NULL,
    max_amount DECIMAL(18, 2) NOT NULL,
    available_amount DECIMAL(18, 8) NOT NULL,  -- How much crypto available
    payment_methods TEXT[],  -- ['Sberbank', 'Tinkoff']
    terms TEXT,  -- Seller's terms
    is_active BOOLEAN DEFAULT true,
    total_trades INTEGER DEFAULT 0,
    successful_trades INTEGER DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    updated_at TIMESTAMP WITH TIME ZONE
);

CREATE INDEX IF NOT EXISTS idx_p2p_offers_type ON p2p_offers(offer_type, is_active);
CREATE INDEX IF NOT EXISTS idx_p2p_offers_crypto ON p2p_offers(crypto_symbol);
CREATE INDEX IF NOT EXISTS idx_p2p_offers_user ON p2p_offers(user_id);

-- P2P trades (active and completed transactions)
CREATE TABLE IF NOT EXISTS p2p_trades (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    offer_id UUID REFERENCES p2p_offers(id) ON DELETE SET NULL,
    buyer_id UUID NOT NULL REFERENCES users(id),
    seller_id UUID NOT NULL REFERENCES users(id),
    crypto_symbol VARCHAR(20) NOT NULL,
    crypto_amount DECIMAL(18, 8) NOT NULL,
    fiat_amount DECIMAL(18, 2) NOT NULL,
    fiat_currency VARCHAR(10) NOT NULL,
    payment_method VARCHAR(100),
    status VARCHAR(20) DEFAULT 'pending' CHECK (status IN (
        'pending',     -- Trade created, waiting for seller confirmation
        'confirmed',   -- Seller confirmed, waiting for payment
        'paid',        -- Buyer marked as paid
        'completed',   -- Seller released crypto
        'cancelled',   -- Trade cancelled
        'disputed'     -- Dispute opened
    )),
    escrow_tx_hash VARCHAR(128),  -- Crypto locked in escrow
    release_tx_hash VARCHAR(128), -- Crypto released to buyer
    buyer_rating SMALLINT CHECK (buyer_rating BETWEEN 1 AND 5),
    seller_rating SMALLINT CHECK (seller_rating BETWEEN 1 AND 5),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    paid_at TIMESTAMP WITH TIME ZONE,
    completed_at TIMESTAMP WITH TIME ZONE
);

CREATE INDEX IF NOT EXISTS idx_p2p_trades_buyer ON p2p_trades(buyer_id);
CREATE INDEX IF NOT EXISTS idx_p2p_trades_seller ON p2p_trades(seller_id);
CREATE INDEX IF NOT EXISTS idx_p2p_trades_status ON p2p_trades(status);

-- P2P user statistics
CREATE TABLE IF NOT EXISTS p2p_user_stats (
    user_id UUID PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    total_trades INTEGER DEFAULT 0,
    successful_trades INTEGER DEFAULT 0,
    total_volume_usd DECIMAL(18, 2) DEFAULT 0,
    rating_sum INTEGER DEFAULT 0,
    rating_count INTEGER DEFAULT 0,
    avg_rating DECIMAL(3, 2) GENERATED ALWAYS AS (
        CASE WHEN rating_count > 0 THEN rating_sum::DECIMAL / rating_count ELSE 0 END
    ) STORED,
    first_trade_at TIMESTAMP WITH TIME ZONE,
    last_trade_at TIMESTAMP WITH TIME ZONE,
    is_verified BOOLEAN DEFAULT false
);

-- =============================================
-- CRYPTO VOUCHERS / CHECKS
-- =============================================

CREATE TABLE IF NOT EXISTS crypto_vouchers (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    code VARCHAR(20) UNIQUE NOT NULL,  -- XXXX-XXXX-XXXX format
    creator_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    amount_usd DECIMAL(18, 2) NOT NULL,
    crypto_symbol VARCHAR(20) NOT NULL,  -- Which crypto to receive
    crypto_amount DECIMAL(18, 8) NOT NULL,
    chain VARCHAR(20) NOT NULL,
    is_single_use BOOLEAN DEFAULT true,
    uses_remaining INTEGER DEFAULT 1,
    password_hash VARCHAR(256),  -- Optional password protection
    comment TEXT,
    status VARCHAR(20) DEFAULT 'active' CHECK (status IN ('active', 'claimed', 'expired', 'revoked')),
    funding_tx_hash VARCHAR(128),  -- TX that funded this voucher
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    expires_at TIMESTAMP WITH TIME ZONE DEFAULT NOW() + INTERVAL '30 days',
    claimed_at TIMESTAMP WITH TIME ZONE
);

CREATE INDEX IF NOT EXISTS idx_vouchers_code ON crypto_vouchers(code);
CREATE INDEX IF NOT EXISTS idx_vouchers_creator ON crypto_vouchers(creator_id);
CREATE INDEX IF NOT EXISTS idx_vouchers_status ON crypto_vouchers(status);

-- Voucher claims history
CREATE TABLE IF NOT EXISTS voucher_claims (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    voucher_id UUID NOT NULL REFERENCES crypto_vouchers(id),
    claimer_id UUID NOT NULL REFERENCES users(id),
    amount_received DECIMAL(18, 8) NOT NULL,
    tx_hash VARCHAR(128),
    claimed_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_voucher_claims_voucher ON voucher_claims(voucher_id);
CREATE INDEX IF NOT EXISTS idx_voucher_claims_user ON voucher_claims(claimer_id);

-- =============================================
-- STAKING / EARN
-- =============================================

-- Available staking products
CREATE TABLE IF NOT EXISTS staking_products (
    id VARCHAR(50) PRIMARY KEY,
    name VARCHAR(100) NOT NULL,
    crypto_symbol VARCHAR(20) NOT NULL,
    chain VARCHAR(20) NOT NULL,
    apy DECIMAL(6, 3) NOT NULL,  -- Annual Percentage Yield (e.g., 4.500)
    min_amount DECIMAL(18, 8) NOT NULL,
    max_amount DECIMAL(18, 8),
    lock_period_days INTEGER DEFAULT 0,  -- 0 = flexible
    is_active BOOLEAN DEFAULT true,
    description TEXT,
    validator_address VARCHAR(128),  -- For on-chain staking
    contract_address VARCHAR(128),   -- For smart contract staking
    total_staked DECIMAL(18, 8) DEFAULT 0,
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- Insert default staking products
INSERT INTO staking_products (id, name, crypto_symbol, chain, apy, min_amount, lock_period_days, description) VALUES
    ('ton-staking', 'TON Liquid Staking', 'TON', 'ton', 4.500, 1, 0, 'Stake TON through Whales pool. Withdraw anytime.'),
    ('eth-staking', 'ETH 2.0 Staking', 'ETH', 'ethereum', 3.800, 0.01, 0, 'Stake ETH in Ethereum 2.0 via Lido.'),
    ('sol-staking', 'SOL Staking', 'SOL', 'solana', 6.200, 0.1, 0, 'Delegate SOL to Marinade validators.'),
    ('usdt-lending', 'USDT Lending', 'USDT', 'polygon', 8.000, 10, 0, 'Lend USDT on Aave for yield.')
ON CONFLICT (id) DO UPDATE SET 
    apy = EXCLUDED.apy,
    min_amount = EXCLUDED.min_amount,
    description = EXCLUDED.description;

-- User staking positions
CREATE TABLE IF NOT EXISTS user_stakes (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    product_id VARCHAR(50) NOT NULL REFERENCES staking_products(id),
    staked_amount DECIMAL(18, 8) NOT NULL,
    staked_amount_usd DECIMAL(18, 2) NOT NULL,
    stake_tx_hash VARCHAR(128),
    unstake_tx_hash VARCHAR(128),
    accrued_rewards DECIMAL(18, 8) DEFAULT 0,
    claimed_rewards DECIMAL(18, 8) DEFAULT 0,
    status VARCHAR(20) DEFAULT 'active' CHECK (status IN ('active', 'unstaking', 'completed')),
    staked_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    unlock_at TIMESTAMP WITH TIME ZONE,
    unstaked_at TIMESTAMP WITH TIME ZONE
);

CREATE INDEX IF NOT EXISTS idx_stakes_user ON user_stakes(user_id);
CREATE INDEX IF NOT EXISTS idx_stakes_product ON user_stakes(product_id);
CREATE INDEX IF NOT EXISTS idx_stakes_status ON user_stakes(status);

-- Staking rewards history
CREATE TABLE IF NOT EXISTS staking_rewards (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    stake_id UUID NOT NULL REFERENCES user_stakes(id),
    amount DECIMAL(18, 8) NOT NULL,
    tx_hash VARCHAR(128),
    type VARCHAR(20) DEFAULT 'accrued' CHECK (type IN ('accrued', 'claimed', 'compounded')),
    created_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- =============================================
-- NFT TRACKING
-- =============================================

-- Cached NFT data from external APIs (Alchemy, OpenSea, etc.)
CREATE TABLE IF NOT EXISTS user_nfts (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    chain VARCHAR(20) NOT NULL,
    contract_address VARCHAR(128) NOT NULL,
    token_id VARCHAR(128) NOT NULL,
    name VARCHAR(256),
    description TEXT,
    image_url TEXT,
    metadata_url TEXT,
    collection_name VARCHAR(256),
    collection_slug VARCHAR(128),
    floor_price_usd DECIMAL(18, 2),
    last_sale_usd DECIMAL(18, 2),
    cached_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    UNIQUE(user_id, chain, contract_address, token_id)
);

CREATE INDEX IF NOT EXISTS idx_nfts_user ON user_nfts(user_id);
CREATE INDEX IF NOT EXISTS idx_nfts_chain ON user_nfts(chain);
CREATE INDEX IF NOT EXISTS idx_nfts_collection ON user_nfts(collection_slug);

-- =============================================
-- SECURITY SETTINGS
-- =============================================

CREATE TABLE IF NOT EXISTS wallet_security_settings (
    user_id UUID PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    pin_hash VARCHAR(256),  -- Hashed PIN code
    biometric_enabled BOOLEAN DEFAULT false,
    auto_lock_minutes INTEGER DEFAULT 5,
    require_password_for_tx BOOLEAN DEFAULT true,
    two_factor_enabled BOOLEAN DEFAULT false,
    two_factor_secret VARCHAR(64),
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- =============================================
-- NOTIFICATION PREFERENCES
-- =============================================

CREATE TABLE IF NOT EXISTS wallet_notifications (
    user_id UUID PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE,
    incoming_transfers BOOLEAN DEFAULT true,
    price_alerts BOOLEAN DEFAULT false,
    market_news BOOLEAN DEFAULT false,
    staking_rewards BOOLEAN DEFAULT true,
    p2p_updates BOOLEAN DEFAULT true,
    updated_at TIMESTAMP WITH TIME ZONE DEFAULT NOW()
);

-- =============================================
-- CONNECTED APPS (WalletConnect / TON Connect)
-- =============================================

CREATE TABLE IF NOT EXISTS wallet_connections (
    id UUID DEFAULT gen_random_uuid() PRIMARY KEY,
    user_id UUID NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    protocol VARCHAR(20) NOT NULL CHECK (protocol IN ('walletconnect', 'tonconnect')),
    dapp_name VARCHAR(256),
    dapp_url TEXT,
    dapp_icon TEXT,
    peer_id VARCHAR(256),
    session_data JSONB,
    chain VARCHAR(20),
    connected_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    last_active_at TIMESTAMP WITH TIME ZONE DEFAULT NOW(),
    is_active BOOLEAN DEFAULT true
);

CREATE INDEX IF NOT EXISTS idx_wallet_connections_user ON wallet_connections(user_id, is_active);

-- =============================================
-- HELPER FUNCTIONS
-- =============================================

-- Generate voucher code
CREATE OR REPLACE FUNCTION generate_voucher_code() RETURNS VARCHAR(14) AS $$
DECLARE
    chars VARCHAR(36) := 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
    result VARCHAR(14) := '';
    i INTEGER;
BEGIN
    FOR i IN 1..12 LOOP
        result := result || substr(chars, floor(random() * 36 + 1)::integer, 1);
        IF i = 4 OR i = 8 THEN
            result := result || '-';
        END IF;
    END LOOP;
    RETURN result;
END;
$$ LANGUAGE plpgsql;

-- Update P2P user stats after trade
CREATE OR REPLACE FUNCTION update_p2p_stats() RETURNS TRIGGER AS $$
BEGIN
    IF NEW.status = 'completed' AND (OLD.status IS NULL OR OLD.status != 'completed') THEN
        -- Update buyer stats
        INSERT INTO p2p_user_stats (user_id, total_trades, successful_trades, total_volume_usd, last_trade_at)
        VALUES (NEW.buyer_id, 1, 1, NEW.fiat_amount, NOW())
        ON CONFLICT (user_id) DO UPDATE SET
            total_trades = p2p_user_stats.total_trades + 1,
            successful_trades = p2p_user_stats.successful_trades + 1,
            total_volume_usd = p2p_user_stats.total_volume_usd + NEW.fiat_amount,
            last_trade_at = NOW();
            
        -- Update seller stats
        INSERT INTO p2p_user_stats (user_id, total_trades, successful_trades, total_volume_usd, last_trade_at)
        VALUES (NEW.seller_id, 1, 1, NEW.fiat_amount, NOW())
        ON CONFLICT (user_id) DO UPDATE SET
            total_trades = p2p_user_stats.total_trades + 1,
            successful_trades = p2p_user_stats.successful_trades + 1,
            total_volume_usd = p2p_user_stats.total_volume_usd + NEW.fiat_amount,
            last_trade_at = NOW();
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_update_p2p_stats
    AFTER INSERT OR UPDATE ON p2p_trades
    FOR EACH ROW EXECUTE FUNCTION update_p2p_stats();

-- Update staking product total
CREATE OR REPLACE FUNCTION update_staking_total() RETURNS TRIGGER AS $$
BEGIN
    IF TG_OP = 'INSERT' AND NEW.status = 'active' THEN
        UPDATE staking_products SET total_staked = total_staked + NEW.staked_amount
        WHERE id = NEW.product_id;
    ELSIF TG_OP = 'UPDATE' AND OLD.status = 'active' AND NEW.status IN ('unstaking', 'completed') THEN
        UPDATE staking_products SET total_staked = total_staked - NEW.staked_amount
        WHERE id = NEW.product_id;
    END IF;
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

CREATE TRIGGER trigger_update_staking_total
    AFTER INSERT OR UPDATE ON user_stakes
    FOR EACH ROW EXECUTE FUNCTION update_staking_total();
