package shamap

// TODO: the paths here are hardcoded, we need to find a way to make them dynamic
// Cindy, change paths to local in order to compile this.
// To compile you use `go build`
// And you can run the test using `go test`

// #include "shamap.h"
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
	C.SHAMapAddStateItem(shamap.shamap, C.CString(key), C.CString(value1), C.uint(size1))
}

func (shamap GoSHAMap) AddTxItem(value1 string, size1 uint32, value2 string, size2 uint32) {
	C.SHAMapAddTxItem(shamap.shamap, C.CString(value1), C.uint(size1), C.CString(value2), C.uint(size2))
}

func (shamap GoSHAMap) UpdateStateItem(key string, value string, size uint32) {
	C.SHAMapUpdateStateItem(shamap.shamap, C.CString(key), C.CString(value), C.uint(size))
}

func (shamap GoSHAMap) DeleteKey(key string) {
	C.SHAMapDeleteKey(shamap.shamap, C.CString(key))
}

func (shamap GoSHAMap) GetHash() string {
	cstr := make([]C.char, 256)
	C.SHAMapGetHash256(shamap.shamap, (*C.char)(unsafe.Pointer(&cstr[0])))
	return strings.ToUpper(hex.EncodeToString([]byte(C.GoString(&cstr[0]))))
}
