#!/bin/bash

# This script is used to check the states in the database.
# parameters: db_checker location and the start seq

if [ $# -ne 2 ]; then
    echo "Usage: $0 <db_checker_location> <start_seq>"
    exit 1
fi

db_checker=$1
start_seq=$2


db_cluster=""
password=""
username=""
keyspace=""
cursor=1
min=32570

while [ $start_seq -ge $min ]; do
    echo "Checking states for $start_seq"
    $db_checker --objects --cursors $cursor --username $username --password $password --keyspace $keyspace --fromLedgerIdx $start_seq  --toLedgerIdx $start_seq  $db_cluster > ~/state_check_${start_seq}.log 2>&1
    if [ $? -ne 0 ]; then
        echo "Failed to do state check for $start_seq"
        exit 1
    fi
    start_seq=$((start_seq-1))
done
