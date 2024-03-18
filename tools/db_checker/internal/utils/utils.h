#ifdef __cplusplus
extern "C" {
#endif

void
GetStatesHashFromLedgerHeader(char* ledgerHeaderBlob, int size, char* hash);

void
GetTxHashFromLedgerHeader(char* ledgerHeaderBlob, int size, char* hash);

void
GetLedgerHashFromLedgerHeader(char* ledgerHeaderBlob, int size, char* hash);

#ifdef __cplusplus
}
#endif
