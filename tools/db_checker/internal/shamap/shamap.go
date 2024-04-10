package shamap

// TODO: the paths here are hardcoded, we need to find a way to make them dynamic
// Cindy, change paths to local in order to compile this.
// To compile you use `go build`
// And you can run the test using `go test`

// #include "shamap.h"
// #include <stdlib.h>
import "C"
import (
	"encoding/hex"
	"strings"
	"unsafe"
)

type GoSHAMap struct {
	shamap C.SHAMap
}

func MakeSHAMapState() GoSHAMap {
	var ret GoSHAMap
	ret.shamap = C.SHAMapStateInit()
	return ret
}

func MakeSHAMapTxMeta() GoSHAMap {
	var ret GoSHAMap
	ret.shamap = C.SHAMapTxMetaInit()
	return ret
}

func SHAMapTest() {
	C.SHAMapTest()
}

func (shamap GoSHAMap) Free() {
	C.SHAMapFree((C.SHAMap)(unsafe.Pointer(shamap.shamap)))
}

func (shamap GoSHAMap) AddStateItem(key string, value1 string, size1 uint32) {
	cKey := C.CString(key)
	cValue1 := C.CString(value1)
	C.SHAMapAddStateItem(shamap.shamap, cKey, cValue1, C.uint(size1))
	C.free(unsafe.Pointer(cKey))
	C.free(unsafe.Pointer(cValue1))
}

func (shamap GoSHAMap) AddTxItem(value1 string, size1 uint32, value2 string, size2 uint32) {
	cValue1 := C.CString(value1)
	cValue2 := C.CString(value2)
	C.SHAMapAddTxItem(shamap.shamap, cValue1, C.uint(size1), cValue2, C.uint(size2))
	C.free(unsafe.Pointer(cValue1))
	C.free(unsafe.Pointer(cValue2))
}

func (shamap GoSHAMap) UpdateStateItem(key string, value string, size uint32) {
	cKey := C.CString(key)
	cValue := C.CString(value)
	C.SHAMapUpdateStateItem(shamap.shamap, cKey, cValue, C.uint(size))
	C.free(unsafe.Pointer(cKey))
	C.free(unsafe.Pointer(cValue))
}

func (shamap GoSHAMap) DeleteKey(key string) {
	cKey := C.CString(key)
	C.SHAMapDeleteKey(shamap.shamap, cKey)
	C.free(unsafe.Pointer(cKey))
}

func (shamap GoSHAMap) GetHash() string {
	//32 is 256/sizeof(char)
	cstr := make([]C.char, 32)
	C.SHAMapGetHash256(shamap.shamap, (*C.char)(unsafe.Pointer(&cstr[0])))
	cbytes := make([]byte, 32)
	for i := range cbytes {
		cbytes[i] = byte(cstr[i])
	}
	return strings.ToUpper(hex.EncodeToString(cbytes))
}
