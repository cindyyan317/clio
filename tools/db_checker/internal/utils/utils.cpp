#include "utils.h"

#include <etl/NFTHelpers.hpp>
#include <ripple/protocol/AccountID.h>
#include <ripple/protocol/LedgerHeader.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/TxMeta.h>
#include <ripple/protocol/nft.h>

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
    unsigned int txSize,
    char* metaBlob,
    unsigned int metaSize,
    unsigned int maxAccount,
    char* accounts,
    unsigned int* accountCount,
    unsigned int* txnIndex
)
{
    ripple::Slice slice{txBlob, (size_t)txSize};
    ripple::STTx txn(slice);
    ripple::SerialIter meta{metaBlob, (size_t)metaSize};
    // no need real ledger index, just set to 0
    ripple::TxMeta txMeta{txn.getTransactionID(), 0, ripple::STObject(meta, ripple::sfMetadata)};
    *txnIndex = txMeta.getIndex();
    auto affectedAccounts = txMeta.getAffectedAccounts();
    uint32_t i = 0;
    for (auto const& account : affectedAccounts) {
        std::memcpy(&accounts[i * ripple::AccountID::size()], account.data(), ripple::AccountID::size());
        i++;
        if (i >= maxAccount) {
            break;
        }
    }
    *accountCount = i;
}

void
GetNFTFromTx(
    char* txBlob,
    unsigned int txSize,
    char* metaBlob,
    unsigned int metaSize,
    unsigned int maxCount,
    unsigned int* count,
    unsigned int* txIdxs,
    char* nftTokenIds,
    char* tokenChanged,
    char* nftChangedId,
    char* owner,
    char* urlExists,
    char* isBurned,
    unsigned int* taxon
)
{
    ripple::Slice slice{txBlob, (size_t)txSize};
    ripple::STTx txn(slice);
    ripple::SerialIter meta{metaBlob, (size_t)metaSize};
    // no need real ledger index, just set to 0
    ripple::TxMeta txMeta{txn.getTransactionID(), 0, ripple::STObject(meta, ripple::sfMetadata)};
    auto [nftTxData, maybeNft] = etl::getNFTDataFromTx(txMeta, txn);
    *count = nftTxData.size();
    if (*count > maxCount) {
        *count = maxCount;
    }
    for (unsigned int i = 0; i < *count; i++) {
        *txIdxs = nftTxData[i].transactionIndex;
        std::memcpy(&nftTokenIds[i * ripple::uint256::size()], nftTxData[i].tokenID.data(), ripple::uint256::size());
    }
    *tokenChanged = maybeNft.has_value() ? 1 : 0;
    if (maybeNft) {
        std::memcpy(nftChangedId, maybeNft->tokenID.data(), ripple::uint256::size());
        std::memcpy(owner, maybeNft->owner.data(), ripple::AccountID::size());
        std::cout << "owner: " << maybeNft->owner << std::endl;
        *urlExists = maybeNft->uri.has_value() ? 1 : 0;
        *isBurned = maybeNft->isBurned ? 1 : 0;
        *taxon = static_cast<uint32_t>(ripple::nft::getTaxon(maybeNft->tokenID));
    }
}
