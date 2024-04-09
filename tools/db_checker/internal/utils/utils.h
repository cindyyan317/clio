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
    unsigned int* txIdxs,  // nft tx index list
    char* nftTokenIds,     // affected nft token id list
    char* tokenChanged,    // if has token changed
    char* nftChangedId,    // changed nft token id
    char* issuer,          // nft issuer
    char* urlExists,       // if nft url exists
    char* isBurned,        // if nft burned
    unsigned int* taxon
);

#ifdef __cplusplus
}
#endif
