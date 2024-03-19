import http.client
import json
import asyncio
from websockets.sync.client import connect
import subprocess
import sys

def get_transaction(transaction):
    with connect(f"wss://xrplcluster.com") as websocket:
        s = r'{"command": "tx", "binary": true, "transaction": "' + transaction + r'"}'
        websocket.send(s)
        message = websocket.recv()
        return message

db_host = ""
db_user = ""
db_pass = ""

def main():
#from grep " Error: Transactions reading" log| awk '{print "\""$7"\","}'
    transactions = [

    ]

    while True:
        line = sys.stdin.readline()
        if not 'Error: Transactions reading' in line:
            continue
        tx_id = line.split()[-1]
        print("received missing transaction: ",tx_id)
        tx_json = get_transaction(tx_id)
        json_data = json.loads(tx_json)
        if 'result' not in json_data or 'status' not in json_data or json_data['status'] != "success":
            print("Error: ", json_data)
            return
        meta = test = json_data['result']['meta']
        tx = json_data['result']['tx']
        hashstr = json_data['result']['hash']
        date = json_data['result']['date']
        ledger = json_data['result']['ledger_index']
        # cql cmd
        cql = f"INSERT INTO clio_fh.transactions  (hash, ledger_sequence, date, transaction, metadata)  VALUES (0x{hashstr}, {ledger}, {date},0x{tx}, 0x{meta});"
        #cql = "select * from clio_fh.ledger_range limit 1;"
        cmd = f'cqlsh -u {db_user} -p {db_pass} {db_host} -e ' + f'"{cql}"'
        print(cmd)
        return_code = subprocess.call(cmd, shell=True)
        print(return_code, "inserted transaction: ", tx_id)

if __name__ == "__main__":
    main()


