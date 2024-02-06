module xrplf/clio/db_checker

go 1.21.6

require (
	internal/shamap v1.0.0
	github.com/alecthomas/kingpin/v2 v2.4.0
	github.com/gocql/gocql v1.6.0
)

replace internal/shamap => ./internal/shamap

require (
	github.com/alecthomas/units v0.0.0-20211218093645-b94a6e3cc137 // indirect
	github.com/golang/snappy v0.0.3 // indirect
	github.com/hailocab/go-hostpool v0.0.0-20160125115350-e80d13ce29ed // indirect
	github.com/xhit/go-str2duration/v2 v2.1.0 // indirect
	gopkg.in/inf.v0 v0.9.1 // indirect
)
