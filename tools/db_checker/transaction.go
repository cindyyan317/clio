package main

import (
	"internal/shamap"
	"internal/utils"
	"log"
	"sync"

	"github.com/gocql/gocql"
)

func TraverseTxHashFromDB(cluster *gocql.ClusterConfig, ledgerIndex uint64, skipSha bool, skipAccountTxCheck bool, skipNFT bool) string {
	session, err := cluster.CreateSession()
	if err != nil {
		log.Fatal(err)
	}

	defer session.Close()
	var hashes [][]byte
	scanner := session.Query("select hash from ledger_transactions where ledger_sequence = ?", ledgerIndex).Iter().Scanner()
	for scanner.Next() {
		var hash []byte
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
			log.Printf("Error: Transactions reading %x %v", hash, err)
			continue
		}
		if !skipSha {
			ptrTxMap.AddTxItem(tx, uint32(len(tx)), metadata, uint32(len(metadata)))
		}
		if !skipAccountTxCheck {
			checkAccountTx(session, ledgerIndex, tx, metadata)
		}
		if !skipNFT {
			checkNFT(session, ledgerIndex, tx, metadata)
		}
	}
	hashFromMap := ""
	if !skipSha {
		hashFromMap = ptrTxMap.GetHash()
		ptrTxMap.Free()
	}
	return hashFromMap
}

func checkNFT(session *gocql.Session, ledgerIndex uint64, tx []byte, metadata []byte) {
	// It may have multiple nft tx in one transaction, eg cancel offers
	const MAX_ITEM = 100
	nftTxData, nftData := utils.GetNFT(tx, uint32(len(tx)), metadata, uint32(len(metadata)), MAX_ITEM)

	if len(nftTxData) == MAX_ITEM {
		log.Printf("Error: too many NFT tx in ledger %d\n", ledgerIndex)
	}

	//nf_token_transactions
	for _, nft := range nftTxData {
		var count int
		err := session.Query(`select count(*) from nf_token_transactions where token_id = ? and seq_idx = (?,?)`, nft.TokenId, ledgerIndex, nft.TxIdx).Scan(&count)
		if err != nil {
			log.Fatalf("Error: %v nf_token_transactions reading %x ledger %d txId %d", err, nft.TokenId, ledgerIndex, nft.TxIdx)
		}
		if count == 0 {
			log.Printf("Error: nf_token_transactions not found for nft %x ledger %d txId %d\n", nft.TokenId, ledgerIndex, nft.TxIdx)
		} else {
			log.Printf("nf_token_transactions found for nft %x ledger %d txId %d\n", nft.TokenId, ledgerIndex, nft.TxIdx)
		}
	}

	for _, nft := range nftData {
		log.Printf("Changed NFT found for ledger %d txId %d tokenId %x issuer %x urlExists %v isBurn %v\n",
			ledgerIndex, nft.TxIdx, nft.TokenId, nft.Issuer, nft.UrlExists, nft.IsBurn)
		//nf_tokens
		var count int
		err := session.Query(`select count(*) from nf_tokens where token_id = ? and sequence = ?`, nft.TokenId, ledgerIndex).Scan(&count)
		if err != nil {
			log.Fatalf("Error: %v nf_tokens reading %x ledger %d", err, nft.TokenId, ledgerIndex)
		}
		if count == 0 {
			log.Printf("Error: nf_tokens not found for nft %x ledger %d\n", nft.TokenId, ledgerIndex)
		}

		// nf_token_uris and issuer_nf_tokens_v2
		if nft.UrlExists {
			err := session.Query(`select count(*) from nf_token_uris where token_id = ? and sequence = ?`, nft.TokenId, ledgerIndex).Scan(&count)
			if err != nil {
				log.Fatalf("Error: %v nf_token_uris reading %x ledger %d", err, nft.TokenId, ledgerIndex)
			}
			if count == 0 {
				log.Printf("Error: nf_token_uris not found for nft %x ledger %d", nft.TokenId, ledgerIndex)
			}

			err = session.Query(`select count(*) from issuer_nf_tokens_v2 where issuer = ? and taxon = ? and token_id = ?`,
				nft.Issuer, nft.Taxon, nft.TokenId).Scan(&count)
			if err != nil {
				log.Fatalf("Error: %v issuer_nf_tokens_v2 reading issuer %x taxon %d token_id %x", err, nft.Issuer, nft.Taxon, nft.TokenId)
			}
			if count == 0 {
				log.Printf("Error: issuer_nf_tokens_v2 not found for issuer %x taxon %d token_id %x\n", nft.Issuer, nft.Taxon, nft.TokenId)
			}
		}

	}
}

func checkAccountTx(session *gocql.Session, ledgerIndex uint64, tx []byte, metadata []byte) {
	const MAX_ACCOUNTS = 1000
	accounts, txIdx := utils.GetAffectAccountsFromTx(tx, uint32(len(tx)), metadata, uint32(len(metadata)), MAX_ACCOUNTS)
	if len(accounts) == MAX_ACCOUNTS {
		log.Printf("Error: too many accounts in ledger %d tx %d\n", ledgerIndex, txIdx)
	}
	for _, account := range accounts {
		var count int
		err := session.Query(`select count(*) from account_tx where account = ? and seq_idx = (?,?)`, account, ledgerIndex, txIdx).Scan(&count)
		if err != nil {
			log.Fatalf("Error: %v account_tx reading %x ledger %d txId %d", err, account, ledgerIndex, txIdx)
		}
		if count == 0 {
			log.Printf("Error: account_tx not found for account %x ledger %d txId %d\n", account, ledgerIndex, txIdx)
		}
	}
}

func getHashesFromLedgerHeader(cluster *gocql.ClusterConfig, ledgerIndex uint64) (string, string) {
	session, err := cluster.CreateSession()
	if err != nil {
		log.Fatal(err)
	}

	defer session.Close()

	var header []byte
	err = session.Query("select header from ledgers where sequence = ?", ledgerIndex).Scan(&header)

	if err != nil {
		log.Printf("Error: %v ledgers reading %d", err, ledgerIndex)
		return "", ""
	}

	txHash := utils.GetTxHashFromLedgerHeader(header, uint32(len(header)))
	stateHash := utils.GetStatesHashFromLedgerHeader(header, uint32(len(header)))
	return stateHash, txHash
}

func checkingTransactionsFromLedger(cluster *gocql.ClusterConfig, startLedgerIndex uint64, endLedgerIndex uint64, step int, skipSHA bool, skipAccount bool, skipNFT bool) uint64 {
	ledgerIndex := endLedgerIndex
	var mismatch uint64 = 0

	for ledgerIndex >= startLedgerIndex {

		thisStep := min(step, int(ledgerIndex-startLedgerIndex+1))
		var wg sync.WaitGroup
		wg.Add(thisStep)

		for i := 0; i < thisStep; i++ {
			seq := ledgerIndex - uint64(i)
			go func() {
				txHashFromDB := TraverseTxHashFromDB(cluster, seq, skipSHA, skipAccount, skipNFT)

				if !skipSHA {
					_, txHash := getHashesFromLedgerHeader(cluster, seq)
					if txHash != txHashFromDB {
						mismatch++
						log.Printf("Error: Tx hash mismatch for ledger %d: %s != %s\n", seq, txHash, txHashFromDB)
					} else {
						log.Printf("Tx hash for ledger %d is correct: %s\n", seq, txHash)
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
