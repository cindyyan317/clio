#include "utils.h"

#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/TxMeta.h>

#include <cstddef>

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

void
GetLedgerHashFromLedgerHeader(char* ledgerHeaderBlob, int size, char* hash)
{
    ripple::Slice slice{ledgerHeaderBlob, (size_t)size};
    ripple::LedgerHeader info = ripple::deserializeHeader(slice, true);
    ripple::uint256 h = info.hash;
    std::memcpy(hash, h.data(), ripple::uint256::size());
}

void
GetAccountTxnIDFromTx(
    char* txBlob,
    int txSize,
    char* metaBlob,
    int metaSize,
    char* accounts,
    unsigned int* accountCount,
    unsigned int* txnIndex
)
{
    ripple::Slice slice{txBlob, (size_t)txSize};
    ripple::STTx txn(slice);
    ripple::SerialIter meta{metaBlob, (size_t)metaSize};
    ripple::TxMeta txMeta{txn.getTransactionID(), 0, ripple::STObject(meta, ripple::sfMetadata)};
    *txnIndex = txMeta.getIndex();
    auto affectedAccounts = txMeta.getAffectedAccounts();
    uint32_t i = 0;
    uint32_t const maxAccounts = 1000;
    for (auto const& account : affectedAccounts) {
        std::memcpy(&accounts[i * ripple::AccountID::size()], account.data(), ripple::AccountID::size());
        i++;
        if (i >= maxAccounts) {
            break;
        }
    }
    *accountCount = i;
}
