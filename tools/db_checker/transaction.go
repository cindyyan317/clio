package main

import (
	"internal/shamap"
	"internal/utils"
	"log"
	"sync"

	"github.com/gocql/gocql"
)

func getTransactionsFromLedger(cluster *gocql.ClusterConfig, ledgerIndex uint64, skipSha bool, skipAccountTxCheck bool) string {
	session, err := cluster.CreateSession()
	if err != nil {
		log.Fatal(err)
	}

	defer session.Close()
	var hashes [][]byte
	scanner := session.Query("select hash from ledger_transactions where ledger_sequence = ?", ledgerIndex).Iter().Scanner()
	for scanner.Next() {
		var (
			hash []byte
		)
		err = scanner.Scan(&hash)
		if err != nil {
			log.Printf("Error: ledger_transactions reading %d", ledgerIndex)
			log.Println(err)
		}
		hashes = append(hashes, hash)
	}

	var ptrTxMap *shamap.GoSHAMap
	if !skipSha {
		txMap := shamap.MakeSHAMapTxMeta()
		ptrTxMap = &txMap
	}
	var tx []byte
	var metadata []byte
	for _, hash := range hashes {
		err = session.Query(`select transaction,metadata from transactions where hash = ?`,
			hash).Scan(&tx, &metadata)
		if err != nil {
			log.Printf("Error: Transactions reading %x", hash)
			log.Println(err)
			continue
		}
		if !skipSha {
			ptrTxMap.AddTxItem(string(tx[:]), uint32(len(tx)), string(metadata[:]), uint32(len(metadata)))
		}
		if !skipAccountTxCheck {
			accounts, txIdx := utils.GetAffectAccountsFromTx(string(tx[:]), uint32(len(tx)), string(metadata[:]), uint32(len(metadata)))
			for _, account := range accounts {
				log.Printf("Ledger %d, TxId %d, Account: %x\n", ledgerIndex, txIdx, account)
				var count int
				err = session.Query(`select count(*) from account_tx where account = ? and seq_idx = (?,?)`, account, ledgerIndex, txIdx+1).Scan(&count)
				if err != nil {
					log.Printf("Error: %v account_tx reading %x ledger %d txId %d", err, account, ledgerIndex, txIdx)
					continue
				}
				if count == 0 {
					log.Printf("Error: account_tx not found for account %x ledger %d txId %d\n", account, ledgerIndex, txIdx)
				} else {
					log.Printf("account_tx found for account %x ledger %d txId %d\n", account, ledgerIndex, txIdx)
				}
			}

		}
	}
	hashFromMap := ""
	if !skipSha {
		hashFromMap = ptrTxMap.GetHash()
		ptrTxMap.Free()
	}
	return hashFromMap
}

func getHashesFromLedgerHeader(cluster *gocql.ClusterConfig, ledgerIndex uint64) (string, string) {
	session, err := cluster.CreateSession()
	if err != nil {
		log.Fatal(err)
	}

	defer session.Close()

	var header []byte
	err = session.Query("select header from ledgers where sequence = ?",
		ledgerIndex).Scan(&header)

	if err != nil {
		log.Printf("Error: ledgers reading %d", ledgerIndex)
		log.Println(err)
		return "", ""
	}

	txHash := utils.GetTxHashFromLedgerHeader(string(header[:]), uint32(len(header)))
	stateHash := utils.GetStatesHashFromLedgerHeader(string(header[:]), uint32(len(header)))
	return stateHash, txHash
}

func checkingTransactionsFromLedger(cluster *gocql.ClusterConfig, startLedgerIndex uint64, endLedgerIndex uint64, step int, skipSHA bool, skipAccount bool) uint64 {
	ledgerIndex := endLedgerIndex
	mismatch := uint64(0)
	for ledgerIndex >= startLedgerIndex {

		thisStep := min(step, int(ledgerIndex-startLedgerIndex+1))
		var wg sync.WaitGroup
		wg.Add(thisStep)

		for i := 0; i < thisStep; i++ {
			seq := ledgerIndex - uint64(i)
			go func() {
				txHashFromDBStr := getTransactionsFromLedger(cluster, seq, skipSHA, skipAccount)

				if !skipSHA {
					_, txHashStr := getHashesFromLedgerHeader(cluster, seq)
					if txHashStr != txHashFromDBStr {
						mismatch++
						log.Printf("Error: Tx hash mismatch for ledger %d: %s != %s\n", seq, txHashStr, txHashFromDBStr)
					} else {
						log.Printf("Tx hash for ledger %d is correct: %s\n\n", seq, txHashStr)
					}
				} else {
					log.Printf("Finish checking tx existence for ledger %d\n", seq)
				}
				wg.Done()
			}()
		}
		wg.Wait()
		ledgerIndex -= uint64(thisStep)
	}

	return mismatch
}
