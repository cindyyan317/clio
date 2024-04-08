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
    int txSize,
    char* metaBlob,
    int metaSize,
    int maxAccount,
    char* accounts,
    unsigned int* accountCount,
    unsigned int* txnIndex
);

#ifdef __cplusplus
}
#endif
