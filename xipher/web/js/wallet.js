/**
 * Xipher Wallet - HD Multi-Chain Crypto Wallet
 * 
 * Features:
 * - BIP44 HD Wallet (ETH, TON, Polygon, Solana)
 * - All balances displayed in USD equivalent
 * - CoinGecko price aggregation + Top 250 tokens
 * - 1inch swap integration
 * - In-chat money transfers: "@friend $50"
 * - Local key storage (secure)
 */

const XipherWallet = (() => {
    'use strict';

    // ============= CONFIGURATION =============
    const CONFIG = {
        // Chain configurations
        chains: {
            ethereum: {
                id: 'ethereum',
                name: 'Ethereum',
                symbol: 'ETH',
                icon: 'Ξ',
                rpcUrl: 'https://eth.llamarpc.com',
                chainId: 1,
                explorer: 'https://etherscan.io',
                decimals: 18,
                coinGeckoId: 'ethereum',
                derivationPath: "m/44'/60'/0'/0/0"
            },
            polygon: {
                id: 'polygon',
                name: 'Polygon',
                symbol: 'MATIC',
                icon: '⬡',
                rpcUrl: 'https://polygon-rpc.com',
                chainId: 137,
                explorer: 'https://polygonscan.com',
                decimals: 18,
                coinGeckoId: 'matic-network',
                derivationPath: "m/44'/60'/0'/0/0"  // Same as ETH (EVM)
            },
            solana: {
                id: 'solana',
                name: 'Solana',
                symbol: 'SOL',
                icon: '◎',
                rpcUrl: 'https://api.mainnet-beta.solana.com',
                chainId: null,
                explorer: 'https://solscan.io',
                decimals: 9,
                coinGeckoId: 'solana',
                derivationPath: "m/44'/501'/0'/0'"
            },
            ton: {
                id: 'ton',
                name: 'TON',
                symbol: 'TON',
                icon: '💎',
                rpcUrl: 'https://toncenter.com/api/v2',
                chainId: null,
                explorer: 'https://tonscan.org',
                decimals: 9,
                coinGeckoId: 'the-open-network',
                derivationPath: "m/44'/607'/0'"
            }
        },
        
        // Stablecoins (USDT on different chains)
        stablecoins: {
            'usdt-ethereum': {
                symbol: 'USDT',
                name: 'Tether USD',
                chain: 'ethereum',
                contract: '0xdAC17F958D2ee523a2206206994597C13D831ec7',
                decimals: 6,
                coinGeckoId: 'tether'
            },
            'usdt-polygon': {
                symbol: 'USDT',
                name: 'Tether USD',
                chain: 'polygon',
                contract: '0xc2132D05D31c914a87C6611C10748AEb04B58e8F',
                decimals: 6,
                coinGeckoId: 'tether'
            },
            'usdc-ethereum': {
                symbol: 'USDC',
                name: 'USD Coin',
                chain: 'ethereum',
                contract: '0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48',
                decimals: 6,
                coinGeckoId: 'usd-coin'
            },
            'usdc-polygon': {
                symbol: 'USDC',
                name: 'USD Coin',
                chain: 'polygon',
                contract: '0x2791Bca1f2de4661ED88A30C99A7a9449Aa84174',
                decimals: 6,
                coinGeckoId: 'usd-coin'
            },
            'usdc-solana': {
                symbol: 'USDC',
                name: 'USD Coin',
                chain: 'solana',
                contract: 'EPjFWdd5AufqSSqeM2qN1xzybapC8G4wEGGkZwyTDt1v',
                decimals: 6,
                coinGeckoId: 'usd-coin'
            }
        },
        
        // API endpoints
        api: {
            coinGecko: 'https://api.coingecko.com/api/v3',
            oneInch: 'https://api.1inch.dev/swap/v5.2',
            relay: '/api/wallet'
        },
        
        // Price cache duration (ms)
        priceCacheDuration: 60000, // 1 minute
        tokenListCacheDuration: 3600000, // 1 hour
        
        // Storage keys
        storageKeys: {
            encryptedSeed: 'xipher_wallet_seed_enc',
            addresses: 'xipher_wallet_addresses',
            txHistory: 'xipher_wallet_tx_history',
            priceCache: 'xipher_wallet_prices',
            preferredChain: 'xipher_wallet_chain',
            tokenList: 'xipher_wallet_tokens',
            userTokens: 'xipher_wallet_user_tokens',
            tokenBalances: 'xipher_wallet_token_balances'
        }
    };

    // ============= STATE =============
    let state = {
        initialized: false,
        hasWallet: false,
        addresses: {},      // { ethereum: '0x...', polygon: '0x...', solana: '...', ton: '...' }
        balances: {},       // { ethereum: { native: '0.5', usd: 1250.50 }, ... }
        totalBalanceUsd: 0,
        prices: {},         // { ethereum: 3200, polygon: 0.85, ... }
        pricesLastUpdate: 0,
        transactions: [],
        preferredChain: 'ethereum',
        isLoading: false,
        // New: All available tokens from CoinGecko
        allTokens: [],      // Full list of tokens from CoinGecko
        tokensLastUpdate: 0,
        userTokens: [],     // Tokens user has added to wallet
        tokenBalances: {},  // { 'bitcoin': { amount: 0.5, usd: 50000 }, ... }
        tokenPrices: {}     // { 'bitcoin': { usd: 100000, change24h: 2.5 }, ... }
    };

    // ============= CRYPTO UTILITIES =============
    
    /**
     * Generate BIP39 mnemonic (12 words)
     */
    async function generateMnemonic() {
        // Use ethers.js if available, otherwise generate manually
        if (typeof ethers !== 'undefined' && ethers.Wallet) {
            const wallet = ethers.Wallet.createRandom();
            return wallet.mnemonic.phrase;
        }
        
        // Fallback: Generate using Web Crypto API
        const entropy = new Uint8Array(16); // 128 bits = 12 words
        crypto.getRandomValues(entropy);
        
        // Simple BIP39 implementation (should use proper library in production)
        const wordlist = await fetchBIP39Wordlist();
        const bits = bytesToBits(entropy);
        const checksum = await sha256Bits(entropy);
        const allBits = bits + checksum.slice(0, 4); // 4 bits checksum for 128 bits
        
        const words = [];
        for (let i = 0; i < 12; i++) {
            const index = parseInt(allBits.slice(i * 11, (i + 1) * 11), 2);
            words.push(wordlist[index]);
        }
        return words.join(' ');
    }
    
    /**
     * Derive addresses from mnemonic for all chains
     */
    async function deriveAddresses(mnemonic) {
        const addresses = {};
        
        // For EVM chains (Ethereum, Polygon) - same address
        if (typeof ethers !== 'undefined') {
            const evmWallet = ethers.Wallet.fromPhrase(mnemonic);
            addresses.ethereum = evmWallet.address;
            addresses.polygon = evmWallet.address;
        } else {
            // Fallback derivation
            addresses.ethereum = await deriveEVMAddress(mnemonic);
            addresses.polygon = addresses.ethereum;
        }
        
        // Solana address derivation
        addresses.solana = await deriveSolanaAddress(mnemonic);
        
        // TON address derivation
        addresses.ton = await deriveTONAddress(mnemonic);
        
        return addresses;
    }
    
    /**
     * Derive EVM address from mnemonic
     */
    async function deriveEVMAddress(mnemonic) {
        // Simplified - in production use proper HD derivation
        const seed = await mnemonicToSeed(mnemonic);
        const privateKey = seed.slice(0, 32);
        
        // Derive public key and address
        // This is simplified - should use secp256k1
        const hash = await crypto.subtle.digest('SHA-256', privateKey);
        const hashArray = new Uint8Array(hash);
        return '0x' + bytesToHex(hashArray.slice(12));
    }
    
    /**
     * Derive Solana address from mnemonic
     */
    async function deriveSolanaAddress(mnemonic) {
        const seed = await mnemonicToSeed(mnemonic);
        // Simplified Solana address derivation
        const hash = await crypto.subtle.digest('SHA-256', seed.slice(0, 32));
        const hashArray = new Uint8Array(hash);
        return base58Encode(hashArray);
    }
    
    /**
     * Derive TON address from mnemonic
     */
    async function deriveTONAddress(mnemonic) {
        const seed = await mnemonicToSeed(mnemonic);
        const hash = await crypto.subtle.digest('SHA-256', seed.slice(0, 32));
        const hashArray = new Uint8Array(hash);
        // Simplified TON address (should use proper TON SDK)
        return 'EQ' + base64UrlEncode(hashArray.slice(0, 32));
    }
    
    /**
     * Convert mnemonic to seed
     */
    async function mnemonicToSeed(mnemonic, passphrase = '') {
        const encoder = new TextEncoder();
        const mnemonicBytes = encoder.encode(mnemonic.normalize('NFKD'));
        const saltBytes = encoder.encode('mnemonic' + passphrase.normalize('NFKD'));
        
        const key = await crypto.subtle.importKey(
            'raw',
            mnemonicBytes,
            { name: 'PBKDF2' },
            false,
            ['deriveBits']
        );
        
        const derivedBits = await crypto.subtle.deriveBits(
            {
                name: 'PBKDF2',
                salt: saltBytes,
                iterations: 2048,
                hash: 'SHA-512'
            },
            key,
            512
        );
        
        return new Uint8Array(derivedBits);
    }
    
    /**
     * Encrypt seed phrase with password
     */
    async function encryptSeed(seed, password) {
        const encoder = new TextEncoder();
        const seedBytes = encoder.encode(seed);
        const passwordBytes = encoder.encode(password);
        
        const salt = crypto.getRandomValues(new Uint8Array(16));
        const iv = crypto.getRandomValues(new Uint8Array(12));
        
        const keyMaterial = await crypto.subtle.importKey(
            'raw',
            passwordBytes,
            { name: 'PBKDF2' },
            false,
            ['deriveKey']
        );
        
        const key = await crypto.subtle.deriveKey(
            {
                name: 'PBKDF2',
                salt: salt,
                iterations: 100000,
                hash: 'SHA-256'
            },
            keyMaterial,
            { name: 'AES-GCM', length: 256 },
            false,
            ['encrypt']
        );
        
        const encrypted = await crypto.subtle.encrypt(
            { name: 'AES-GCM', iv: iv },
            key,
            seedBytes
        );
        
        // Combine salt + iv + encrypted data
        const result = new Uint8Array(salt.length + iv.length + encrypted.byteLength);
        result.set(salt, 0);
        result.set(iv, salt.length);
        result.set(new Uint8Array(encrypted), salt.length + iv.length);
        
        return bytesToBase64(result);
    }
    
    /**
     * Decrypt seed phrase
     */
    async function decryptSeed(encryptedData, password) {
        const encoder = new TextEncoder();
        const passwordBytes = encoder.encode(password);
        const data = base64ToBytes(encryptedData);
        
        const salt = data.slice(0, 16);
        const iv = data.slice(16, 28);
        const encrypted = data.slice(28);
        
        const keyMaterial = await crypto.subtle.importKey(
            'raw',
            passwordBytes,
            { name: 'PBKDF2' },
            false,
            ['deriveKey']
        );
        
        const key = await crypto.subtle.deriveKey(
            {
                name: 'PBKDF2',
                salt: salt,
                iterations: 100000,
                hash: 'SHA-256'
            },
            keyMaterial,
            { name: 'AES-GCM', length: 256 },
            false,
            ['decrypt']
        );
        
        try {
            const decrypted = await crypto.subtle.decrypt(
                { name: 'AES-GCM', iv: iv },
                key,
                encrypted
            );
            
            return new TextDecoder().decode(decrypted);
        } catch (e) {
            throw new Error('Invalid password');
        }
    }

    // ============= PRICE & BALANCE FETCHING =============
    
    /**
     * Fetch prices from CoinGecko
     */
    async function fetchPrices() {
        const now = Date.now();
        if (now - state.pricesLastUpdate < CONFIG.priceCacheDuration && Object.keys(state.prices).length > 0) {
            return state.prices;
        }
        
        try {
            const ids = Object.values(CONFIG.chains)
                .map(c => c.coinGeckoId)
                .concat(['tether', 'usd-coin'])
                .join(',');
            
            const response = await fetch(
                `${CONFIG.api.coinGecko}/simple/price?ids=${ids}&vs_currencies=usd&include_24hr_change=true`
            );
            
            if (!response.ok) throw new Error('Price fetch failed');
            
            const data = await response.json();
            
            // Map to our chain IDs
            state.prices = {};
            for (const [chainId, chain] of Object.entries(CONFIG.chains)) {
                if (data[chain.coinGeckoId]) {
                    state.prices[chainId] = {
                        usd: data[chain.coinGeckoId].usd,
                        change24h: data[chain.coinGeckoId].usd_24h_change || 0
                    };
                }
            }
            
            state.prices.usdt = { usd: 1, change24h: 0 };
            state.prices.usdc = { usd: 1, change24h: 0 };
            
            state.pricesLastUpdate = now;
            
            // Cache prices
            localStorage.setItem(CONFIG.storageKeys.priceCache, JSON.stringify({
                prices: state.prices,
                timestamp: now
            }));
            
            return state.prices;
        } catch (error) {
            console.error('Price fetch error:', error);
            
            // Try to use cached prices
            try {
                const cached = JSON.parse(localStorage.getItem(CONFIG.storageKeys.priceCache) || '{}');
                if (cached.prices) {
                    state.prices = cached.prices;
                    return state.prices;
                }
            } catch (e) {}
            
            // Fallback prices
            return {
                ethereum: { usd: 3200, change24h: 0 },
                polygon: { usd: 0.85, change24h: 0 },
                solana: { usd: 150, change24h: 0 },
                ton: { usd: 5.5, change24h: 0 },
                usdt: { usd: 1, change24h: 0 },
                usdc: { usd: 1, change24h: 0 }
            };
        }
    }
    
    /**
     * Fetch all tokens from CoinGecko (Top 250 by market cap) via server proxy
     */
    async function fetchAllTokens(forceRefresh = false) {
        const now = Date.now();
        
        // Check cache
        if (!forceRefresh && state.allTokens.length > 0 && 
            now - state.tokensLastUpdate < CONFIG.tokenListCacheDuration) {
            return state.allTokens;
        }
        
        // Try localStorage cache
        if (!forceRefresh) {
            try {
                const cached = JSON.parse(localStorage.getItem(CONFIG.storageKeys.tokenList) || '{}');
                if (cached.tokens && cached.timestamp && 
                    now - cached.timestamp < CONFIG.tokenListCacheDuration) {
                    state.allTokens = cached.tokens;
                    state.tokensLastUpdate = cached.timestamp;
                    return state.allTokens;
                }
            } catch (e) {}
        }
        
        try {
            // Fetch via server proxy to avoid CORS
            const response = await fetch('/api/wallet/tokens?per_page=250&page=1');
            
            if (!response.ok) throw new Error('Token list fetch failed');
            
            const tokens = await response.json();
            
            state.allTokens = tokens.map(token => ({
                id: token.id,
                symbol: token.symbol.toUpperCase(),
                name: token.name,
                image: token.image,
                currentPrice: token.current_price,
                marketCap: token.market_cap,
                marketCapRank: token.market_cap_rank,
                priceChange24h: token.price_change_percentage_24h || 0,
                volume24h: token.total_volume,
                circulatingSupply: token.circulating_supply,
                ath: token.ath,
                athDate: token.ath_date
            }));
            
            state.tokensLastUpdate = now;
            
            // Cache to localStorage
            localStorage.setItem(CONFIG.storageKeys.tokenList, JSON.stringify({
                tokens: state.allTokens,
                timestamp: now
            }));
            
            return state.allTokens;
        } catch (error) {
            console.error('Token list fetch error:', error);
            return state.allTokens || [];
        }
    }
    
    /**
     * Search tokens by name or symbol
     */
    function searchTokens(query) {
        if (!query || query.length < 1) return state.allTokens.slice(0, 50);
        
        const q = query.toLowerCase();
        return state.allTokens.filter(token => 
            token.symbol.toLowerCase().includes(q) ||
            token.name.toLowerCase().includes(q)
        ).slice(0, 50);
    }
    
    /**
     * Get token by ID
     */
    function getToken(tokenId) {
        return state.allTokens.find(t => t.id === tokenId);
    }
    
    /**
     * Add token to user's portfolio
     */
    function addUserToken(tokenId, amount = 0) {
        const token = getToken(tokenId);
        if (!token) return false;
        
        // Check if already added
        if (state.userTokens.find(t => t.id === tokenId)) {
            return false;
        }
        
        state.userTokens.push({
            id: tokenId,
            symbol: token.symbol,
            name: token.name,
            image: token.image,
            amount: parseFloat(amount) || 0,
            addedAt: Date.now()
        });
        
        // Recalculate balance
        state.tokenBalances[tokenId] = {
            amount: parseFloat(amount) || 0,
            usd: (parseFloat(amount) || 0) * (token.currentPrice || 0)
        };
        
        saveUserTokens();
        return true;
    }
    
    /**
     * Remove token from user's portfolio
     */
    function removeUserToken(tokenId) {
        const index = state.userTokens.findIndex(t => t.id === tokenId);
        if (index === -1) return false;
        
        state.userTokens.splice(index, 1);
        delete state.tokenBalances[tokenId];
        
        saveUserTokens();
        return true;
    }
    
    /**
     * Update token amount in portfolio
     */
    function updateTokenAmount(tokenId, amount) {
        const userToken = state.userTokens.find(t => t.id === tokenId);
        if (!userToken) return false;
        
        const token = getToken(tokenId);
        userToken.amount = parseFloat(amount) || 0;
        
        state.tokenBalances[tokenId] = {
            amount: userToken.amount,
            usd: userToken.amount * (token?.currentPrice || 0)
        };
        
        saveUserTokens();
        recalculateTotalBalance();
        return true;
    }
    
    /**
     * Save user tokens to localStorage
     */
    function saveUserTokens() {
        localStorage.setItem(CONFIG.storageKeys.userTokens, JSON.stringify(state.userTokens));
        localStorage.setItem(CONFIG.storageKeys.tokenBalances, JSON.stringify(state.tokenBalances));
    }
    
    /**
     * Load user tokens from localStorage
     */
    function loadUserTokens() {
        try {
            state.userTokens = JSON.parse(localStorage.getItem(CONFIG.storageKeys.userTokens) || '[]');
            state.tokenBalances = JSON.parse(localStorage.getItem(CONFIG.storageKeys.tokenBalances) || '{}');
        } catch (e) {
            state.userTokens = [];
            state.tokenBalances = {};
        }
    }
    
    /**
     * Refresh prices for user tokens
     */
    async function refreshUserTokenPrices() {
        if (state.userTokens.length === 0) return;
        
        const ids = state.userTokens.map(t => t.id).join(',');
        
        try {
            // Use server proxy to avoid CORS
            const response = await fetch(`/api/wallet/prices?ids=${encodeURIComponent(ids)}`);
            
            if (!response.ok) return;
            
            const prices = await response.json();
            
            for (const token of state.userTokens) {
                if (prices[token.id]) {
                    state.tokenPrices[token.id] = {
                        usd: prices[token.id].usd,
                        change24h: prices[token.id].usd_24h_change || 0
                    };
                    
                    // Update balance
                    state.tokenBalances[token.id] = {
                        amount: token.amount,
                        usd: token.amount * prices[token.id].usd
                    };
                }
            }
            
            recalculateTotalBalance();
        } catch (error) {
            console.error('Token price refresh error:', error);
        }
    }
    
    /**
     * Recalculate total balance including user tokens
     */
    function recalculateTotalBalance() {
        state.totalBalanceUsd = 0;
        
        // Native chain balances
        for (const [chainId, balance] of Object.entries(state.balances)) {
            state.totalBalanceUsd += balance.usd || 0;
        }
        
        // User token balances
        for (const [tokenId, balance] of Object.entries(state.tokenBalances)) {
            state.totalBalanceUsd += balance.usd || 0;
        }
        
        updateUI();
    }
    
    /**
     * Get categories of tokens (for filtering)
     */
    async function fetchTokenCategories() {
        try {
            const response = await fetch(`${CONFIG.api.coinGecko}/coins/categories/list`);
            if (!response.ok) return [];
            return await response.json();
        } catch (error) {
            return [];
        }
    }
    
    /**
     * Fetch native balance for a chain
     */
    async function fetchNativeBalance(chainId, address) {
        const chain = CONFIG.chains[chainId];
        if (!chain) return { native: '0', usd: 0 };
        
        try {
            let balance = '0';
            
            if (chainId === 'ethereum' || chainId === 'polygon') {
                // EVM chains - use eth_getBalance
                const response = await fetch(chain.rpcUrl, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        jsonrpc: '2.0',
                        method: 'eth_getBalance',
                        params: [address, 'latest'],
                        id: 1
                    })
                });
                
                const data = await response.json();
                if (data.result) {
                    const wei = BigInt(data.result);
                    balance = (Number(wei) / Math.pow(10, chain.decimals)).toFixed(6);
                }
            } else if (chainId === 'solana') {
                // Solana RPC
                const response = await fetch(chain.rpcUrl, {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        jsonrpc: '2.0',
                        method: 'getBalance',
                        params: [address],
                        id: 1
                    })
                });
                
                const data = await response.json();
                if (data.result && data.result.value) {
                    balance = (data.result.value / Math.pow(10, chain.decimals)).toFixed(6);
                }
            } else if (chainId === 'ton') {
                // TON API
                const response = await fetch(`${chain.rpcUrl}/getAddressBalance?address=${address}`);
                const data = await response.json();
                if (data.ok && data.result) {
                    balance = (Number(data.result) / Math.pow(10, chain.decimals)).toFixed(6);
                }
            }
            
            const prices = await fetchPrices();
            const price = prices[chainId]?.usd || 0;
            const usdValue = parseFloat(balance) * price;
            
            return {
                native: balance,
                usd: usdValue
            };
        } catch (error) {
            console.error(`Balance fetch error for ${chainId}:`, error);
            return { native: '0', usd: 0 };
        }
    }
    
    /**
     * Fetch all balances
     */
    async function fetchAllBalances() {
        if (!state.addresses || Object.keys(state.addresses).length === 0) {
            return {};
        }
        
        state.isLoading = true;
        updateUI();
        
        try {
            const balancePromises = Object.entries(state.addresses).map(async ([chainId, address]) => {
                const balance = await fetchNativeBalance(chainId, address);
                return [chainId, balance];
            });
            
            const results = await Promise.all(balancePromises);
            
            state.balances = {};
            state.totalBalanceUsd = 0;
            
            for (const [chainId, balance] of results) {
                state.balances[chainId] = balance;
                state.totalBalanceUsd += balance.usd;
            }
            
            state.isLoading = false;
            updateUI();
            
            return state.balances;
        } catch (error) {
            console.error('Fetch all balances error:', error);
            state.isLoading = false;
            updateUI();
            return {};
        }
    }

    // ============= TRANSACTION HANDLING =============
    
    /**
     * Send native token
     */
    async function sendTransaction(toAddress, amount, chainId, password) {
        const chain = CONFIG.chains[chainId];
        if (!chain) throw new Error('Invalid chain');
        
        // Get seed and derive private key
        const encryptedSeed = localStorage.getItem(CONFIG.storageKeys.encryptedSeed);
        if (!encryptedSeed) throw new Error('Wallet not found');
        
        const seed = await decryptSeed(encryptedSeed, password);
        
        // Build and sign transaction based on chain
        let signedTx;
        
        if (chainId === 'ethereum' || chainId === 'polygon') {
            signedTx = await buildEVMTransaction(seed, toAddress, amount, chain);
        } else if (chainId === 'solana') {
            signedTx = await buildSolanaTransaction(seed, toAddress, amount);
        } else if (chainId === 'ton') {
            signedTx = await buildTONTransaction(seed, toAddress, amount);
        }
        
        // Relay transaction through backend (no private keys exposed)
        const response = await fetch(`${CONFIG.api.relay}/relay-tx`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                chain: chainId,
                signed_tx: signedTx,
                token: getSessionToken()
            })
        });
        
        const result = await response.json();
        
        if (!result.success) {
            throw new Error(result.error || 'Transaction failed');
        }
        
        // Save to history
        addTransaction({
            type: 'sent',
            chain: chainId,
            amount: amount,
            to: toAddress,
            txHash: result.txHash,
            timestamp: Date.now()
        });
        
        // Refresh balances
        await fetchAllBalances();
        
        return result;
    }
    
    /**
     * Build EVM transaction
     */
    async function buildEVMTransaction(mnemonic, toAddress, amount, chain) {
        if (typeof ethers !== 'undefined') {
            const wallet = ethers.Wallet.fromPhrase(mnemonic);
            const provider = new ethers.JsonRpcProvider(chain.rpcUrl);
            const connectedWallet = wallet.connect(provider);
            
            // Get nonce and gas price
            const nonce = await provider.getTransactionCount(connectedWallet.address);
            const feeData = await provider.getFeeData();
            
            const tx = {
                to: toAddress,
                value: ethers.parseEther(amount.toString()),
                nonce: nonce,
                gasLimit: 21000n, // Standard ETH transfer
                maxFeePerGas: feeData.maxFeePerGas,
                maxPriorityFeePerGas: feeData.maxPriorityFeePerGas,
                chainId: chain.chainId || 137 // Polygon by default
            };
            
            const signedTx = await connectedWallet.signTransaction(tx);
            return signedTx;
        }
        
        // Simplified fallback (in production, use proper tx signing)
        throw new Error('Ethers.js library required for EVM transactions');
    }
    
    /**
     * Build Solana transaction
     */
    async function buildSolanaTransaction(mnemonic, toAddress, amount) {
        // Simplified - in production use @solana/web3.js
        throw new Error('Solana transactions require @solana/web3.js');
    }
    
    /**
     * Build TON transaction
     */
    async function buildTONTransaction(mnemonic, toAddress, amount) {
        // Simplified - in production use TON SDK
        throw new Error('TON transactions require TON SDK');
    }

    // ============= SWAP INTEGRATION (1inch) =============
    
    /**
     * Get swap quote from 1inch
     */
    async function getSwapQuote(fromToken, toToken, amount, chainId) {
        const chainIdNum = CONFIG.chains[chainId]?.chainId;
        if (!chainIdNum) throw new Error('Swap not supported on this chain');
        
        try {
            const response = await fetch(
                `${CONFIG.api.oneInch}/${chainIdNum}/quote?` + new URLSearchParams({
                    src: fromToken,
                    dst: toToken,
                    amount: amount
                })
            );
            
            const data = await response.json();
            return data;
        } catch (error) {
            console.error('Swap quote error:', error);
            throw error;
        }
    }
    
    /**
     * Execute swap via 1inch
     */
    async function executeSwap(fromToken, toToken, amount, chainId, password) {
        const chainIdNum = CONFIG.chains[chainId]?.chainId;
        if (!chainIdNum) throw new Error('Swap not supported on this chain');
        
        // Get swap transaction data from 1inch
        const response = await fetch(
            `${CONFIG.api.oneInch}/${chainIdNum}/swap?` + new URLSearchParams({
                src: fromToken,
                dst: toToken,
                amount: amount,
                from: state.addresses[chainId],
                slippage: 1
            })
        );
        
        const swapData = await response.json();
        
        // Sign and relay the transaction
        const encryptedSeed = localStorage.getItem(CONFIG.storageKeys.encryptedSeed);
        const seed = await decryptSeed(encryptedSeed, password);
        
        // Sign the swap transaction
        if (typeof ethers !== 'undefined') {
            const wallet = ethers.Wallet.fromPhrase(seed);
            const provider = new ethers.JsonRpcProvider(CONFIG.chains[chainId].rpcUrl);
            const connectedWallet = wallet.connect(provider);
            
            const signedTx = await connectedWallet.signTransaction(swapData.tx);
            
            // Relay
            const relayResponse = await fetch(`${CONFIG.api.relay}/relay-tx`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    chain: chainId,
                    signedTx: signedTx,
                    session_token: getSessionToken()
                })
            });
            
            return await relayResponse.json();
        }
        
        throw new Error('Ethers.js required for swaps');
    }

    // ============= IN-CHAT MONEY TRANSFER =============
    
    /**
     * Parse message for money transfer pattern
     * Pattern: @username $amount or @username amount USDT
     */
    function parseMoneyTransfer(message) {
        // Pattern: @username $50 or @username 50 USDT
        const dollarPattern = /@(\w+)\s+\$(\d+(?:\.\d{1,2})?)/;
        const tokenPattern = /@(\w+)\s+(\d+(?:\.\d{1,2})?)\s*(USDT|USDC|ETH|SOL|TON|MATIC)/i;
        
        let match = message.match(dollarPattern);
        if (match) {
            return {
                recipient: match[1],
                amount: parseFloat(match[2]),
                currency: 'USD',
                original: match[0]
            };
        }
        
        match = message.match(tokenPattern);
        if (match) {
            return {
                recipient: match[1],
                amount: parseFloat(match[2]),
                currency: match[3].toUpperCase(),
                original: match[0]
            };
        }
        
        return null;
    }
    
    /**
     * Send money to user via chat
     */
    async function sendMoneyToUser(recipientUsername, amountUsd, password) {
        // Resolve username to wallet address
        const userResponse = await fetch('/api/resolve-username', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username: recipientUsername })
        });
        
        const userData = await userResponse.json();
        if (!userData.success || !userData.wallet_address) {
            throw new Error('User has no wallet or not found');
        }
        
        // Determine best chain for transfer (prefer stablecoins on Polygon for low fees)
        const preferredChain = 'polygon';
        const recipientAddress = userData.wallet_address;
        
        // Convert USD to USDT amount (1:1)
        const usdtAmount = amountUsd;
        
        // Check if we have enough USDT, otherwise swap
        const balance = state.balances[preferredChain];
        
        // For now, send native token equivalent
        const prices = await fetchPrices();
        const nativeAmount = amountUsd / (prices[preferredChain]?.usd || 1);
        
        // Execute transfer
        const result = await sendTransaction(
            recipientAddress,
            nativeAmount,
            preferredChain,
            password
        );
        
        return {
            ...result,
            amountUsd: amountUsd,
            recipient: recipientUsername
        };
    }

    // ============= WALLET MANAGEMENT =============
    
    /**
     * Create new wallet
     */
    async function createWallet(password) {
        const mnemonic = await generateMnemonic();
        const addresses = await deriveAddresses(mnemonic);
        
        // Encrypt and store seed
        const encryptedSeed = await encryptSeed(mnemonic, password);
        localStorage.setItem(CONFIG.storageKeys.encryptedSeed, encryptedSeed);
        localStorage.setItem(CONFIG.storageKeys.addresses, JSON.stringify(addresses));
        
        state.addresses = addresses;
        state.hasWallet = true;
        state.initialized = true;
        
        // Register addresses with backend
        await registerWalletAddresses(addresses);
        
        // Save to server for cross-device sync
        await saveWalletToServer();
        
        // Initial balance fetch
        await fetchAllBalances();
        
        updateUI();
        
        return {
            mnemonic: mnemonic,
            addresses: addresses
        };
    }
    
    /**
     * Import wallet from seed phrase
     */
    async function importWallet(mnemonic, password) {
        // Validate mnemonic
        const words = mnemonic.trim().split(/\s+/);
        if (words.length !== 12 && words.length !== 24) {
            throw new Error('Invalid seed phrase length');
        }
        
        const addresses = await deriveAddresses(mnemonic);
        
        // Encrypt and store
        const encryptedSeed = await encryptSeed(mnemonic, password);
        localStorage.setItem(CONFIG.storageKeys.encryptedSeed, encryptedSeed);
        localStorage.setItem(CONFIG.storageKeys.addresses, JSON.stringify(addresses));
        
        state.addresses = addresses;
        state.hasWallet = true;
        state.initialized = true;
        
        // Register addresses with backend
        await registerWalletAddresses(addresses);
        
        // Save to server for cross-device sync
        await saveWalletToServer();
        
        // Fetch balances
        await fetchAllBalances();
        
        updateUI();
        
        return { addresses };
    }
    
    /**
     * Register wallet addresses with backend
     */
    async function registerWalletAddresses(addresses) {
        try {
            await fetch(`${CONFIG.api.relay}/register`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    addresses: addresses,
                    session_token: getSessionToken()
                })
            });
        } catch (error) {
            console.error('Failed to register wallet:', error);
        }
    }
    
    /**
     * Save wallet to server (for cross-device sync)
     */
    async function saveWalletToServer() {
        const encryptedSeed = localStorage.getItem(CONFIG.storageKeys.encryptedSeed);
        const addressesJson = localStorage.getItem(CONFIG.storageKeys.addresses);
        
        if (!encryptedSeed) return false;
        
        try {
            const response = await fetch('/api/wallet/save', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    token: getSessionToken(),
                    encrypted_seed: encryptedSeed,
                    salt: '', // Already included in encrypted seed
                    addresses: addressesJson,
                    user_tokens: JSON.stringify(state.userTokens)
                })
            });
            
            const result = await response.json();
            return result.status === 'success';
        } catch (error) {
            console.error('Failed to save wallet to server:', error);
            return false;
        }
    }
    
    /**
     * Load wallet from server (for cross-device sync)
     */
    async function loadWalletFromServer() {
        try {
            const response = await fetch('/api/wallet/get', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    token: getSessionToken()
                })
            });
            
            const result = await response.json();
            
            if (result.status === 'success' && result.data) {
                const data = result.data;
                
                if (data.encrypted_seed) {
                    // Store locally
                    localStorage.setItem(CONFIG.storageKeys.encryptedSeed, data.encrypted_seed);
                    localStorage.setItem(CONFIG.storageKeys.addresses, data.addresses || '{}');
                    
                    // Load user tokens if exists
                    if (data.user_tokens) {
                        try {
                            state.userTokens = JSON.parse(data.user_tokens);
                            localStorage.setItem(CONFIG.storageKeys.userTokens, data.user_tokens);
                        } catch (e) {}
                    }
                    
                    // Parse addresses
                    try {
                        state.addresses = JSON.parse(data.addresses || '{}');
                    } catch (e) {
                        state.addresses = {};
                    }
                    
                    state.hasWallet = true;
                    return true;
                }
            }
            
            return false;
        } catch (error) {
            console.error('Failed to load wallet from server:', error);
            return false;
        }
    }
    
    /**
     * Sync wallet data with server (balances, tokens)
     */
    async function syncWalletToServer() {
        try {
            await fetch('/api/wallet/update', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    token: getSessionToken(),
                    balances: JSON.stringify(state.balances),
                    user_tokens: JSON.stringify(state.userTokens)
                })
            });
        } catch (error) {
            console.error('Failed to sync wallet:', error);
        }
    }
    
    /**
     * Check if wallet exists
     */
    function hasWallet() {
        return !!localStorage.getItem(CONFIG.storageKeys.encryptedSeed);
    }
    
    /**
     * Initialize wallet from storage
     */
    async function initialize() {
        if (state.initialized) return;
        
        const encryptedSeed = localStorage.getItem(CONFIG.storageKeys.encryptedSeed);
        const addressesJson = localStorage.getItem(CONFIG.storageKeys.addresses);
        
        if (encryptedSeed && addressesJson) {
            try {
                state.addresses = JSON.parse(addressesJson);
                state.hasWallet = true;
                
                // Load cached prices
                const cachedPrices = localStorage.getItem(CONFIG.storageKeys.priceCache);
                if (cachedPrices) {
                    const parsed = JSON.parse(cachedPrices);
                    state.prices = parsed.prices || {};
                    state.pricesLastUpdate = parsed.timestamp || 0;
                }
                
                // Load tx history
                const txHistory = localStorage.getItem(CONFIG.storageKeys.txHistory);
                if (txHistory) {
                    state.transactions = JSON.parse(txHistory);
                }
                
                // Load user tokens
                loadUserTokens();
                
                // Fetch fresh data in background
                fetchAllBalances();
                fetchAllTokens();
                refreshUserTokenPrices();
            } catch (e) {
                console.error('Wallet init error:', e);
            }
        } else {
            // Try to load wallet from server (cross-device sync)
            const loadedFromServer = await loadWalletFromServer();
            
            if (loadedFromServer) {
                // Wallet found on server, now loaded locally
                console.log('Wallet loaded from server');
                
                // Load user tokens
                loadUserTokens();
                
                // Fetch fresh data
                fetchAllBalances();
                fetchAllTokens();
                refreshUserTokenPrices();
            } else {
                // Even without wallet, load token list for browsing
                fetchAllTokens();
            }
        }
        
        state.initialized = true;
        updateUI();
    }
    
    /**
     * Delete wallet
     */
    async function deleteWallet() {
        // Delete from server too
        try {
            await fetch('/api/wallet/delete', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ token: getSessionToken() })
            });
        } catch (e) {}
        
        localStorage.removeItem(CONFIG.storageKeys.encryptedSeed);
        localStorage.removeItem(CONFIG.storageKeys.addresses);
        localStorage.removeItem(CONFIG.storageKeys.txHistory);
        localStorage.removeItem(CONFIG.storageKeys.userTokens);
        localStorage.removeItem(CONFIG.storageKeys.tokenBalances);
        
        state = {
            initialized: true,
            hasWallet: false,
            addresses: {},
            balances: {},
            totalBalanceUsd: 0,
            prices: {},
            pricesLastUpdate: 0,
            transactions: [],
            preferredChain: 'ethereum',
            isLoading: false,
            allTokens: state.allTokens, // Keep token list
            tokensLastUpdate: state.tokensLastUpdate,
            userTokens: [],
            tokenBalances: {},
            tokenPrices: {}
        };
        
        updateUI();
    }

    // ============= TRANSACTION HISTORY =============
    
    function addTransaction(tx) {
        state.transactions.unshift(tx);
        
        // Keep only last 100 transactions
        if (state.transactions.length > 100) {
            state.transactions = state.transactions.slice(0, 100);
        }
        
        localStorage.setItem(CONFIG.storageKeys.txHistory, JSON.stringify(state.transactions));
    }
    
    function getTransactionHistory() {
        return state.transactions;
    }

    // ============= UI HELPERS =============
    
    function formatUSD(amount) {
        return new Intl.NumberFormat('en-US', {
            style: 'currency',
            currency: 'USD',
            minimumFractionDigits: 2,
            maximumFractionDigits: 2
        }).format(amount);
    }
    
    function formatCrypto(amount, symbol) {
        const num = parseFloat(amount);
        if (num === 0) return `0 ${symbol}`;
        if (num < 0.0001) return `<0.0001 ${symbol}`;
        return `${num.toFixed(4)} ${symbol}`;
    }
    
    function shortenAddress(address, chars = 4) {
        if (!address) return '';
        return `${address.slice(0, chars + 2)}...${address.slice(-chars)}`;
    }
    
    function getSessionToken() {
        return localStorage.getItem('session_token') || '';
    }

    // ============= UI UPDATE =============
    
    function updateUI() {
        const event = new CustomEvent('walletUpdate', {
            detail: {
                hasWallet: state.hasWallet,
                addresses: state.addresses,
                balances: state.balances,
                totalBalanceUsd: state.totalBalanceUsd,
                prices: state.prices,
                isLoading: state.isLoading,
                transactions: state.transactions
            }
        });
        window.dispatchEvent(event);
    }

    // ============= UTILITY FUNCTIONS =============
    
    function bytesToHex(bytes) {
        return Array.from(bytes)
            .map(b => b.toString(16).padStart(2, '0'))
            .join('');
    }
    
    function hexToBytes(hex) {
        const bytes = [];
        for (let i = 0; i < hex.length; i += 2) {
            bytes.push(parseInt(hex.substr(i, 2), 16));
        }
        return new Uint8Array(bytes);
    }
    
    function bytesToBase64(bytes) {
        return btoa(String.fromCharCode(...bytes));
    }
    
    function base64ToBytes(base64) {
        return Uint8Array.from(atob(base64), c => c.charCodeAt(0));
    }
    
    function base64UrlEncode(bytes) {
        return btoa(String.fromCharCode(...bytes))
            .replace(/\+/g, '-')
            .replace(/\//g, '_')
            .replace(/=/g, '');
    }
    
    function bytesToBits(bytes) {
        let bits = '';
        for (const byte of bytes) {
            bits += byte.toString(2).padStart(8, '0');
        }
        return bits;
    }
    
    async function sha256Bits(bytes) {
        const hash = await crypto.subtle.digest('SHA-256', bytes);
        return bytesToBits(new Uint8Array(hash));
    }
    
    // Base58 encoding for Solana addresses
    const BASE58_ALPHABET = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz';
    
    function base58Encode(bytes) {
        if (bytes.length === 0) return '';
        
        const digits = [0];
        for (let i = 0; i < bytes.length; i++) {
            let carry = bytes[i];
            for (let j = 0; j < digits.length; j++) {
                carry += digits[j] << 8;
                digits[j] = carry % 58;
                carry = (carry / 58) | 0;
            }
            while (carry > 0) {
                digits.push(carry % 58);
                carry = (carry / 58) | 0;
            }
        }
        
        let result = '';
        for (let i = 0; i < bytes.length && bytes[i] === 0; i++) {
            result += BASE58_ALPHABET[0];
        }
        for (let i = digits.length - 1; i >= 0; i--) {
            result += BASE58_ALPHABET[digits[i]];
        }
        
        return result;
    }
    
    // BIP39 wordlist - load from external file or use cached
    let _bip39WordlistCache = null;
    
    async function fetchBIP39Wordlist() {
        // Use cached wordlist if available
        if (_bip39WordlistCache) return _bip39WordlistCache;
        
        // Check if global BIP39_WORDLIST exists (loaded from bip39-words.js)
        if (typeof BIP39_WORDLIST !== 'undefined' && BIP39_WORDLIST.length === 2048) {
            _bip39WordlistCache = BIP39_WORDLIST;
            return _bip39WordlistCache;
        }
        
        // Fallback: try to fetch from file
        try {
            const response = await fetch('/js/bip39-wordlist.txt');
            if (response.ok) {
                const text = await response.text();
                _bip39WordlistCache = text.trim().split('\n');
                if (_bip39WordlistCache.length === 2048) {
                    return _bip39WordlistCache;
                }
            }
        } catch (e) {
            console.error('Failed to fetch BIP39 wordlist:', e);
        }
        
        throw new Error('BIP39 wordlist not available. Please include bip39-words.js');
    }

    // ============= PUBLIC API =============
    return {
        // Initialization
        initialize,
        hasWallet,
        createWallet,
        importWallet,
        deleteWallet,
        resetWallet: deleteWallet,
        
        // Server sync (cross-device)
        saveWalletToServer,
        loadWalletFromServer,
        syncWalletToServer,
        
        // State
        getState: () => ({ ...state }),
        getAddresses: () => ({ ...state.addresses }),
        getBalances: () => ({ ...state.balances }),
        getTotalBalanceUsd: () => state.totalBalanceUsd,
        getPrices: () => ({ ...state.prices }),
        
        // Seed phrase access (with password verification)
        getSeedPhrase: async (password) => {
            try {
                const encrypted = localStorage.getItem(CONFIG.storageKeys.encryptedSeed);
                if (!encrypted) return null;
                const decrypted = await decryptSeed(encrypted, password);
                return decrypted;
            } catch (e) {
                return null;
            }
        },
        
        // Transactions
        sendTransaction,
        getTransactionHistory,
        
        // Chat integration
        parseMoneyTransfer,
        sendMoneyToUser,
        
        // Swap
        getSwapQuote,
        executeSwap,
        
        // Prices
        fetchPrices,
        fetchAllBalances,
        
        // Token management (NEW)
        fetchAllTokens,
        searchTokens,
        getToken,
        addUserToken,
        removeUserToken,
        updateTokenAmount,
        getUserTokens: () => [...state.userTokens],
        getTokenBalances: () => ({ ...state.tokenBalances }),
        getTokenPrices: () => ({ ...state.tokenPrices }),
        getAllTokens: () => [...state.allTokens],
        refreshUserTokenPrices,
        
        // Utilities
        formatUSD,
        formatCrypto,
        shortenAddress,
        
        // Config
        getChains: () => ({ ...CONFIG.chains }),
        getStablecoins: () => ({ ...CONFIG.stablecoins })
    };
})();

// Auto-initialize when DOM ready
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => XipherWallet.initialize());
} else {
    XipherWallet.initialize();
}

// Export for module systems
if (typeof module !== 'undefined' && module.exports) {
    module.exports = XipherWallet;
}
