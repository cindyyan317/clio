# This script is used to pull all entities indexes from a ledger. It is just for assisting debug.
import http.client
import json
from websockets.sync.client import connect
import subprocess
import sys

# the source of data
URL_WS = 'ws://localhost:6006'

def get_ledger_data(ledger, marker=None):
    with connect(URL_WS) as websocket:
        s = r'{"command": "ledger_data", "binary": true, "ledger_index": ' + ledger + r'}'
        if marker:
            s = r'{"command": "ledger_data", "binary": true, "ledger_index": ' + ledger + r', "marker": "' + marker + r'"}'
        websocket.send(s)
        message = websocket.recv()
        return message



if __name__ == '__main__':
    if len(sys.argv) < 2:
        print('Usage: python3 pull_indexes.py <ledger_index>')
        sys.exit(1)
    ledger = sys.argv[1]
    print('Pull ledger_data from: ',ledger)

    marker = None
    while True:
        ledger_data = get_ledger_data(ledger, marker)
        json_data = json.loads(ledger_data)
        if 'result' not in json_data or 'status' not in json_data or json_data['status'] != "success":
            print('Error: ', json_data)
            sys.exit(1)
        for state in json_data['result']['state']:
            print('state: ', state['index'])
        if 'marker' not in json_data['result']:
            break
        marker = json_data['result']['marker']
        print('Next marker: ', marker)



