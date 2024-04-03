#!/bin/bash

# This script is used to fix the ledgerhash in the database. 
# parameters: db_checker location and the start seq

if [ $# -ne 2 ]; then
    echo "Usage: $0 <db_checker_location> <start_seq>"
    exit 1
fi

db_checker=$1
start_seq=$2


db_cluster=""
step=500000
password=""
username=""
keyspace=""
db_step=2

while [ $start_seq -gt 0 ]; do
    end_seq=$(($start_seq - $step))
    if [ $end_seq -lt 0 ]; then
        end_seq=0
    fi
    echo "Fixing ledgerhash from $start_seq to $end_seq"
    $db_checker --ledgerHashFix --username $username --password $password --keyspace $keyspace --fromLedgerIdx $end_seq  --toLedgerIdx $start_seq --step $db_step $db_cluster > ~/fix_ledgerhash_${end_seq}_${start_seq}.log 2>&1 
    if [ $? -ne 0 ]; then
        echo "Failed to fix ledgerhash from $start_seq to $end_seq"
        exit 1
    fi
    start_seq=$end_seq
done


