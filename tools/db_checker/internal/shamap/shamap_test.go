package shamap

import (
	"fmt"
	"testing"
)

func TestSHAMap(t *testing.T) {
	shamap := MakeSHAMap()
	defer shamap.Free()

	shamap.AddStateItem("1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC", "", 0)
	// shamap.AddTxItem("1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC", "", 0, "", 0)
	hash := shamap.GetHash()

	fmt.Println(hash)

	fmt.Println("c++ says: ")
	SHAMapTest()
}
