#include <ripple/basics/SHAMapHash.h>
namespace ripple {

enum class SHAMapType {
    TRANSACTION = 1,  // A tree of transactions
    STATE = 2,        // A tree of state nodes
    FREE = 3,         // A tree not part of a ledger
};

inline std::string
to_string(SHAMapType t)
{
    switch (t) {
        case SHAMapType::TRANSACTION:
            return "Transaction Tree";
        case SHAMapType::STATE:
            return "State Tree";
        case SHAMapType::FREE:
            return "Free Tree";
        default:
            return std::to_string(safe_cast<std::underlying_type_t<SHAMapType>>(t));
    }
}

class SHAMapMissingNode : public std::runtime_error {
public:
    SHAMapMissingNode(SHAMapType t, SHAMapHash const& hash)
        : std::runtime_error("Missing Node: " + to_string(t) + ": hash " + to_string(hash))
    {
    }

    SHAMapMissingNode(SHAMapType t, uint256 const& id)
        : std::runtime_error("Missing Node: " + to_string(t) + ": id " + to_string(id))
    {
    }
};
}  // namespace ripple
