package main

import (
	"encoding/hex"
	"internal/utils"
	"log"
	"runtime"
	"sync"

	"github.com/gocql/gocql"
)

func getLedgerHashFromLedgerHeader(cluster *gocql.ClusterConfig, ledgerIndex uint64) string {
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
		log.Fatal(err) // the ledger header MUST be present for the ledger
	}

	ledgerHash := utils.GetLedgerHashFromLedgerHeader(string(header[:]), uint32(len(header)))
	return ledgerHash
}

func getSeqFromLedgerHash(cluster *gocql.ClusterConfig, ledgerHash string, seq uint64, ledgerHashFix bool) uint64 {
	session, err := cluster.CreateSession()
	if err != nil {
		log.Fatal(err)
	}

	defer session.Close()

	var header uint64
	header = 0
	hash, _ := hex.DecodeString(ledgerHash)
	err = session.Query("select sequence from ledger_hashes where hash = ?",
		hash).Scan(&header)

	if err != nil {
		log.Printf("Error: Ledger hash reading %x : %d, %v", hash, seq, err)
		if ledgerHashFix {
			err = session.Query("insert into ledger_hashes (hash, sequence) values (?, ?)", hash, seq).Exec()
			if err != nil {
				log.Printf("Error: Ledger hash insert %x : %d, %v", hash, seq, err)
			}
			// double confirm if the insert is successful
			session.Query("select sequence from ledger_hashes where hash = ?",
				hash).Scan(&header)
			if header == seq {
				log.Printf("Success: Ledger hash fixed %x : %d", hash, seq)
			} else {
				log.Printf("Failed: Ledger hash fixed %x : %d", hash, seq)
			}
		}
	}

	return header
}

func checkingLedgerHash(cluster *gocql.ClusterConfig, startLedgerIndex uint64, endLedgerIndex uint64, step int, ledgerHashFix bool) uint64 {
	ledgerIndex := endLedgerIndex
	mismatch := uint64(0)
	// memProfileFile, err := os.Create("mem.prof")
	// if err != nil {
	// 	panic(err)
	// }
	// defer memProfileFile.Close()

	for ledgerIndex >= startLedgerIndex {

		thisStep := min(step, int(ledgerIndex-startLedgerIndex+1))
		var wg sync.WaitGroup
		wg.Add(thisStep)

		for i := 0; i < thisStep; i++ {
			seq := ledgerIndex - uint64(i)
			go func() {
				ledgerHashStr := getLedgerHashFromLedgerHeader(cluster, seq)
				seqFromTable := getSeqFromLedgerHash(cluster, ledgerHashStr, seq, ledgerHashFix)
				if seqFromTable != seq {
					mismatch++
					log.Printf("Error: Ledger hash mismatch for ledger %d: %s\n", seq, ledgerHashStr)
				}
				wg.Done()
			}()
		}
		wg.Wait()
		runtime.GC()
		ledgerIndex -= uint64(thisStep)
	}
	// Write memory profile to file
	// if err := pprof.WriteHeapProfile(memProfileFile); err != nil {
	// 	panic(err)
	// }
	return mismatch
}
