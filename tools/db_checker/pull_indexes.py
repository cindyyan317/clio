# This script is used to pull all entities indexes from a ledger. It is just for assisting debug.
import json
from websockets.sync.client import connect
import sys


def get_ledger_data(ws, ledger, marker=None):
    with connect(ws, max_size=None) as websocket:
        s = r'{"command": "ledger_data", "binary": true, "ledger_index": ' + ledger + r'}'
        if marker:
            s = r'{"command": "ledger_data",  "binary": true, "ledger_index": ' + ledger + r', "marker": "' + marker + r'"}'
        websocket.send(s)
        message = websocket.recv()
        return message



if __name__ == '__main__':
    if len(sys.argv) < 3:
        print('Usage: python3 pull_indexes.py <ws_url> <ledger_index>')
        sys.exit(1)
    ledger = sys.argv[2]
    url = sys.argv[1]
    print('Pull ledger_data from: ',ledger, ' url: ', url)

    marker = None
    while True:
        ledger_data = get_ledger_data(url,ledger, marker)
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



