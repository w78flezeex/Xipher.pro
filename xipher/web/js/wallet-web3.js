/**
 * Xipher Web3 Integration
 * Real blockchain interactions for P2P, Staking, NFT
 */

const XipherWeb3 = (() => {
    'use strict';

    // Contract addresses (to be deployed)
    const CONTRACTS = {
        polygon: {
            p2pEscrow: '0x0000000000000000000000000000000000000000', // Deploy and set
            usdt: '0xc2132D05D31c914a87C6611C10748AEb04B58e8F',
            usdc: '0x2791Bca1f2de4661ED88A30C99A7a9449Aa84174'
        },
        ethereum: {
            p2pEscrow: '0x0000000000000000000000000000000000000000',
            usdt: '0xdAC17F958D2ee523a2206206994597C13D831ec7',
            usdc: '0xA0b86991c6218b36c1d19D4a2e9Eb0cE3606eB48'
        }
    };

    // P2P Escrow ABI (minimal)
    const P2P_ESCROW_ABI = [
        "function createTrade(address buyer, address token, uint256 amount, uint256 fiatAmount, string fiatCurrency) payable returns (bytes32)",
        "function confirmPayment(bytes32 tradeId)",
        "function releaseCrypto(bytes32 tradeId)",
        "function cancelTrade(bytes32 tradeId)",
        "function openDispute(bytes32 tradeId)",
        "function getTrade(bytes32 tradeId) view returns (tuple(address seller, address buyer, address token, uint256 amount, uint256 fiatAmount, string fiatCurrency, uint8 status, uint256 createdAt, uint256 expiresAt))",
        "event TradeCreated(bytes32 indexed tradeId, address indexed seller, address token, uint256 amount)",
        "event TradeCompleted(bytes32 indexed tradeId, address indexed buyer, uint256 amount)"
    ];

    // Staking protocols
    const STAKING_PROTOCOLS = {
        // Lido stETH on Ethereum
        lidoETH: {
            address: '0xae7ab96520DE3A18E5e111B5EaAb095312D7fE84',
            abi: [
                "function submit(address referral) payable returns (uint256)",
                "function balanceOf(address account) view returns (uint256)"
            ],
            apy: 3.8
        },
        // Lido stMATIC on Polygon  
        lidoMATIC: {
            address: '0x9ee91F9f426fA633d227f7a9b000E28b9dfd8599',
            abi: [
                "function submit(uint256 amount, address referral) returns (uint256)",
                "function balanceOf(address account) view returns (uint256)"
            ],
            apy: 4.2
        },
        // AAVE v3 Polygon for USDT/USDC lending
        aaveV3Polygon: {
            pool: '0x794a61358D6845594F94dc1DB02A252b5b4814aD',
            abi: [
                "function supply(address asset, uint256 amount, address onBehalfOf, uint16 referralCode)",
                "function withdraw(address asset, uint256 amount, address to) returns (uint256)"
            ],
            apy: 5.5
        }
    };

    // ERC20 ABI
    const ERC20_ABI = [
        "function approve(address spender, uint256 amount) returns (bool)",
        "function allowance(address owner, address spender) view returns (uint256)",
        "function balanceOf(address account) view returns (uint256)",
        "function transfer(address to, uint256 amount) returns (bool)"
    ];

    /**
     * Get ethers provider and signer
     */
    async function getSignerAndProvider(chainId) {
        if (typeof ethers === 'undefined') {
            throw new Error('ethers.js not loaded');
        }

        const wallet = XipherWallet.getState();
        if (!wallet.isInitialized) {
            throw new Error('Wallet not initialized');
        }

        const chain = XipherWallet.getChainConfig(chainId);
        const provider = new ethers.JsonRpcProvider(chain.rpcUrl);
        
        // Get mnemonic and create wallet
        const seed = await XipherWallet.getMnemonic();
        const signer = ethers.Wallet.fromPhrase(seed).connect(provider);
        
        return { provider, signer, chain };
    }

    // =============== P2P ESCROW ===============

    /**
     * Create P2P trade with escrow
     */
    async function createP2PTrade(buyerAddress, tokenAddress, amount, fiatAmount, fiatCurrency, chainId = 'polygon') {
        const { signer } = await getSignerAndProvider(chainId);
        const contractAddress = CONTRACTS[chainId]?.p2pEscrow;
        
        if (!contractAddress || contractAddress === '0x0000000000000000000000000000000000000000') {
            throw new Error('P2P Escrow contract not deployed on this chain');
        }

        const escrow = new ethers.Contract(contractAddress, P2P_ESCROW_ABI, signer);
        
        let tx;
        if (tokenAddress === ethers.ZeroAddress || !tokenAddress) {
            // Native token (ETH/MATIC)
            tx = await escrow.createTrade(
                buyerAddress,
                ethers.ZeroAddress,
                ethers.parseEther(amount.toString()),
                Math.round(fiatAmount * 100), // cents
                fiatCurrency,
                { value: ethers.parseEther(amount.toString()) }
            );
        } else {
            // ERC20 token
            const token = new ethers.Contract(tokenAddress, ERC20_ABI, signer);
            const amountWei = ethers.parseUnits(amount.toString(), 6); // USDT/USDC = 6 decimals
            
            // Approve escrow
            const allowance = await token.allowance(signer.address, contractAddress);
            if (allowance < amountWei) {
                const approveTx = await token.approve(contractAddress, amountWei);
                await approveTx.wait();
            }
            
            tx = await escrow.createTrade(
                buyerAddress,
                tokenAddress,
                amountWei,
                Math.round(fiatAmount * 100),
                fiatCurrency
            );
        }
        
        const receipt = await tx.wait();
        
        // Get trade ID from event
        const event = receipt.logs.find(log => {
            try {
                return escrow.interface.parseLog(log)?.name === 'TradeCreated';
            } catch { return false; }
        });
        
        const tradeId = event ? escrow.interface.parseLog(event).args.tradeId : null;
        
        return {
            success: true,
            txHash: receipt.hash,
            tradeId: tradeId
        };
    }

    /**
     * Confirm fiat payment (buyer)
     */
    async function confirmP2PPayment(tradeId, chainId = 'polygon') {
        const { signer } = await getSignerAndProvider(chainId);
        const escrow = new ethers.Contract(CONTRACTS[chainId].p2pEscrow, P2P_ESCROW_ABI, signer);
        
        const tx = await escrow.confirmPayment(tradeId);
        const receipt = await tx.wait();
        
        return { success: true, txHash: receipt.hash };
    }

    /**
     * Release crypto to buyer (seller)
     */
    async function releaseP2PCrypto(tradeId, chainId = 'polygon') {
        const { signer } = await getSignerAndProvider(chainId);
        const escrow = new ethers.Contract(CONTRACTS[chainId].p2pEscrow, P2P_ESCROW_ABI, signer);
        
        const tx = await escrow.releaseCrypto(tradeId);
        const receipt = await tx.wait();
        
        return { success: true, txHash: receipt.hash };
    }

    /**
     * Cancel trade (before payment)
     */
    async function cancelP2PTrade(tradeId, chainId = 'polygon') {
        const { signer } = await getSignerAndProvider(chainId);
        const escrow = new ethers.Contract(CONTRACTS[chainId].p2pEscrow, P2P_ESCROW_ABI, signer);
        
        const tx = await escrow.cancelTrade(tradeId);
        const receipt = await tx.wait();
        
        return { success: true, txHash: receipt.hash };
    }

    // =============== STAKING ===============

    /**
     * Stake ETH via Lido
     */
    async function stakeETHLido(amount) {
        const { signer } = await getSignerAndProvider('ethereum');
        const lido = new ethers.Contract(STAKING_PROTOCOLS.lidoETH.address, STAKING_PROTOCOLS.lidoETH.abi, signer);
        
        const tx = await lido.submit(ethers.ZeroAddress, {
            value: ethers.parseEther(amount.toString())
        });
        const receipt = await tx.wait();
        
        return {
            success: true,
            txHash: receipt.hash,
            stToken: 'stETH',
            amount: amount,
            apy: STAKING_PROTOCOLS.lidoETH.apy
        };
    }

    /**
     * Stake MATIC via Lido
     */
    async function stakeMATICLido(amount) {
        const { signer } = await getSignerAndProvider('polygon');
        const lido = new ethers.Contract(STAKING_PROTOCOLS.lidoMATIC.address, STAKING_PROTOCOLS.lidoMATIC.abi, signer);
        
        const tx = await lido.submit(
            ethers.parseEther(amount.toString()),
            ethers.ZeroAddress
        );
        const receipt = await tx.wait();
        
        return {
            success: true,
            txHash: receipt.hash,
            stToken: 'stMATIC',
            amount: amount,
            apy: STAKING_PROTOCOLS.lidoMATIC.apy
        };
    }

    /**
     * Supply USDT/USDC to AAVE (lending)
     */
    async function supplyToAave(tokenSymbol, amount) {
        const { signer } = await getSignerAndProvider('polygon');
        
        const tokenAddress = tokenSymbol === 'USDT' 
            ? CONTRACTS.polygon.usdt 
            : CONTRACTS.polygon.usdc;
        
        const pool = new ethers.Contract(
            STAKING_PROTOCOLS.aaveV3Polygon.pool,
            STAKING_PROTOCOLS.aaveV3Polygon.abi,
            signer
        );
        
        const token = new ethers.Contract(tokenAddress, ERC20_ABI, signer);
        const amountWei = ethers.parseUnits(amount.toString(), 6);
        
        // Approve AAVE pool
        const allowance = await token.allowance(signer.address, STAKING_PROTOCOLS.aaveV3Polygon.pool);
        if (allowance < amountWei) {
            const approveTx = await token.approve(STAKING_PROTOCOLS.aaveV3Polygon.pool, amountWei);
            await approveTx.wait();
        }
        
        const tx = await pool.supply(tokenAddress, amountWei, signer.address, 0);
        const receipt = await tx.wait();
        
        return {
            success: true,
            txHash: receipt.hash,
            protocol: 'AAVE v3',
            amount: amount,
            apy: STAKING_PROTOCOLS.aaveV3Polygon.apy
        };
    }

    /**
     * Withdraw from AAVE
     */
    async function withdrawFromAave(tokenSymbol, amount) {
        const { signer } = await getSignerAndProvider('polygon');
        
        const tokenAddress = tokenSymbol === 'USDT' 
            ? CONTRACTS.polygon.usdt 
            : CONTRACTS.polygon.usdc;
        
        const pool = new ethers.Contract(
            STAKING_PROTOCOLS.aaveV3Polygon.pool,
            STAKING_PROTOCOLS.aaveV3Polygon.abi,
            signer
        );
        
        const amountWei = ethers.parseUnits(amount.toString(), 6);
        const tx = await pool.withdraw(tokenAddress, amountWei, signer.address);
        const receipt = await tx.wait();
        
        return { success: true, txHash: receipt.hash };
    }

    // =============== NFT ===============

    /**
     * Fetch NFTs from multiple chains using Alchemy
     */
    async function fetchNFTs(address) {
        const alchemyKey = 'demo'; // Replace with real key
        const chains = [
            { id: 'ethereum', url: `https://eth-mainnet.g.alchemy.com/nft/v3/${alchemyKey}/getNFTsForOwner?owner=${address}` },
            { id: 'polygon', url: `https://polygon-mainnet.g.alchemy.com/nft/v3/${alchemyKey}/getNFTsForOwner?owner=${address}` }
        ];
        
        const allNFTs = [];
        
        for (const chain of chains) {
            try {
                const response = await fetch(chain.url);
                const data = await response.json();
                
                if (data.ownedNfts) {
                    for (const nft of data.ownedNfts) {
                        allNFTs.push({
                            id: `${chain.id}-${nft.contract.address}-${nft.tokenId}`,
                            chain: chain.id,
                            contractAddress: nft.contract.address,
                            tokenId: nft.tokenId,
                            name: nft.name || nft.title || `NFT #${nft.tokenId}`,
                            description: nft.description,
                            image: nft.image?.cachedUrl || nft.image?.originalUrl || nft.media?.[0]?.gateway,
                            collection: nft.contract.name || 'Unknown Collection'
                        });
                    }
                }
            } catch (e) {
                console.error(`Failed to fetch NFTs from ${chain.id}:`, e);
            }
        }
        
        return allNFTs;
    }

    /**
     * Transfer NFT
     */
    async function transferNFT(contractAddress, tokenId, toAddress, chainId) {
        const { signer } = await getSignerAndProvider(chainId);
        
        const nftAbi = [
            "function safeTransferFrom(address from, address to, uint256 tokenId)",
            "function transferFrom(address from, address to, uint256 tokenId)"
        ];
        
        const nft = new ethers.Contract(contractAddress, nftAbi, signer);
        
        try {
            const tx = await nft.safeTransferFrom(signer.address, toAddress, tokenId);
            const receipt = await tx.wait();
            return { success: true, txHash: receipt.hash };
        } catch (e) {
            // Fallback to transferFrom
            const tx = await nft.transferFrom(signer.address, toAddress, tokenId);
            const receipt = await tx.wait();
            return { success: true, txHash: receipt.hash };
        }
    }

    // =============== VOUCHERS ===============

    /**
     * Create crypto voucher (send to escrow)
     */
    async function createVoucher(amount, tokenAddress, password, chainId = 'polygon') {
        const { signer, provider } = await getSignerAndProvider(chainId);
        
        // Generate voucher key from password
        const voucherHash = ethers.keccak256(ethers.toUtf8Bytes(password + Date.now()));
        const voucherAddress = ethers.computeAddress(voucherHash);
        
        let tx;
        if (!tokenAddress || tokenAddress === ethers.ZeroAddress) {
            tx = await signer.sendTransaction({
                to: voucherAddress,
                value: ethers.parseEther(amount.toString())
            });
        } else {
            const token = new ethers.Contract(tokenAddress, ERC20_ABI, signer);
            tx = await token.transfer(voucherAddress, ethers.parseUnits(amount.toString(), 6));
        }
        
        const receipt = await tx.wait();
        
        // Store voucher in backend
        const code = generateVoucherCode();
        await fetch('/api/wallet/vouchers/create', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({
                token: localStorage.getItem('token'),
                code: code,
                amount_crypto: amount,
                crypto_symbol: tokenAddress ? 'USDT' : 'MATIC',
                voucher_address: voucherAddress,
                voucher_key: voucherHash,
                password_protected: !!password
            })
        });
        
        return {
            success: true,
            code: code,
            txHash: receipt.hash,
            voucherAddress: voucherAddress
        };
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

    // =============== PUBLIC API ===============

    return {
        // P2P
        createP2PTrade,
        confirmP2PPayment,
        releaseP2PCrypto,
        cancelP2PTrade,
        
        // Staking
        stakeETHLido,
        stakeMATICLido,
        supplyToAave,
        withdrawFromAave,
        
        // NFT
        fetchNFTs,
        transferNFT,
        
        // Vouchers
        createVoucher,
        
        // Config
        CONTRACTS,
        STAKING_PROTOCOLS
    };
})();

// Export
if (typeof module !== 'undefined' && module.exports) {
    module.exports = XipherWeb3;
}
