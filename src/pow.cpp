// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "chainparams.h"
#include "primitives/block.h"
#include "uint256.h"
#include "util.h"
#include "validation.h"

unsigned int static KimotoGravityWell(const CBlockIndex* pindexLast, uint64_t TargetBlocksSpacingSeconds, uint64_t PastBlocksMin, uint64_t PastBlocksMax, const Consensus::Params& params)
{
    const CBlockIndex  *BlockLastSolved = pindexLast;
    const CBlockIndex  *BlockReading    = pindexLast;

    uint64_t  PastBlocksMass        = 0;
    int64_t   PastRateActualSeconds = 0;
    int64_t   PastRateTargetSeconds = 0;
    double  PastRateAdjustmentRatio = double(1);
    arith_uint256 PastDifficultyAverage;
    arith_uint256 PastDifficultyAveragePrev;
    arith_uint256 bnProofOfWorkLimit = UintToArith256(params.powLimit);
    arith_uint256 bnProofOfStakeLimit = UintToArith256(Params().ProofOfStakeLimit());
    arith_uint256 bnProofOfStakeReset = UintToArith256(uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff")); // 1
    arith_uint256 BlockDifficulty;
    arith_uint256 BlockDifficultyAverage;

    double EventHorizonDeviation;
    double EventHorizonDeviationFast;
    double EventHorizonDeviationSlow;

    bool fProofOfStake = false;
    if (pindexLast && pindexLast->nHeight >= Params().LastProofOfWorkHeight())
        fProofOfStake = true;

    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || !fProofOfStake) //PoW always lowest difficulty
    {
        return bnProofOfWorkLimit.GetCompact();
    }
    else if (fProofOfStake && (uint64_t)(BlockLastSolved->nHeight - Params().LastProofOfWorkHeight()) < PastBlocksMin)
    {
        // difficulty is reset at the first PastBlocksMin PoSV blocks, which will be used to calculate PastDifficultyAverage later
       return bnProofOfStakeReset.GetCompact();
    }

    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > (fProofOfStake ? Params().LastProofOfWorkHeight() : 0); i++)
    {
        if (PastBlocksMax > 0 && i > PastBlocksMax)
            break;

        PastBlocksMass++;

        if (i == 1)
        {
            PastDifficultyAverage.SetCompact(BlockReading->nBits);
            BlockDifficulty = PastDifficultyAverage;
            //LogPrintf("KimotoGravityWell() - i: %u,       BlockDifficulty:  %08x  %s %u\n", i, BlockDifficulty.GetCompact(), ArithToUint256(BlockDifficulty).ToString(), BlockDifficulty.getdouble());
            //LogPrintf("KimotoGravityWell() - i: %u, PastDifficultyAverage:  %08x  %s %u\n", i, PastDifficultyAverage.GetCompact(), ArithToUint256(PastDifficultyAverage).ToString(), PastDifficultyAverage.getdouble());
        }
        else
        {
            BlockDifficulty.SetCompact(BlockReading->nBits);
            //LogPrintf("KimotoGravityWell() - i: %u,       BlockDifficulty:  %08x  %s %u %u %u\n", i, BlockDifficulty.GetCompact(), ArithToUint256(BlockDifficulty).ToString(), BlockDifficulty.getdouble(), (int64_t)(1-1/i), (int64_t)(1/i));
            BlockDifficultyAverage = BlockDifficulty / (int64_t)i;
            //LogPrintf("KimotoGravityWell() - i: %u,    BlockDifficultyAvg:  %08x  %s %u\n", i, BlockDifficultyAverage.GetCompact(), ArithToUint256(BlockDifficultyAverage).ToString(), BlockDifficultyAverage.getdouble());
            PastDifficultyAverage = PastDifficultyAveragePrev * (int64_t)(i-1);
            PastDifficultyAverage = PastDifficultyAverage / (int64_t)i;
            //LogPrintf("KimotoGravityWell() - i: %u, PastDifficultyAverage:  %08x  %s %u\n", i, PastDifficultyAverage.GetCompact(), ArithToUint256(PastDifficultyAverage).ToString(), PastDifficultyAverage.getdouble());
            PastDifficultyAverage = BlockDifficultyAverage + PastDifficultyAverage;
            //LogPrintf("KimotoGravityWell() - i: %u, PastDifficultyAverage:  %08x  %s %u\n", i, PastDifficultyAverage.GetCompact(), ArithToUint256(PastDifficultyAverage).ToString(), PastDifficultyAverage.getdouble());
        }
        PastDifficultyAveragePrev = PastDifficultyAverage;

        PastRateActualSeconds = BlockLastSolved->GetBlockTime() - BlockReading->GetBlockTime();
        PastRateTargetSeconds = TargetBlocksSpacingSeconds * PastBlocksMass;
        PastRateAdjustmentRatio = double(1);

        if (PastRateActualSeconds < 0)
            PastRateActualSeconds = 0;

        if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0)
            PastRateAdjustmentRatio = double(PastRateTargetSeconds) / double(PastRateActualSeconds);

        EventHorizonDeviation = 1 + (0.7084 * pow((double(PastBlocksMass)/double(144)), -1.228));
        EventHorizonDeviationFast = EventHorizonDeviation;
        EventHorizonDeviationSlow = 1 / EventHorizonDeviation;

        if (PastBlocksMass >= PastBlocksMin)
        {
            if ((PastRateAdjustmentRatio <= EventHorizonDeviationSlow) || (PastRateAdjustmentRatio >= EventHorizonDeviationFast))
            {
                assert(BlockReading);
                break;
            }
        }

        if (BlockReading->pprev == NULL)
        {
            assert(BlockReading);
            break;
        }

        BlockReading = BlockReading->pprev;

    }

    //LogPrintf("Rate:  %u\n", (double(PastRateActualSeconds)/double(PastRateTargetSeconds)));
    //LogPrintf("Average Before:  %08x  %s %u\n", PastDifficultyAverage.GetCompact(), ArithToUint256(PastDifficultyAverage).ToString(), PastDifficultyAverage.getdouble());

    if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0)
    {
        PastDifficultyAverage *= PastRateActualSeconds;
        PastDifficultyAverage /= PastRateTargetSeconds;
    }
    //LogPrintf("Average After:  %08x  %s %u\n", PastDifficultyAverage.GetCompact(), ArithToUint256(PastDifficultyAverage).ToString(), PastDifficultyAverage.getdouble());

    arith_uint256 bnNew = PastDifficultyAverage;

    if (!fProofOfStake && bnNew > bnProofOfWorkLimit)
    {
        bnNew = bnProofOfWorkLimit;
    }
    else if (fProofOfStake && bnNew > bnProofOfStakeLimit)
    {
        bnNew = bnProofOfStakeLimit;
    }

     /// debug print
    if (fDebug)
    {
        LogPrintf("Difficulty Retarget - Kimoto Gravity Well\n");
        LogPrintf("PastBlocksMass = %u, PastRateAdjustmentRatio = %g\n", PastBlocksMass, PastRateAdjustmentRatio);
        LogPrintf("Before: %08x  %s %u\n", BlockLastSolved->nBits, ArithToUint256(arith_uint256().SetCompact(BlockLastSolved->nBits)).ToString(), arith_uint256().SetCompact(BlockLastSolved->nBits).getdouble());
        LogPrintf("After:  %08x  %s %u\n", bnNew.GetCompact(), ArithToUint256(bnNew).ToString(), bnNew.getdouble());
    }

     return bnNew.GetCompact();
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const Consensus::Params& params)
{
    return CalculateNextWorkRequired(pindexLast, 0, params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    // Genesis block
    if (pindexLast == NULL)
        return UintToArith256(params.powLimit).GetCompact();

    // mine blocks at the lowest diff
    if (Params().GetConsensus().fPowAllowMinDifficultyBlocks) {
        if (chainActive.Tip()->nHeight < Params().LastProofOfWorkHeight())
            return UintToArith256(params.powLimit).GetCompact();
        else 
            return UintToArith256(Params().ProofOfStakeLimit()).GetCompact();
    }

    int64_t PastSecondsMin = Params().StakeMinAge();
    int64_t PastSecondsMax = 604800; //7 * 24 * 60 * 60 (1 week)

    uint64_t PastBlocksMin = PastSecondsMin / nTargetSpacing;
    uint64_t PastBlocksMax = PastSecondsMax / nTargetSpacing;
    
    return KimotoGravityWell(pindexLast, nTargetSpacing, PastBlocksMin, PastBlocksMax, params);
}

bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params& params)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;

    bnTarget.SetCompact(nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    // Check proof of work matches claimed amount
    if (UintToArith256(hash) > bnTarget)
        return false;

    return true;
}
