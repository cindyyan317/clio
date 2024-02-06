package utils

// #include "utils.h"
import "C"
import (
	"encoding/hex"
	"strings"
	"unsafe"
)

func GetTxHashFromLedgerHeader(blob string, size uint32) string {
	cstr := make([]C.char, 256)
	C.GetTxHashFromLedgerHeader(C.CString(blob), C.int(size), (*C.char)(unsafe.Pointer(&cstr[0])))
	return strings.ToUpper(hex.EncodeToString([]byte(C.GoString(&cstr[0]))))
}

func GetStatesHashFromLedgerHeader(blob string, size uint32) string {
	cstr := make([]C.char, 256)
	C.GetStatesHashFromLedgerHeader(C.CString(blob), C.int(size), (*C.char)(unsafe.Pointer(&cstr[0])))
	return strings.ToUpper(hex.EncodeToString([]byte(C.GoString(&cstr[0]))))
}
