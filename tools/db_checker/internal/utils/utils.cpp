#include "utils.h"

#include <ripple/protocol/LedgerHeader.h>

void
GetStatesHashFromLedgerHeader(char* ledgerHeaderBlob, int size, char* hash)
{
    ripple::Slice slice{ledgerHeaderBlob, (size_t)size};
    ripple::LedgerHeader info = ripple::deserializeHeader(slice);
    ripple::uint256 h = info.accountHash;
    std::memcpy(hash, h.data(), ripple::uint256::size());
}

void
GetTxHashFromLedgerHeader(char* ledgerHeaderBlob, int size, char* hash)
{
    ripple::Slice slice{ledgerHeaderBlob, (size_t)size};
    ripple::LedgerHeader info = ripple::deserializeHeader(slice);
    ripple::uint256 h = info.txHash;
    std::memcpy(hash, h.data(), ripple::uint256::size());
}
