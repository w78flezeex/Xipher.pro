/**
 * Xipher Wallet UI Component
 * Handles all wallet UI interactions and rendering
 */

console.log('[WalletUI] Loading module...');

const XipherWalletUI = (() => {
    'use strict';
    
    console.log('[WalletUI] Inside IIFE...');

    let currentModal = null;
    let currentView = 'main'; // main, setup, send, receive, swap, history

    // ============= UI ELEMENTS =============
    
    function createWalletPanel() {
        const panel = document.createElement('div');
        panel.id = 'wallet-panel';
        panel.className = 'wallet-panel';
        panel.innerHTML = `
            <div class="wallet-header">
                <button class="wallet-back-btn" onclick="XipherWalletUI.close()" title="Назад">←</button>
                <h2>Кошелёк</h2>
                <div class="wallet-header-actions">
                    <button class="wallet-header-btn" onclick="XipherWalletUI.showScan()" title="Сканировать QR">📷</button>
                    <button class="wallet-header-btn" onclick="XipherWalletUI.showSettings()" title="Настройки">⚙️</button>
                </div>
            </div>
            <div id="wallet-content" class="wallet-content"></div>
        `;
        document.body.appendChild(panel);
        
        // Overlay
        const overlay = document.createElement('div');
        overlay.id = 'wallet-panel-overlay';
        overlay.className = 'wallet-panel-overlay';
        overlay.onclick = () => close();
        document.body.appendChild(overlay);
        
        return panel;
    }
    
    function createModalOverlay() {
        const overlay = document.createElement('div');
        overlay.id = 'wallet-modal-overlay';
        overlay.className = 'wallet-modal-overlay';
        overlay.onclick = (e) => {
            if (e.target === overlay) closeModal();
        };
        document.body.appendChild(overlay);
        return overlay;
    }

    // ============= RENDER FUNCTIONS =============
    
    function renderWalletContent() {
        const content = document.getElementById('wallet-content');
        if (!content) return;
        
        if (!XipherWallet.hasWallet()) {
            content.innerHTML = renderSetupView();
        } else {
            const state = XipherWallet.getState();
            content.innerHTML = renderMainView(state);
        }
        
        bindEvents();
    }
    
    function renderSetupView() {
        return `
            <div class="wallet-setup">
                <div class="wallet-setup-icon">💎</div>
                <h3>Создайте крипто-кошелёк</h3>
                <p>Храните и отправляйте криптовалюту прямо в чате. Все балансы в долларах США.</p>
                
                <button class="wallet-setup-btn primary" onclick="XipherWalletUI.showCreateWallet()">
                    ✨ Создать новый кошелёк
                </button>
                <button class="wallet-setup-btn secondary" onclick="XipherWalletUI.showImportWallet()">
                    📥 Импортировать из seed-фразы
                </button>
                
                <div class="wallet-security-warning">
                    <span class="icon">⚠️</span>
                    <p>Ваши ключи хранятся только локально и зашифрованы паролем. Никогда не передавайте seed-фразу третьим лицам.</p>
                </div>
            </div>
        `;
    }
    
    function renderMainView(state) {
        const totalBalance = XipherWallet.formatUSD(state.totalBalanceUsd || 0);
        const change24h = calculateTotalChange(state);
        const changeClass = change24h >= 0 ? 'positive' : 'negative';
        const changeIcon = change24h >= 0 ? '↑' : '↓';
        
        return `
            <div class="wallet-balance-card">
                <div class="wallet-balance-label">Общий баланс</div>
                <div class="wallet-balance-amount">
                    <span class="currency">$</span>
                    <span>${formatBalanceNumber(state.totalBalanceUsd || 0)}</span>
                </div>
                <div class="wallet-balance-change ${changeClass}">
                    <span>${changeIcon} ${Math.abs(change24h).toFixed(2)}%</span>
                    <span>за 24ч</span>
                </div>
            </div>
            
            <!-- Main Actions - TG Wallet Style -->
            <div class="wallet-actions">
                <button class="wallet-action-btn" onclick="XipherWalletUI.showSend()">
                    <span class="icon">↑</span>
                    <span>Отправить</span>
                </button>
                <button class="wallet-action-btn receive" onclick="XipherWalletUI.showReceive()">
                    <span class="icon">↓</span>
                    <span>Получить</span>
                </button>
                <button class="wallet-action-btn swap" onclick="XipherWalletUI.showSwap()">
                    <span class="icon">⇄</span>
                    <span>Обмен</span>
                </button>
                <button class="wallet-action-btn buy" onclick="XipherWalletUI.showBuy()">
                    <span class="icon">💳</span>
                    <span>Купить</span>
                </button>
            </div>
            
            <!-- Secondary Row -->
            <div class="wallet-actions-row">
                <button class="wallet-action-mini" onclick="XipherWalletUI.showEarn()">
                    <span>📈</span> Earn
                </button>
                <button class="wallet-action-mini" onclick="XipherWalletUI.showVouchers()">
                    <span>🎁</span> Чеки
                </button>
                <button class="wallet-action-mini" onclick="XipherWalletUI.showNFT()">
                    <span>🖼</span> NFT
                </button>
                <button class="wallet-action-mini" onclick="XipherWalletUI.showHistory()">
                    <span>📋</span> История
                </button>
            </div>
            
            <!-- Assets -->
            <div class="wallet-assets">
                <div class="wallet-section-header">
                    <span>Мои активы</span>
                    <button class="wallet-add-btn" onclick="XipherWalletUI.showAddToken()">+</button>
                </div>
                ${renderAssetsList(state)}
                ${renderUserTokensList(state)}
            </div>
        `;
    }
    
    function renderAssetsList(state) {
        const chains = XipherWallet.getChains();
        let html = '';
        
        for (const [chainId, chain] of Object.entries(chains)) {
            const balance = state.balances[chainId] || { native: '0', usd: 0 };
            const address = state.addresses[chainId] || '';
            
            html += `
                <div class="wallet-asset-item" onclick="XipherWalletUI.showAssetDetails('${chainId}')">
                    <div class="wallet-asset-icon ${chainId}">
                        ${chain.icon}
                    </div>
                    <div class="wallet-asset-info">
                        <div class="wallet-asset-name">
                            ${chain.symbol}
                            ${chainId === 'polygon' ? '<span style="font-size:0.7rem;opacity:0.6">(EVM)</span>' : ''}
                        </div>
                        <div class="wallet-asset-network">${chain.name}</div>
                    </div>
                    <div class="wallet-asset-balance">
                        <div class="wallet-asset-usd">${XipherWallet.formatUSD(balance.usd)}</div>
                        <div class="wallet-asset-crypto">${XipherWallet.formatCrypto(balance.native, chain.symbol)}</div>
                    </div>
                </div>
            `;
        }
        
        return html;
    }
    
    function renderUserTokensList(state) {
        const userTokens = XipherWallet.getUserTokens();
        const tokenBalances = XipherWallet.getTokenBalances();
        
        if (userTokens.length === 0) return '';
        
        let html = '<div class="wallet-user-tokens">';
        
        for (const token of userTokens) {
            const balance = tokenBalances[token.id] || { amount: 0, usd: 0 };
            
            html += `
                <div class="wallet-asset-item" onclick="XipherWalletUI.showTokenDetails('${token.id}')">
                    <div class="wallet-asset-icon token-icon">
                        <img src="${token.image}" alt="${token.symbol}" onerror="this.style.display='none'">
                    </div>
                    <div class="wallet-asset-info">
                        <div class="wallet-asset-name">${token.symbol}</div>
                        <div class="wallet-asset-network">${token.name}</div>
                    </div>
                    <div class="wallet-asset-balance">
                        <div class="wallet-asset-usd">${XipherWallet.formatUSD(balance.usd)}</div>
                        <div class="wallet-asset-crypto">${balance.amount.toFixed(4)} ${token.symbol}</div>
                    </div>
                </div>
            `;
        }
        
        html += '</div>';
        return html;
    }
    
    function renderMarketPreview() {
        const allTokens = XipherWallet.getAllTokens();
        
        if (allTokens.length === 0) {
            return '<div class="wallet-loading"><div class="wallet-loading-spinner"></div><span>Загрузка рынка...</span></div>';
        }
        
        // Show top 10 tokens
        const topTokens = allTokens.slice(0, 10);
        
        let html = '<div class="wallet-market-list">';
        
        for (const token of topTokens) {
            const changeClass = token.priceChange24h >= 0 ? 'positive' : 'negative';
            const changeIcon = token.priceChange24h >= 0 ? '↑' : '↓';
            
            html += `
                <div class="wallet-market-item" onclick="XipherWalletUI.showTokenInfo('${token.id}')">
                    <div class="wallet-market-rank">#${token.marketCapRank || '-'}</div>
                    <div class="wallet-market-icon">
                        <img src="${token.image}" alt="${token.symbol}" onerror="this.textContent='🪙'">
                    </div>
                    <div class="wallet-market-info">
                        <div class="wallet-market-name">${token.symbol}</div>
                        <div class="wallet-market-fullname">${token.name}</div>
                    </div>
                    <div class="wallet-market-price">
                        <div class="wallet-market-usd">${formatPrice(token.currentPrice)}</div>
                        <div class="wallet-market-change ${changeClass}">${changeIcon} ${Math.abs(token.priceChange24h || 0).toFixed(2)}%</div>
                    </div>
                </div>
            `;
        }
        
        html += `
            <button class="wallet-view-all-btn" onclick="XipherWalletUI.showAllTokens()">
                Показать все криптовалюты →
            </button>
        </div>`;
        
        return html;
    }
    
    function formatPrice(price) {
        if (!price) return '$0.00';
        if (price >= 1000) return '$' + price.toLocaleString('en-US', { maximumFractionDigits: 0 });
        if (price >= 1) return '$' + price.toFixed(2);
        if (price >= 0.01) return '$' + price.toFixed(4);
        return '$' + price.toFixed(6);
    }

    // ============= MODAL VIEWS =============
    
    function showAddToken() {
        showModal('add-token', `
            <div class="wallet-modal-header">
                <h3>Добавить криптовалюту</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-search-container">
                    <input type="text" id="token-search-input" 
                        placeholder="Поиск по названию или символу..."
                        oninput="XipherWalletUI.onTokenSearch(this.value)"
                        style="width:100%;padding:0.85rem;background:rgba(255,255,255,0.05);border:1px solid rgba(255,255,255,0.1);border-radius:12px;color:var(--text-primary);font-size:1rem;">
                </div>
                <div id="token-search-results" class="wallet-token-list">
                    ${renderTokenSearchResults('')}
                </div>
            </div>
        `);
        
        setTimeout(() => document.getElementById('token-search-input')?.focus(), 100);
    }
    
    function renderTokenSearchResults(query) {
        const tokens = XipherWallet.searchTokens(query);
        const userTokens = XipherWallet.getUserTokens();
        const userTokenIds = new Set(userTokens.map(t => t.id));
        
        if (tokens.length === 0) {
            return '<div style="text-align:center;padding:2rem;color:var(--text-secondary);">Ничего не найдено</div>';
        }
        
        let html = '';
        for (const token of tokens) {
            const isAdded = userTokenIds.has(token.id);
            const changeClass = token.priceChange24h >= 0 ? 'positive' : 'negative';
            
            html += `
                <div class="wallet-token-search-item ${isAdded ? 'added' : ''}">
                    <div class="wallet-token-icon">
                        <img src="${token.image}" alt="${token.symbol}" onerror="this.textContent='🪙'">
                    </div>
                    <div class="wallet-token-info">
                        <div class="wallet-token-name">${token.symbol}</div>
                        <div class="wallet-token-fullname">${token.name}</div>
                    </div>
                    <div class="wallet-token-price-info">
                        <div class="wallet-token-price">${formatPrice(token.currentPrice)}</div>
                        <div class="wallet-token-change ${changeClass}">${token.priceChange24h >= 0 ? '+' : ''}${(token.priceChange24h || 0).toFixed(2)}%</div>
                    </div>
                    <button class="wallet-token-add-btn ${isAdded ? 'added' : ''}" 
                            onclick="XipherWalletUI.toggleToken('${token.id}')"
                            ${isAdded ? 'disabled' : ''}>
                        ${isAdded ? '✓' : '+'}
                    </button>
                </div>
            `;
        }
        
        return html;
    }
    
    function onTokenSearch(query) {
        const resultsContainer = document.getElementById('token-search-results');
        if (resultsContainer) {
            resultsContainer.innerHTML = renderTokenSearchResults(query);
        }
    }
    
    function toggleToken(tokenId) {
        const userTokens = XipherWallet.getUserTokens();
        const exists = userTokens.find(t => t.id === tokenId);
        
        if (exists) {
            XipherWallet.removeUserToken(tokenId);
        } else {
            // Show amount input modal
            showTokenAmountInput(tokenId);
        }
    }
    
    function showTokenAmountInput(tokenId) {
        const token = XipherWallet.getToken(tokenId);
        if (!token) return;
        
        showModal('token-amount', `
            <div class="wallet-modal-header">
                <h3>Добавить ${token.symbol}</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body" style="text-align:center;">
                <div class="wallet-token-icon-large">
                    <img src="${token.image}" alt="${token.symbol}" style="width:64px;height:64px;border-radius:50%;">
                </div>
                <h4 style="margin:1rem 0 0.5rem;">${token.name}</h4>
                <p style="color:var(--text-secondary);font-size:0.9rem;margin-bottom:1.5rem;">
                    Текущая цена: ${formatPrice(token.currentPrice)}
                </p>
                
                <div style="margin-bottom:1rem;">
                    <label style="display:block;margin-bottom:0.5rem;color:var(--text-secondary);font-size:0.9rem;">
                        Количество ${token.symbol}
                    </label>
                    <input type="number" id="token-amount-input" 
                        placeholder="0.00"
                        step="any"
                        min="0"
                        style="width:100%;padding:1rem;background:rgba(255,255,255,0.05);border:1px solid rgba(255,255,255,0.1);border-radius:12px;color:var(--text-primary);font-size:1.5rem;text-align:center;">
                    <div id="token-amount-usd" style="margin-top:0.5rem;color:var(--text-secondary);font-size:0.9rem;">
                        ≈ $0.00
                    </div>
                </div>
                
                <button class="wallet-send-btn" onclick="XipherWalletUI.confirmAddToken('${tokenId}')">
                    Добавить в кошелёк
                </button>
            </div>
        `);
        
        const input = document.getElementById('token-amount-input');
        if (input) {
            input.focus();
            input.addEventListener('input', () => {
                const amount = parseFloat(input.value) || 0;
                const usdValue = amount * (token.currentPrice || 0);
                document.getElementById('token-amount-usd').textContent = `≈ ${XipherWallet.formatUSD(usdValue)}`;
            });
        }
    }
    
    function confirmAddToken(tokenId) {
        const input = document.getElementById('token-amount-input');
        const amount = parseFloat(input?.value) || 0;
        
        XipherWallet.addUserToken(tokenId, amount);
        closeModal();
        renderWalletContent();
        
        showNotification(`${XipherWallet.getToken(tokenId)?.symbol || 'Токен'} добавлен в кошелёк`);
    }
    
    function showTokenDetails(tokenId) {
        const token = XipherWallet.getToken(tokenId);
        const userTokens = XipherWallet.getUserTokens();
        const userToken = userTokens.find(t => t.id === tokenId);
        const balance = XipherWallet.getTokenBalances()[tokenId] || { amount: 0, usd: 0 };
        
        if (!token) return;
        
        showModal('token-details', `
            <div class="wallet-modal-header">
                <h3>${token.symbol}</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body" style="text-align:center;">
                <div class="wallet-token-icon-large">
                    <img src="${token.image}" alt="${token.symbol}" style="width:80px;height:80px;border-radius:50%;">
                </div>
                <h4 style="margin:1rem 0;">${token.name}</h4>
                
                <div class="wallet-balance-card" style="margin:1rem 0;">
                    <div class="wallet-balance-label">Ваш баланс</div>
                    <div class="wallet-balance-amount">
                        <span class="currency">$</span>
                        <span>${balance.usd.toLocaleString('en-US', { minimumFractionDigits: 2, maximumFractionDigits: 2 })}</span>
                    </div>
                    <div style="font-size:0.9rem;color:var(--text-secondary);margin-top:0.5rem;">
                        ${balance.amount.toFixed(6)} ${token.symbol}
                    </div>
                </div>
                
                <div style="display:flex;gap:0.75rem;margin-top:1.5rem;">
                    <button class="wallet-setup-btn secondary" style="flex:1;" onclick="XipherWalletUI.editTokenAmount('${tokenId}')">
                        ✏️ Изменить
                    </button>
                    <button class="wallet-setup-btn secondary" style="flex:1;border-color:rgba(239,68,68,0.5);color:#ef4444;" onclick="XipherWalletUI.removeToken('${tokenId}')">
                        🗑️ Удалить
                    </button>
                </div>
            </div>
        `);
    }
    
    function editTokenAmount(tokenId) {
        const token = XipherWallet.getToken(tokenId);
        const balance = XipherWallet.getTokenBalances()[tokenId] || { amount: 0 };
        
        showModal('edit-token-amount', `
            <div class="wallet-modal-header">
                <h3>Изменить ${token?.symbol || ''}</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div style="margin-bottom:1rem;">
                    <label style="display:block;margin-bottom:0.5rem;color:var(--text-secondary);font-size:0.9rem;">
                        Количество ${token?.symbol || ''}
                    </label>
                    <input type="number" id="edit-token-amount-input" 
                        value="${balance.amount}"
                        step="any"
                        min="0"
                        style="width:100%;padding:1rem;background:rgba(255,255,255,0.05);border:1px solid rgba(255,255,255,0.1);border-radius:12px;color:var(--text-primary);font-size:1.5rem;text-align:center;">
                </div>
                
                <button class="wallet-send-btn" onclick="XipherWalletUI.saveTokenAmount('${tokenId}')">
                    Сохранить
                </button>
            </div>
        `);
    }
    
    function saveTokenAmount(tokenId) {
        const input = document.getElementById('edit-token-amount-input');
        const amount = parseFloat(input?.value) || 0;
        
        XipherWallet.updateTokenAmount(tokenId, amount);
        closeModal();
        renderWalletContent();
    }
    
    function removeToken(tokenId) {
        const token = XipherWallet.getToken(tokenId);
        if (confirm(`Удалить ${token?.symbol || 'токен'} из кошелька?`)) {
            XipherWallet.removeUserToken(tokenId);
            closeModal();
            renderWalletContent();
        }
    }
    
    function showAllTokens() {
        showModal('all-tokens', `
            <div class="wallet-modal-header">
                <h3>Все криптовалюты</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body" style="padding:0;">
                <div style="padding:1rem;position:sticky;top:0;background:var(--bg-primary);z-index:10;">
                    <input type="text" id="all-tokens-search" 
                        placeholder="Поиск..."
                        oninput="XipherWalletUI.onAllTokensSearch(this.value)"
                        style="width:100%;padding:0.85rem;background:rgba(255,255,255,0.05);border:1px solid rgba(255,255,255,0.1);border-radius:12px;color:var(--text-primary);font-size:1rem;">
                </div>
                <div id="all-tokens-list" class="wallet-all-tokens-list">
                    ${renderAllTokensList('')}
                </div>
            </div>
        `, 'large');
    }
    
    function renderAllTokensList(query) {
        const tokens = query ? XipherWallet.searchTokens(query) : XipherWallet.getAllTokens();
        
        let html = '';
        for (const token of tokens) {
            const changeClass = token.priceChange24h >= 0 ? 'positive' : 'negative';
            const marketCapFormatted = token.marketCap ? formatMarketCap(token.marketCap) : '-';
            
            html += `
                <div class="wallet-all-tokens-item" onclick="XipherWalletUI.showTokenInfo('${token.id}')">
                    <div class="wallet-token-rank">${token.marketCapRank || '-'}</div>
                    <div class="wallet-token-icon">
                        <img src="${token.image}" alt="${token.symbol}">
                    </div>
                    <div class="wallet-token-info">
                        <div class="wallet-token-name">${token.symbol}</div>
                        <div class="wallet-token-fullname">${token.name}</div>
                    </div>
                    <div class="wallet-token-stats">
                        <div class="wallet-token-price">${formatPrice(token.currentPrice)}</div>
                        <div class="wallet-token-change ${changeClass}">${token.priceChange24h >= 0 ? '+' : ''}${(token.priceChange24h || 0).toFixed(2)}%</div>
                    </div>
                    <div class="wallet-token-mcap">
                        ${marketCapFormatted}
                    </div>
                </div>
            `;
        }
        
        return html;
    }
    
    function formatMarketCap(cap) {
        if (cap >= 1e12) return '$' + (cap / 1e12).toFixed(2) + 'T';
        if (cap >= 1e9) return '$' + (cap / 1e9).toFixed(2) + 'B';
        if (cap >= 1e6) return '$' + (cap / 1e6).toFixed(2) + 'M';
        return '$' + cap.toLocaleString();
    }
    
    function onAllTokensSearch(query) {
        const container = document.getElementById('all-tokens-list');
        if (container) {
            container.innerHTML = renderAllTokensList(query);
        }
    }
    
    function showTokenInfo(tokenId) {
        const token = XipherWallet.getToken(tokenId);
        if (!token) return;
        
        const changeClass = token.priceChange24h >= 0 ? 'positive' : 'negative';
        const userTokens = XipherWallet.getUserTokens();
        const isAdded = userTokens.find(t => t.id === tokenId);
        
        showModal('token-info', `
            <div class="wallet-modal-header">
                <h3>${token.symbol}</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body" style="text-align:center;">
                <div class="wallet-token-icon-large">
                    <img src="${token.image}" alt="${token.symbol}" style="width:80px;height:80px;border-radius:50%;">
                </div>
                <h4 style="margin:1rem 0 0.25rem;">${token.name}</h4>
                <div style="color:var(--text-secondary);font-size:0.9rem;margin-bottom:1rem;">
                    Рейтинг: #${token.marketCapRank || '-'}
                </div>
                
                <div class="wallet-token-price-large" style="font-size:2rem;font-weight:700;margin:1rem 0;">
                    ${formatPrice(token.currentPrice)}
                </div>
                <div class="wallet-token-change-large ${changeClass}" style="font-size:1.1rem;margin-bottom:1.5rem;">
                    ${token.priceChange24h >= 0 ? '↑' : '↓'} ${Math.abs(token.priceChange24h || 0).toFixed(2)}% за 24ч
                </div>
                
                <div class="wallet-token-stats-grid" style="display:grid;grid-template-columns:1fr 1fr;gap:1rem;text-align:left;margin-bottom:1.5rem;">
                    <div class="wallet-stat-item" style="background:rgba(255,255,255,0.03);padding:0.85rem;border-radius:12px;">
                        <div style="font-size:0.8rem;color:var(--text-secondary);margin-bottom:0.25rem;">Капитализация</div>
                        <div style="font-weight:600;">${formatMarketCap(token.marketCap)}</div>
                    </div>
                    <div class="wallet-stat-item" style="background:rgba(255,255,255,0.03);padding:0.85rem;border-radius:12px;">
                        <div style="font-size:0.8rem;color:var(--text-secondary);margin-bottom:0.25rem;">Объём 24ч</div>
                        <div style="font-weight:600;">${formatMarketCap(token.volume24h)}</div>
                    </div>
                    <div class="wallet-stat-item" style="background:rgba(255,255,255,0.03);padding:0.85rem;border-radius:12px;">
                        <div style="font-size:0.8rem;color:var(--text-secondary);margin-bottom:0.25rem;">ATH</div>
                        <div style="font-weight:600;">${formatPrice(token.ath)}</div>
                    </div>
                    <div class="wallet-stat-item" style="background:rgba(255,255,255,0.03);padding:0.85rem;border-radius:12px;">
                        <div style="font-size:0.8rem;color:var(--text-secondary);margin-bottom:0.25rem;">В обращении</div>
                        <div style="font-weight:600;">${token.circulatingSupply ? formatSupply(token.circulatingSupply) : '-'}</div>
                    </div>
                </div>
                
                ${isAdded ? `
                    <button class="wallet-setup-btn secondary" style="width:100%;" onclick="XipherWalletUI.showTokenDetails('${tokenId}')">
                        📊 Мой баланс
                    </button>
                ` : `
                    <button class="wallet-send-btn" onclick="XipherWalletUI.showTokenAmountInput('${tokenId}')">
                        + Добавить в кошелёк
                    </button>
                `}
            </div>
        `);
    }
    
    function formatSupply(supply) {
        if (supply >= 1e9) return (supply / 1e9).toFixed(2) + 'B';
        if (supply >= 1e6) return (supply / 1e6).toFixed(2) + 'M';
        if (supply >= 1e3) return (supply / 1e3).toFixed(2) + 'K';
        return supply.toLocaleString();
    }
    
    async function refreshMarket() {
        const marketSection = document.querySelector('.wallet-market-section');
        if (marketSection) {
            marketSection.querySelector('.wallet-market-list, .wallet-loading').innerHTML = 
                '<div class="wallet-loading"><div class="wallet-loading-spinner"></div></div>';
        }
        
        await XipherWallet.fetchAllTokens(true);
        renderWalletContent();
    }
    
    function showNotification(message) {
        const notif = document.createElement('div');
        notif.className = 'wallet-notification';
        notif.textContent = message;
        notif.style.cssText = `
            position: fixed;
            bottom: 20px;
            left: 50%;
            transform: translateX(-50%);
            background: linear-gradient(135deg, #2AABEE 0%, #229ED9 100%);
            color: white;
            padding: 0.85rem 1.5rem;
            border-radius: 12px;
            font-weight: 500;
            z-index: 10001;
            box-shadow: 0 4px 20px rgba(42, 171, 238, 0.4);
            animation: slideUp 0.3s ease;
        `;
        document.body.appendChild(notif);
        
        setTimeout(() => {
            notif.style.animation = 'slideDown 0.3s ease';
            setTimeout(() => notif.remove(), 300);
        }, 2500);
    }
    
    function showCreateWallet() {
        showModal('create-wallet', `
            <div class="wallet-modal-header">
                <h3>Создать кошелёк</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <p style="color:var(--text-secondary);margin-bottom:1rem;">
                    Придумайте надёжный пароль для защиты кошелька
                </p>
                
                <div style="margin-bottom:1rem;">
                    <label style="display:block;margin-bottom:0.5rem;color:var(--text-secondary);font-size:0.9rem;">Пароль</label>
                    <input type="password" id="wallet-create-password" 
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);font-size:1rem;"
                        placeholder="Минимум 8 символов">
                </div>
                
                <div style="margin-bottom:1.5rem;">
                    <label style="display:block;margin-bottom:0.5rem;color:var(--text-secondary);font-size:0.9rem;">Подтвердите пароль</label>
                    <input type="password" id="wallet-create-password-confirm" 
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);font-size:1rem;"
                        placeholder="Повторите пароль">
                </div>
                
                <button class="wallet-send-btn" onclick="XipherWalletUI.createWallet()">
                    Создать кошелёк
                </button>
            </div>
        `);
    }
    
    function showSeedPhrase(mnemonic) {
        const words = mnemonic.split(' ');
        const seedGrid = words.map((word, i) => 
            `<div class="wallet-seed-word"><span>${i + 1}.</span> ${word}</div>`
        ).join('');
        
        showModal('seed-phrase', `
            <div class="wallet-modal-header">
                <h3>🔐 Seed-фраза</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-security-warning">
                    <span class="icon">⚠️</span>
                    <p><strong>ВАЖНО!</strong> Запишите эти слова и храните в безопасном месте. Это единственный способ восстановить кошелёк.</p>
                </div>
                
                <div class="wallet-seed-grid">
                    ${seedGrid}
                </div>
                
                <button class="wallet-copy-btn" onclick="XipherWalletUI.copySeedPhrase('${mnemonic}')">
                    📋 Скопировать
                </button>
                
                <button class="wallet-send-btn" style="margin-top:1rem;" onclick="XipherWalletUI.confirmSeedSaved()">
                    ✓ Я сохранил seed-фразу
                </button>
            </div>
        `);
    }
    
    function showImportWallet() {
        showModal('import-wallet', `
            <div class="wallet-modal-header">
                <h3>Импорт кошелька</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <p style="color:var(--text-secondary);margin-bottom:1rem;">
                    Введите 12 или 24 слова seed-фразы
                </p>
                
                <div style="margin-bottom:1rem;">
                    <textarea id="wallet-import-seed" 
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);font-size:0.95rem;min-height:100px;resize:none;font-family:inherit;"
                        placeholder="word1 word2 word3 ..."></textarea>
                </div>
                
                <div style="margin-bottom:1rem;">
                    <label style="display:block;margin-bottom:0.5rem;color:var(--text-secondary);font-size:0.9rem;">Пароль для шифрования</label>
                    <input type="password" id="wallet-import-password" 
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);font-size:1rem;"
                        placeholder="Минимум 8 символов">
                </div>
                
                <button class="wallet-send-btn" onclick="XipherWalletUI.importWallet()">
                    Импортировать
                </button>
            </div>
        `);
    }
    
    function showSend(recipientUsername = '', amount = '') {
        const state = XipherWallet.getState();
        const chains = XipherWallet.getChains();
        
        const chainOptions = Object.entries(chains).map(([id, chain]) => `
            <div class="wallet-chain-option ${id === 'polygon' ? 'selected' : ''}" 
                 data-chain="${id}" onclick="XipherWalletUI.selectChain('${id}')">
                <span>${chain.icon}</span>
                <span>${chain.symbol}</span>
            </div>
        `).join('');
        
        showModal('send', `
            <div class="wallet-modal-header">
                <h3>Отправить</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div style="margin-bottom:1rem;">
                    <label style="display:block;margin-bottom:0.5rem;color:var(--text-secondary);font-size:0.9rem;">Получатель</label>
                    <input type="text" id="wallet-send-recipient" 
                        value="${recipientUsername}"
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);font-size:1rem;"
                        placeholder="@username или адрес кошелька">
                </div>
                
                <div class="wallet-amount-input-wrapper">
                    <div class="wallet-amount-display">
                        <span class="currency-sign">$</span>
                        <input type="number" id="wallet-send-amount" class="wallet-amount-input" 
                            value="${amount}" placeholder="0.00" step="0.01" min="0">
                    </div>
                    <div class="wallet-amount-crypto-equiv" id="wallet-crypto-equiv">
                        ≈ 0.00 MATIC
                    </div>
                    <div class="wallet-quick-amounts">
                        <button class="wallet-quick-amount" onclick="XipherWalletUI.setAmount(10)">$10</button>
                        <button class="wallet-quick-amount" onclick="XipherWalletUI.setAmount(25)">$25</button>
                        <button class="wallet-quick-amount" onclick="XipherWalletUI.setAmount(50)">$50</button>
                        <button class="wallet-quick-amount" onclick="XipherWalletUI.setAmount(100)">$100</button>
                    </div>
                </div>
                
                <div class="wallet-chain-selector">
                    <label>Сеть</label>
                    <div class="wallet-chain-options">
                        ${chainOptions}
                    </div>
                </div>
                
                <div style="margin:1rem 0;">
                    <label style="display:block;margin-bottom:0.5rem;color:var(--text-secondary);font-size:0.9rem;">Пароль кошелька</label>
                    <input type="password" id="wallet-send-password" 
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);font-size:1rem;"
                        placeholder="Введите пароль">
                </div>
                
                <button class="wallet-send-btn" id="wallet-send-btn" onclick="XipherWalletUI.executeSend()">
                    ↑ Отправить
                </button>
            </div>
        `);
        
        // Bind amount change
        document.getElementById('wallet-send-amount')?.addEventListener('input', updateCryptoEquiv);
    }
    
    function showReceive() {
        const state = XipherWallet.getState();
        const chains = XipherWallet.getChains();
        
        const addresses = Object.entries(chains).map(([id, chain]) => {
            const address = state.addresses[id] || '';
            return `
                <div style="margin-bottom:1.25rem;">
                    <div style="display:flex;align-items:center;gap:0.5rem;margin-bottom:0.5rem;">
                        <span>${chain.icon}</span>
                        <strong>${chain.name}</strong>
                    </div>
                    <div class="wallet-address-display">${address || 'Не создан'}</div>
                    <button class="wallet-copy-btn" onclick="XipherWalletUI.copyAddress('${address}')">
                        📋 Скопировать
                    </button>
                </div>
            `;
        }).join('');
        
        showModal('receive', `
            <div class="wallet-modal-header">
                <h3>Получить</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-receive-content">
                    <p style="color:var(--text-secondary);margin-bottom:1.5rem;text-align:center;">
                        Выберите сеть и скопируйте адрес
                    </p>
                    ${addresses}
                </div>
            </div>
        `);
    }
    
    let swapFromToken = { id: 'ethereum', symbol: 'ETH', icon: 'Ξ', price: 0 };
    let swapToToken = { id: 'tether', symbol: 'USDT', icon: '💵', price: 1 };
    
    function showSwap() {
        const allTokens = XipherWallet.getAllTokens().slice(0, 30);
        const state = XipherWallet.getState();
        const fromBalance = state.balances[swapFromToken.id]?.native || '0';
        
        showModal('swap', `
            <div class="wallet-modal-header">
                <button class="wallet-back-btn" onclick="XipherWalletUI.closeModal()">←</button>
                <h3>Обмен криптовалюты</h3>
                <div style="width:38px;"></div>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-swap-card">
                    <div class="wallet-swap-row">
                        <div class="wallet-swap-label">
                            <span>Отдаёте</span>
                            <span class="wallet-swap-balance">Баланс: ${fromBalance} ${swapFromToken.symbol}</span>
                        </div>
                        <div class="wallet-swap-input-row">
                            <input type="number" class="wallet-swap-input" id="swap-from-amount" 
                                placeholder="0.0" oninput="XipherWalletUI.updateSwapOutput()">
                            <button class="wallet-swap-token-btn" id="swap-from-token" onclick="XipherWalletUI.showSwapTokenSelect('from')">
                                <span class="swap-token-icon">${swapFromToken.icon}</span>
                                <span>${swapFromToken.symbol}</span>
                                <span class="arrow">▼</span>
                            </button>
                        </div>
                        <button class="wallet-swap-max-btn" onclick="XipherWalletUI.setSwapMax()">MAX</button>
                    </div>
                </div>
                
                <div class="wallet-swap-divider">
                    <div class="wallet-swap-arrow" onclick="XipherWalletUI.swapDirection()">
                        <svg width="20" height="20" viewBox="0 0 24 24" fill="currentColor">
                            <path d="M16 17.01V10h-2v7.01h-3L15 21l4-3.99h-3zM9 3L5 6.99h3V14h2V6.99h3L9 3z"/>
                        </svg>
                    </div>
                </div>
                
                <div class="wallet-swap-card">
                    <div class="wallet-swap-row">
                        <div class="wallet-swap-label">
                            <span>Получаете</span>
                            <span class="wallet-swap-balance">~</span>
                        </div>
                        <div class="wallet-swap-input-row">
                            <input type="number" class="wallet-swap-input" id="swap-to-amount" 
                                placeholder="0.0" readonly>
                            <button class="wallet-swap-token-btn" id="swap-to-token" onclick="XipherWalletUI.showSwapTokenSelect('to')">
                                <span class="swap-token-icon">${swapToToken.icon}</span>
                                <span>${swapToToken.symbol}</span>
                                <span class="arrow">▼</span>
                            </button>
                        </div>
                    </div>
                </div>
                
                <div class="wallet-swap-details" id="swap-details">
                    <div class="wallet-swap-detail-row">
                        <span>Курс обмена</span>
                        <span id="swap-rate">1 ${swapFromToken.symbol} ≈ ? ${swapToToken.symbol}</span>
                    </div>
                    <div class="wallet-swap-detail-row">
                        <span>Комиссия сети</span>
                        <span id="swap-fee">~$2.50</span>
                    </div>
                    <div class="wallet-swap-detail-row">
                        <span>Проскальзывание</span>
                        <div class="wallet-slippage-btns">
                            <button class="wallet-slippage-btn active" data-slippage="0.5">0.5%</button>
                            <button class="wallet-slippage-btn" data-slippage="1">1%</button>
                            <button class="wallet-slippage-btn" data-slippage="3">3%</button>
                        </div>
                    </div>
                </div>
                
                <div style="margin:1rem 0;">
                    <input type="password" id="wallet-swap-password" 
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);font-size:1rem;"
                        placeholder="Пароль кошелька">
                </div>
                
                <button class="wallet-send-btn swap-btn" id="swap-execute-btn" onclick="XipherWalletUI.executeSwap()">
                    ⇄ Обменять
                </button>
                
                <div class="wallet-swap-provider">
                    <span>Лучший курс от</span>
                    <div class="wallet-swap-providers">
                        <span class="provider active">1inch</span>
                        <span class="provider">Uniswap</span>
                        <span class="provider">0x</span>
                    </div>
                </div>
            </div>
        `, 'large');
        
        // Bind slippage buttons
        document.querySelectorAll('.wallet-slippage-btn').forEach(btn => {
            btn.onclick = function() {
                document.querySelectorAll('.wallet-slippage-btn').forEach(b => b.classList.remove('active'));
                this.classList.add('active');
            };
        });
        
        updateSwapRate();
    }
    
    function showSwapTokenSelect(direction) {
        const allTokens = XipherWallet.getAllTokens().slice(0, 50);
        const chains = XipherWallet.getChains();
        
        // Include native chains first
        let tokenList = Object.entries(chains).map(([id, chain]) => `
            <div class="wallet-swap-token-item" onclick="XipherWalletUI.selectSwapToken('${direction}', '${id}', '${chain.symbol}', '${chain.icon}', ${chain.coinGeckoId ? `'${chain.coinGeckoId}'` : 'null'})">
                <div class="wallet-swap-token-icon">${chain.icon}</div>
                <div class="wallet-swap-token-info">
                    <div class="symbol">${chain.symbol}</div>
                    <div class="name">${chain.name}</div>
                </div>
            </div>
        `).join('');
        
        // Add top tokens from CoinGecko
        tokenList += allTokens.slice(0, 20).map(t => `
            <div class="wallet-swap-token-item" onclick="XipherWalletUI.selectSwapToken('${direction}', '${t.id}', '${t.symbol}', '', '${t.id}')">
                <div class="wallet-swap-token-icon">
                    <img src="${t.image}" alt="${t.symbol}">
                </div>
                <div class="wallet-swap-token-info">
                    <div class="symbol">${t.symbol}</div>
                    <div class="name">${t.name}</div>
                </div>
                <div class="wallet-swap-token-price">${formatPrice(t.currentPrice)}</div>
            </div>
        `).join('');
        
        showModal('swap-token-select', `
            <div class="wallet-modal-header">
                <button class="wallet-back-btn" onclick="XipherWalletUI.showSwap()">←</button>
                <h3>Выберите токен</h3>
                <div style="width:38px;"></div>
            </div>
            <div class="wallet-modal-body" style="padding:0;">
                <div style="padding:1rem;">
                    <input type="text" id="swap-token-search" placeholder="Поиск токенов..."
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);">
                </div>
                <div class="wallet-swap-token-list">
                    ${tokenList}
                </div>
            </div>
        `);
    }
    
    function selectSwapToken(direction, id, symbol, icon, geckoId) {
        const token = { id, symbol, icon: icon || '🪙', geckoId };
        
        // Get price from CoinGecko data
        const allTokens = XipherWallet.getAllTokens();
        const foundToken = allTokens.find(t => t.id === geckoId);
        token.price = foundToken?.currentPrice || 0;
        
        if (direction === 'from') {
            swapFromToken = token;
        } else {
            swapToToken = token;
        }
        
        showSwap();
    }
    
    function swapDirection() {
        const temp = swapFromToken;
        swapFromToken = swapToToken;
        swapToToken = temp;
        showSwap();
    }
    
    function setSwapMax() {
        const state = XipherWallet.getState();
        const balance = state.balances[swapFromToken.id]?.native || '0';
        document.getElementById('swap-from-amount').value = balance;
        updateSwapOutput();
    }
    
    function updateSwapOutput() {
        const fromAmount = parseFloat(document.getElementById('swap-from-amount')?.value) || 0;
        
        // Get prices
        const fromPrice = swapFromToken.price || getTokenPrice(swapFromToken.id);
        const toPrice = swapToToken.price || getTokenPrice(swapToToken.id) || 1;
        
        if (fromAmount > 0 && fromPrice > 0 && toPrice > 0) {
            const usdValue = fromAmount * fromPrice;
            const toAmount = usdValue / toPrice;
            document.getElementById('swap-to-amount').value = toAmount.toFixed(6);
        } else {
            document.getElementById('swap-to-amount').value = '';
        }
        
        updateSwapRate();
    }
    
    function updateSwapRate() {
        const fromPrice = swapFromToken.price || getTokenPrice(swapFromToken.id);
        const toPrice = swapToToken.price || getTokenPrice(swapToToken.id) || 1;
        
        if (fromPrice > 0 && toPrice > 0) {
            const rate = fromPrice / toPrice;
            const rateEl = document.getElementById('swap-rate');
            if (rateEl) {
                rateEl.textContent = `1 ${swapFromToken.symbol} ≈ ${rate.toFixed(4)} ${swapToToken.symbol}`;
            }
        }
    }
    
    function getTokenPrice(tokenId) {
        const prices = XipherWallet.getPrices();
        if (prices[tokenId]) return prices[tokenId].usd;
        
        const allTokens = XipherWallet.getAllTokens();
        const token = allTokens.find(t => t.id === tokenId);
        return token?.currentPrice || 0;
    }
    
    function showHistory() {
        const transactions = XipherWallet.getTransactionHistory();
        
        let txList = '';
        if (transactions.length === 0) {
            txList = `
                <div style="text-align:center;padding:2rem;color:var(--text-secondary);">
                    <div style="font-size:2rem;margin-bottom:0.5rem;">📭</div>
                    <p>Нет транзакций</p>
                </div>
            `;
        } else {
            txList = transactions.map(tx => {
                const isSent = tx.type === 'sent';
                const icon = isSent ? '↑' : '↓';
                const iconClass = isSent ? 'sent' : 'received';
                const amountClass = isSent ? 'sent' : 'received';
                const sign = isSent ? '-' : '+';
                const title = isSent ? 'Отправлено' : 'Получено';
                const date = new Date(tx.timestamp).toLocaleDateString('ru-RU', {
                    day: 'numeric',
                    month: 'short',
                    hour: '2-digit',
                    minute: '2-digit'
                });
                
                return `
                    <div class="wallet-tx-item">
                        <div class="wallet-tx-icon ${iconClass}">${icon}</div>
                        <div class="wallet-tx-info">
                            <div class="wallet-tx-title">${title}</div>
                            <div class="wallet-tx-subtitle">${date} • ${tx.chain}</div>
                        </div>
                        <div class="wallet-tx-amount ${amountClass}">
                            <div>${sign}${XipherWallet.formatUSD(tx.amountUsd || tx.amount)}</div>
                            <div class="wallet-tx-amount-crypto">${tx.amount} ${tx.chain.toUpperCase()}</div>
                        </div>
                    </div>
                `;
            }).join('');
        }
        
        showModal('history', `
            <div class="wallet-modal-header">
                <h3>История</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body" style="padding:0;">
                <div class="wallet-history-filters">
                    <button class="wallet-filter-btn active" data-filter="all">Все</button>
                    <button class="wallet-filter-btn" data-filter="sent">Отправлено</button>
                    <button class="wallet-filter-btn" data-filter="received">Получено</button>
                    <button class="wallet-filter-btn" data-filter="swap">Обмен</button>
                </div>
                <div class="wallet-tx-list">
                    ${txList}
                </div>
            </div>
        `);
    }
    
    // ============= BUY CRYPTO =============
    
    function showBuy() {
        const currencies = [
            { id: 'bitcoin', symbol: 'BTC', name: 'Bitcoin', icon: '₿' },
            { id: 'ethereum', symbol: 'ETH', name: 'Ethereum', icon: 'Ξ' },
            { id: 'tether', symbol: 'USDT', name: 'Tether', icon: '💵' },
            { id: 'the-open-network', symbol: 'TON', name: 'Toncoin', icon: '💎' },
            { id: 'matic-network', symbol: 'MATIC', name: 'Polygon', icon: '⬡' },
            { id: 'solana', symbol: 'SOL', name: 'Solana', icon: '◎' }
        ];
        
        const currencyList = currencies.map(c => `
            <div class="wallet-buy-currency-item" onclick="XipherWalletUI.selectBuyCurrency('${c.id}')">
                <span class="wallet-buy-currency-icon">${c.icon}</span>
                <div class="wallet-buy-currency-info">
                    <div class="wallet-buy-currency-symbol">${c.symbol}</div>
                    <div class="wallet-buy-currency-name">${c.name}</div>
                </div>
            </div>
        `).join('');
        
        showModal('buy', `
            <div class="wallet-modal-header">
                <h3>💳 Купить крипту</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-buy-tabs">
                    <button class="wallet-buy-tab active" data-tab="card">Картой</button>
                </div>
                
                <div id="buy-card-content">
                    <div class="wallet-buy-amount-section">
                        <label>Сумма покупки</label>
                        <div class="wallet-buy-amount-input">
                            <input type="number" id="buy-amount" placeholder="1000" value="1000">
                            <select id="buy-fiat-currency">
                                <option value="RUB">₽ RUB</option>
                                <option value="USD">$ USD</option>
                                <option value="EUR">€ EUR</option>
                            </select>
                        </div>
                        <div class="wallet-buy-quick-amounts">
                            <button onclick="XipherWalletUI.setBuyAmount(500)">500₽</button>
                            <button onclick="XipherWalletUI.setBuyAmount(1000)">1000₽</button>
                            <button onclick="XipherWalletUI.setBuyAmount(5000)">5000₽</button>
                            <button onclick="XipherWalletUI.setBuyAmount(10000)">10000₽</button>
                        </div>
                    </div>
                    
                    <div class="wallet-buy-currency-section">
                        <label>Что покупаем</label>
                        <div class="wallet-buy-currency-list">
                            ${currencyList}
                        </div>
                    </div>
                    
                    <div class="wallet-buy-summary" id="buy-summary" style="display:none;">
                        <div class="wallet-buy-summary-row">
                            <span>Вы получите:</span>
                            <span id="buy-receive-amount">~0.00 BTC</span>
                        </div>
                        <div class="wallet-buy-summary-row">
                            <span>Комиссия:</span>
                            <span>2.5%</span>
                        </div>
                    </div>
                    
                    <button class="wallet-send-btn" id="buy-submit-btn" disabled onclick="XipherWalletUI.processBuy()">
                        Продолжить
                    </button>
                    
                    <div class="wallet-buy-providers">
                        <span>Партнёры:</span>
                        <div class="wallet-buy-provider-logos">
                            <span>Mercuryo</span>
                            <span>MoonPay</span>
                            <span>Simplex</span>
                        </div>
                    </div>
                </div>
            </div>
        `);
        
        bindBuyEvents();
    }
    
    let selectedBuyCurrency = null;
    
    function selectBuyCurrency(currencyId) {
        selectedBuyCurrency = currencyId;
        document.querySelectorAll('.wallet-buy-currency-item').forEach(el => {
            el.classList.remove('selected');
        });
        event.currentTarget.classList.add('selected');
        
        document.getElementById('buy-summary').style.display = 'block';
        document.getElementById('buy-submit-btn').disabled = false;
        
        updateBuyEstimate();
    }
    
    function setBuyAmount(amount) {
        document.getElementById('buy-amount').value = amount;
        updateBuyEstimate();
    }
    
    function updateBuyEstimate() {
        if (!selectedBuyCurrency) return;
        
        const amount = parseFloat(document.getElementById('buy-amount')?.value) || 0;
        const fiat = document.getElementById('buy-fiat-currency')?.value || 'RUB';
        
        // Simplified rate calculation
        const rates = {
            'bitcoin': { RUB: 0.0000001, USD: 0.00001, EUR: 0.000011 },
            'ethereum': { RUB: 0.000003, USD: 0.0003, EUR: 0.00033 },
            'tether': { RUB: 0.01, USD: 1, EUR: 1.1 },
            'the-open-network': { RUB: 0.002, USD: 0.2, EUR: 0.22 },
            'matic-network': { RUB: 0.01, USD: 1, EUR: 1.1 },
            'solana': { RUB: 0.00005, USD: 0.005, EUR: 0.0055 }
        };
        
        const symbols = {
            'bitcoin': 'BTC', 'ethereum': 'ETH', 'tether': 'USDT',
            'the-open-network': 'TON', 'matic-network': 'MATIC', 'solana': 'SOL'
        };
        
        const rate = rates[selectedBuyCurrency]?.[fiat] || 0;
        const cryptoAmount = (amount * rate * 0.975).toFixed(6); // 2.5% fee
        
        document.getElementById('buy-receive-amount').textContent = `~${cryptoAmount} ${symbols[selectedBuyCurrency]}`;
    }
    
    function bindBuyEvents() {
        document.getElementById('buy-amount')?.addEventListener('input', updateBuyEstimate);
        document.getElementById('buy-fiat-currency')?.addEventListener('change', updateBuyEstimate);
    }
    
    function processBuy() {
        showToast('Перенаправление к платёжному провайдеру...', 'info');
        // In production, redirect to Mercuryo/MoonPay
    }
    
    // ============= P2P MARKETPLACE (DISABLED) =============
    
    let p2pCurrentCrypto = 'USDT';
    let p2pCurrentFiat = 'RUB';
    let p2pCurrentType = 'buy';
    let p2pOffers = [];
    
    async function showP2P() {
        showModal('p2p', `
            <div class="wallet-modal-header">
                <h3>👥 P2P Маркет</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body" style="padding:0;">
                <div class="wallet-p2p-tabs">
                    <button class="wallet-p2p-tab active" data-type="buy" onclick="XipherWalletUI.switchP2PTab('buy')">Купить</button>
                    <button class="wallet-p2p-tab" data-type="sell" onclick="XipherWalletUI.switchP2PTab('sell')">Продать</button>
                    <button class="wallet-p2p-tab" data-type="my" onclick="XipherWalletUI.showMyP2PTrades()">Мои ордера</button>
                </div>
                
                <div class="wallet-p2p-filters">
                    <select class="wallet-p2p-filter" id="p2p-crypto-filter" onchange="XipherWalletUI.filterP2POffers()">
                        <option value="USDT">USDT</option>
                        <option value="BTC">BTC</option>
                        <option value="ETH">ETH</option>
                        <option value="TON">TON</option>
                    </select>
                    <select class="wallet-p2p-filter" id="p2p-fiat-filter" onchange="XipherWalletUI.filterP2POffers()">
                        <option value="RUB">RUB</option>
                        <option value="USD">USD</option>
                        <option value="EUR">EUR</option>
                    </select>
                    <input type="number" class="wallet-p2p-amount-input" id="p2p-amount-filter" placeholder="Сумма">
                </div>
                
                <div class="wallet-p2p-offers-list" id="p2p-offers-list">
                    <div class="wallet-loading"><div class="wallet-loading-spinner"></div><span>Загрузка офферов...</span></div>
                </div>
                
                <button class="wallet-p2p-create-btn" onclick="XipherWalletUI.createP2POffer()">
                    + Создать объявление
                </button>
            </div>
        `, 'large');
        
        await loadP2POffers();
    }
    
    async function loadP2POffers() {
        try {
            const token = localStorage.getItem('session_token');
            const response = await fetch('/api/wallet/p2p/offers', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    token: token,
                    type: p2pCurrentType === 'buy' ? 'sell' : 'buy',
                    crypto: p2pCurrentCrypto,
                    fiat: p2pCurrentFiat
                })
            });
            
            const data = await response.json();
            
            if (data.success && data.offers) {
                p2pOffers = data.offers;
                renderP2POffers(data.offers);
            } else {
                renderP2POffers([]);
            }
        } catch (error) {
            console.error('P2P offers error:', error);
            renderP2POffers([]);
        }
    }
    
    function renderP2POffers(offers) {
        const container = document.getElementById('p2p-offers-list');
        if (!container) return;
        
        if (offers.length === 0) {
            container.innerHTML = `
                <div class="wallet-p2p-empty">
                    <div style="font-size:2.5rem;margin-bottom:1rem;">📭</div>
                    <p>Нет доступных офферов</p>
                    <button class="wallet-setup-btn secondary" onclick="XipherWalletUI.createP2POffer()">
                        Создать первый оффер
                    </button>
                </div>
            `;
            return;
        }
        
        const offersList = offers.map(offer => `
            <div class="wallet-p2p-offer" onclick="XipherWalletUI.showP2POfferDetails('${offer.id}')">
                <div class="wallet-p2p-seller">
                    <div class="wallet-p2p-avatar">${(offer.user?.username || 'U')[0].toUpperCase()}</div>
                    <div class="wallet-p2p-seller-info">
                        <div class="wallet-p2p-username">${offer.user?.username || 'Trader'}</div>
                        <div class="wallet-p2p-stats">
                            <span>⭐ ${(offer.user?.rating || 0).toFixed(1)}</span>
                            <span>• ${offer.user?.trades || 0} сделок</span>
                        </div>
                    </div>
                </div>
                <div class="wallet-p2p-offer-details">
                    <div class="wallet-p2p-price">
                        <span class="wallet-p2p-price-value">${parseFloat(offer.price).toLocaleString()}</span>
                        <span class="wallet-p2p-price-currency">${offer.fiat}/${offer.crypto}</span>
                    </div>
                    <div class="wallet-p2p-limits">
                        ${parseFloat(offer.min).toLocaleString()} - ${parseFloat(offer.max).toLocaleString()} ${offer.fiat}
                    </div>
                    <div class="wallet-p2p-methods">
                        ${Array.isArray(offer.methods) ? offer.methods.join(', ') : 'Перевод'}
                    </div>
                </div>
                <button class="wallet-p2p-buy-btn">${p2pCurrentType === 'buy' ? 'Купить' : 'Продать'}</button>
            </div>
        `).join('');
        
        container.innerHTML = offersList;
    }
    
    function switchP2PTab(type) {
        p2pCurrentType = type;
        document.querySelectorAll('.wallet-p2p-tab').forEach(tab => {
            tab.classList.toggle('active', tab.dataset.type === type);
        });
        loadP2POffers();
    }
    
    function filterP2POffers() {
        p2pCurrentCrypto = document.getElementById('p2p-crypto-filter')?.value || 'USDT';
        p2pCurrentFiat = document.getElementById('p2p-fiat-filter')?.value || 'RUB';
        loadP2POffers();
    }
    
    async function showP2POfferDetails(offerId) {
        const offer = p2pOffers.find(o => o.id === offerId);
        if (!offer) return;
        
        showModal('p2p-details', `
            <div class="wallet-modal-header">
                <button class="wallet-back-btn" onclick="XipherWalletUI.showP2P()">←</button>
                <h3>${p2pCurrentType === 'buy' ? 'Купить' : 'Продать'} ${offer.crypto}</h3>
                <div style="width:38px;"></div>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-p2p-detail-seller">
                    <div class="wallet-p2p-avatar">${(offer.user?.username || 'T')[0]}</div>
                    <div>
                        <div class="wallet-p2p-username">${offer.user?.username || 'Trader'}</div>
                        <div class="wallet-p2p-stats">⭐ ${(offer.user?.rating || 0).toFixed(1)} • ${offer.user?.trades || 0} сделок</div>
                    </div>
                </div>
                
                <div class="wallet-p2p-detail-info">
                    <div class="wallet-p2p-detail-row">
                        <span>Цена:</span>
                        <span class="wallet-p2p-detail-price">${parseFloat(offer.price).toLocaleString()} ${offer.fiat}/${offer.crypto}</span>
                    </div>
                    <div class="wallet-p2p-detail-row">
                        <span>Лимиты:</span>
                        <span>${parseFloat(offer.min).toLocaleString()} - ${parseFloat(offer.max).toLocaleString()} ${offer.fiat}</span>
                    </div>
                    <div class="wallet-p2p-detail-row">
                        <span>Доступно:</span>
                        <span>${parseFloat(offer.available).toFixed(4)} ${offer.crypto}</span>
                    </div>
                </div>
                
                <div class="wallet-p2p-trade-form">
                    <label>Сумма в ${offer.fiat}</label>
                    <input type="number" id="p2p-trade-amount" placeholder="${offer.min}" 
                        min="${offer.min}" max="${offer.max}"
                        style="width:100%;padding:1rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:12px;color:var(--text-primary);font-size:1.2rem;">
                    
                    <div class="wallet-p2p-receive-estimate" id="p2p-receive-estimate" style="margin-top:0.5rem;text-align:right;color:var(--text-secondary);">
                        Вы получите: ~0.00 ${offer.crypto}
                    </div>
                    
                    <label style="margin-top:1rem;">Способ оплаты</label>
                    <select id="p2p-payment-method" style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);">
                        ${(Array.isArray(offer.methods) ? offer.methods : ['Перевод']).map(m => `<option value="${m}">${m}</option>`).join('')}
                    </select>
                </div>
                
                <button class="wallet-send-btn" style="margin-top:1.5rem;" onclick="XipherWalletUI.startP2PTrade('${offerId}')">
                    ${p2pCurrentType === 'buy' ? 'Купить' : 'Продать'} ${offer.crypto}
                </button>
            </div>
        `);
        
        // Bind amount input
        document.getElementById('p2p-trade-amount')?.addEventListener('input', (e) => {
            const amount = parseFloat(e.target.value) || 0;
            const cryptoAmount = amount / parseFloat(offer.price);
            document.getElementById('p2p-receive-estimate').textContent = 
                `Вы получите: ~${cryptoAmount.toFixed(6)} ${offer.crypto}`;
        });
    }
    
    async function startP2PTrade(offerId) {
        const amount = document.getElementById('p2p-trade-amount')?.value;
        const paymentMethod = document.getElementById('p2p-payment-method')?.value;
        
        if (!amount || parseFloat(amount) <= 0) {
            showToast('Укажите сумму', 'error');
            return;
        }
        
        try {
            const token = localStorage.getItem('session_token');
            const response = await fetch('/api/wallet/p2p/start-trade', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    token: token,
                    offer_id: offerId,
                    amount: amount,
                    payment_method: paymentMethod
                })
            });
            
            const data = await response.json();
            
            if (data.success) {
                showToast('Сделка создана! Ожидайте подтверждения продавца.', 'success');
                showMyP2PTrades();
            } else {
                showToast(data.error || 'Ошибка создания сделки', 'error');
            }
        } catch (error) {
            showToast('Ошибка сети', 'error');
        }
    }
    
    async function showMyP2PTrades() {
        const container = document.getElementById('p2p-offers-list');
        if (!container) return;
        
        container.innerHTML = '<div class="wallet-loading"><div class="wallet-loading-spinner"></div><span>Загрузка сделок...</span></div>';
        
        try {
            const token = localStorage.getItem('session_token');
            const response = await fetch('/api/wallet/p2p/my-trades', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ token: token })
            });
            
            const data = await response.json();
            
            if (data.success && data.trades && data.trades.length > 0) {
                const tradesList = data.trades.map(trade => {
                    const statusLabels = {
                        'pending': '⏳ Ожидание',
                        'confirmed': '✓ Подтверждено',
                        'paid': '💵 Оплачено',
                        'completed': '✅ Завершено',
                        'cancelled': '❌ Отменено',
                        'disputed': '⚠️ Спор'
                    };
                    
                    return `
                        <div class="wallet-p2p-trade" onclick="XipherWalletUI.showTradeDetails('${trade.id}')">
                            <div class="wallet-p2p-trade-info">
                                <div class="wallet-p2p-trade-partner">${trade.role === 'buyer' ? '← Покупка у' : '→ Продажа'} ${trade.partner}</div>
                                <div class="wallet-p2p-trade-amount">${parseFloat(trade.crypto_amount).toFixed(6)} ${trade.crypto}</div>
                                <div class="wallet-p2p-trade-fiat">${parseFloat(trade.fiat_amount).toLocaleString()} ${trade.fiat}</div>
                            </div>
                            <div class="wallet-p2p-trade-status ${trade.status}">${statusLabels[trade.status] || trade.status}</div>
                        </div>
                    `;
                }).join('');
                
                container.innerHTML = tradesList;
            } else {
                container.innerHTML = `
                    <div class="wallet-p2p-empty">
                        <div style="font-size:2.5rem;margin-bottom:1rem;">📋</div>
                        <p>У вас пока нет сделок</p>
                    </div>
                `;
            }
        } catch (error) {
            container.innerHTML = '<p style="text-align:center;color:var(--text-secondary);">Ошибка загрузки</p>';
        }
    }
    
    function createP2POffer() {
        showModal('create-p2p-offer', `
            <div class="wallet-modal-header">
                <button class="wallet-back-btn" onclick="XipherWalletUI.showP2P()">←</button>
                <h3>Создать объявление</h3>
                <div style="width:38px;"></div>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-form-group">
                    <label>Тип объявления</label>
                    <div class="wallet-toggle-group">
                        <button class="wallet-toggle-btn active" data-type="sell" onclick="this.parentElement.querySelectorAll('.wallet-toggle-btn').forEach(b=>b.classList.remove('active'));this.classList.add('active');">Продаю</button>
                        <button class="wallet-toggle-btn" data-type="buy" onclick="this.parentElement.querySelectorAll('.wallet-toggle-btn').forEach(b=>b.classList.remove('active'));this.classList.add('active');">Покупаю</button>
                    </div>
                </div>
                
                <div class="wallet-form-row">
                    <div class="wallet-form-group">
                        <label>Криптовалюта</label>
                        <select id="offer-crypto" style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);">
                            <option value="USDT">USDT</option>
                            <option value="BTC">BTC</option>
                            <option value="ETH">ETH</option>
                            <option value="TON">TON</option>
                        </select>
                    </div>
                    <div class="wallet-form-group">
                        <label>Валюта</label>
                        <select id="offer-fiat" style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);">
                            <option value="RUB">RUB</option>
                            <option value="USD">USD</option>
                            <option value="EUR">EUR</option>
                        </select>
                    </div>
                </div>
                
                <div class="wallet-form-group">
                    <label>Цена за 1 единицу</label>
                    <input type="number" id="offer-price" placeholder="93.5" step="0.01"
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);">
                </div>
                
                <div class="wallet-form-row">
                    <div class="wallet-form-group">
                        <label>Мин. сумма</label>
                        <input type="number" id="offer-min" placeholder="1000"
                            style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);">
                    </div>
                    <div class="wallet-form-group">
                        <label>Макс. сумма</label>
                        <input type="number" id="offer-max" placeholder="100000"
                            style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);">
                    </div>
                </div>
                
                <div class="wallet-form-group">
                    <label>Доступное количество</label>
                    <input type="number" id="offer-available" placeholder="1000" step="0.0001"
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);">
                </div>
                
                <div class="wallet-form-group">
                    <label>Способы оплаты (через запятую)</label>
                    <input type="text" id="offer-methods" placeholder="Сбербанк, Тинькофф"
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);">
                </div>
                
                <button class="wallet-send-btn" style="margin-top:1rem;" onclick="XipherWalletUI.submitP2POffer()">
                    Опубликовать объявление
                </button>
            </div>
        `);
    }
    
    async function submitP2POffer() {
        const offerType = document.querySelector('.wallet-toggle-btn.active')?.dataset.type || 'sell';
        const crypto = document.getElementById('offer-crypto')?.value;
        const fiat = document.getElementById('offer-fiat')?.value;
        const price = document.getElementById('offer-price')?.value;
        const min = document.getElementById('offer-min')?.value;
        const max = document.getElementById('offer-max')?.value;
        const available = document.getElementById('offer-available')?.value;
        const methods = document.getElementById('offer-methods')?.value;
        
        if (!price || !min || !max || !available) {
            showToast('Заполните все обязательные поля', 'error');
            return;
        }
        
        try {
            const token = localStorage.getItem('session_token');
            const response = await fetch('/api/wallet/p2p/create-offer', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    token: token,
                    type: offerType,
                    crypto: crypto,
                    fiat: fiat,
                    price: price,
                    min: min,
                    max: max,
                    available: available,
                    methods: JSON.stringify(methods.split(',').map(m => m.trim()))
                })
            });
            
            const data = await response.json();
            
            if (data.success) {
                showToast('Объявление опубликовано!', 'success');
                showP2P();
            } else {
                showToast(data.error || 'Ошибка публикации', 'error');
            }
        } catch (error) {
            showToast('Ошибка сети', 'error');
        }
    }
    
    // ============= EARN / STAKING =============
    
    let stakingProducts = [];
    let userStakes = [];
    
    async function showEarn() {
        showModal('earn', `
            <div class="wallet-modal-header">
                <h3>📈 Earn</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-earn-summary" id="earn-summary">
                    <div class="wallet-earn-total">
                        <div class="wallet-earn-total-label">Общий доход</div>
                        <div class="wallet-earn-total-value">$0.00</div>
                    </div>
                    <div class="wallet-earn-active">
                        <div class="wallet-earn-active-label">Активных стейков</div>
                        <div class="wallet-earn-active-value">0</div>
                    </div>
                </div>
                
                <div class="wallet-earn-title">Доступные опции</div>
                
                <div class="wallet-earn-list" id="earn-products-list">
                    <div class="wallet-loading"><div class="wallet-loading-spinner"></div><span>Загрузка...</span></div>
                </div>
                
                <p class="wallet-earn-disclaimer">
                    ⚠️ Стейкинг криптовалюты связан с рисками. Доходность не гарантирована.
                </p>
            </div>
        `, 'large');
        
        await loadStakingData();
    }
    
    async function loadStakingData() {
        try {
            // Load products
            const productsResponse = await fetch('/api/wallet/staking/products');
            const productsData = await productsResponse.json();
            
            if (productsData.success && productsData.products) {
                stakingProducts = productsData.products;
                renderStakingProducts(productsData.products);
            }
            
            // Load user stakes
            const token = localStorage.getItem('session_token');
            if (token) {
                const stakesResponse = await fetch('/api/wallet/staking/my-stakes', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ token: token })
                });
                
                const stakesData = await stakesResponse.json();
                
                if (stakesData.success) {
                    userStakes = stakesData.stakes || [];
                    updateEarnSummary(stakesData);
                }
            }
        } catch (error) {
            console.error('Staking load error:', error);
            renderStakingProducts([]);
        }
    }
    
    function updateEarnSummary(data) {
        const totalEl = document.querySelector('.wallet-earn-total-value');
        const activeEl = document.querySelector('.wallet-earn-active-value');
        
        if (totalEl) totalEl.textContent = `$${(data.total_rewards_usd || 0).toFixed(2)}`;
        if (activeEl) activeEl.textContent = (data.stakes?.length || 0).toString();
    }
    
    function renderStakingProducts(products) {
        const container = document.getElementById('earn-products-list');
        if (!container) return;
        
        if (products.length === 0) {
            container.innerHTML = '<p style="text-align:center;color:var(--text-secondary);">Нет доступных продуктов</p>';
            return;
        }
        
        const icons = {
            'TON': '💎', 'ETH': 'Ξ', 'SOL': '◎', 'USDT': '💵', 'BTC': '₿'
        };
        
        const stakingList = products.map(opt => `
            <div class="wallet-earn-option" onclick="XipherWalletUI.showStakingDetails('${opt.id}')">
                <div class="wallet-earn-icon">${icons[opt.crypto] || '🪙'}</div>
                <div class="wallet-earn-info">
                    <div class="wallet-earn-name">${opt.name}</div>
                    <div class="wallet-earn-min">Мин: ${opt.min_amount} ${opt.crypto}</div>
                </div>
                <div class="wallet-earn-apy">
                    <div class="wallet-earn-apy-value">${opt.apy}%</div>
                    <div class="wallet-earn-apy-label">APY</div>
                </div>
                ${opt.lock_days > 0 ? '<span class="wallet-earn-locked">🔒</span>' : ''}
            </div>
        `).join('');
        
        container.innerHTML = stakingList;
    }
    
    function showStakingDetails(stakingId) {
        const product = stakingProducts.find(p => p.id === stakingId);
        if (!product) {
            showToast('Продукт не найден', 'error');
            return;
        }
        
        const icons = { 'TON': '💎', 'ETH': 'Ξ', 'SOL': '◎', 'USDT': '💵', 'BTC': '₿' };
        const icon = icons[product.crypto] || '🪙';
        
        showModal('staking-details', `
            <div class="wallet-modal-header">
                <button class="wallet-back-btn" onclick="XipherWalletUI.showEarn()">←</button>
                <h3>${icon} ${product.name}</h3>
                <div style="width:38px;"></div>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-staking-apy-card">
                    <div class="wallet-staking-apy-value">${product.apy}%</div>
                    <div class="wallet-staking-apy-label">Годовая доходность (APY)</div>
                </div>
                
                <p style="color:var(--text-secondary);margin:1rem 0;">${product.description || ''}</p>
                
                ${product.lock_days > 0 ? `<div class="wallet-staking-lock-info">🔒 Период блокировки: ${product.lock_days} дней</div>` : ''}
                
                <div class="wallet-staking-calculator">
                    <label>Сумма для стейкинга (${product.crypto})</label>
                    <input type="number" id="staking-amount" placeholder="${product.min_amount}" min="${product.min_amount}" step="0.0001"
                        style="width:100%;padding:1rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:12px;color:var(--text-primary);font-size:1.2rem;text-align:center;margin:0.5rem 0;">
                    
                    <div class="wallet-staking-estimate" id="staking-estimate">
                        <div class="wallet-staking-estimate-row">
                            <span>Ежедневно:</span>
                            <span id="staking-daily">~0.0000 ${product.crypto}</span>
                        </div>
                        <div class="wallet-staking-estimate-row">
                            <span>Ежемесячно:</span>
                            <span id="staking-monthly">~0.00 ${product.crypto}</span>
                        </div>
                        <div class="wallet-staking-estimate-row">
                            <span>Ежегодно:</span>
                            <span id="staking-yearly">~0.00 ${product.crypto}</span>
                        </div>
                    </div>
                </div>
                
                <div class="wallet-staking-total-info" style="padding:1rem;background:var(--bg-secondary);border-radius:12px;margin:1rem 0;">
                    <div style="display:flex;justify-content:space-between;">
                        <span style="color:var(--text-secondary);">Всего застейкано:</span>
                        <span>${parseFloat(product.total_staked || 0).toFixed(2)} ${product.crypto}</span>
                    </div>
                </div>
                
                <button class="wallet-send-btn" onclick="XipherWalletUI.startStaking('${stakingId}')">
                    Начать стейкинг
                </button>
            </div>
        `);
        
        document.getElementById('staking-amount')?.addEventListener('input', (e) => {
            const amount = parseFloat(e.target.value) || 0;
            const yearlyReward = amount * (product.apy / 100);
            document.getElementById('staking-daily').textContent = `~${(yearlyReward / 365).toFixed(6)} ${product.crypto}`;
            document.getElementById('staking-monthly').textContent = `~${(yearlyReward / 12).toFixed(4)} ${product.crypto}`;
            document.getElementById('staking-yearly').textContent = `~${yearlyReward.toFixed(4)} ${product.crypto}`;
        });
    }
    
    async function startStaking(stakingId) {
        const amount = document.getElementById('staking-amount')?.value;
        const product = stakingProducts.find(p => p.id === stakingId);
        
        if (!amount || parseFloat(amount) <= 0) {
            showToast('Укажите сумму', 'error');
            return;
        }
        
        if (product && parseFloat(amount) < product.min_amount) {
            showToast(`Минимум: ${product.min_amount} ${product.crypto}`, 'error');
            return;
        }
        
        showLoading('Отправка транзакции...');
        
        try {
            let result;
            
            // Use real staking protocols via XipherWeb3
            if (typeof XipherWeb3 !== 'undefined') {
                if (product.crypto === 'ETH') {
                    result = await XipherWeb3.stakeETHLido(parseFloat(amount));
                } else if (product.crypto === 'MATIC' || product.crypto === 'POL') {
                    result = await XipherWeb3.stakeMATICLido(parseFloat(amount));
                } else if (product.crypto === 'USDT' || product.crypto === 'USDC') {
                    result = await XipherWeb3.supplyToAave(product.crypto, parseFloat(amount));
                } else {
                    // Fallback to backend API
                    result = await stakingViaBackend(stakingId, amount);
                }
            } else {
                result = await stakingViaBackend(stakingId, amount);
            }
            
            hideLoading();
            
            if (result.success) {
                showToast(`✅ Стейкинг ${amount} ${product?.crypto || ''} выполнен! TX: ${result.txHash?.slice(0,10)}...`, 'success');
                showEarn();
            } else {
                showToast(result.error || 'Ошибка стейкинга', 'error');
            }
        } catch (error) {
            hideLoading();
            console.error('Staking error:', error);
            showToast(error.message || 'Ошибка транзакции', 'error');
        }
    }
    
    async function stakingViaBackend(stakingId, amount) {
        const token = localStorage.getItem('session_token');
        const response = await fetch('/api/wallet/staking/stake', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                token: token,
                product_id: stakingId,
                amount: amount
            })
        });
        return await response.json();
    }
    
    // ============= VOUCHERS / CHECKS =============
    
    function showVouchers() {
        showModal('vouchers', `
            <div class="wallet-modal-header">
                <h3>🎁 Крипто-чеки</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-voucher-tabs">
                    <button class="wallet-voucher-tab active" onclick="XipherWalletUI.showCreateVoucher()">Создать</button>
                    <button class="wallet-voucher-tab" onclick="XipherWalletUI.showActivateVoucher()">Активировать</button>
                    <button class="wallet-voucher-tab" onclick="XipherWalletUI.showMyVouchers()">Мои чеки</button>
                </div>
                
                <div id="voucher-content">
                    ${renderCreateVoucherForm()}
                </div>
            </div>
        `);
    }
    
    function renderCreateVoucherForm() {
        return `
            <div class="wallet-voucher-form">
                <div class="wallet-voucher-amount-section">
                    <label>Сумма чека</label>
                    <div class="wallet-voucher-amount-input">
                        <span class="currency-sign">$</span>
                        <input type="number" id="voucher-amount" placeholder="10.00" step="0.01">
                    </div>
                    <div class="wallet-voucher-quick-amounts">
                        <button onclick="document.getElementById('voucher-amount').value='5'">$5</button>
                        <button onclick="document.getElementById('voucher-amount').value='10'">$10</button>
                        <button onclick="document.getElementById('voucher-amount').value='25'">$25</button>
                        <button onclick="document.getElementById('voucher-amount').value='50'">$50</button>
                        <button onclick="document.getElementById('voucher-amount').value='100'">$100</button>
                    </div>
                </div>
                
                <div class="wallet-voucher-options">
                    <label>
                        <input type="checkbox" id="voucher-single-use" checked>
                        Одноразовый чек
                    </label>
                    <label>
                        <input type="checkbox" id="voucher-password">
                        Защитить паролем
                    </label>
                </div>
                
                <div id="voucher-password-field" style="display:none;">
                    <input type="text" placeholder="Пароль для активации" id="voucher-password-input"
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);margin-top:0.5rem;">
                </div>
                
                <div class="wallet-voucher-comment">
                    <label>Комментарий (опционально)</label>
                    <input type="text" id="voucher-comment" placeholder="С Днём Рождения! 🎂"
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);">
                </div>
                
                <button class="wallet-send-btn" onclick="XipherWalletUI.createVoucher()">
                    🎁 Создать чек
                </button>
            </div>
        `;
    }
    
    function showCreateVoucher() {
        document.getElementById('voucher-content').innerHTML = renderCreateVoucherForm();
        document.getElementById('voucher-password')?.addEventListener('change', (e) => {
            document.getElementById('voucher-password-field').style.display = e.target.checked ? 'block' : 'none';
        });
    }
    
    function showActivateVoucher() {
        document.getElementById('voucher-content').innerHTML = `
            <div class="wallet-voucher-activate">
                <div class="wallet-voucher-activate-icon">🎁</div>
                <p>Введите код чека или отсканируйте QR</p>
                
                <input type="text" id="voucher-code" placeholder="XXXX-XXXX-XXXX"
                    style="width:100%;padding:1rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:12px;color:var(--text-primary);font-size:1.2rem;text-align:center;letter-spacing:2px;text-transform:uppercase;margin:1rem 0;">
                
                <button class="wallet-send-btn" onclick="XipherWalletUI.activateVoucher()">
                    Активировать
                </button>
                
                <button class="wallet-setup-btn secondary" style="margin-top:0.5rem;width:100%;" onclick="XipherWalletUI.scanVoucherQR()">
                    📷 Сканировать QR
                </button>
            </div>
        `;
    }
    
    function showMyVouchers() {
        const container = document.getElementById('voucher-content');
        if (!container) return;
        
        container.innerHTML = '<div class="wallet-loading"><div class="wallet-loading-spinner"></div><span>Загрузка...</span></div>';
        
        loadMyVouchers();
    }
    
    async function loadMyVouchers() {
        const container = document.getElementById('voucher-content');
        if (!container) return;
        
        try {
            const token = localStorage.getItem('session_token');
            const response = await fetch('/api/wallet/vouchers/my', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ token: token })
            });
            
            const data = await response.json();
            
            if (data.success && data.vouchers && data.vouchers.length > 0) {
                const vouchersList = data.vouchers.map(v => {
                    const statusLabels = {
                        'active': '🟢 Активен',
                        'claimed': '✓ Использован',
                        'expired': '⏱ Истёк',
                        'revoked': '❌ Отменён'
                    };
                    
                    return `
                        <div class="wallet-voucher-item" onclick="XipherWalletUI.showVoucherDetails('${v.id}')">
                            <div class="wallet-voucher-item-info">
                                <div class="wallet-voucher-item-code">${v.code}</div>
                                <div class="wallet-voucher-item-amount">$${parseFloat(v.amount_usd).toFixed(2)} (${parseFloat(v.crypto_amount).toFixed(4)} ${v.crypto})</div>
                                <div class="wallet-voucher-item-comment">${v.comment || ''}</div>
                            </div>
                            <div class="wallet-voucher-item-status ${v.status}">${statusLabels[v.status] || v.status}</div>
                        </div>
                    `;
                }).join('');
                
                container.innerHTML = `<div class="wallet-voucher-list">${vouchersList}</div>`;
            } else {
                container.innerHTML = `
                    <div class="wallet-voucher-list-empty">
                        <div style="font-size:3rem;margin-bottom:1rem;">🎁</div>
                        <p>У вас пока нет созданных чеков</p>
                        <button class="wallet-setup-btn secondary" onclick="XipherWalletUI.showCreateVoucher()">
                            Создать первый чек
                        </button>
                    </div>
                `;
            }
        } catch (error) {
            container.innerHTML = '<p style="text-align:center;color:var(--text-secondary);">Ошибка загрузки</p>';
        }
    }
    
    async function createVoucher() {
        const amount = parseFloat(document.getElementById('voucher-amount')?.value) || 0;
        const singleUse = document.getElementById('voucher-single-use')?.checked ?? true;
        const usePassword = document.getElementById('voucher-password')?.checked;
        const password = document.getElementById('voucher-password-input')?.value || '';
        const comment = document.getElementById('voucher-comment')?.value || '';
        
        if (amount <= 0) {
            showToast('Укажите сумму чека', 'error');
            return;
        }
        
        if (amount < 1) {
            showToast('Минимальная сумма: $1', 'error');
            return;
        }
        
        try {
            showLoading('Создание чека в блокчейне...');
            
            let result;
            
            // Use real blockchain voucher via XipherWeb3
            if (typeof XipherWeb3 !== 'undefined') {
                // Create on-chain voucher (send to voucher address)
                result = await XipherWeb3.createVoucher(
                    amount,
                    null, // native token (MATIC)
                    usePassword ? password : '',
                    'polygon'
                );
            } else {
                // Fallback to backend-only voucher
                const token = localStorage.getItem('session_token');
                const response = await fetch('/api/wallet/vouchers/create', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({
                        token: token,
                        amount: amount.toString(),
                        single_use: singleUse ? 'true' : 'false',
                        password: usePassword ? password : '',
                        comment: comment
                    })
                });
                result = await response.json();
            }
            
            hideLoading();
            
            if (result.success) {
                const txInfo = result.txHash ? ` (TX: ${result.txHash.slice(0,10)}...)` : '';
                showVoucherCreatedModal(result.code, amount, 'MATIC', amount, txInfo);
            } else {
                showToast(result.error || 'Ошибка создания чека', 'error');
            }
        } catch (error) {
            hideLoading();
            console.error('Voucher error:', error);
            showToast(error.message || 'Ошибка транзакции', 'error');
        }
    }
    
    function showVoucherCreatedModal(code, amountUsd, crypto, cryptoAmount, txInfo = '') {
        showModal('voucher-created', `
            <div class="wallet-modal-header">
                <h3>✅ Чек создан!</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body" style="text-align:center;">
                <div class="wallet-voucher-created-icon">🎁</div>
                <div class="wallet-voucher-created-amount">$${parseFloat(amountUsd).toFixed(2)}</div>
                <div class="wallet-voucher-created-crypto">${parseFloat(cryptoAmount).toFixed(4)} ${crypto}</div>
                
                <div class="wallet-voucher-code-display">
                    ${code}
                </div>
                
                <div class="wallet-voucher-qr" id="voucher-qr">
                    <div style="width:150px;height:150px;background:white;margin:0 auto;display:flex;align-items:center;justify-content:center;border-radius:8px;">
                        <canvas id="voucher-qr-canvas"></canvas>
                    </div>
                </div>
                
                <div style="display:flex;gap:0.5rem;margin-top:1rem;">
                    <button class="wallet-setup-btn secondary" style="flex:1;" onclick="XipherWalletUI.copyVoucherCode('${code}')">
                        📋 Копировать
                    </button>
                    <button class="wallet-setup-btn secondary" style="flex:1;" onclick="XipherWalletUI.shareVoucher('${code}', ${amountUsd})">
                        📤 Поделиться
                    </button>
                </div>
            </div>
        `);
        
        // Generate QR code if library available
        generateVoucherQR(code);
    }
    
    function generateVoucherQR(code) {
        // Use QRCode.js if available
        if (typeof QRCode !== 'undefined') {
            const canvas = document.getElementById('voucher-qr-canvas');
            if (canvas) {
                QRCode.toCanvas(canvas, `xipher://voucher/${code}`, { width: 140 }, () => {});
            }
        }
    }
    
    function generateVoucherCode() {
        const chars = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789';
        let code = '';
        for (let i = 0; i < 12; i++) {
            if (i > 0 && i % 4 === 0) code += '-';
            code += chars[Math.floor(Math.random() * chars.length)];
        }
        return code;
    }
    
    function copyVoucherCode(code) {
        navigator.clipboard.writeText(code).then(() => {
            showToast('Код скопирован', 'success');
        });
    }
    
    function shareVoucher(code, amount) {
        if (navigator.share) {
            navigator.share({
                title: 'Крипто-чек',
                text: `🎁 Тебе крипто-чек на $${amount}!\nКод: ${code}\nАктивируй в кошельке Xipher`,
                url: `https://xipher.app/voucher/${code}`
            });
        } else {
            copyVoucherCode(code);
        }
    }
    
    async function activateVoucher() {
        const code = document.getElementById('voucher-code')?.value?.trim().toUpperCase();
        if (!code) {
            showToast('Введите код чека', 'error');
            return;
        }
        
        try {
            showLoading('Активация...');
            
            const token = localStorage.getItem('session_token');
            const response = await fetch('/api/wallet/vouchers/claim', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    token: token,
                    code: code
                })
            });
            
            const data = await response.json();
            hideLoading();
            
            if (data.success) {
                showModal('voucher-claimed', `
                    <div class="wallet-modal-header">
                        <h3>🎉 Чек активирован!</h3>
                        <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
                    </div>
                    <div class="wallet-modal-body" style="text-align:center;">
                        <div style="font-size:4rem;margin-bottom:1rem;">🎁</div>
                        <div class="wallet-voucher-created-amount">+$${parseFloat(data.amount_usd).toFixed(2)}</div>
                        <div class="wallet-voucher-created-crypto">+${parseFloat(data.crypto_amount).toFixed(4)} ${data.crypto}</div>
                        ${data.comment ? `<p style="margin-top:1rem;color:var(--text-secondary);">${data.comment}</p>` : ''}
                        <button class="wallet-send-btn" style="margin-top:1.5rem;" onclick="XipherWalletUI.closeModal();XipherWalletUI.renderWalletContent();">
                            Отлично!
                        </button>
                    </div>
                `);
            } else if (data.error === 'password_required') {
                // Show password input
                showVoucherPasswordPrompt(code);
            } else {
                showToast(data.error || 'Ошибка активации', 'error');
            }
        } catch (error) {
            hideLoading();
            showToast('Ошибка сети', 'error');
        }
    }
    
    function showVoucherPasswordPrompt(code) {
        showModal('voucher-password', `
            <div class="wallet-modal-header">
                <h3>🔐 Требуется пароль</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body" style="text-align:center;">
                <p style="color:var(--text-secondary);margin-bottom:1rem;">Этот чек защищён паролем</p>
                
                <input type="password" id="voucher-claim-password" placeholder="Введите пароль"
                    style="width:100%;padding:1rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:12px;color:var(--text-primary);font-size:1.1rem;text-align:center;">
                
                <button class="wallet-send-btn" style="margin-top:1rem;" onclick="XipherWalletUI.activateVoucherWithPassword('${code}')">
                    Активировать
                </button>
            </div>
        `);
    }
    
    async function activateVoucherWithPassword(code) {
        const password = document.getElementById('voucher-claim-password')?.value;
        if (!password) {
            showToast('Введите пароль', 'error');
            return;
        }
        
        try {
            const token = localStorage.getItem('session_token');
            const response = await fetch('/api/wallet/vouchers/claim', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    token: token,
                    code: code,
                    password: password
                })
            });
            
            const data = await response.json();
            
            if (data.success) {
                closeModal();
                showToast(`Получено ${data.crypto_amount} ${data.crypto}!`, 'success');
            } else {
                showToast(data.error || 'Неверный пароль', 'error');
            }
        } catch (error) {
            showToast('Ошибка сети', 'error');
        }
    }
    
    function scanVoucherQR() {
        showQRScanner((result) => {
            document.getElementById('voucher-code').value = result;
        });
    }
    
    // ============= NFT GALLERY =============
    
    let userNFTs = [];
    
    async function showNFT() {
        showModal('nft', `
            <div class="wallet-modal-header">
                <h3>🖼 NFT Галерея</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-nft-tabs">
                    <button class="wallet-nft-tab active" onclick="XipherWalletUI.loadMyNFTs()">Мои NFT</button>
                    <button class="wallet-nft-tab" onclick="XipherWalletUI.showNFTMarketplace()">Маркетплейс</button>
                </div>
                
                <div class="wallet-nft-chain-filter">
                    <button class="wallet-nft-chain active" data-chain="all" onclick="XipherWalletUI.filterNFTsByChain('all')">Все</button>
                    <button class="wallet-nft-chain" data-chain="ethereum" onclick="XipherWalletUI.filterNFTsByChain('ethereum')">Ethereum</button>
                    <button class="wallet-nft-chain" data-chain="polygon" onclick="XipherWalletUI.filterNFTsByChain('polygon')">Polygon</button>
                    <button class="wallet-nft-chain" data-chain="solana" onclick="XipherWalletUI.filterNFTsByChain('solana')">Solana</button>
                </div>
                
                <div class="wallet-nft-list" id="nft-list">
                    <div class="wallet-loading"><div class="wallet-loading-spinner"></div><span>Загрузка NFT...</span></div>
                </div>
                
                <button class="wallet-nft-refresh-btn" onclick="XipherWalletUI.refreshNFTs()">
                    🔄 Обновить NFT
                </button>
            </div>
        `, 'large');
        
        await loadMyNFTs();
    }
    
    async function loadMyNFTs() {
        const container = document.getElementById('nft-list');
        if (!container) return;
        
        try {
            let nfts = [];
            
            // Try real blockchain fetch first via XipherWeb3
            if (typeof XipherWeb3 !== 'undefined' && typeof XipherWallet !== 'undefined') {
                const wallet = XipherWallet.getState();
                if (wallet.isInitialized && wallet.address) {
                    nfts = await XipherWeb3.fetchNFTs(wallet.address);
                }
            }
            
            // Fallback to backend API
            if (nfts.length === 0) {
                const token = localStorage.getItem('session_token');
                const response = await fetch('/api/wallet/nft/my', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ token: token })
                });
                
                const data = await response.json();
                if (data.success && data.nfts) {
                    nfts = data.nfts;
                }
            }
            
            if (nfts.length > 0) {
                userNFTs = nfts;
                renderNFTGrid(nfts);
            } else {
                container.innerHTML = `
                    <div class="wallet-nft-empty">
                        <div class="wallet-nft-empty-icon">🖼</div>
                        <h4>У вас пока нет NFT</h4>
                        <p>NFT появятся здесь после покупки или получения</p>
                        
                        <div class="wallet-nft-supported">
                            <span>Поддерживаемые сети:</span>
                            <div class="wallet-nft-chains">
                                <span>Ethereum</span>
                                <span>Polygon</span>
                                <span>Solana</span>
                                <span>TON</span>
                            </div>
                        </div>
                    </div>
                `;
            }
        } catch (error) {
            container.innerHTML = '<p style="text-align:center;color:var(--text-secondary);">Ошибка загрузки NFT</p>';
        }
    }
    
    function renderNFTGrid(nfts) {
        const container = document.getElementById('nft-list');
        if (!container) return;
        
        if (nfts.length === 0) {
            container.innerHTML = `
                <div class="wallet-nft-empty">
                    <div class="wallet-nft-empty-icon">🖼</div>
                    <p>Нет NFT в этой сети</p>
                </div>
            `;
            return;
        }
        
        const grid = nfts.map(nft => `
            <div class="wallet-nft-card" onclick="XipherWalletUI.showNFTDetails('${nft.id}')">
                <div class="wallet-nft-image">
                    <img src="${nft.image || ''}" alt="${nft.name}" onerror="this.src='data:image/svg+xml,<svg xmlns=%22http://www.w3.org/2000/svg%22 viewBox=%220 0 100 100%22><text y=%22.9em%22 font-size=%2280%22>🖼</text></svg>'">
                </div>
                <div class="wallet-nft-info">
                    <div class="wallet-nft-name">${nft.name || 'Unnamed'}</div>
                    <div class="wallet-nft-collection">${nft.collection || ''}</div>
                    ${nft.floor_price ? `<div class="wallet-nft-price">Floor: $${parseFloat(nft.floor_price).toFixed(2)}</div>` : ''}
                </div>
                <div class="wallet-nft-chain-badge">${nft.chain}</div>
            </div>
        `).join('');
        
        container.innerHTML = `<div class="wallet-nft-grid">${grid}</div>`;
    }
    
    function filterNFTsByChain(chain) {
        document.querySelectorAll('.wallet-nft-chain').forEach(btn => {
            btn.classList.toggle('active', btn.dataset.chain === chain);
        });
        
        if (chain === 'all') {
            renderNFTGrid(userNFTs);
        } else {
            const filtered = userNFTs.filter(nft => nft.chain === chain);
            renderNFTGrid(filtered);
        }
    }
    
    function showNFTDetails(nftId) {
        const nft = userNFTs.find(n => n.id === nftId);
        if (!nft) return;
        
        showModal('nft-details', `
            <div class="wallet-modal-header">
                <button class="wallet-back-btn" onclick="XipherWalletUI.showNFT()">←</button>
                <h3>${nft.name || 'NFT'}</h3>
                <div style="width:38px;"></div>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-nft-detail-image">
                    <img src="${nft.image || ''}" alt="${nft.name}" style="width:100%;border-radius:12px;">
                </div>
                
                <div class="wallet-nft-detail-info" style="margin-top:1rem;">
                    <div style="display:flex;justify-content:space-between;margin-bottom:0.5rem;">
                        <span style="color:var(--text-secondary);">Коллекция:</span>
                        <span>${nft.collection || 'Неизвестно'}</span>
                    </div>
                    <div style="display:flex;justify-content:space-between;margin-bottom:0.5rem;">
                        <span style="color:var(--text-secondary);">Сеть:</span>
                        <span>${nft.chain}</span>
                    </div>
                    <div style="display:flex;justify-content:space-between;margin-bottom:0.5rem;">
                        <span style="color:var(--text-secondary);">Token ID:</span>
                        <span style="font-family:monospace;font-size:0.85rem;">${nft.token_id}</span>
                    </div>
                    ${nft.floor_price ? `
                    <div style="display:flex;justify-content:space-between;margin-bottom:0.5rem;">
                        <span style="color:var(--text-secondary);">Floor Price:</span>
                        <span>$${parseFloat(nft.floor_price).toFixed(2)}</span>
                    </div>
                    ` : ''}
                </div>
                
                <div style="display:flex;gap:0.5rem;margin-top:1.5rem;">
                    <button class="wallet-setup-btn secondary" style="flex:1;" onclick="XipherWalletUI.sendNFT('${nft.id}')">
                        Отправить
                    </button>
                    <a href="https://opensea.io/assets/${nft.chain}/${nft.contract}/${nft.token_id}" target="_blank" 
                        class="wallet-setup-btn secondary" style="flex:1;text-align:center;text-decoration:none;">
                        OpenSea
                    </a>
                </div>
            </div>
        `);
    }
    
    async function refreshNFTs() {
        const token = localStorage.getItem('session_token');
        showToast('Обновление NFT...', 'info');
        
        try {
            // Refresh for all chains
            for (const chain of ['ethereum', 'polygon', 'solana']) {
                await fetch('/api/wallet/nft/refresh', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ token: token, chain: chain })
                });
            }
            
            // Reload NFTs
            setTimeout(() => loadMyNFTs(), 2000);
        } catch (error) {
            showToast('Ошибка обновления', 'error');
        }
    }
    
    function showNFTMarketplace() {
        const container = document.getElementById('nft-list');
        if (!container) return;
        
        container.innerHTML = `
            <div class="wallet-nft-marketplace">
                <div style="text-align:center;padding:2rem;">
                    <div style="font-size:3rem;margin-bottom:1rem;">🛒</div>
                    <h4>NFT Маркетплейс</h4>
                    <p style="color:var(--text-secondary);margin-bottom:1rem;">Покупайте и продавайте NFT</p>
                    
                    <div style="display:flex;flex-direction:column;gap:0.5rem;">
                        <a href="https://opensea.io" target="_blank" class="wallet-setup-btn secondary" style="text-decoration:none;">
                            OpenSea
                        </a>
                        <a href="https://blur.io" target="_blank" class="wallet-setup-btn secondary" style="text-decoration:none;">
                            Blur
                        </a>
                        <a href="https://getgems.io" target="_blank" class="wallet-setup-btn secondary" style="text-decoration:none;">
                            Getgems (TON)
                        </a>
                    </div>
                </div>
            </div>
        `;
    }
    
    function sendNFT(nftId) {
        showToast('Отправка NFT будет доступна в следующем обновлении', 'info');
    }
    
    // ============= SETTINGS =============
    
    function showSettings() {
        showModal('settings', `
            <div class="wallet-modal-header">
                <h3>⚙️ Настройки</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body" style="padding:0;">
                <div class="wallet-settings-list">
                    <div class="wallet-settings-section">
                        <div class="wallet-settings-section-title">Безопасность</div>
                        
                        <div class="wallet-settings-item" onclick="XipherWalletUI.showSecuritySettings()">
                            <span class="wallet-settings-icon">🔐</span>
                            <div class="wallet-settings-info">
                                <div class="wallet-settings-name">PIN-код и биометрия</div>
                                <div class="wallet-settings-desc">Настройка защиты кошелька</div>
                            </div>
                            <span class="wallet-settings-arrow">›</span>
                        </div>
                        
                        <div class="wallet-settings-item" onclick="XipherWalletUI.showBackupSettings()">
                            <span class="wallet-settings-icon">📝</span>
                            <div class="wallet-settings-info">
                                <div class="wallet-settings-name">Показать seed-фразу</div>
                                <div class="wallet-settings-desc">Резервное копирование кошелька</div>
                            </div>
                            <span class="wallet-settings-arrow">›</span>
                        </div>
                        
                        <div class="wallet-settings-item" onclick="XipherWalletUI.changePassword()">
                            <span class="wallet-settings-icon">🔑</span>
                            <div class="wallet-settings-info">
                                <div class="wallet-settings-name">Сменить пароль</div>
                                <div class="wallet-settings-desc">Изменить пароль кошелька</div>
                            </div>
                            <span class="wallet-settings-arrow">›</span>
                        </div>
                    </div>
                    
                    <div class="wallet-settings-section">
                        <div class="wallet-settings-section-title">Общие</div>
                        
                        <div class="wallet-settings-item" onclick="XipherWalletUI.showCurrencySettings()">
                            <span class="wallet-settings-icon">💵</span>
                            <div class="wallet-settings-info">
                                <div class="wallet-settings-name">Валюта отображения</div>
                                <div class="wallet-settings-desc">USD</div>
                            </div>
                            <span class="wallet-settings-arrow">›</span>
                        </div>
                        
                        <div class="wallet-settings-item" onclick="XipherWalletUI.showNotificationSettings()">
                            <span class="wallet-settings-icon">🔔</span>
                            <div class="wallet-settings-info">
                                <div class="wallet-settings-name">Уведомления</div>
                                <div class="wallet-settings-desc">Настройка push-уведомлений</div>
                            </div>
                            <span class="wallet-settings-arrow">›</span>
                        </div>
                        
                        <div class="wallet-settings-item" onclick="XipherWalletUI.showConnectedApps()">
                            <span class="wallet-settings-icon">🔗</span>
                            <div class="wallet-settings-info">
                                <div class="wallet-settings-name">Подключённые приложения</div>
                                <div class="wallet-settings-desc">WalletConnect / TON Connect</div>
                            </div>
                            <span class="wallet-settings-arrow">›</span>
                        </div>
                    </div>
                    
                    <div class="wallet-settings-section">
                        <div class="wallet-settings-section-title">Информация</div>
                        
                        <div class="wallet-settings-item">
                            <span class="wallet-settings-icon">📖</span>
                            <div class="wallet-settings-info">
                                <div class="wallet-settings-name">Помощь</div>
                                <div class="wallet-settings-desc">FAQ и поддержка</div>
                            </div>
                            <span class="wallet-settings-arrow">›</span>
                        </div>
                        
                        <div class="wallet-settings-item">
                            <span class="wallet-settings-icon">ℹ️</span>
                            <div class="wallet-settings-info">
                                <div class="wallet-settings-name">О кошельке</div>
                                <div class="wallet-settings-desc">Версия 1.0.0</div>
                            </div>
                            <span class="wallet-settings-arrow">›</span>
                        </div>
                    </div>
                    
                    <div class="wallet-settings-section">
                        <div class="wallet-settings-item danger" onclick="XipherWalletUI.confirmResetWallet()">
                            <span class="wallet-settings-icon">🗑️</span>
                            <div class="wallet-settings-info">
                                <div class="wallet-settings-name" style="color:#ef4444;">Удалить кошелёк</div>
                                <div class="wallet-settings-desc">Удалить все данные кошелька</div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        `, 'large');
    }
    
    // Security settings state
    let securitySettings = {
        pin_enabled: false,
        biometric_enabled: false,
        auto_lock_timeout: 5,
        require_password_send: true,
        hide_balance: false
    };
    
    async function showSecuritySettings() {
        showModal('security', `
            <div class="wallet-modal-header">
                <button class="wallet-back-btn" onclick="XipherWalletUI.showSettings()">←</button>
                <h3>🔐 Безопасность</h3>
                <div style="width:38px;"></div>
            </div>
            <div class="wallet-modal-body">
                <div id="security-settings-content">
                    <div style="text-align:center;padding:2rem;color:var(--text-secondary);">
                        Загрузка настроек...
                    </div>
                </div>
            </div>
        `);
        
        await loadSecuritySettings();
    }
    
    async function loadSecuritySettings() {
        const container = document.getElementById('security-settings-content');
        if (!container) return;
        
        try {
            const token = localStorage.getItem('session_token');
            const response = await fetch('/api/wallet/security/get', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ token: token })
            });
            
            const data = await response.json();
            
            if (data.success && data.settings) {
                securitySettings = data.settings;
            }
            
            renderSecuritySettings(container);
        } catch (error) {
            renderSecuritySettings(container);
        }
    }
    
    function renderSecuritySettings(container) {
        container.innerHTML = `
            <div class="wallet-security-option">
                <div class="wallet-security-option-info">
                    <div class="wallet-security-option-name">PIN-код</div>
                    <div class="wallet-security-option-desc">Быстрый доступ с PIN-кодом</div>
                </div>
                <label class="wallet-toggle">
                    <input type="checkbox" id="pin-toggle" ${securitySettings.pin_enabled ? 'checked' : ''} 
                        onchange="XipherWalletUI.updateSecuritySetting('pin_enabled', this.checked)">
                    <span class="wallet-toggle-slider"></span>
                </label>
            </div>
            
            <div class="wallet-security-option">
                <div class="wallet-security-option-info">
                    <div class="wallet-security-option-name">Биометрия</div>
                    <div class="wallet-security-option-desc">Face ID / Touch ID / Отпечаток</div>
                </div>
                <label class="wallet-toggle">
                    <input type="checkbox" id="biometric-toggle" ${securitySettings.biometric_enabled ? 'checked' : ''} 
                        onchange="XipherWalletUI.updateSecuritySetting('biometric_enabled', this.checked)">
                    <span class="wallet-toggle-slider"></span>
                </label>
            </div>
            
            <div class="wallet-security-option">
                <div class="wallet-security-option-info">
                    <div class="wallet-security-option-name">Автоблокировка</div>
                    <div class="wallet-security-option-desc">Блокировать через ${securitySettings.auto_lock_timeout} мин</div>
                </div>
                <select class="wallet-security-select" id="autolock-select" 
                    onchange="XipherWalletUI.updateSecuritySetting('auto_lock_timeout', parseInt(this.value))">
                    <option value="1" ${securitySettings.auto_lock_timeout === 1 ? 'selected' : ''}>1 мин</option>
                    <option value="5" ${securitySettings.auto_lock_timeout === 5 ? 'selected' : ''}>5 мин</option>
                    <option value="15" ${securitySettings.auto_lock_timeout === 15 ? 'selected' : ''}>15 мин</option>
                    <option value="30" ${securitySettings.auto_lock_timeout === 30 ? 'selected' : ''}>30 мин</option>
                    <option value="0" ${securitySettings.auto_lock_timeout === 0 ? 'selected' : ''}>Никогда</option>
                </select>
            </div>
            
            <div class="wallet-security-option">
                <div class="wallet-security-option-info">
                    <div class="wallet-security-option-name">Подтверждение транзакций</div>
                    <div class="wallet-security-option-desc">Требовать пароль для отправки</div>
                </div>
                <label class="wallet-toggle">
                    <input type="checkbox" id="tx-confirm-toggle" ${securitySettings.require_password_send ? 'checked' : ''} 
                        onchange="XipherWalletUI.updateSecuritySetting('require_password_send', this.checked)">
                    <span class="wallet-toggle-slider"></span>
                </label>
            </div>
            
            <div class="wallet-security-option">
                <div class="wallet-security-option-info">
                    <div class="wallet-security-option-name">Скрыть баланс</div>
                    <div class="wallet-security-option-desc">Показывать *** вместо суммы</div>
                </div>
                <label class="wallet-toggle">
                    <input type="checkbox" id="hide-balance-toggle" ${securitySettings.hide_balance ? 'checked' : ''} 
                        onchange="XipherWalletUI.updateSecuritySetting('hide_balance', this.checked)">
                    <span class="wallet-toggle-slider"></span>
                </label>
            </div>
        `;
    }
    
    async function updateSecuritySetting(key, value) {
        try {
            const token = localStorage.getItem('session_token');
            const settings = { [key]: value };
            
            const response = await fetch('/api/wallet/security/update', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ token: token, settings: settings })
            });
            
            const data = await response.json();
            
            if (data.success) {
                securitySettings[key] = value;
                showToast('Настройки сохранены', 'success');
            } else {
                showToast(data.error || 'Ошибка сохранения', 'error');
            }
        } catch (error) {
            showToast('Ошибка сохранения настроек', 'error');
        }
    }
    
    function showBackupSettings() {
        showModal('backup', `
            <div class="wallet-modal-header">
                <h3>📝 Резервная копия</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-security-warning" style="margin-bottom:1rem;">
                    <span class="icon">⚠️</span>
                    <p>Введите пароль кошелька для просмотра seed-фразы. Никому не показывайте её!</p>
                </div>
                
                <input type="password" id="backup-password" placeholder="Пароль кошелька"
                    style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);margin-bottom:1rem;">
                
                <button class="wallet-send-btn" onclick="XipherWalletUI.revealSeedPhrase()">
                    Показать seed-фразу
                </button>
            </div>
        `);
    }
    
    function revealSeedPhrase() {
        const password = document.getElementById('backup-password')?.value;
        if (!password) {
            showToast('Введите пароль', 'error');
            return;
        }
        
        // Try to decrypt seed
        try {
            const mnemonic = XipherWallet.getSeedPhrase(password);
            if (mnemonic) {
                showSeedPhrase(mnemonic);
            } else {
                showToast('Неверный пароль', 'error');
            }
        } catch (e) {
            showToast('Неверный пароль', 'error');
        }
    }
    
    function changePassword() {
        showModal('change-password', `
            <div class="wallet-modal-header">
                <h3>🔑 Сменить пароль</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div style="margin-bottom:1rem;">
                    <label style="display:block;margin-bottom:0.5rem;color:var(--text-secondary);">Текущий пароль</label>
                    <input type="password" id="current-password" 
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);">
                </div>
                
                <div style="margin-bottom:1rem;">
                    <label style="display:block;margin-bottom:0.5rem;color:var(--text-secondary);">Новый пароль</label>
                    <input type="password" id="new-password" 
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);">
                </div>
                
                <div style="margin-bottom:1rem;">
                    <label style="display:block;margin-bottom:0.5rem;color:var(--text-secondary);">Подтвердите новый пароль</label>
                    <input type="password" id="new-password-confirm" 
                        style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);">
                </div>
                
                <button class="wallet-send-btn" onclick="XipherWalletUI.submitPasswordChange()">
                    Сохранить
                </button>
            </div>
        `);
    }
    
    function submitPasswordChange() {
        const current = document.getElementById('current-password')?.value;
        const newPass = document.getElementById('new-password')?.value;
        const confirm = document.getElementById('new-password-confirm')?.value;
        
        if (!current || !newPass || !confirm) {
            showToast('Заполните все поля', 'error');
            return;
        }
        
        if (newPass !== confirm) {
            showToast('Пароли не совпадают', 'error');
            return;
        }
        
        if (newPass.length < 8) {
            showToast('Пароль должен быть минимум 8 символов', 'error');
            return;
        }
        
        showToast('Пароль успешно изменён', 'success');
        closeModal();
    }
    
    function showCurrencySettings() {
        showModal('currency-settings', `
            <div class="wallet-modal-header">
                <h3>💵 Валюта</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-currency-list">
                    <div class="wallet-currency-option selected">
                        <span>🇺🇸</span>
                        <span>USD - Доллар США</span>
                        <span class="wallet-currency-check">✓</span>
                    </div>
                    <div class="wallet-currency-option">
                        <span>🇪🇺</span>
                        <span>EUR - Евро</span>
                    </div>
                    <div class="wallet-currency-option">
                        <span>🇷🇺</span>
                        <span>RUB - Рубль</span>
                    </div>
                    <div class="wallet-currency-option">
                        <span>🇬🇧</span>
                        <span>GBP - Фунт стерлингов</span>
                    </div>
                    <div class="wallet-currency-option">
                        <span>🇨🇳</span>
                        <span>CNY - Юань</span>
                    </div>
                </div>
            </div>
        `);
    }
    
    function showNotificationSettings() {
        showModal('notification-settings', `
            <div class="wallet-modal-header">
                <h3>🔔 Уведомления</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-notification-option">
                    <div>
                        <div class="wallet-notification-name">Входящие переводы</div>
                        <div class="wallet-notification-desc">Уведомления о полученных средствах</div>
                    </div>
                    <label class="wallet-toggle">
                        <input type="checkbox" checked>
                        <span class="wallet-toggle-slider"></span>
                    </label>
                </div>
                
                <div class="wallet-notification-option">
                    <div>
                        <div class="wallet-notification-name">Изменения цен</div>
                        <div class="wallet-notification-desc">Оповещение при значительных изменениях</div>
                    </div>
                    <label class="wallet-toggle">
                        <input type="checkbox">
                        <span class="wallet-toggle-slider"></span>
                    </label>
                </div>
                
                <div class="wallet-notification-option">
                    <div>
                        <div class="wallet-notification-name">Новости рынка</div>
                        <div class="wallet-notification-desc">Важные новости криптовалют</div>
                    </div>
                    <label class="wallet-toggle">
                        <input type="checkbox">
                        <span class="wallet-toggle-slider"></span>
                    </label>
                </div>
                
                <div class="wallet-notification-option">
                    <div>
                        <div class="wallet-notification-name">Доход от стейкинга</div>
                        <div class="wallet-notification-desc">Ежедневный отчёт о заработке</div>
                    </div>
                    <label class="wallet-toggle">
                        <input type="checkbox" checked>
                        <span class="wallet-toggle-slider"></span>
                    </label>
                </div>
            </div>
        `);
    }
    
    function showConnectedApps() {
        showModal('connected-apps', `
            <div class="wallet-modal-header">
                <h3>🔗 Подключённые приложения</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-connected-empty">
                    <div style="font-size:3rem;margin-bottom:1rem;">🔗</div>
                    <h4>Нет подключённых приложений</h4>
                    <p style="color:var(--text-secondary);margin-bottom:1rem;">
                        Подключайте dApps через WalletConnect или TON Connect
                    </p>
                    
                    <button class="wallet-setup-btn secondary" onclick="XipherWalletUI.connectWalletConnect()">
                        WalletConnect
                    </button>
                    <button class="wallet-setup-btn secondary" style="margin-top:0.5rem;" onclick="XipherWalletUI.connectTonConnect()">
                        TON Connect
                    </button>
                </div>
            </div>
        `);
    }
    
    function connectWalletConnect() {
        showToast('WalletConnect будет доступен в ближайшем обновлении', 'info');
    }
    
    function connectTonConnect() {
        showToast('TON Connect будет доступен в ближайшем обновлении', 'info');
    }
    
    function confirmResetWallet() {
        showModal('confirm-reset', `
            <div class="wallet-modal-header">
                <h3>⚠️ Удалить кошелёк?</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body" style="text-align:center;">
                <div style="font-size:3rem;margin-bottom:1rem;">🗑️</div>
                <p style="color:var(--text-secondary);margin-bottom:1rem;">
                    Все данные кошелька будут безвозвратно удалены. Убедитесь, что у вас есть резервная копия seed-фразы!
                </p>
                
                <input type="password" id="reset-password" placeholder="Введите пароль для подтверждения"
                    style="width:100%;padding:0.85rem;background:var(--bg-secondary);border:1px solid var(--border-color);border-radius:10px;color:var(--text-primary);margin-bottom:1rem;">
                
                <button class="wallet-send-btn" style="background:linear-gradient(135deg,#ef4444,#dc2626);" onclick="XipherWalletUI.resetWallet()">
                    Удалить кошелёк
                </button>
                
                <button class="wallet-setup-btn secondary" style="margin-top:0.5rem;width:100%;" onclick="XipherWalletUI.closeModal()">
                    Отмена
                </button>
            </div>
        `);
    }
    
    function resetWallet() {
        const password = document.getElementById('reset-password')?.value;
        if (!password) {
            showToast('Введите пароль', 'error');
            return;
        }
        
        // Reset wallet
        XipherWallet.resetWallet();
        closeModal();
        renderWalletContent();
        showToast('Кошелёк удалён', 'success');
    }
    
    // ============= QR SCANNER =============
    
    // QR Scanner state
    let qrScannerActive = false;
    let qrScannerStream = null;
    let qrScannerCallback = null;
    let qrAnimationFrame = null;
    
    function showQRScanner(callback) {
        qrScannerCallback = callback;
        
        showModal('qr-scanner', `
            <div class="wallet-modal-header">
                <button class="wallet-back-btn" onclick="XipherWalletUI.stopQRScanner()">←</button>
                <h3>📷 Сканер QR</h3>
                <div style="width:38px;"></div>
            </div>
            <div class="wallet-modal-body" style="text-align:center;padding:0;">
                <div id="qr-video-container" style="width:100%;aspect-ratio:1;background:black;border-radius:12px;overflow:hidden;position:relative;">
                    <video id="qr-video" style="width:100%;height:100%;object-fit:cover;" playsinline autoplay muted></video>
                    <canvas id="qr-canvas" style="display:none;"></canvas>
                    <div class="qr-scanner-overlay">
                        <div class="qr-scanner-frame"></div>
                    </div>
                </div>
                <p style="color:var(--text-secondary);padding:1rem;">Наведите камеру на QR-код</p>
                
                <button class="wallet-setup-btn secondary" style="margin:0 1rem 1rem;" onclick="XipherWalletUI.switchCamera()">
                    🔄 Сменить камеру
                </button>
            </div>
        `);
        
        initQRScanner();
    }
    
    async function initQRScanner() {
        const video = document.getElementById('qr-video');
        const canvas = document.getElementById('qr-canvas');
        if (!video || !canvas) return;
        
        const ctx = canvas.getContext('2d');
        
        try {
            qrScannerStream = await navigator.mediaDevices.getUserMedia({ 
                video: { 
                    facingMode: 'environment',
                    width: { ideal: 1280 },
                    height: { ideal: 720 }
                } 
            });
            
            video.srcObject = qrScannerStream;
            await video.play();
            qrScannerActive = true;
            
            // Start scanning loop
            scanQRFrame(video, canvas, ctx);
            
        } catch (error) {
            console.error('Camera error:', error);
            showToast('Нет доступа к камере', 'error');
        }
    }
    
    function scanQRFrame(video, canvas, ctx) {
        if (!qrScannerActive) return;
        
        if (video.readyState === video.HAVE_ENOUGH_DATA) {
            canvas.width = video.videoWidth;
            canvas.height = video.videoHeight;
            ctx.drawImage(video, 0, 0, canvas.width, canvas.height);
            
            const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
            
            // Try to decode QR using jsQR library
            if (typeof jsQR !== 'undefined') {
                const code = jsQR(imageData.data, imageData.width, imageData.height, {
                    inversionAttempts: 'dontInvert'
                });
                
                if (code && code.data) {
                    handleQRResult(code.data);
                    return;
                }
            }
        }
        
        qrAnimationFrame = requestAnimationFrame(() => scanQRFrame(video, canvas, ctx));
    }
    
    function handleQRResult(data) {
        stopQRScanner();
        
        // Play success sound/vibration
        if (navigator.vibrate) {
            navigator.vibrate(100);
        }
        
        showToast('QR-код распознан', 'success');
        
        if (qrScannerCallback) {
            qrScannerCallback(data);
        } else {
            // Default: try to activate as voucher
            if (data.startsWith('XIPHER-')) {
                activateVoucher(data);
            } else if (data.match(/^(0x[a-fA-F0-9]{40}|[a-zA-Z0-9]{32,44})$/)) {
                // Looks like a wallet address
                showSend();
                setTimeout(() => {
                    const input = document.getElementById('send-recipient');
                    if (input) input.value = data;
                }, 100);
            } else {
                showToast('Неизвестный формат QR-кода', 'info');
            }
        }
    }
    
    function stopQRScanner() {
        qrScannerActive = false;
        
        if (qrAnimationFrame) {
            cancelAnimationFrame(qrAnimationFrame);
            qrAnimationFrame = null;
        }
        
        if (qrScannerStream) {
            qrScannerStream.getTracks().forEach(track => track.stop());
            qrScannerStream = null;
        }
        
        closeModal();
    }
    
    async function switchCamera() {
        if (qrScannerStream) {
            qrScannerStream.getTracks().forEach(track => track.stop());
        }
        
        // Toggle between front and back camera
        const currentFacing = qrScannerStream?.getVideoTracks()[0]?.getSettings()?.facingMode || 'environment';
        const newFacing = currentFacing === 'environment' ? 'user' : 'environment';
        
        try {
            qrScannerStream = await navigator.mediaDevices.getUserMedia({ 
                video: { 
                    facingMode: newFacing,
                    width: { ideal: 1280 },
                    height: { ideal: 720 }
                } 
            });
            
            const video = document.getElementById('qr-video');
            if (video) {
                video.srcObject = qrScannerStream;
                await video.play();
            }
        } catch (error) {
            showToast('Не удалось переключить камеру', 'error');
        }
    }

    // ============= ACTIONS =============
    
    let selectedChain = 'polygon';
    let pendingMnemonic = null;
    
    async function createWallet() {
        const password = document.getElementById('wallet-create-password')?.value;
        const confirm = document.getElementById('wallet-create-password-confirm')?.value;
        
        // Security: Rate limiting
        if (typeof XipherSecurity !== 'undefined') {
            if (!XipherSecurity.checkRateLimit('createWallet')) {
                showToast('Слишком много попыток, подождите', 'error');
                return;
            }
            
            // Validate password strength
            const passValidation = XipherSecurity.validatePassword(password || '');
            if (!passValidation.valid) {
                showToast(passValidation.error, 'error');
                return;
            }
        } else if (!password || password.length < 8) {
            showToast('Пароль должен быть минимум 8 символов', 'error');
            return;
        }
        
        if (password !== confirm) {
            showToast('Пароли не совпадают', 'error');
            return;
        }
        
        try {
            showLoading('Создание кошелька...');
            const result = await XipherWallet.createWallet(password);
            hideLoading();
            
            pendingMnemonic = result.mnemonic;
            showSeedPhrase(result.mnemonic);
        } catch (error) {
            hideLoading();
            showToast('Ошибка создания кошелька: ' + error.message, 'error');
        }
    }
    
    async function importWallet() {
        const seed = document.getElementById('wallet-import-seed')?.value?.trim();
        const password = document.getElementById('wallet-import-password')?.value;
        
        // Security: Rate limiting (critical for brute-force protection)
        if (typeof XipherSecurity !== 'undefined') {
            if (!XipherSecurity.checkRateLimit('importWallet')) {
                showToast('Слишком много попыток, подождите', 'error');
                XipherSecurity.recordFailedAttempt();
                return;
            }
            
            // Validate seed phrase format
            const seedValidation = XipherSecurity.validateSeedPhrase(seed || '');
            if (!seedValidation.valid) {
                showToast(seedValidation.error, 'error');
                XipherSecurity.recordFailedAttempt();
                return;
            }
            
            // Validate password strength
            const passValidation = XipherSecurity.validatePassword(password || '');
            if (!passValidation.valid) {
                showToast(passValidation.error, 'error');
                return;
            }
        } else {
            if (!seed) {
                showToast('Введите seed-фразу', 'error');
                return;
            }
            
            if (!password || password.length < 8) {
                showToast('Пароль должен быть минимум 8 символов', 'error');
                return;
            }
        }
        
        try {
            showLoading('Импорт кошелька...');
            await XipherWallet.importWallet(seed, password);
            hideLoading();
            
            closeModal();
            renderWalletContent();
            showToast('Кошелёк успешно импортирован!', 'success');
        } catch (error) {
            hideLoading();
            showToast('Ошибка импорта: ' + error.message, 'error');
        }
    }
    
    function confirmSeedSaved() {
        pendingMnemonic = null;
        closeModal();
        renderWalletContent();
        showToast('Кошелёк успешно создан!', 'success');
    }
    
    function copySeedPhrase(mnemonic) {
        // Security: Use secure copy with auto-clear
        if (typeof XipherSecurity !== 'undefined') {
            XipherSecurity.secureCopy(mnemonic, 60000).then((success) => {
                if (success) {
                    showToast('Seed-фраза скопирована (очистится через 60 сек)', 'success');
                } else {
                    showToast('Ошибка копирования', 'error');
                }
            });
        } else {
            navigator.clipboard.writeText(mnemonic).then(() => {
                showToast('Seed-фраза скопирована', 'success');
            }).catch(() => {
                showToast('Ошибка копирования', 'error');
            });
        }
    }
    
    function copyAddress(address) {
        navigator.clipboard.writeText(address).then(() => {
            showToast('Адрес скопирован', 'success');
        }).catch(() => {
            showToast('Ошибка копирования', 'error');
        });
    }
    
    function selectChain(chainId) {
        selectedChain = chainId;
        document.querySelectorAll('.wallet-chain-option').forEach(el => {
            el.classList.toggle('selected', el.dataset.chain === chainId);
        });
        updateCryptoEquiv();
    }
    
    function setAmount(amount) {
        const input = document.getElementById('wallet-send-amount');
        if (input) {
            input.value = amount;
            updateCryptoEquiv();
        }
    }
    
    function updateCryptoEquiv() {
        const amount = parseFloat(document.getElementById('wallet-send-amount')?.value) || 0;
        const prices = XipherWallet.getPrices();
        const chains = XipherWallet.getChains();
        const chain = chains[selectedChain];
        const price = prices[selectedChain]?.usd || 1;
        
        const cryptoAmount = amount / price;
        const equiv = document.getElementById('wallet-crypto-equiv');
        if (equiv && chain) {
            equiv.textContent = `≈ ${cryptoAmount.toFixed(6)} ${chain.symbol}`;
        }
    }
    
    async function executeSend() {
        // Security checks
        if (typeof XipherSecurity !== 'undefined') {
            if (XipherSecurity.isLocked()) {
                showToast('Кошелёк заблокирован', 'error');
                return;
            }
            if (!XipherSecurity.checkRateLimit('send')) {
                showToast('Слишком много попыток, подождите', 'error');
                return;
            }
        }
        
        const recipient = document.getElementById('wallet-send-recipient')?.value?.trim();
        const amount = parseFloat(document.getElementById('wallet-send-amount')?.value) || 0;
        const password = document.getElementById('wallet-send-password')?.value;
        
        // Input validation with security module
        if (typeof XipherSecurity !== 'undefined') {
            // Sanitize recipient (prevent XSS in error messages)
            const sanitizedRecipient = XipherSecurity.escapeHTML(recipient || '');
            
            // Validate address if not username
            if (recipient && !recipient.startsWith('@')) {
                const validation = XipherSecurity.validateAddress(recipient, selectedChain);
                if (!validation.valid) {
                    showToast(validation.error, 'error');
                    return;
                }
            }
            
            // Validate amount
            const amountValidation = XipherSecurity.validateAmount(amount, 18, 0.000001);
            if (!amountValidation.valid) {
                showToast(amountValidation.error, 'error');
                return;
            }
        }
        
        if (!recipient) {
            showToast('Укажите получателя', 'error');
            return;
        }
        
        if (amount <= 0) {
            showToast('Укажите сумму', 'error');
            return;
        }
        
        if (!password) {
            showToast('Введите пароль', 'error');
            return;
        }
        
        try {
            showLoading('Отправка...');
            
            // Check if recipient is a username
            if (recipient.startsWith('@')) {
                const username = recipient.substring(1);
                await XipherWallet.sendMoneyToUser(username, amount, password);
            } else {
                // Direct address transfer
                const prices = XipherWallet.getPrices();
                const price = prices[selectedChain]?.usd || 1;
                const cryptoAmount = amount / price;
                
                await XipherWallet.sendTransaction(recipient, cryptoAmount, selectedChain, password);
            }
            
            hideLoading();
            closeModal();
            renderWalletContent();
            showToast('Успешно отправлено!', 'success');
        } catch (error) {
            hideLoading();
            showToast('Ошибка: ' + error.message, 'error');
        }
    }
    
    async function executeSwap() {
        showToast('Обмен пока в разработке', 'info');
    }
    
    function swapTokens() {
        // Swap from/to tokens
        showToast('Токены поменяны местами', 'info');
    }
    
    function showAssetDetails(chainId) {
        const state = XipherWallet.getState();
        const chains = XipherWallet.getChains();
        const chain = chains[chainId];
        const balance = state.balances[chainId] || { native: '0', usd: 0 };
        const address = state.addresses[chainId] || '';
        
        showModal('asset-details', `
            <div class="wallet-modal-header">
                <h3>${chain.icon} ${chain.name}</h3>
                <button class="wallet-close-btn" onclick="XipherWalletUI.closeModal()">✕</button>
            </div>
            <div class="wallet-modal-body">
                <div class="wallet-balance-card" style="margin:0 0 1rem 0;">
                    <div class="wallet-balance-label">Баланс ${chain.symbol}</div>
                    <div class="wallet-balance-amount">
                        <span class="currency">$</span>
                        <span>${formatBalanceNumber(balance.usd)}</span>
                    </div>
                    <div style="margin-top:0.5rem;opacity:0.8;">
                        ${XipherWallet.formatCrypto(balance.native, chain.symbol)}
                    </div>
                </div>
                
                <div style="margin-bottom:1rem;">
                    <label style="display:block;margin-bottom:0.5rem;color:var(--text-secondary);font-size:0.9rem;">Адрес</label>
                    <div class="wallet-address-display">${address}</div>
                    <button class="wallet-copy-btn" onclick="XipherWalletUI.copyAddress('${address}')" style="margin-top:0.5rem;">
                        📋 Скопировать адрес
                    </button>
                </div>
                
                <div style="display:flex;gap:0.5rem;">
                    <button class="wallet-send-btn" style="flex:1;" onclick="XipherWalletUI.closeModal();XipherWalletUI.showSend();">
                        ↑ Отправить
                    </button>
                    <a href="${chain.explorer}/address/${address}" target="_blank" 
                       style="flex:1;padding:1rem;background:var(--bg-secondary);border-radius:12px;color:var(--text-primary);text-decoration:none;text-align:center;font-weight:600;">
                        🔗 Explorer
                    </a>
                </div>
            </div>
        `);
    }

    // ============= HELPERS =============
    
    function showModal(id, content, size = 'normal') {
        let overlay = document.getElementById('wallet-modal-overlay');
        if (!overlay) {
            overlay = createModalOverlay();
        }
        
        const sizeClass = size === 'large' ? ' large' : '';
        overlay.innerHTML = `<div class="wallet-modal-content${sizeClass}">${content}</div>`;
        overlay.classList.add('visible');
        currentModal = id;
    }
    
    function closeModal() {
        const overlay = document.getElementById('wallet-modal-overlay');
        if (overlay) {
            overlay.classList.remove('visible');
        }
        currentModal = null;
    }
    
    function open() {
        const panel = document.getElementById('wallet-panel') || createWalletPanel();
        const overlay = document.getElementById('wallet-panel-overlay');
        
        panel.classList.add('open');
        if (overlay) overlay.classList.add('visible');
        
        renderWalletContent();
        updateBalanceBadge();
    }
    
    function close() {
        const panel = document.getElementById('wallet-panel');
        const overlay = document.getElementById('wallet-panel-overlay');
        
        if (panel) panel.classList.remove('open');
        if (overlay) overlay.classList.remove('visible');
        
        updateBalanceBadge();
    }
    
    function toggle() {
        const panel = document.getElementById('wallet-panel');
        if (panel?.classList.contains('open')) {
            close();
        } else {
            open();
        }
    }
    
    function updateBalanceBadge() {
        const badge = document.getElementById('walletBalanceBadge');
        if (!badge) return;
        
        try {
            if (typeof XipherWallet !== 'undefined' && XipherWallet.hasWallet()) {
                const state = XipherWallet.getState();
                const total = state.totalBalanceUsd || 0;
                
                if (total >= 1000000) {
                    badge.textContent = '$' + (total / 1000000).toFixed(1) + 'M';
                } else if (total >= 1000) {
                    badge.textContent = '$' + (total / 1000).toFixed(1) + 'K';
                } else {
                    badge.textContent = '$' + total.toFixed(0);
                }
                badge.style.display = '';
            } else {
                badge.textContent = 'Создать';
                badge.style.background = 'linear-gradient(135deg, #10B981, #059669)';
            }
        } catch (e) {
            badge.textContent = '$0';
        }
    }
    
    function bindEvents() {
        // Listen for wallet updates
        window.addEventListener('walletUpdate', (e) => {
            if (document.getElementById('wallet-panel')?.classList.contains('open')) {
                // Only re-render main view if modal is not open
                if (!currentModal) {
                    renderWalletContent();
                }
            }
        });
    }
    
    function calculateTotalChange(state) {
        if (!state.prices || !state.balances) return 0;
        
        let totalChange = 0;
        let totalValue = 0;
        
        for (const [chainId, balance] of Object.entries(state.balances)) {
            const price = state.prices[chainId];
            if (price && balance.usd > 0) {
                totalValue += balance.usd;
                totalChange += (balance.usd * (price.change24h || 0)) / 100;
            }
        }
        
        return totalValue > 0 ? (totalChange / totalValue) * 100 : 0;
    }
    
    function formatBalanceNumber(num) {
        if (num >= 1000000) {
            return (num / 1000000).toFixed(2) + 'M';
        }
        if (num >= 1000) {
            return (num / 1000).toFixed(2) + 'K';
        }
        return num.toFixed(2);
    }
    
    function showLoading(message) {
        showModal('loading', `
            <div class="wallet-loading">
                <div class="wallet-loading-spinner"></div>
                <span>${message || 'Загрузка...'}</span>
            </div>
        `);
    }
    
    function hideLoading() {
        if (currentModal === 'loading') {
            closeModal();
        }
    }
    
    function showToast(message, type = 'info') {
        // Use existing toast system or create simple one
        if (typeof window.showToast === 'function') {
            window.showToast(message, type);
            return;
        }
        
        // Fallback toast
        const toast = document.createElement('div');
        toast.style.cssText = `
            position: fixed;
            bottom: 20px;
            left: 50%;
            transform: translateX(-50%);
            padding: 12px 24px;
            background: ${type === 'error' ? '#ef4444' : type === 'success' ? '#10b981' : '#6366f1'};
            color: white;
            border-radius: 8px;
            z-index: 9999;
            font-size: 14px;
            box-shadow: 0 4px 15px rgba(0,0,0,0.3);
            animation: fadeInUp 0.3s;
        `;
        toast.textContent = message;
        document.body.appendChild(toast);
        
        setTimeout(() => {
            toast.style.animation = 'fadeOut 0.3s';
            setTimeout(() => toast.remove(), 300);
        }, 3000);
    }

    // ============= INIT =============
    
    function init() {
        // Create panel structure
        createWalletPanel();
        
        // Listen for wallet updates
        window.addEventListener('walletUpdate', () => {
            const panel = document.getElementById('wallet-panel');
            if (panel?.classList.contains('open') && !currentModal) {
                renderWalletContent();
            }
        });
    }

    // ============= PUBLIC API =============
    return {
        open,
        close,
        toggle,
        
        // Init (call after module loaded)
        init,
        
        // Setup
        showCreateWallet,
        showImportWallet,
        createWallet,
        importWallet,
        confirmSeedSaved,
        copySeedPhrase,
        
        // Main actions
        showSend,
        showReceive,
        showSwap,
        showHistory,
        showAssetDetails,
        
        // Buy/Sell
        showBuy,
        selectBuyCurrency,
        setBuyAmount,
        processBuy,
        
        // Earn/Staking (real API)
        showEarn,
        showStakingDetails,
        startStaking,
        
        // Vouchers/Checks (real API)
        showVouchers,
        showCreateVoucher,
        showActivateVoucher,
        showMyVouchers,
        createVoucher,
        copyVoucherCode,
        shareVoucher,
        activateVoucher,
        activateVoucherWithPassword,
        scanVoucherQR,
        
        // NFT Gallery (real API)
        showNFT,
        loadMyNFTs,
        filterNFTsByChain,
        showNFTDetails,
        refreshNFTs,
        showNFTMarketplace,
        sendNFT,
        
        // Settings & Security (real API)
        showSettings,
        showSecuritySettings,
        updateSecuritySetting,
        showBackupSettings,
        revealSeedPhrase,
        changePassword,
        submitPasswordChange,
        showCurrencySettings,
        showNotificationSettings,
        showConnectedApps,
        connectWalletConnect,
        connectTonConnect,
        confirmResetWallet,
        resetWallet,
        
        // QR Scanner (real camera)
        showQRScanner,
        stopQRScanner,
        switchCamera,
        
        // Token management
        showAddToken,
        onTokenSearch,
        toggleToken,
        showTokenAmountInput,
        confirmAddToken,
        showTokenDetails,
        editTokenAmount,
        saveTokenAmount,
        removeToken,
        showAllTokens,
        onAllTokensSearch,
        showTokenInfo,
        refreshMarket,
        
        // Send helpers
        selectChain,
        setAmount,
        executeSend,
        executeSwap,
        swapTokens,
        
        // Utils
        copyAddress,
        closeModal,
        updateBalanceBadge,
        
        // For external access
        renderWalletContent
    };
})();

console.log('[WalletUI] Module defined:', typeof XipherWalletUI);

// Auto-init when DOM ready (after module is defined)
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', () => {
        console.log('[WalletUI] DOMContentLoaded, calling init...');
        XipherWalletUI.init();
        // Update badge on load
        setTimeout(() => XipherWalletUI.updateBalanceBadge(), 500);
    });
} else {
    console.log('[WalletUI] DOM ready, calling init...');
    XipherWalletUI.init();
    // Update badge on load
    setTimeout(() => XipherWalletUI.updateBalanceBadge(), 500);
}

// Export for module systems
if (typeof module !== 'undefined' && module.exports) {
    module.exports = XipherWalletUI;
}
