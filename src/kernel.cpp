// Copyright (c) 2012-2013 The PPCoin developers
// Copyright (c) 2014 The R3VCoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/assign/list_of.hpp>
#include <math.h>
#include "chainparams.h"
#include "kernel.h"
#include "script/script.h"
#include "txdb.h"
#include "validation.h"

using namespace std;

typedef map<int, uint64_t> MapModifierCheckpoints;

// This leads to a modifier selection interval of 27489 seconds,
// which is roughly 7 hours 38 minutes, just a bit shorter than
// the minimum stake age of 8 hours.
//unsigned int nModifierInterval = 13 * 60;
unsigned int nModifierInterval = 300; //2.9 hours, shorter than min stake age of 3 hours

// linear coin-aging function
int64_t GetCoinAgeWeightLinear(int64_t nIntervalBeginning, int64_t nIntervalEnd)
{
    // Kernel hash weight starts from 0 at the min age
    // this change increases active coins participating the hash and helps
    // to secure the network when proof-of-stake difficulty is low
    return max((int64_t)0, min(nIntervalEnd - nIntervalBeginning - Params().StakeMinAge(), (int64_t)Params().StakeMaxAge()));
}

/* PoSV: Coin-aging function
 * =================================================
 * WARNING
 * =================================================
 * The parameters used in this function are the
 * solutions to a set of intricate mathematical
 * equations chosen specifically to incentivise
 * owners of R3VCoin to participate in minting.
 * These parameters are also affected by the values
 * assigned to other variables such as expected
 * block confirmation time.
 * If you are merely forking the source code of
 * R3VCoin, it's highly UNLIKELY that this set of
 * parameters work for your purpose. In particular,
 * if you have tweaked the values of other variables,
 * this set of parameters are certainly no longer
 * valid. You should revert back to the linear
 * function above or the security of your network
 * will be significantly impaired.
 * In short, do not use or change this function
 * unless you have spoken to the author.
 * =================================================
 * DO NOT USE OR CHANGE UNLESS YOU ABSOLUTELY
 * KNOW WHAT YOU ARE DOING.
 * =================================================
 */
int64_t GetCoinAgeWeight(int64_t nIntervalBeginning, int64_t nIntervalEnd)
{
    if (nIntervalBeginning <= 0)
    {
        LogPrintf("WARNING *** GetCoinAgeWeight: nIntervalBeginning (%d) <= 0\n", nIntervalBeginning);
        return 0;
    }

    int64_t nSeconds = max((int64_t)0, nIntervalEnd - nIntervalBeginning - Params().StakeMinAge());
    double days = double(nSeconds) / (24 * 60 * 60);
    double weight = 0;

    if (days <= 7)
    {
        weight = -0.00408163 * pow(days, 3) + 0.05714286 * pow(days, 2) + days;
    }
    else
    {
        weight = 8.4 * log(days) - 7.94564525;
    }

    return min((int64_t)(weight * 24 * 60 * 60), (int64_t)Params().StakeMaxAge());
}

// Get the last stake modifier and its generation time from a given block
static bool GetLastStakeModifier(const CBlockIndex* pindex, uint64_t& nStakeModifier, int64_t& nModifierTime)
{
    if (!pindex)
        return error("GetLastStakeModifier: null pindex");
    while (pindex && pindex->pprev && !pindex->GeneratedStakeModifier())
        pindex = pindex->pprev;
    if (!pindex->GeneratedStakeModifier())
        return error("GetLastStakeModifier: no generation at genesis block");
    nStakeModifier = pindex->nStakeModifier;
    nModifierTime = pindex->GetBlockTime();
    return true;
}

// Get selection interval section (in seconds)
static int64_t GetStakeModifierSelectionIntervalSection(int nSection)
{
    assert (nSection >= 0 && nSection < 64);
    return (nModifierInterval * 63 / (63 + ((63 - nSection) * (MODIFIER_INTERVAL_RATIO - 1))));
}

// Get stake modifier selection interval (in seconds)
static int64_t GetStakeModifierSelectionInterval()
{
    int64_t nSelectionInterval = 0;
    for (int nSection=0; nSection<64; nSection++)
        nSelectionInterval += GetStakeModifierSelectionIntervalSection(nSection);

    if (GetBoolArg("-printstakemodifier", false))
        LogPrintf("GetStakeModifierSelectionInterval : %d\n", nSelectionInterval);

    return nSelectionInterval;
}

// select a block from the candidate blocks in vSortedByTimestamp, excluding
// already selected blocks in vSelectedBlocks, and with timestamp up to
// nSelectionIntervalStop.
static bool SelectBlockFromCandidates(vector<pair<int64_t, uint256> >& vSortedByTimestamp, map<uint256, const CBlockIndex*>& mapSelectedBlocks,
    int64_t nSelectionIntervalStop, uint64_t nStakeModifierPrev, const CBlockIndex** pindexSelected)
{
    bool fSelected = false;
    uint256 hashBest = uint256();
    *pindexSelected = (const CBlockIndex*) 0;
    BOOST_FOREACH(const PAIRTYPE(int64_t, uint256)& item, vSortedByTimestamp)
    {
        if (!mapBlockIndex.count(item.second))
            return error("SelectBlockFromCandidates: failed to find block index for candidate block %s", item.second.ToString().c_str());
        const CBlockIndex* pindex = mapBlockIndex[item.second];
        if (fSelected && pindex->GetBlockTime() > nSelectionIntervalStop)
            break;
        if (mapSelectedBlocks.count(pindex->GetBlockHash()) > 0)
            continue;
        // compute the selection hash by hashing its proof-hash and the
        // previous proof-of-stake modifier
        CDataStream ss(SER_GETHASH, 0);
        ss << pindex->hashProof << nStakeModifierPrev;
        arith_uint256 hashSelection_arith = UintToArith256(Hash(ss.begin(), ss.end()));
        // the selection hash is divided by 2**32 so that proof-of-stake block
        // is always favored over proof-of-work block. this is to preserve
        // the energy efficiency property
        if (pindex->IsProofOfStake())
            hashSelection_arith >>= 32;
        if (fSelected && hashSelection_arith < UintToArith256(hashBest))
        {
            hashBest = ArithToUint256(hashSelection_arith);
            *pindexSelected = (const CBlockIndex*) pindex;
        }
        else if (!fSelected)
        {
            fSelected = true;
            hashBest = ArithToUint256(hashSelection_arith);
            *pindexSelected = (const CBlockIndex*) pindex;
        }
    }
    if (GetBoolArg("-printstakemodifier", false))
        LogPrintf("SelectBlockFromCandidates: selection hash=%s\n", hashBest.ToString().c_str());
    return fSelected;
}

// Stake Modifier (hash modifier of proof-of-stake):
// The purpose of stake modifier is to prevent a txout (coin) owner from
// computing future proof-of-stake generated by this txout at the time
// of transaction confirmation. To meet kernel protocol, the txout
// must hash with a future stake modifier to generate the proof.
// Stake modifier consists of bits each of which is contributed from a
// selected block of a given block group in the past.
// The selection of a block is based on a hash of the block's proof-hash and
// the previous stake modifier.
// Stake modifier is recomputed at a fixed time interval instead of every 
// block. This is to make it difficult for an attacker to gain control of
// additional bits in the stake modifier, even after generating a chain of
// blocks.
bool ComputeNextStakeModifier(const CBlockIndex* pindexPrev, uint64_t& nStakeModifier, bool& fGeneratedStakeModifier)
{
    nStakeModifier = 0;
    fGeneratedStakeModifier = false;
    if (!pindexPrev)
    {
        fGeneratedStakeModifier = true;
        return true;  // genesis block's modifier is 0
    }
    // First find current stake modifier and its generation block time
    // if it's not old enough, return the same stake modifier
    int64_t nModifierTime = 0;
    if (!GetLastStakeModifier(pindexPrev, nStakeModifier, nModifierTime))
        return error("ComputeNextStakeModifier: unable to get last modifier");

    if (GetBoolArg("-printstakemodifier", false))
        LogPrintf("ComputeNextStakeModifier: prev modifier=0x%016x time=%s height=%d\n", nStakeModifier, DateTimeStrFormat(nModifierTime).c_str(), pindexPrev->nHeight);

    if (nModifierTime / nModifierInterval >= pindexPrev->GetBlockTime() / nModifierInterval)
        return true;

    // Sort candidate blocks by timestamp
    vector<pair<int64_t, uint256> > vSortedByTimestamp;
    vSortedByTimestamp.reserve(64 * nModifierInterval / nTargetSpacing);
    int64_t nSelectionInterval = GetStakeModifierSelectionInterval();
    int64_t nSelectionIntervalStart = (pindexPrev->GetBlockTime() / nModifierInterval) * nModifierInterval - nSelectionInterval;
    const CBlockIndex* pindex = pindexPrev;
    while (pindex && pindex->GetBlockTime() >= nSelectionIntervalStart)
    {
        vSortedByTimestamp.push_back(make_pair(pindex->GetBlockTime(), pindex->GetBlockHash()));
        pindex = pindex->pprev;
    }
    int nHeightFirstCandidate = pindex ? (pindex->nHeight + 1) : 0;
    reverse(vSortedByTimestamp.begin(), vSortedByTimestamp.end());
    sort(vSortedByTimestamp.begin(), vSortedByTimestamp.end());

    // Select 64 blocks from candidate blocks to generate stake modifier
    uint64_t nStakeModifierNew = 0;
    int64_t nSelectionIntervalStop = nSelectionIntervalStart;
    map<uint256, const CBlockIndex*> mapSelectedBlocks;
    
    if (GetBoolArg("-printstakemodifier", false))
        LogPrintf("ComputeNextStakeModifier: nSelectionIntervalStart=%u vSortedByTimestamp.size()=%u\n", nSelectionIntervalStart, (int)vSortedByTimestamp.size());

    for (int nRound=0; nRound<min(64, (int)vSortedByTimestamp.size()); nRound++)
    {
        // add an interval section to the current selection round
        nSelectionIntervalStop += GetStakeModifierSelectionIntervalSection(nRound);
        // select a block from the candidates of current round
        if (!SelectBlockFromCandidates(vSortedByTimestamp, mapSelectedBlocks, nSelectionIntervalStop, nStakeModifier, &pindex))
            return error("ComputeNextStakeModifier: unable to select block at round %d", nRound);
        // write the entropy bit of the selected block
        nStakeModifierNew |= (((uint64_t)pindex->GetStakeEntropyBit()) << nRound);
        // add the selected block from candidates to selected list
        mapSelectedBlocks.insert(make_pair(pindex->GetBlockHash(), pindex));
        if (GetBoolArg("-printstakemodifier", false))
            LogPrintf("ComputeNextStakeModifier: selected round %d stop=%s height=%d bit=%d\n", nRound, DateTimeStrFormat(nSelectionIntervalStop).c_str(), pindex->nHeight, pindex->GetStakeEntropyBit());
    }

    // Print selection map for visualization of the selected blocks
    if (fDebug && GetBoolArg("-printstakemodifier", false))
    {
        string strSelectionMap = "";
        // '-' indicates proof-of-work blocks not selected
        strSelectionMap.insert(0, pindexPrev->nHeight - nHeightFirstCandidate + 1, '-');
        pindex = pindexPrev;
        while (pindex && pindex->nHeight >= nHeightFirstCandidate)
        {
            // '=' indicates proof-of-stake blocks not selected
            if (pindex->IsProofOfStake())
                strSelectionMap.replace(pindex->nHeight - nHeightFirstCandidate, 1, "=");
            pindex = pindex->pprev;
        }
        BOOST_FOREACH(const PAIRTYPE(uint256, const CBlockIndex*)& item, mapSelectedBlocks)
        {
            // 'S' indicates selected proof-of-stake blocks
            // 'W' indicates selected proof-of-work blocks
            strSelectionMap.replace(item.second->nHeight - nHeightFirstCandidate, 1, item.second->IsProofOfStake()? "S" : "W");
        }
        LogPrintf("ComputeNextStakeModifier: selection height [%d, %d] map %s\n", nHeightFirstCandidate, pindexPrev->nHeight, strSelectionMap.c_str());
    }

    if (GetBoolArg("-printstakemodifier", false))
        LogPrintf("ComputeNextStakeModifier: new modifier=0x%016x time=%s nHeight=%d\n",nStakeModifierNew,
            DateTimeStrFormat(pindexPrev->GetBlockTime()).c_str(), pindexPrev->nHeight+1);

    nStakeModifier = nStakeModifierNew;
    fGeneratedStakeModifier = true;
    return true;
}

// The stake modifier used to hash for a stake kernel is chosen as the stake
// modifier about a selection interval later than the coin generating the kernel
static bool GetKernelStakeModifier(uint256 hashBlockFrom, uint64_t& nStakeModifier, int& nStakeModifierHeight, int64_t& nStakeModifierTime, bool fPrintProofOfStake)
{
    nStakeModifier = 0;
    if (!mapBlockIndex.count(hashBlockFrom))
        return error("GetKernelStakeModifier() : block not indexed");
    const CBlockIndex* pindexFrom = mapBlockIndex[hashBlockFrom];
    nStakeModifierHeight = pindexFrom->nHeight;
    nStakeModifierTime = pindexFrom->GetBlockTime();
    int64_t nStakeModifierSelectionInterval = GetStakeModifierSelectionInterval();
    const CBlockIndex* pindex = pindexFrom;
    // loop to find the stake modifier later by a selection interval
    //LogPrintf("GetKernelStakeModifier() - nStakeModifierHeight: %u, nStakeModifierTime:%u\n", nStakeModifierHeight, nStakeModifierTime);
    while (nStakeModifierTime < pindexFrom->GetBlockTime() + nStakeModifierSelectionInterval)
    {
        if (!chainActive.Next(pindex))
        {   // reached best block; may happen if node is behind on block chain
            if (fPrintProofOfStake || (pindex->GetBlockTime() + Params().StakeMinAge() - nStakeModifierSelectionInterval > GetAdjustedTime())) {
                return error("GetKernelStakeModifier() : reached best block at height %d (%u) from block at height %d, StakeMinAge %u, GetAdjustedTime %u, ActiveHeight %u",
                    pindex->nHeight, pindex->GetBlockTime(), pindexFrom->nHeight, Params().StakeMinAge(), GetAdjustedTime(), chainActive.Height());
            } else {
                return false;
            }
        }
        pindex = chainActive.Next(pindex);
        nStakeModifierHeight = pindex->nHeight;
        nStakeModifierTime = pindex->GetBlockTime();
        //LogPrintf("GetKernelStakeModifier() - nStakeModifierHeight: %u, nStakeModifierTime:%u\n", nStakeModifierHeight, nStakeModifierTime);
    }
    nStakeModifier = pindex->nStakeModifier;
    return true;
}

// PoSV kernel protocol
// coinstake must meet hash target according to the protocol:
// kernel (input 0) must meet the formula
//     hash(nStakeModifier + txPrev.block.nTime + txPrev.offset + txPrev.nTime + txPrev.vout.n + nTime) < bnTarget * nCoinDayWeight
// this ensures that the chance of getting a coinstake is proportional to the
// amount of coin age one owns.
// The reason this hash is chosen is the following:
//   nStakeModifier: scrambles computation to make it very difficult to precompute
//                  future proof-of-stake at the time of the coin's confirmation
//   txPrev.block.nTime: prevent nodes from guessing a good timestamp to
//                       generate transaction for future advantage
//   txPrev.offset: offset of txPrev inside block, to reduce the chance of 
//                  nodes generating coinstake at the same time
//   txPrev.nTime: reduce the chance of nodes generating coinstake at the same
//                 time
//   txPrev.vout.n: output number of txPrev, to reduce the chance of nodes
//                  generating coinstake at the same time
//   block/tx hash should not be used here as they can be generated in vast
//   quantities so as to generate blocks faster, degrading the system back into
//   a proof-of-work situation.
//
bool CheckStakeKernelHash(unsigned int nBits, const CBlockHeader& blockFrom, unsigned int nTxPrevOffset, const CTransactionRef& txPrev, const COutPoint& prevout, unsigned int nTimeTx, uint256& hashProofOfStake, uint256& targetProofOfStake, bool fPrintProofOfStake)
{
    const CTransaction& tx_prev = *txPrev;
    unsigned int nTimeBlockFrom = blockFrom.GetBlockTime();
    unsigned int nTimeTxPrev = tx_prev.nTime;

    // deal with missing timestamps in PoW blocks
    if (nTimeTxPrev == 0)
        nTimeTxPrev = nTimeBlockFrom;

    if (nTimeTx < nTimeTxPrev)  { // Transaction timestamp violation
        return error("CheckStakeKernelHash() : nTime violation: nTimeTx < txPrev.nTime, %u < %u", nTimeTx, nTimeTxPrev);
    }

    if (nTimeBlockFrom + Params().StakeMinAge() > nTimeTx) { // Min age requirement
        return error("CheckStakeKernelHash() : min age violation");
    }

    arith_uint256 bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);
    int64_t nValueIn = tx_prev.vout[prevout.n].nValue;
    int64_t nCoinAgeWeight = GetCoinAgeWeight((int64_t)nTimeTxPrev, (int64_t)nTimeTx);
    uint256 hashBlockFrom = blockFrom.GetHash();
    arith_uint256 bnCoinDayWeight = arith_uint256(nValueIn) * nCoinAgeWeight / COIN / (24 * 60 * 60);
    
    //High: nBits, nValue, coinAgeWeight (older transaction)
    targetProofOfStake = ArithToUint256(bnCoinDayWeight * bnTargetPerCoinDay);

    // Calculate hash
    uint64_t nStakeModifier = 0;
    int nStakeModifierHeight = 0;
    int64_t nStakeModifierTime = 0;

    if (!GetKernelStakeModifier(hashBlockFrom, nStakeModifier, nStakeModifierHeight, nStakeModifierTime, fPrintProofOfStake))
        return false;

    CHashWriter ss(SER_GETHASH, 0);
    ss << nStakeModifier;
    ss << nTimeBlockFrom << nTxPrevOffset << nTimeTxPrev << prevout.n << nTimeTx;
    hashProofOfStake = ss.GetHash();

    // Now check if proof-of-stake hash meets target protocol
    if (UintToArith256(hashProofOfStake) > UintToArith256(targetProofOfStake)) {
        
        /*if (fPrintProofOfStake)
        {
            LogPrintf("CheckStakeKernelHash() : using modifier 0x%016x at height=%d timestamp=%s for block from height=%d timestamp=%s txid=%s\n",
                nStakeModifier, nStakeModifierHeight,
                DateTimeStrFormat(nStakeModifierTime).c_str(),
                mapBlockIndex[hashBlockFrom]->nHeight,
                DateTimeStrFormat(blockFrom.GetBlockTime()).c_str(),
                tx_prev.GetHash().ToString());
            LogPrintf("CheckStakeKernelHash() : check modifier=0x%016x nTimeBlockFrom=%u nTxPrevOffset=%u nTimeTxPrev=%u nPrevout=%u nTimeTx=%u hashProof=%s(0x%x) targetProof=%s(0x%x)\n",
                nStakeModifier,
                nTimeBlockFrom, nTxPrevOffset, nTimeTxPrev, prevout.n, nTimeTx,
                hashProofOfStake.ToString(), UintToArith256(hashProofOfStake).getdouble(), 
                targetProofOfStake.ToString(), UintToArith256(targetProofOfStake).getdouble());
            LogPrintf("CheckStakeKernelHash() : check nBits=0x%016x nValueIn=%u nCoinAgeWeight=%u bnCoinDayWeight=0x%x bnTargetPerCoinDay=0x%x\n",
                nBits,
                nValueIn, nCoinAgeWeight,
                bnCoinDayWeight.getdouble(),
                bnTargetPerCoinDay.getdouble());
        }*/

        return false;
    } else {
        //LogPrintf("CheckStakeKernelHash() : OK\n");
        return true;
    }
}

// Check kernel hash target and coinstake signature
bool CheckProofOfStake(const CTransactionRef& tx, unsigned int nBits, uint256& hashProofOfStake, uint256& targetProofOfStake)
{
    const CTransaction& ctx = *tx;

    if (!ctx.IsCoinStake())
        return error("CheckProofOfStake() : called on non-coinstake %s", ctx.GetHash().ToString().c_str());

    // Kernel (input 0) must match the stake hash target per coin age (nBits)
    const CTxIn& txin = ctx.vin[0];

    // First try finding the previous transaction in database
    CTransactionRef txPrev;
    uint256 hashTxPrev = txin.prevout.hash;
    uint256 hashBlock;
    if (!GetTransaction(hashTxPrev, txPrev, Params().GetConsensus(), hashBlock, true))
        return error("CheckProofOfStake() : INFO: read txPrev failed");  // previous transaction not in main chain, may occur during initial download

    // Verify signature
    if (!VerifySignature(txPrev, tx, 0))
        return error("CheckProofOfStake() : VerifySignature failed on coinstake %s", ctx.GetHash().ToString().c_str());

    // Read block header
    if (!mapBlockIndex.count(hashBlock))
        return error("CheckProofOfStake() : block not indexed"); // unable to read block of previous transaction

    CBlock block;
    if (!ReadBlockFromDisk(block, mapBlockIndex[hashBlock], Params().GetConsensus()))
        return error("CheckProofOfStake() : read block failed"); // unable to read block of previous transaction

    if (!CheckStakeKernelHash(nBits, block, txin.prevout.n, txPrev, txin.prevout, ctx.nTime, hashProofOfStake, targetProofOfStake, fDebug))
        return error("CheckProofOfStake() : INFO: check kernel failed on coinstake %s, hashProof=%s", ctx.GetHash().ToString().c_str(), hashProofOfStake.ToString().c_str()); // may occur during initial download or if behind on block chain sync

    return true;
}

// Check whether the coinstake timestamp meets protocol
bool CheckCoinStakeTimestamp(int64_t nTimeBlock, int64_t nTimeTx)
{
    // v0.3 protocol
    return (nTimeBlock == nTimeTx);
}

// PoSV: total coin age spent in transaction, in the unit of coin-days.
// Only those coins meeting minimum age requirement counts. As those
// transactions not in main chain are not currently indexed so we
// might not find out about their coin age. Older transactions are
// guaranteed to be in main chain by sync-checkpoint. This rule is
// introduced to help nodes establish a consistent view of the coin
// age (trust score) of competing branches.
uint64_t GetCoinAge(const CTransaction& tx)
{
    arith_uint256 bnCentSecond = 0;  // coin age in the unit of cent-seconds
    uint64_t nCoinAge = 0;

    if (tx.IsCoinBase())
        return 0;

    for (unsigned int i = 0; i < tx.vin.size(); i++)
    {
        const CTxIn& txin = tx.vin[i];
        // First try finding the previous transaction in database
        CTransactionRef txPrev;
        uint256 hashTxPrev = txin.prevout.hash;
        uint256 hashBlock;
        if (!GetTransaction(hashTxPrev, txPrev, Params().GetConsensus(), hashBlock, true))
            continue;  // previous transaction not in main chain

        // Read block header
        CBlock block;
        if (!mapBlockIndex.count(hashBlock))
            return 0; // unable to read block of previous transaction
        if (!ReadBlockFromDisk(block, mapBlockIndex[hashBlock], Params().GetConsensus()))
            return 0; // unable to read block of previous transaction
        if (block.nTime + Params().StakeMinAge() > tx.nTime)
            continue; // only count coins meeting min age requirement

        const CTransaction& ctxPrev = *txPrev;
        
        int64_t nTime = ctxPrev.nTime;
        // deal with missing timestamps in PoW blocks
        if (block.IsProofOfWork()) {
            nTime = block.nTime;
        }

        if (tx.nTime < nTime)
            return 0;  // Transaction timestamp violation

        int64_t nValueIn = ctxPrev.vout[txin.prevout.n].nValue;
        int64_t nTimeWeight = GetCoinAgeWeight(nTime, tx.nTime);
        bnCentSecond += arith_uint256(nValueIn) * nTimeWeight / CENT;

        if (fDebug && GetBoolArg("-printcoinage", false))
            LogPrintf("coin age nValueIn=%s nTime=%d, txPrev.nTime=%d, nTimeWeight=%s bnCentSecond=%s\n",
                nValueIn, tx.nTime, nTime, nTimeWeight, bnCentSecond.ToString().c_str());
    }

    arith_uint256 bnCoinDay = bnCentSecond * CENT / COIN / (24 * 60 * 60);
    if (fDebug && GetBoolArg("-printcoinage", false))
        LogPrintf("coin age bnCoinDay=%s\n", bnCoinDay.ToString().c_str());
    nCoinAge = ArithToUint256(bnCoinDay).GetUint64(0);
    return nCoinAge;
}


uint64_t GetCoinAge(const CTransactionRef& tx)
{
    const CTransaction& ctx = *tx;
    return GetCoinAge(ctx);
}

// PoSV: total coin age spent in block, in the unit of coin-days.
uint64_t GetCoinAge(const CBlock& block)
{
    uint64_t nCoinAge = 0;

    for (unsigned int i = 0; i < block.vtx.size(); i++) {
        const CTransaction& ctx = *block.vtx[i];
        nCoinAge += GetCoinAge(ctx);
    }

    if (fDebug && GetBoolArg("-printcoinage", false))
        LogPrintf("block coin age total nCoinDays=%s\n", nCoinAge);
    return nCoinAge;
}


