package main

import (
	"encoding/hex"
	"fmt"
	"log"
	"slices"
	"strings"
	"sync"
	"time"

	"internal/shamap"
	"internal/utils"

	"github.com/alecthomas/kingpin/v2"
	"github.com/gocql/gocql"
)

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

func getObjectsIndex(cluster *gocql.ClusterConfig, ledgerIndex uint64, from []byte, to []byte) [][]byte {

	session, err := cluster.CreateSession()
	if err != nil {
		log.Fatal(err)
	}
	defer session.Close()

	first := make([]byte, 32)
	end := make([]byte, 32)

	for i := 0; i < 32; i++ {
		first[i] = 0
		end[i] = 0xff
	}

	cursorThread := make([]byte, 32)
	copy(cursorThread, from)
	var ret [][]byte
	for {
		if slices.Compare(first, from) != 0 && slices.Compare(end, from) != 0 {
			var object []byte
			err = session.Query("select object from objects where key = ? and sequence <= ? order by sequence desc limit 1",
				from, ledgerIndex).Scan(&object)
			if err != nil {
				log.Fatal(err)
			}
			if len(object) == 0 {
				log.Printf("wrong object fetched %x for seq %d", from, ledgerIndex)
			}
		}
		// add in shamap

		// find next
		next := make([]byte, 32)
		err = session.Query(`select next from successor where key = ? and seq <= ? order by seq desc limit 1`,
			from, ledgerIndex).Scan(&next)
		if err != nil {
			log.Fatal(err)
		}
		// over scope
		if slices.Compare(next, to) > 0 {
			break
		}
		fmt.Printf("next : %x cursor thread : %x end to: %x\n", next, cursorThread, to)
		ret = append(ret, from)
		from = next
	}
	return ret
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
			log.Fatal(err)
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
			log.Fatal(err)
		}
		txMap.AddTxItem(string(tx[:]), uint32(len(tx)), string(metadata[:]), uint32(len(metadata)))
	}
	hashFromMap := txMap.GetHash()
	data, err := hex.DecodeString(hashFromMap)
	log.Printf("hash from map: %x\n", data)
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

	fmt.Printf("%x \n", ret)
	// filter out the deleted objects

	for i := 0; i < len(ret); i++ {
		var object []byte
		err = session.Query("select object from objects where key = ? and sequence <= ? order by sequence desc limit 1",
			ret[i], startIdx).Scan(&object)
		if err != nil {
			log.Fatal(err)
		}
		if len(object) == 0 {
			fmt.Printf("cursor can not be deleted object %x\n", ret[i])
			ret = append(ret[:i], ret[i+1:]...)
			i--
		}
	}

	return ret, nil
}

func LoadStatesFromCursor(cluster *gocql.ClusterConfig, ledgerIndex uint64, first []byte, end []byte) {
	fmt.Printf("Loading states from %x to %x\n", first, end)

	// load indexes from successor
	getObjectsIndex(cluster, ledgerIndex, first, end)
}

func LoadStatesFromCursors(cluster *gocql.ClusterConfig, ledgerIndex uint64, cursors [][]byte) {

	first := make([]byte, 32)
	end := make([]byte, 32)

	for i := 0; i < 32; i++ {
		first[i] = 0
		end[i] = 0xff
	}

	var wg sync.WaitGroup
	wg.Add(1 + len(cursors))
	for _, cursor := range cursors {
		firstCpy := make([]byte, 32)
		cursorCpy := make([]byte, 32)
		copy(firstCpy, first)
		copy(cursorCpy, cursor)
		go func() {
			LoadStatesFromCursor(cluster, ledgerIndex, firstCpy, cursorCpy)
			wg.Done()
		}()
		first = cursor
	}

	LoadStatesFromCursor(cluster, ledgerIndex, first, end)
	wg.Done()

	wg.Wait()

}

var (
	clusterHosts      = kingpin.Arg("hosts", "Your Scylla nodes IP addresses, comma separated (i.e. 192.168.1.1,192.168.1.2,192.168.1.3)").Required().String()
	earliestLedgerIdx = kingpin.Flag("ledgerIdx", "Sets the earliest ledger_index to keep untouched").Short('i').Required().Uint64()
	diff              = kingpin.Flag("diff", "Set the diff numbers to be used to loading ledger in parallel").Short('d').Default("16").Uint32()

	clusterTimeout        = kingpin.Flag("timeout", "Maximum duration for query execution in millisecond").Short('t').Default("15000").Int()
	clusterNumConnections = kingpin.Flag("cluster-number-of-connections", "Number of connections per host per session (in our case, per thread)").Short('b').Default("1").Int()
	clusterCQLVersion     = kingpin.Flag("cql-version", "The CQL version to use").Short('l').Default("3.0.0").String()
	clusterPageSize       = kingpin.Flag("cluster-page-size", "Page size of results").Short('p').Default("5000").Int()
	keyspace              = kingpin.Flag("keyspace", "Keyspace to use").Short('k').Default("clio_fh").String()

	userName = kingpin.Flag("username", "Username to use when connecting to the cluster").String()
	password = kingpin.Flag("password", "Password to use when connecting to the cluster").String()

	numberOfParallelClientThreads = 1 // the calculated number of parallel threads the client should run
)

func checkingStatesFromLedger(cluster *gocql.ClusterConfig, ledgerIndex uint64, diff uint32) {
	for true {
		fmt.Printf("Checking states for ledger %d\n", ledgerIndex)

		cursor, _ := getLedgerStatesCursor(cluster, diff, ledgerIndex)

		//using cursor to start loading from DB
		LoadStatesFromCursors(cluster, ledgerIndex, cursor)

		time.Sleep(1 * time.Second)
		ledgerIndex++
	}
}

func checkingTransactionsFromLedger(cluster *gocql.ClusterConfig, startLedgerIndex uint64, endLedgerIndex uint64) {
	ledgerIndex := startLedgerIndex
	for ledgerIndex <= endLedgerIndex {
		log.Printf("Checking txs for ledger %d\n", ledgerIndex)
		_, txHashStr := getHashesFromLedgerHeader(cluster, ledgerIndex)
		txHashFromDBStr := getTransactionsFromLedger(cluster, ledgerIndex)
		txHash, _ := hex.DecodeString(txHashStr)
		txHashFromDB, _ := hex.DecodeString(txHashFromDBStr)
		if slices.Compare(txHash, txHashFromDB) != 0 {
			log.Fatalf("Tx hash mismatch for ledger %d: %x != %x\n", ledgerIndex, txHash, txHashFromDB)
		}
		log.Printf("Tx hash for ledger %d is correct: %x\n", ledgerIndex, txHash)
		ledgerIndex++
	}
}

func main() {
	// test shamap bindings
	shamap.SHAMapTest()
	// map := shamap.MakeSHAMap()
	// map.AddStateItem("key", "value", 4)
	// hash := map.GetHash()
	// fmt.Printf("hash: %s\n", hash)

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
	fmt.Printf("DB ledger seq range: %d, %d\n", earliestLedgerIdxInDB, latestLedgerIdxInDB)

	if earliestLedgerIdxInDB > *earliestLedgerIdx || latestLedgerIdxInDB < *earliestLedgerIdx {
		log.Fatalf("Requested sequence %d not in the DB range %d-%d\n", *earliestLedgerIdx, earliestLedgerIdxInDB, latestLedgerIdxInDB)
	}

	//start checking from ledgerIndex, stop when the process ends
	//go checkingStatesFromLedger(cluster, *earliestLedgerIdx, *diff)
	checkingTransactionsFromLedger(cluster, *earliestLedgerIdx, latestLedgerIdxInDB)
}
