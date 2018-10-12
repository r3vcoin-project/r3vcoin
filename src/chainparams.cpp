// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/merkle.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

#include "chainparamsseeds.h"

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "Sep 18th 2018 was such a nice day...";
    const CScript genesisOutputScript = CScript() << ParseHex("040184710fa689ad5023690c80f3a49c8f13f8d45b8c857fbcbc8bc4a8e4d3eb4b10f4d4604fa08dce601aaf0f470216fe1b51850b4acf21b179c45070ac7b03a9") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 4200000;
        consensus.BIP34Height = 2147483647; //No force BIP34 until the end
        consensus.BIP34Hash = uint256S("");
        consensus.BIP65Height = 2147483647; //No force BIP65 until the end
        consensus.BIP66Height = 2147483647; //No force BIP66 until the end
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); 
        consensus.nPowTargetTimespan = 86400; // 24 hours
        consensus.nPowTargetSpacing = 60; // 60 seconds
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1080; // 75% of 1440
        consensus.nMinerConfirmationWindow = 1440; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1485561600; // January 28, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1517356801; // January 31st, 2018

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1485561600; // January 28, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 0; // January 31st, 2018

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000001ead1cf90ca0e");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0xa69d95c81f2a6eaa4005ebc1905e2cd77f8ebbac2ec3c59992b958c13eb1d1ea"); //13369

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xfb;
        pchMessageStart[1] = 0xc0;
        pchMessageStart[2] = 0xb6;
        pchMessageStart[3] = 0xdb;
        nDefaultPort = 13843;
        nPruneAfterHeight = 100000;

        // PoSV
        bnProofOfStakeLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); 
        nLastProofOfWorkHeight = 200;
        nStakeMinAge = 10800; //8 * 60 * 60; // 8 hours
        nStakeMaxAge = 45 * 24 *  60 * 60; // 45 days

        genesis = CreateGenesisBlock(1537228800, 4290069, 0x1e0ffff0, 1, 16665000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();

        assert(consensus.hashGenesisBlock == uint256S("0x1d8b5363be3e70575fbc6fde8ca5e3207d3a932b25a519130385e41dbb274861"));
        assert(genesis.hashMerkleRoot == uint256S("0xe34480b213a07b15a4390f71e038a636ac5ea5269cb7ac05e98e2902aaac2261"));

        // Note that of those with the service bits flag, most only support a subset of possible options
        vSeeds.push_back(CDNSSeedData("sg.seed.r3v.co", "sg.seed.r3v.co"));
        vSeeds.push_back(CDNSSeedData("jp.seed.r3v.co", "jp.seed.r3v.co"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,60);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,50);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,189);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container<std::vector<unsigned char> >();

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            (   0, uint256S("0x1d8b5363be3e70575fbc6fde8ca5e3207d3a932b25a519130385e41dbb274861"))
            (   4, uint256S("0x35e155604f26bfac675a8786f2550c126067ad8ed0bbc54c6692ed0a7422e0e3"))
            (   8, uint256S("0x9795a116e3b7abe29f6e0849939d98a7181897965ba47e0886b38d01b386bc9c"))
            (  16, uint256S("0xd85f0c1fbbffce73dad49ac1e7c087c086356d0098bae897254d82288c5374cf"))
            (  32, uint256S("0x36b3576df83780a062799926e51f4e6004200b1d35a8e9f2577cad1f75fef1ff"))
            (  64, uint256S("0x3bf3dc464621d872b6d1c2a747930b2dde0f715ddb1343bad482a82d6439a642"))
            ( 128, uint256S("0xee00f470117337bb998238526975abd3450c7491be4e2254aadfde49ef8f969d"))
            ( 256, uint256S("0xe11bc0ae0cb2e7492d46d7aae962190d4638ac4b166050ec16a442d144360a58"))
            ( 512, uint256S("0x542b430da6db69e0eca35141d60bfea8ca7caf841c4122e0f32e6c87ba0efe1d"))
            (1024, uint256S("0xe2e3af84c05f0f9f0932fc634ccf75001655721cc9ed566c7db0f18f4fe04657"))
            (2048, uint256S("0x3e804157591e93368321430a528ec56c10fb39a83a73d4d55f5860806d77b2b4"))
            (4096, uint256S("0xeb43690c3ba5b21c5905e01663faf7cf5ceaf7a1f3d980fdd41f932ea60947a2"))
            (8192, uint256S("0x5f6d52156a1fda768ab6e31c0d6c5148885ffc55fb14267a6f4c35182c775dd5"))
        };

        chainTxData = ChainTxData{
            // Data as of block 8a02ce2ceadbccd333e1e28b5e9c300638131ae7fd3f673127210da633fe5f38 (height 120).
            //1516328121, // * UNIX timestamp of last known number of transactions
            //121,  // * total number of transactions between genesis and that timestamp
                    //   (the tx=... number in the SetBestChain debug.log lines)
            //0.00007 = 121 / (1516328121 - 1514764800)    // * estimated number of transactions per second after that timestamp
            1539070550,
            1,
            0.01667 // 1 transaction per block per 60 seconds
        };
        
        modifierCheckpointData = (CModifierCheckpointData) {
            boost::assign::map_list_of
                (    0, uint64_t(0x0faf9118) )
                /*(    2, 0xcd14ad17 )
                (    4, 0x5e457b8c )
                (    8, 0xa2748ec7 )
                (   16, 0x9616e10f )
                (   32, 0x3156039f )
                (   64, 0xc283f40b )
                (  128, 0x43fbad93 )
                (  256, 0x47750cce )
                (  512, 0xc1be8d21 )
                ( 1024, 0xafea7ca5 )
                ( 2048, 0x5f290305 )
                ( 4096, 0x77adbfd7 )
                ( 8192, 0x77f0fcd0 )*/
        };

    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 4200000;
        consensus.BIP34Height = 2147483647; //No force BIP34 until the end
        consensus.BIP34Hash = uint256S("");
        consensus.BIP65Height = 2147483647; //No force BIP65 until the end
        consensus.BIP66Height = 2147483647; //No force BIP66 until the end
        consensus.powLimit = uint256S("00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 86400; // 24 hours
        consensus.nPowTargetSpacing = 60; // 60 seconds
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1080; // 75% for testchains
        consensus.nMinerConfirmationWindow = 1440; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1483228800; // January 1, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1517356801; // January 31st, 2018

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1483228800; // January 1, 2017
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 0; // January 31st, 2018

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00"); //0

        pchMessageStart[0] = 0xfe;
        pchMessageStart[1] = 0xc3;
        pchMessageStart[2] = 0xb9;
        pchMessageStart[3] = 0xde;
        nDefaultPort = 15843;
        nPruneAfterHeight = 1000;
        nLastProofOfWorkHeight = 200; // Last POW block

        genesis = CreateGenesisBlock(1535760000, 1629334, 0x1e0ffff0, 1, 16665000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        
        assert(consensus.hashGenesisBlock == uint256S("0xd0eed5514212911af67c13a3ebe6ea738650782ec4d4740d930262e98f1174fa"));
        assert(genesis.hashMerkleRoot == uint256S("0xe34480b213a07b15a4390f71e038a636ac5ea5269cb7ac05e98e2902aaac2261"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        //vSeeds.push_back(CDNSSeedData("r3vcoin.org", "testnet-seed.r3vcoin.org"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,58);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            (0, uint256S("0xd0eed5514212911af67c13a3ebe6ea738650782ec4d4740d930262e98f1174fa"))
        };

        chainTxData = ChainTxData{
            // Data as of block 02bc6e4f150f62cc4bc62e67bbf2a2cbfe0cc57a436a3474b258a88b7acb7240 (height 99)
            //1516947604,
            //100,
            //0.00007 = 100 / (1516947604 - 1515542400)
            1535760000,
            1,
            0.0167
        };

        modifierCheckpointData = (CModifierCheckpointData) {
            boost::assign::map_list_of
            (   0, 0xfd11f4e7 )
        };
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP34Height = 2147483647; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 0; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 0; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 86400; // 24 hours
        consensus.nPowTargetSpacing = 60; // 60 seconds
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 1; // 75% for testchains
        consensus.nMinerConfirmationWindow = 1; // Faster than normal for regtest
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 999999999999ULL;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        pchMessageStart[0] = 0xff;
        pchMessageStart[1] = 0xc4;
        pchMessageStart[2] = 0xba;
        pchMessageStart[3] = 0xdf;
        nDefaultPort = 18844;
        nPruneAfterHeight = 1000;
        nLastProofOfWorkHeight = 200;
        bnProofOfStakeLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");

        genesis = CreateGenesisBlock(1535760000, 2, 0x207fffff, 1, 16665000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();

        assert(consensus.hashGenesisBlock == uint256S("0x0cccd3c5fca0af8383865dc88f07def9d86b03cdcf79619a40a708419b917bf9"));
        assert(genesis.hashMerkleRoot == uint256S("0xe34480b213a07b15a4390f71e038a636ac5ea5269cb7ac05e98e2902aaac2261"));
        
        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true; 

        checkpointData = (CCheckpointData){
            boost::assign::map_list_of
            ( 0, uint256S("0x0cccd3c5fca0af8383865dc88f07def9d86b03cdcf79619a40a708419b917bf9"))
        };

        chainTxData = ChainTxData{
            1535760000,
            1,
            0.0167
        };

        modifierCheckpointData = (CModifierCheckpointData) {
            boost::assign::map_list_of
            (   0, 0xfd11f4e7 )
        };
        
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SCRIPT_ADDRESS2] = std::vector<unsigned char>(1,58);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();
    }

    void UpdateBIP9Parameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
    {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
    }
};
static CRegTestParams regTestParams;

static CChainParams *pCurrentParams = 0;

const CChainParams &Params() {
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams& Params(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
            return mainParams;
    else if (chain == CBaseChainParams::TESTNET)
            return testNetParams;
    else if (chain == CBaseChainParams::REGTEST)
            return regTestParams;
    else
        throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

void UpdateRegtestBIP9Parameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    regTestParams.UpdateBIP9Parameters(d, nStartTime, nTimeout);
}
