import http.client
import json
import asyncio
from websockets.sync.client import connect
import subprocess

def get_transaction(transaction):
    with connect(f"wss://xrplcluster.com") as websocket:
        s = r'{"command": "tx", "binary": true, "transaction": "' + transaction + r'"}'
        websocket.send(s)
        message = websocket.recv()
        return message

db_host = ""
db_user = ""
db_pass = ""

#from grep " Error: Transactions reading" log| awk '{print "\""$7"\","}'
def main():
    transactions = [
"49a407a665e3a276d152dcc124572e248bbc5e3b28eb160db0edab329423f0dd",
"ae152fb3101ed0470562d3b7b19c0b8ec8ae4afa3928a694a904607a889e87ba",
"b151c62351be6db04408d0fc0f454482fb614892a795b370188aa1d6b752587f",
"b5faed950fe5a63d1307a1f025ca75bd2b31e11564a7aea764a274d7267932ea",
"3882653652bbb5785dd6509d7a512d055c59f639354adecc02355b326fa06a33",
"3a3ec3cbd45d10b70db2a6b5097a7717a5a52181e6b0a44a7c46643395b366f9",
"fce996eb45f78169802c45940fe727523b00ccbbd3750968beb90b8ba90bf77c",
"c9d71342c4155fef7de19dd35de2198c52d0d15cd5baaf0dc35e5cf1724cab5d",
"6e38f56e14375cc803b86020033b3854f5f6adcdb092faba80042132feba4c6d",
"07ecd269956eea249b859985119dd2390df493c353cc1b16b28ffa6204f72eda",
"0b93bd1e53c637cc894631e14cc84d7d5dceed8f3654c1e39b9995d482125abc",
"d8755399949bc72969110abe49aa6f1ce389c9d32655ceec666e5910c504fd36",
"9c9a2b32839b73aa5b20f057a0ef6a8b2265e19c94920bd7de5f28ae5dea11fc",
"839898ac769860cbbfd3f1f74ce8c4b3d5a0d7cf2bc52bd81be921b25597c16f",
"1ec767843099af25c24ef1ffc7e90c637d18f18310b1ffdd53a9b759ec35013d",
"bc5399988ecd11de1c6bb8ef8090ee619cf1a89f410ff01d5fb145adbe4c4b5c",
"d4a5ef78a03aaa6e9baeffc4b1dda23f2711f638c0cec664a6e0ee028a7bb638",
"a0b56412319a2f626cb1ae5ab57aee67d838f6a356ef13915bd4b071894d6245",
    ]

    for tx in transactions:
        tx_json = get_transaction(tx)
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
        print(return_code)

if __name__ == "__main__":
    main()
