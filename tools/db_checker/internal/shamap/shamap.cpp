#include "shamap.h"

#include "shamap/SHAMap.h"
#include "shamap/SHAMapItem.h"
#include "shamap/SHAMapTreeNode.h"

#include <ripple/basics/Slice.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/STTx.h>
#include <ripple/protocol/Serializer.h>

#include <cstring>
#include <iostream>

struct SHAMapWrapper {
    ripple::SHAMap map;

    SHAMapWrapper(ripple::SHAMapType t) : map{t}
    {
    }
};

SHAMap
SHAMapInit()
{
    auto ret = new SHAMapWrapper{ripple::SHAMapType::STATE};
    return (SHAMap)ret;
}

void
SHAMapFree(SHAMap m)
{
    SHAMapWrapper* w = (SHAMapWrapper*)m;
    delete w;
}

void
SHAMapAddStateItem(SHAMap m, char const* key, char const* value, unsigned valueSize)
{
    SHAMapWrapper* w = (SHAMapWrapper*)m;
    ripple::uint256 k{key};
    ripple::Slice slice{value, valueSize};
    w->map.addItem(ripple::SHAMapNodeType::tnACCOUNT_STATE, make_shamapitem(k, slice));
}

void
SHAMapAddTxItem(
    SHAMap m,
    char const* key,
    char const* value1,
    unsigned value1Size,
    char const* value2,
    unsigned value2Size
)
{
    SHAMapWrapper* w = (SHAMapWrapper*)m;
    ripple::uint256 k{key};
    ripple::Slice slice1{value1, value1Size};
    ripple::Slice slice2{value2, value2Size};

    ripple::SerialIter sit(ripple::Slice{value1, value1Size});
    ripple::STTx sttx{sit};

    ripple::Serializer s(value1Size + value2Size + 16);

    s.addVL(slice1);
    s.addVL(slice2);

    w->map.addItem(
        ripple::SHAMapNodeType::tnTRANSACTION_MD, ripple::make_shamapitem(sttx.getTransactionID(), s.slice())
    );
}

void
SHAMapUpdateStateItem(SHAMap m, char const* key, char const* value, unsigned valueSize)
{
    SHAMapWrapper* w = (SHAMapWrapper*)m;
    ripple::uint256 k{key};
    ripple::Slice slice{value, valueSize};
    w->map.updateGiveItem(ripple::SHAMapNodeType::tnACCOUNT_STATE, make_shamapitem(k, slice));
}

void
SHAMapDeleteKey(SHAMap m, char const* key)
{
    SHAMapWrapper* w = (SHAMapWrapper*)m;
    ripple::uint256 k{key};
    w->map.delItem(k);
}

void
SHAMapGetHash256(SHAMap m, char* hash)
{
    SHAMapWrapper* w = (SHAMapWrapper*)m;
    auto h = w->map.getHash().as_uint256();
    std::memcpy(hash, h.data(), ripple::uint256::size());
}

void
SHAMapTest()
{
    ripple::SHAMap stateMap = ripple::SHAMap(ripple::SHAMapType::STATE);

    auto mm = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
    ripple::uint256 key = ripple::uint256{mm};
    char const* bb = "";
    ripple::Slice slice{bb, 0};
    stateMap.addItem(ripple::SHAMapNodeType::tnACCOUNT_STATE, make_shamapitem(key, slice));

    std::cout << stateMap.getHash().as_uint256() << std::endl;
}
