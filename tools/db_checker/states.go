package main

import (
	"fmt"
	"internal/shamap"
	"log"
	"slices"
	"sync"

	"github.com/gocql/gocql"
)

var CURSOR_START = make([]byte, 32)
var CURSOR_END = make([]byte, 32)

var statesMapMutex sync.Mutex

func init() {
	for i := 0; i < 32; i++ {
		CURSOR_START[i] = 0
		CURSOR_END[i] = 0xff
	}
}

func LoadStatesFromCursor(cluster *gocql.ClusterConfig, stateMap *shamap.GoSHAMap, ledgerIndex uint64, from []byte, to []byte) {
	log.Printf("Start loading states from cursor %x to %x", from, to)

	session, err := cluster.CreateSession()
	if err != nil {
		log.Fatal(err)
	}
	defer session.Close()

	cursorThread := make([]byte, 32)
	copy(cursorThread, from)
	log.Printf("Cursor : %x Begin\n", cursorThread)
	for {
		if slices.Compare(CURSOR_START, from) != 0 && slices.Compare(CURSOR_END, from) != 0 {
			var object []byte
			err = session.Query("select object from objects where key = ? and sequence <= ? order by sequence desc limit 1",
				from, ledgerIndex).Scan(&object)
			if err != nil {
				log.Printf("Error happen when fetch object for %x - %d", from, ledgerIndex)
				log.Fatal(err)
			}
			if len(object) == 0 {
				log.Printf("wrong object fetched %x for seq %d", from, ledgerIndex)
			}
			// add in shamap
			statesMapMutex.Lock()
			stateMap.AddStateItem(string(from[:]), string(object[:]), uint32(len(object)))
			statesMapMutex.Unlock()
		}
		// find next
		next := make([]byte, 32)
		err = session.Query(`select next from successor where key = ? and seq <= ? order by seq desc limit 1`,
			from, ledgerIndex).Scan(&next)
		if err != nil {
			log.Printf("Error when fetch next from successor for %x", from)
			log.Fatal(err)
		}
		// over scope
		if slices.Compare(next, to) >= 0 {
			break
		}
		//log.Printf("next : %x cursor thread : %x end to: %x\n", next, cursorThread, to)
		from = next
	}
	log.Printf("Cursor : %x Finished\n", cursorThread)
}

func getLedgerStatesCursor(cluster *gocql.ClusterConfig, diff uint32, startIdx uint64) ([][]byte, error) {

	session, err := cluster.CreateSession()
	if err != nil {
		log.Fatal(err)
	}

	var ret [][]byte
	defer session.Close()

	var i uint32
	for i = 0; i < diff; i++ {
		scanner := session.Query("select key from diff where seq = ?", startIdx-uint64(i)).Iter().Scanner()
		for scanner.Next() {
			var (
				key []byte
			)
			err = scanner.Scan(&key)
			if err != nil {
				log.Fatal(err)
			}
			ret = append(ret, key)
		}
	}

	slices.SortFunc(ret, func(a, b []byte) int {
		for i := range a {
			if a[i] != b[i] {
				if a[i] < b[i] {
					return -1
				}
				return 1
			}
		}
		return 0
	})

	ret = slices.CompactFunc(ret, func(a, b []byte) bool {
		return slices.Compare(a, b) == 0
	})

	fmt.Printf("cursors before filter: %x \n", ret)
	// filter out the deleted objects

	for i := 0; i < len(ret); i++ {
		var object []byte
		err = session.Query("select object from objects where key = ? and sequence <= ? order by sequence desc limit 1",
			ret[i], startIdx).Scan(&object)
		if err != nil {
			log.Fatal(err)
		}
		if len(object) == 0 {
			log.Printf("Remove deleted object from cursor list %x\n", ret[i])
			ret = append(ret[:i], ret[i+1:]...)
			i--
		}
	}

	return ret, nil
}

func LoadStatesFromCursors(cluster *gocql.ClusterConfig, ledgerIndex uint64, cursors [][]byte) string {

	first := make([]byte, 32)
	end := make([]byte, 32)

	copy(first, CURSOR_START)
	copy(end, CURSOR_END)

	var wg sync.WaitGroup
	wg.Add(1 + len(cursors))
	statesMap := shamap.MakeSHAMapState()
	for _, cursor := range cursors {
		firstCpy := make([]byte, 32)
		cursorCpy := make([]byte, 32)
		copy(firstCpy, first)
		copy(cursorCpy, cursor)
		go func() {
			LoadStatesFromCursor(cluster, &statesMap, ledgerIndex, firstCpy, cursorCpy)
			wg.Done()
		}()
		first = cursor
	}

	LoadStatesFromCursor(cluster, &statesMap, ledgerIndex, first, end)
	wg.Done()

	wg.Wait()

	hash := statesMap.GetHash()
	statesMap.Free()
	return hash
}

func checkingStatesFromLedger(cluster *gocql.ClusterConfig, startLedgerIndex uint64, endLedgerIndex uint64, diff uint32) uint64 {
	ledgerIndex := startLedgerIndex
	mismatch := uint64(0)
	for ledgerIndex <= endLedgerIndex {
		log.Printf("Checking states for ledger %d\n", ledgerIndex)

		cursor, _ := getLedgerStatesCursor(cluster, diff, ledgerIndex)
		//using cursor to start loading from DB
		ledgerHashFromMap := LoadStatesFromCursors(cluster, ledgerIndex, cursor)
		ledgerHashFromHeader, _ := getHashesFromLedgerHeader(cluster, ledgerIndex)

		if ledgerHashFromHeader != ledgerHashFromMap {
			mismatch++
			log.Printf("state hash mismatch for ledger %d: %s != %s\n", ledgerIndex, ledgerHashFromMap, ledgerHashFromHeader)
		}
		log.Printf("States hash for ledger %d is correct: %s\n\n", ledgerIndex, ledgerHashFromHeader)
		ledgerIndex++
	}
	return mismatch
}
