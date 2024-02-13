This tool is for checking the integrity of the database. It checks for transactions and states crptographically. 
How it works:
1. It reads the successor of the specified sequence number from the database to build a map of the ledgers. Add all the states to SHAMap. Then compare the hash from SHAMap with the hash from the ledger header.
2. It reads the transactions from the database. Similar to the states, it adds all the transactions to SHAMap. Then compare the hash from SHAMap with the tx_hash from the ledger header.

Beware that this tool is just checking successor/objects transactions/ledger_transactions/ledger tables. 

How to use it:
./tools/db_checker/db_checker --skip-tx --diff 6 --username scylla --password xxxxxx --keyspace clio_fh --ledgerIdx 84261373 127.0.0.1 

diff number is the cursors to traverse the whole ledger. 



