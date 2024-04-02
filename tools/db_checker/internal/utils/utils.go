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
	cblob := C.CString(blob)
	C.GetLedgerHashFromLedgerHeader(cblob, C.int(size), (*C.char)(unsafe.Pointer(&cbytes[0])))
	C.free(unsafe.Pointer(cblob))
	return strings.ToUpper(hex.EncodeToString(cbytes))
}
