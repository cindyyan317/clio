#ifdef __cplusplus
extern "C" {
#endif

void
GetStatesHashFromLedgerHeader(char* ledgerHeaderBlob, int size, char* hash);

void
GetTxHashFromLedgerHeader(char* ledgerHeaderBlob, int size, char* hash);

void
GetLedgerHashFromLedgerHeader(char* ledgerHeaderBlob, int size, char* hash);

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
);

void
GetNFTFromTx(
    char* txBlob,
    unsigned int txSize,
    char* metaBlob,
    unsigned int metaSize,
    unsigned int maxCount,
    unsigned int* count,
    unsigned int* txIdxs,   // nft tx index list
    char* nftTokenIds,      // affected nft token id list
    char* tokenChanged,     // if has token changed
    char* nftChangedId,     // changed nft token id
    char* issuer,           // nft issuer
    char* uriExists,        // if nft uri exists
    unsigned int* uriSize,  // uri length
    char* isBurned,         // if nft burned
    unsigned int* taxon
);

void
GetUriFromTx(char* txBlob, unsigned int txSize, char* metaBlob, unsigned int metaSize, char* uri);

void
GetDiffFromTx(
    char* txBlob,
    unsigned int txSize,
    char* metaBlob,
    unsigned int metaSize,
    unsigned int maxNode,
    unsigned int* createdNum,
    char* createdIndexes,
    unsigned int* deletedNum,
    char* deletedIndexes,
    unsigned int* updatedNum,
    char* updatedIndexes
);

#ifdef __cplusplus
}
#endif
