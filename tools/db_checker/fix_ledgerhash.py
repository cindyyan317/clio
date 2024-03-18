import http.client
import json
import asyncio
from websockets.sync.client import connect
import subprocess

def get_ledger(seq):
    with connect(f"wss://xrplcluster.com") as websocket:
        s = r'{"command": "ledger","ledger_index":' + str(seq) + r'}'
        websocket.send(s)
        message = websocket.recv()
        return message

db_host = ""
db_user = ""
db_pass = ""

#from grep " Error: Ledger hash reading" log_hash| awk '{print "\""$10"\","}'
def main():
    seqs = [
        78893764,
    ]

    for seq in seqs:
        ledger_json = get_ledger(seq)
        json_data = json.loads(ledger_json)
        if 'result' not in json_data or 'status' not in json_data or json_data['status'] != "success":
            print("Error: ", json_data)
            return
        ledger_hash = test = json_data['result']['ledger_hash']
        
        # cql cmd
        cql = f"INSERT INTO clio_fh.ledger_hashes  (hash, sequence)  VALUES (0x{ledger_hash}, {seq});"
        #cql = "select * from clio_fh.ledger_range limit 1;"
        cmd = f'cqlsh -u {db_user} -p {db_pass} {db_host} -e ' + f'"{cql}"'
        print(cmd)
        return_code = subprocess.call(cmd, shell=True)
        print(return_code)

if __name__ == "__main__":
    main()


