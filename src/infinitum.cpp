// Copyright (c) 2009-2015 The Infinitum developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "infinitum.h"
#include "main.h" // chainActive

#include <assert.h>

int GetCycle(int nHeight)
{
    // Genesis block (nHeight == 0) is not tallied for dustvote, so it actually
    //   precedes voting Cycle #0.
    // However, this function returns "0" for the genesis block as well, and that's
    //   actually a feature (see how CChain::SetTip() behaves when setting
    //   it to the genesis)
    return (nHeight - 1) / INFINITUM_CHAIN_CYCLE_BLOCKS;
}

int NumCyclesBetween(int nOutputHeight, int nInputHeight)
{
  assert(nOutputHeight > 0);
  assert(nInputHeight > 0);
  assert(nOutputHeight <= nInputHeight);
  return GetCycle(nInputHeight) - GetCycle(nOutputHeight);
}

bool TooManyCyclesBetween(int nOutputHeight, int nInputHeight)
{
  return NumCyclesBetween(nOutputHeight, nInputHeight) > INFINITUM_TRANSACTION_EXPIRATION_CYCLES;
}

bool IsSpendingPrunedInputs(const CCoinsViewCache &view, const CTransaction &tx, int nHeight, bool &fDustPruned)
{
    fDustPruned = false;
    BOOST_FOREACH(const CTxIn &txin, tx.vin) {
	const CCoins *coins = view.AccessCoins(txin.prevout.hash);
	// Check for abandoned old inputs that are unspendable.
	if (TooManyCyclesBetween(coins->nHeight, nHeight)) {
	    return true;
	}
	// Check for inputs pruned due to the dust-prune value vote.
	if (chainActive.GetMinSpendableOutputValue(coins->nHeight, nHeight) > coins->vout[txin.prevout.n].nValue) {
	    fDustPruned = true;
	    return true;
	}
    }
    return false;
}
