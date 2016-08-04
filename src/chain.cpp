// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "infinitum.h"
#include "chain.h"

#include <boost/foreach.hpp>

using namespace std;

/**
 * CChain implementation
 */

CAmount CChain::GetMinSpendableOutputValue(uint64_t nOutputBlockHeight, uint64_t nInputBlockHeight) const {
    // Example: Start 0, End 2. The transaction with the UTXO is somewhere inside Cycle 0, and the transaction
    //  that is trying to spend it is somewhere inside Cycle 2.
    // We have to check then what the dust vote result was at the end of Cycle 0 (vmin[0]), and then what the 
    //  result was at the end of Cycle 1 (vmin[1]), which are the cycle borders that are crossed between the 
    //  middle of Cycle 0 and the middle of Cycle 2.
    
    int nStartCycle = GetCycle(nOutputBlockHeight);
    int nEndCycle = GetCycle(nInputBlockHeight);
    
    CAmount nMaximumFound = 0; // The highest prune threshold crossed is the one that we apply.
    for (int nCycle = nStartCycle; nCycle < nEndCycle; ++nCycle)
	nMaximumFound = std::max(nMaximumFound, vMinSpendableOutputValues[nCycle]);
    return nMaximumFound;
}

void CChain::SetTip(CBlockIndex *pindex) {
    if (pindex == NULL) {
        vChain.clear();
	vMinSpendableOutputValues.clear(); // Infinitum:: update vMinSpendableOutputValues
        return;
    }
    /*
      // Infinitum:: update vMinSpendableOutputValues
      Original code.
      
    vChain.resize(pindex->nHeight + 1);
    while (pindex && vChain[pindex->nHeight] != pindex) {
        vChain[pindex->nHeight] = pindex;
        pindex = pindex->pprev;
    }
    */

    // Infinitum:: update vMinSpendableOutputValues as well as the vChain

    // Height ints:
    // -1 = no genesis block
    //  0 = just the genesis block (block at height #0)
    //int nOldChainHeight = vChain.size() - 1;
    int nNewChainHeight = pindex->nHeight;
    int nFirstCommonNodeHeight = nNewChainHeight;
    
    vChain.resize(nNewChainHeight + 1);
    while (pindex && vChain[pindex->nHeight] != pindex) {
        vChain[pindex->nHeight] = pindex;
        pindex = pindex->pprev;
	--nFirstCommonNodeHeight;
    }

    // From first common height to new height, the vminspendableoutputvalues need recomputing
    // the interval touched by the first common height + 1 (changed block) is the first interval to recompute
    // GetCycle won't itself return e.g. -1 if you pass 0 (genesis) to it; it will return 0 which is incorrect 
    //  in theory but it does what we want. we want this because endcycle will be -1 then, making sure no
    //  update runs.
    int nStartCycle = GetCycle(nFirstCommonNodeHeight + 1);

    // the end cycle is the first *whole* cycle given by the new chain height
    // if the end cycle is less than the start cycle, then the spendable output values array is empty
    // end cycle can be -1 if there is no end cycle, which resizes the vminspend vector to 0
    int nEndCycle = GetCycle(nNewChainHeight);
    if (nNewChainHeight % INFINITUM_CHAIN_CYCLE_BLOCKS != 0)
	--nEndCycle; // new height block doesn't land squarely at the end of its cycle, so it isn't whole

    // cycles past the end are deleted
    vMinSpendableOutputValues.resize(nEndCycle + 1); // Cycle #0 is the first one, so +1 to fit it in

    // cycles from start to end are recomputed/re-tallied
    // nEndCycle is less than nStartCycle for most of the calls to this method, in which case this loop
    //   doesn't run (i.e. nothing to update)
    for (int nCycle = nStartCycle; nCycle <= nEndCycle; ++nCycle) {

	std::vector<int> vVoteCounts; // element 0 = ndustvote 0's count; element 1 = ndustvote 1's count etc.
	vVoteCounts.resize(256);

	int64_t nFirstHeight = 1 + (nStartCycle * INFINITUM_CHAIN_CYCLE_BLOCKS);
	int64_t nLastHeight = (nEndCycle + 1) * INFINITUM_CHAIN_CYCLE_BLOCKS;

	// Tally votes
	for (int i = nFirstHeight; i <= nLastHeight; ++i) {
	    int nVote = (vChain[i]->nVersion >> 8) & 0xFF;
	    ++vVoteCounts[nVote];
	}

	// Compute the winning vote
	CAmount nWinningVote = 0;

	// will discard the lowest 5% votes and keep whatever the next vote is,
	// so the dust value is the lowest common denominator among the 95% of miners that
	// are on the side of wanting the highest dust value.
	int nDiscardBudget = INFINITUM_CHAIN_CYCLE_BLOCKS / 20;
	int nIndex = 0;
	BOOST_FOREACH(int nVoteCount, vVoteCounts) {
	  nDiscardBudget -= nVoteCount;
	  if (nDiscardBudget < 0) {
	    nWinningVote = 1 << nIndex; // votes for "0" say dust is 2^0, votes for "1" say dust is 2^1, etc.
	    break;
	  }
	  ++nIndex;
	}

	// Update it
	vMinSpendableOutputValues[nCycle] = nWinningVote;
    }
    
    
    /*
      // Old settip (less complicated and more expensive, recomputes everything every time)

    if (pindex == NULL) {
        vChain.clear();
	vMinSpendableOutputValues.clear();
        return;
    }
    vChain.resize(pindex->nHeight + 1);
    while (pindex && vChain[pindex->nHeight] != pindex) {
        vChain[pindex->nHeight] = pindex;
        pindex = pindex->pprev;
    }

    // **************************************************************************************************
    // **************************************************************************************************
    // **************************************************************************************************
    // **************************************************************************************************
    // Infinitum:: update the spendable values tally for UTXOs that go through snapshot events.
    // **************************************************************************************************
    // **************************************************************************************************
    // **************************************************************************************************
    // **************************************************************************************************
    // **************************************************************************************************
    // FIXME/TODO: MUST do the nonstupid version of this that doesn't recompute the entire thing 
    //  every time a tip is set. This is just to get the rest of the code done faster.
    // **************************************************************************************************
    // **************************************************************************************************
    // **************************************************************************************************
    // **************************************************************************************************

    vMinSpendableOutputValues.clear(); // yep we are stupid right now
    std::vector<int> vVoteCounts; // element 0 = ndustvote 0's count; element 1 = ndustvote 1's count etc.
    vVoteCounts.resize(256);
    int nIntervalVotes = 0; // total votes cast into vVoteCounts (i.e. num block headers tallied so far)
    for (uint64_t nIndex = 1; nIndex < vChain.size(); ++nIndex) {

        int nVote = (vChain[nIndex]->nVersion >> 8) & 0xFF;

	++vVoteCounts[nVote];
	++nIntervalVotes;

	// done tallying 2 whole years of block headers
	if (nIntervalVotes >= INFINITUM_CHAIN_CYCLE_BLOCKS) {
	
	CAmount nWinningVote = 0;

	// will discard the lowest 5% votes and keep whatever the next vote is,
	// so the dust value is the lowest common denominator among the 95% of miners that
	// are on the side of wanting the highest dust value.
	int nDiscardBudget = INFINITUM_CHAIN_CYCLE_BLOCKS / 20;
	int nIndex = 0;
	BOOST_FOREACH(int nVoteCount, vVoteCounts) {
	  nDiscardBudget -= nVoteCount;
	  if (nDiscardBudget < 0) {
	    nWinningVote = 1 << nIndex; // votes for "0" say dust is 2^0, votes for "1" say dust is 2^1, etc.
	    break;
	  }
	  ++nIndex;
	}

	// the winner "prune dust" value for this block cycle
	vMinSpendableOutputValues.push_back(nWinningVote);

	// reset for the next cycle, if there is any
	vVoteCounts.clear();
	vVoteCounts.resize(256);
	nIntervalVotes = 0;
      }
    }
    */
}

CBlockLocator CChain::GetLocator(const CBlockIndex *pindex) const {
    int nStep = 1;
    std::vector<uint256> vHave;
    vHave.reserve(32);

    if (!pindex)
        pindex = Tip();
    while (pindex) {
        vHave.push_back(pindex->GetBlockHash());
        // Stop when we have added the genesis block.
        if (pindex->nHeight == 0)
            break;
        // Exponentially larger steps back, plus the genesis block.
        int nHeight = std::max(pindex->nHeight - nStep, 0);
        if (Contains(pindex)) {
            // Use O(1) CChain index if possible.
            pindex = (*this)[nHeight];
        } else {
            // Otherwise, use O(log n) skiplist.
            pindex = pindex->GetAncestor(nHeight);
        }
        if (vHave.size() > 10)
            nStep *= 2;
    }

    return CBlockLocator(vHave);
}

const CBlockIndex *CChain::FindFork(const CBlockIndex *pindex) const {
    if (pindex == NULL) {
        return NULL;
    }
    if (pindex->nHeight > Height())
        pindex = pindex->GetAncestor(Height());
    while (pindex && !Contains(pindex))
        pindex = pindex->pprev;
    return pindex;
}

/** Turn the lowest '1' bit in the binary representation of a number into a '0'. */
int static inline InvertLowestOne(int n) { return n & (n - 1); }

/** Compute what height to jump back to with the CBlockIndex::pskip pointer. */
int static inline GetSkipHeight(int height) {
    if (height < 2)
        return 0;

    // Determine which height to jump back to. Any number strictly lower than height is acceptable,
    // but the following expression seems to perform well in simulations (max 110 steps to go back
    // up to 2**18 blocks).
    return (height & 1) ? InvertLowestOne(InvertLowestOne(height - 1)) + 1 : InvertLowestOne(height);
}

CBlockIndex* CBlockIndex::GetAncestor(int height)
{
    if (height > nHeight || height < 0)
        return NULL;

    CBlockIndex* pindexWalk = this;
    int heightWalk = nHeight;
    while (heightWalk > height) {
        int heightSkip = GetSkipHeight(heightWalk);
        int heightSkipPrev = GetSkipHeight(heightWalk - 1);
        if (pindexWalk->pskip != NULL &&
            (heightSkip == height ||
             (heightSkip > height && !(heightSkipPrev < heightSkip - 2 &&
                                       heightSkipPrev >= height)))) {
            // Only follow pskip if pprev->pskip isn't better than pskip->pprev.
            pindexWalk = pindexWalk->pskip;
            heightWalk = heightSkip;
        } else {
            pindexWalk = pindexWalk->pprev;
            heightWalk--;
        }
    }
    return pindexWalk;
}

const CBlockIndex* CBlockIndex::GetAncestor(int height) const
{
    return const_cast<CBlockIndex*>(this)->GetAncestor(height);
}

void CBlockIndex::BuildSkip()
{
    if (pprev)
        pskip = pprev->GetAncestor(GetSkipHeight(nHeight));
}
