//
// Based off of https://github.com/scylladb/scylla-code-samples/blob/master/efficient_full_table_scan_example_code/efficient_full_table_scan.go
//

package main

import (
	"log"
	"math"
	"math/rand"
	"os"
	"slices"
	"strings"
	"time"

	"github.com/alecthomas/kingpin/v2"
	"github.com/gocql/gocql"
)

const (
	cursorsPerToken = 1
)

var (
	clusterHosts      = kingpin.Arg("hosts", "Your Scylla nodes IP addresses, comma separated (i.e. 192.168.1.1,192.168.1.2,192.168.1.3)").Required().String()
	keyspace          = kingpin.Flag("keyspace", "Keyspace to use").Short('k').Default("clio_fh").String()
	clusterTimeout    = kingpin.Flag("timeout", "Maximum duration for query execution in millisecond").Short('t').Default("15000").Int()
	clusterCQLVersion = kingpin.Flag("cql-version", "The CQL version to use").Short('l').Default("3.0.0").String()

	userName = kingpin.Flag("username", "Username to use when connecting to the cluster").String()
	password = kingpin.Flag("password", "Password to use when connecting to the cluster").String()

	miss = 0
)

func fetchCursor(token uint64, cluster *gocql.ClusterConfig) [][]byte {
	query := "SELECT key, object FROM objects WHERE token(key) > ? LIMIT 1"

	session, err := cluster.CreateSession()
	if err != nil {
		log.Fatal(err)
	}

	defer session.Close()

	ret := make([][]byte, 0)
	iter := session.Query(query, token).Iter().Scanner()

	for iter.Next() {
		var key, object []byte
		if err := iter.Scan(&key, &object); err != nil {
			log.Fatal(err)
		}
		//if object != nil {
		//	log.Printf("%d, %x", token, key)
		ret = append(ret, key)
	}

	// if err := iter.Err(); err != nil {
	// 	log.Fatal(err)
	// }
	log.Printf("%x", ret)

	// check object exists

	query = "select object from objects where key = ?"

	o := make([]byte, 0)
	newRet := make([][]byte, 0)
	for _, key := range ret {
		if err := session.Query(query, key).Scan(&o); err != nil {
			log.Fatal(err)
		}
		if len(o) == 0 {
			log.Printf("object deleted %x", key)
			miss++
		} else {
			newRet = append(newRet, key)
		}
	}
	return ret
}

func generateCursorFromObjects(cursorNum int, cluster *gocql.ClusterConfig) [][]byte {
	tokenNumFloat := float64(cursorNum) / float64(cursorsPerToken)

	tokenNum := int(math.Ceil(tokenNumFloat))

	ret := make([][]byte, 0)
	for i := 0; i < tokenNum; i++ {
		cursor := rand.Uint64()

		cursors := fetchCursor(cursor, cluster)
		ret = append(ret, cursors...)
		log.Println(cursor)
	}

	log.Printf("miss %d get %d", miss, len(ret))

	if len(ret) > cursorNum {
		ret = ret[:cursorNum]
	}

	return ret
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

func main() {
	log.SetOutput(os.Stdout)
	log.SetFlags(log.LstdFlags | log.Lshortfile)

	kingpin.Parse()

	hosts := strings.Split(*clusterHosts, ",")

	log.Println(hosts)
	cluster := gocql.NewCluster(hosts...)
	cluster.Keyspace = *keyspace
	cluster.Timeout = time.Duration(*clusterTimeout * 1000 * 1000)
	cluster.CQLVersion = *clusterCQLVersion
	cluster.Consistency = gocql.Quorum
	cluster.ProtoVersion = 4

	if *userName != "" {
		cluster.Authenticator = gocql.PasswordAuthenticator{
			Username: *userName,
			Password: *password,
		}
	}
	//getLedgerRange(cluster)
	cursors := generateCursorFromObjects(100, cluster)

	slices.SortFunc(cursors, func(a, b []byte) int {
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

	cursors = slices.CompactFunc(cursors, func(a, b []byte) bool {
		return slices.Compare(a, b) == 0
	})

	log.Printf("size of cursors %d", len(cursors))

	for _, cursor := range cursors {
		log.Printf("%x", cursor)

		// i := cursor[0:4]
		// log.Printf("%x", i)
	}

}
