package utils

// #include "utils.h"
import "C"
import (
	"encoding/hex"
	"strings"
	"unsafe"
)

func GetTxHashFromLedgerHeader(blob string, size uint32) string {
	cbytes := make([]byte, 32)
	C.GetTxHashFromLedgerHeader(C.CString(blob), C.int(size), (*C.char)(unsafe.Pointer(&cbytes[0])))
	return strings.ToUpper(hex.EncodeToString(cbytes))
}

func GetStatesHashFromLedgerHeader(blob string, size uint32) string {
	cbytes := make([]byte, 32)
	C.GetStatesHashFromLedgerHeader(C.CString(blob), C.int(size), (*C.char)(unsafe.Pointer(&cbytes[0])))
	return strings.ToUpper(hex.EncodeToString(cbytes))
}

func GetLedgerHashFromLedgerHeader(blob string, size uint32) string {
	cbytes := make([]byte, 32)
	C.GetLedgerHashFromLedgerHeader(C.CString(blob), C.int(size), (*C.char)(unsafe.Pointer(&cbytes[0])))
	return strings.ToUpper(hex.EncodeToString(cbytes))
}
