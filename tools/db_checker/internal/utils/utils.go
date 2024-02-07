package utils

// #include "utils.h"
import "C"
import (
	"encoding/hex"
	"strings"
	"unsafe"
)

func GetTxHashFromLedgerHeader(blob string, size uint32) string {
	cstr := make([]C.char, 32)
	C.GetTxHashFromLedgerHeader(C.CString(blob), C.int(size), (*C.char)(unsafe.Pointer(&cstr[0])))
	cbytes := make([]byte, 32)
	for i := range cbytes {
		cbytes[i] = byte(cstr[i])
	}
	return strings.ToUpper(hex.EncodeToString(cbytes))
}

func GetStatesHashFromLedgerHeader(blob string, size uint32) string {
	cstr := make([]C.char, 32)
	C.GetStatesHashFromLedgerHeader(C.CString(blob), C.int(size), (*C.char)(unsafe.Pointer(&cstr[0])))
	cbytes := make([]byte, 32)
	for i := range cbytes {
		cbytes[i] = byte(cstr[i])
	}
	return strings.ToUpper(hex.EncodeToString(cbytes))
}
