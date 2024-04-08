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
    unsigned int* txIdxs,
    char* nftTokenIds,
    char* tokenChanged,
    char* nftChangedId,
    char* owner,
    char* urlExists,
    char* isBurned,
    unsigned int* taxon
);

#ifdef __cplusplus
}
#endif
