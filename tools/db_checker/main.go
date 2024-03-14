// To check the transactions :  ./tools/db_checker/db_checker --tx --username xxx --password xxx --keyspace xxx --fromLedgerIdx 86562170 --toLedgerIdx 86562172
package main

import (
	"fmt"
	"log"
	"os"
	"slices"
	"strings"
	"sync"
	"time"

	"internal/shamap"
	"internal/utils"

	"github.com/alecthomas/kingpin/v2"
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

func getLedgerRange(cluster *gocql.ClusterConfig) (uint64, uint64, error) {
	var (
		firstLedgerIdx  uint64
		latestLedgerIdx uint64
	)

	session, err := cluster.CreateSession()
	if err != nil {
		log.Fatal(err)
	}

	defer session.Close()

	if err := session.Query("select sequence from ledger_range where is_latest = ?", false).Scan(&firstLedgerIdx); err != nil {
		return 0, 0, err
	}

	if err := session.Query("select sequence from ledger_range where is_latest = ?", true).Scan(&latestLedgerIdx); err != nil {
		return 0, 0, err
	}

	log.Printf("DB ledger range is %d:%d\n", firstLedgerIdx, latestLedgerIdx)
	return firstLedgerIdx, latestLedgerIdx, nil
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

func getTransactionsFromLedger(cluster *gocql.ClusterConfig, ledgerIndex uint64) string {
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

	txMap := shamap.MakeSHAMapTxMeta()

	for _, hash := range hashes {
		var tx []byte
		var metadata []byte
		err = session.Query(`select transaction,metadata from transactions where hash = ?`,
			hash).Scan(&tx, &metadata)
		if err != nil {
			log.Printf("Error: Transactions reading %x", hash)
			log.Println(err)
			continue
		}
		txMap.AddTxItem(string(tx[:]), uint32(len(tx)), string(metadata[:]), uint32(len(metadata)))
	}
	hashFromMap := txMap.GetHash()
	txMap.Free()
	return hashFromMap
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

var (
	clusterHosts  = kingpin.Arg("hosts", "Your Scylla nodes IP addresses, comma separated (i.e. 192.168.1.1,192.168.1.2,192.168.1.3)").Required().String()
	fromLedgerIdx = kingpin.Flag("fromLedgerIdx", "Sets the ledger_index to start validation").Short('f').Required().Uint64()
	toLedgerIdx   = kingpin.Flag("toLedgerIdx", "Sets the ledger_index to end validation").Short('e').Default("0").Uint64()
	diff          = kingpin.Flag("diff", "Set the diff numbers to be used to loading ledger in parallel").Short('d').Default("16").Uint32()
	tx            = kingpin.Flag("tx", "Whether to do tx validation").Default("false").Bool()
	step          = kingpin.Flag("step", "Set the tx numbers to be validated concurrently").Short('s').Default("50").Int()
	objects       = kingpin.Flag("objects", "Whether to do objects validation").Default("false").Bool()

	clusterTimeout        = kingpin.Flag("timeout", "Maximum duration for query execution in millisecond").Short('t').Default("90000").Int()
	clusterNumConnections = kingpin.Flag("cluster-number-of-connections", "Number of connections per host per session (in our case, per thread)").Short('b').Default("1").Int()
	clusterCQLVersion     = kingpin.Flag("cql-version", "The CQL version to use").Short('l').Default("3.0.0").String()
	clusterPageSize       = kingpin.Flag("cluster-page-size", "Page size of results").Short('p').Default("5000").Int()
	keyspace              = kingpin.Flag("keyspace", "Keyspace to use").Short('k').Default("clio_fh").String()

	userName = kingpin.Flag("username", "Username to use when connecting to the cluster").String()
	password = kingpin.Flag("password", "Password to use when connecting to the cluster").String()
)

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

func checkingTransactionsFromLedger(cluster *gocql.ClusterConfig, startLedgerIndex uint64, endLedgerIndex uint64, step int) uint64 {
	ledgerIndex := endLedgerIndex
	mismatch := uint64(0)
	for ledgerIndex >= startLedgerIndex {

		thisStep := min(step, int(ledgerIndex-startLedgerIndex+1))
		var wg sync.WaitGroup
		wg.Add(thisStep)

		for i := 0; i < thisStep; i++ {
			seq := ledgerIndex - uint64(i)
			go func() {
				_, txHashStr := getHashesFromLedgerHeader(cluster, seq)
				txHashFromDBStr := getTransactionsFromLedger(cluster, seq)

				if txHashStr != txHashFromDBStr {
					mismatch++
					log.Printf("Error: Tx hash mismatch for ledger %d: %s != %s\n", seq, txHashStr, txHashFromDBStr)
				} else {
					log.Printf("Tx hash for ledger %d is correct: %s\n\n", seq, txHashStr)
				}
				wg.Done()
			}()
		}
		wg.Wait()
		ledgerIndex -= uint64(thisStep)
	}

	return mismatch
}

func main() {
	log.SetOutput(os.Stdout)
	log.SetFlags(log.LstdFlags | log.Lshortfile)
	kingpin.Parse()

	hosts := strings.Split(*clusterHosts, ",")
	cluster := gocql.NewCluster(hosts...)
	cluster.Timeout = time.Duration(*clusterTimeout * 1000 * 1000)
	cluster.NumConns = *clusterNumConnections
	cluster.CQLVersion = *clusterCQLVersion
	cluster.PageSize = *clusterPageSize
	cluster.Keyspace = *keyspace

	if *userName != "" {
		cluster.Authenticator = gocql.PasswordAuthenticator{
			Username: *userName,
			Password: *password,
		}
	}

	earliestLedgerIdxInDB, latestLedgerIdxInDB, err := getLedgerRange(cluster)
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("DB ledger seq range: %d, %d\n", earliestLedgerIdxInDB, latestLedgerIdxInDB)

	if *toLedgerIdx == 0 {
		*toLedgerIdx = latestLedgerIdxInDB
	}

	if *fromLedgerIdx > *toLedgerIdx {
		log.Fatalf("Invalid range: fromLedgerIdx %d > toLedgerIdx %d\n", *fromLedgerIdx, *toLedgerIdx)
	}

	if earliestLedgerIdxInDB > *fromLedgerIdx || latestLedgerIdxInDB < *fromLedgerIdx {
		log.Fatalf("Requested sequence %d not in the DB range %d-%d\n", *fromLedgerIdx, earliestLedgerIdxInDB, latestLedgerIdxInDB)
	}

	if earliestLedgerIdxInDB > *toLedgerIdx || latestLedgerIdxInDB < *toLedgerIdx {
		log.Fatalf("Requested sequence %d not in the DB range %d-%d\n", *fromLedgerIdx, earliestLedgerIdxInDB, latestLedgerIdxInDB)
	}

	//start checking from ledgerIndex, stop when the process ends
	mismatchCh := make(chan uint64)

	if *objects {
		go func() {
			log.Printf("Checking objects from range: %d to %d\n", *fromLedgerIdx, *toLedgerIdx)
			seq := checkingStatesFromLedger(cluster, *fromLedgerIdx, *toLedgerIdx, *diff)
			mismatchCh <- seq
		}()
	} else if *tx {
		go func() {
			log.Printf("Checking tx from range: %d to %d\n", *fromLedgerIdx, *toLedgerIdx)
			mismatch := checkingTransactionsFromLedger(cluster, *fromLedgerIdx, *toLedgerIdx, *step)
			mismatchCh <- mismatch
		}()
	}

	mismatch := <-mismatchCh
	log.Printf("Finish check from %d to %d : mismatches %d", *toLedgerIdx, *fromLedgerIdx, mismatch)
}
