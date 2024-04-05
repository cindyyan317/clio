// To check the transactions :  ./tools/db_checker/db_checker --tx --username xxx --password xxx --keyspace xxx --fromLedgerIdx 86562170 --toLedgerIdx 86562172
package main

import (
	"log"
	"os"
	"strings"
	"time"

	"github.com/alecthomas/kingpin/v2"
	"github.com/gocql/gocql"
	//	_ "net/http/pprof"
	//
	// "net/http/pprof"
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

var (
	clusterHosts  = kingpin.Arg("hosts", "Your Scylla nodes IP addresses, comma separated (i.e. 192.168.1.1,192.168.1.2,192.168.1.3)").Required().String()
	fromLedgerIdx = kingpin.Flag("fromLedgerIdx", "Sets the ledger_index to start validation").Short('f').Required().Uint64()
	toLedgerIdx   = kingpin.Flag("toLedgerIdx", "Sets the ledger_index to end validation").Short('e').Default("0").Uint64()
	//transactions table
	tx            = kingpin.Flag("tx", "Whether to do tx validation").Default("false").Bool()
	txSkipSha     = kingpin.Flag("txSkipSha", "Whether to skip SHA hash for tx validation").Default("false").Bool()
	txSkipAccount = kingpin.Flag("txSkipAccount", "Whether to skip account_tx check for tx validation").Default("false").Bool()
	step          = kingpin.Flag("step", "Set the tx numbers to be validated concurrently").Short('s').Default("50").Int()
	//objects + successor
	diff    = kingpin.Flag("diff", "Set the diff numbers to be used to loading ledger in parallel").Short('d').Default("16").Uint32()
	objects = kingpin.Flag("objects", "Whether to do objects validation").Default("false").Bool()
	//ledger_hash table
	ledgerHash    = kingpin.Flag("ledgerHash", "Whether to do ledger_hash table validation").Default("false").Bool()
	ledgerHashFix = kingpin.Flag("ledgerHashFix", "Whether to do ledger_hash table validation and fix it in place").Default("false").Bool()

	clusterTimeout        = kingpin.Flag("timeout", "Maximum duration for query execution in millisecond").Short('t').Default("90000").Int()
	clusterNumConnections = kingpin.Flag("cluster-number-of-connections", "Number of connections per host per session (in our case, per thread)").Short('b').Default("1").Int()
	clusterCQLVersion     = kingpin.Flag("cql-version", "The CQL version to use").Short('l').Default("3.0.0").String()
	keyspace              = kingpin.Flag("keyspace", "Keyspace to use").Short('k').Default("clio_fh").String()

	userName = kingpin.Flag("username", "Username to use when connecting to the cluster").String()
	password = kingpin.Flag("password", "Password to use when connecting to the cluster").String()
)

func main() {
	log.SetOutput(os.Stdout)
	log.SetFlags(log.LstdFlags | log.Lshortfile)
	kingpin.Parse()

	hosts := strings.Split(*clusterHosts, ",")
	cluster := gocql.NewCluster(hosts...)
	cluster.Timeout = time.Duration(*clusterTimeout * 1000 * 1000)
	cluster.NumConns = *clusterNumConnections
	cluster.CQLVersion = *clusterCQLVersion
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

	if *fromLedgerIdx == 0 {
		*fromLedgerIdx = earliestLedgerIdxInDB
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

	// go func() {
	// 	http.ListenAndServe("localhost:8080", nil)
	// }()

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
			mismatch := checkingTransactionsFromLedger(cluster, *fromLedgerIdx, *toLedgerIdx, *step, *txSkipSha, *txSkipAccount)
			mismatchCh <- mismatch
		}()
	} else if *ledgerHash || *ledgerHashFix {
		go func() {
			log.Printf("Checking ledger hash from range: %d to %d\n", *fromLedgerIdx, *toLedgerIdx)
			mismatch := checkingLedgerHash(cluster, *fromLedgerIdx, *toLedgerIdx, *step, *ledgerHashFix)
			mismatchCh <- mismatch
		}()
	}
	mismatch := <-mismatchCh
	log.Printf("Finish check from %d to %d : mismatches %d", *toLedgerIdx, *fromLedgerIdx, mismatch)
}
