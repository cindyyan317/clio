package utils

// #include "utils.h"
// #include <stdlib.h>
import "C"
import (
	"encoding/hex"
	"strings"
	"unsafe"
)

func GetTxHashFromLedgerHeader(blob []byte, size uint32) string {
	const txHashSize = 32
	cbytes := make([]byte, txHashSize)
	cblob := C.CBytes(blob)
	C.GetTxHashFromLedgerHeader((*C.char)(cblob), C.int(size), (*C.char)(unsafe.Pointer(&cbytes[0])))
	C.free(cblob)
	return strings.ToUpper(hex.EncodeToString(cbytes))
}

func GetStatesHashFromLedgerHeader(blob []byte, size uint32) string {
	const stateHashSize = 32
	cbytes := make([]byte, stateHashSize)
	cblob := C.CBytes(blob)
	C.GetStatesHashFromLedgerHeader((*C.char)(cblob), C.int(size), (*C.char)(unsafe.Pointer(&cbytes[0])))
	C.free(cblob)
	return strings.ToUpper(hex.EncodeToString(cbytes))
}

func GetLedgerHashFromLedgerHeader(blob []byte, size uint32) string {
	const ledgerHashSize = 32
	cbytes := make([]byte, ledgerHashSize)
	cblob := C.CBytes(blob)
	C.GetLedgerHashFromLedgerHeader((*C.char)(cblob), C.int(size), (*C.char)(unsafe.Pointer(&cbytes[0])))
	C.free(cblob)
	return strings.ToUpper(hex.EncodeToString(cbytes))
}

func GetAffectAccountsFromTx(tx []byte, txSize uint32, meta []byte, metaSize uint32, maxAccount uint32) ([][]byte, uint32) {
	const accountSize = 20 // AccountId is 160 bits -> 20 bytes
	cbytes := make([]byte, accountSize*maxAccount)
	cTx := C.CBytes(tx)
	cMeta := C.CBytes(meta)
	var txIdx uint32
	var count uint32
	C.GetAccountTxnIDFromTx((*C.char)(cTx), C.uint(txSize), (*C.char)(cMeta), C.uint(metaSize), C.uint(maxAccount), (*C.char)(unsafe.Pointer(&cbytes[0])), (*C.uint)(unsafe.Pointer(&count)), (*C.uint)(unsafe.Pointer(&txIdx)))
	C.free(cTx)
	C.free(cMeta)
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
	UriExists bool
	Uri       []byte
	IsBurn    bool
}

func GetNFT(tx []byte, txSize uint32, meta []byte, metaSize uint32, maxCount uint32) ([]NFTTxData, []NFTData) {
	const tokenSize = 32   // uint256 -> 32 bytes
	const accountSize = 20 // AccountId is 160 bits -> 20 bytes
	cTx := C.CBytes(tx)
	cMeta := C.CBytes(meta)
	var count uint32
	var txIdx uint32
	tokens := make([]byte, maxCount*tokenSize)
	var hasTokenChanged byte
	tokenChangedId := make([]byte, tokenSize)
	account := make([]byte, accountSize)
	var uriExists byte
	var uriLen uint32
	var isBurn byte
	var taxon uint32
	C.GetNFTFromTx((*C.char)(cTx), C.uint(txSize), (*C.char)(cMeta), C.uint(metaSize),
		C.uint(maxCount), (*C.uint)(unsafe.Pointer(&count)),
		(*C.uint)(unsafe.Pointer(&txIdx)), (*C.char)(unsafe.Pointer(&tokens[0])),
		(*C.char)(unsafe.Pointer(&hasTokenChanged)), (*C.char)(unsafe.Pointer(&tokenChangedId[0])),
		(*C.char)(unsafe.Pointer(&account[0])), (*C.char)(unsafe.Pointer(&uriExists)), (*C.uint)(unsafe.Pointer(&uriLen)),
		(*C.char)(unsafe.Pointer(&isBurn)), (*C.uint)(unsafe.Pointer(&taxon)))

	nftTxs := make([]NFTTxData, count)
	for i := uint32(0); i < count; i++ {
		nftTxs[i].TxIdx = txIdx
		nftTxs[i].TokenId = tokens[i*tokenSize : (i+1)*tokenSize]
	}

	nft := make([]NFTData, 0)

	if hasTokenChanged == 1 {
		nft = append(nft, NFTData{TokenId: tokenChangedId, Issuer: account, TxIdx: txIdx})
		nft[0].IsBurn = isBurn == 1
		nft[0].UriExists = uriExists == 1
		nft[0].Taxon = taxon
		if nft[0].UriExists {
			uri := make([]byte, uriLen)
			if uriLen != 0 {
				C.GetUriFromTx((*C.char)(cTx), C.uint(txSize), (*C.char)(cMeta), C.uint(metaSize), (*C.char)(unsafe.Pointer(&uri[0])))
			}
			nft[0].Uri = uri
		}
	}
	C.free(cTx)
	C.free(cMeta)
	return nftTxs, nft
}
