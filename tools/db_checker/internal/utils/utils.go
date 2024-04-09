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

func GetAffectAccountsFromTx(tx string, txSize uint32, meta string, metaSize uint32, maxAccount uint32, accountSize uint32) ([][]byte, uint32) {
	cbytes := make([]byte, accountSize*maxAccount)
	cTx := C.CString(tx)
	cMeta := C.CString(meta)
	var txIdx uint32
	var count uint32
	C.GetAccountTxnIDFromTx(cTx, C.uint(txSize), cMeta, C.uint(metaSize), C.uint(maxAccount), (*C.char)(unsafe.Pointer(&cbytes[0])), (*C.uint)(unsafe.Pointer(&count)), (*C.uint)(unsafe.Pointer(&txIdx)))
	C.free(unsafe.Pointer(cTx))
	C.free(unsafe.Pointer(cMeta))
	accountBytes := make([][]byte, count)
	for i := uint32(0); i < count; i++ {
		accountBytes[i] = cbytes[i*accountSize : (i+1)*accountSize]
	}
	return accountBytes, txIdx
}

type NFTTxData struct {
	TxIdx   uint32
	TokenId []byte
}

type NFTData struct {
	TxIdx     uint32
	TokenId   []byte
	Issuer    []byte
	Taxon     uint32
	UrlExists bool
	IsBurn    bool
}

func GetNFT(tx string, txSize uint32, meta string, metaSize uint32, maxCount uint32, tokenSize uint32) ([]NFTTxData, []NFTData) {
	cTx := C.CString(tx)
	cMeta := C.CString(meta)
	var count uint32
	var txIdx uint32
	tokens := make([]byte, maxCount*tokenSize)
	var hasTokenChanged byte
	tokenChangedId := make([]byte, tokenSize)
	account := make([]byte, 20)
	var urlExists byte
	var isBurn byte
	var taxon uint32
	C.GetNFTFromTx(cTx, C.uint(txSize), cMeta, C.uint(metaSize),
		C.uint(maxCount), (*C.uint)(unsafe.Pointer(&count)),
		(*C.uint)(unsafe.Pointer(&txIdx)), (*C.char)(unsafe.Pointer(&tokens[0])),
		(*C.char)(unsafe.Pointer(&hasTokenChanged)), (*C.char)(unsafe.Pointer(&tokenChangedId[0])),
		(*C.char)(unsafe.Pointer(&account[0])), (*C.char)(unsafe.Pointer(&urlExists)),
		(*C.char)(unsafe.Pointer(&isBurn)), (*C.uint)(unsafe.Pointer(&taxon)))
	C.free(unsafe.Pointer(cTx))
	C.free(unsafe.Pointer(cMeta))

	nftTxs := make([]NFTTxData, count)
	for i := uint32(0); i < count; i++ {
		nftTxs[i].TxIdx = txIdx
		nftTxs[i].TokenId = tokens[i*tokenSize : (i+1)*tokenSize]
	}

	nft := make([]NFTData, 0)

	if hasTokenChanged == 1 {
		nft = append(nft, NFTData{TokenId: tokenChangedId, Issuer: account, TxIdx: txIdx})
		nft[0].IsBurn = isBurn == 1
		nft[0].UrlExists = urlExists == 1
		nft[0].Taxon = taxon

	}
	return nftTxs, nft
}
