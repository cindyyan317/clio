package utils

// #include "utils.h"
// #include <stdlib.h>
import "C"
import (
	"encoding/hex"
	"strings"
	"unsafe"
)

func GetTxHashFromLedgerHeader(blob string, size uint32) string {
	cbytes := make([]byte, 32)
	cblob := C.CString(blob)
	C.GetTxHashFromLedgerHeader(cblob, C.int(size), (*C.char)(unsafe.Pointer(&cbytes[0])))
	C.free(unsafe.Pointer(cblob))
	return strings.ToUpper(hex.EncodeToString(cbytes))
}

func GetStatesHashFromLedgerHeader(blob string, size uint32) string {
	cbytes := make([]byte, 32)
	cblob := C.CString(blob)
	C.GetStatesHashFromLedgerHeader(cblob, C.int(size), (*C.char)(unsafe.Pointer(&cbytes[0])))
	C.free(unsafe.Pointer(cblob))
	return strings.ToUpper(hex.EncodeToString(cbytes))
}

func GetLedgerHashFromLedgerHeader(blob string, size uint32) string {
	cbytes := make([]byte, 32)
	cblob := C.CString(blob)
	C.GetLedgerHashFromLedgerHeader(cblob, C.int(size), (*C.char)(unsafe.Pointer(&cbytes[0])))
	C.free(unsafe.Pointer(cblob))
	return strings.ToUpper(hex.EncodeToString(cbytes))
}

func GetAffectAccountsFromTx(tx string, txSize uint32, meta string, metaSize uint32) ([][]byte, uint32) {
	//AccountId is 160 bits -> 20 bytes
	//Max capacity is 1000 accounts
	MAX := uint32(1000)
	SIZE := uint32(20)
	cbytes := make([]byte, MAX*SIZE)
	cTx := C.CString(tx)
	cMeta := C.CString(meta)
	var txIdx uint32
	var count uint32
	C.GetAccountTxnIDFromTx(cTx, C.int(txSize), cMeta, C.int(metaSize), (*C.char)(unsafe.Pointer(&cbytes[0])), (*C.uint)(unsafe.Pointer(&count)), (*C.uint)(unsafe.Pointer(&txIdx)))
	C.free(unsafe.Pointer(cTx))
	C.free(unsafe.Pointer(cMeta))
	accountBytes := make([][]byte, count)
	for i := uint32(0); i < count; i++ {
		accountBytes[i] = cbytes[i*SIZE : (i+1)*SIZE]
	}
	return accountBytes, txIdx
}
