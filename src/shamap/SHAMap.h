#pragma once

#include "shamap/SHAMapLeafNode.h"

#include <shamap/SHAMapInnerNode.h>
#include <shamap/SHAMapTreeNode.h>
#include <shamap/impl/Type.h>

#include <cassert>
#include <stack>
#include <vector>

namespace ripple {

/** Describes the current state of a given SHAMap */
enum class SHAMapState {
    /** The map is in flux and objects can be added and removed.

        Example: map underlying the open ledger.
     */
    Modifying = 0,

    /** The map is set in stone and cannot be changed.

        Example: a map underlying a given closed ledger.
     */
    Immutable = 1,

    /** The map's hash is fixed but valid nodes may be missing and can be added.

        Example: a map that's syncing a given peer's closing ledger.
     */
    Synching = 2,

    /** The map is known to not be valid.

        Example: usually synching a corrupt ledger.
     */
    Invalid = 3,
};

class SHAMap {
    SHAMapType const type_;
    std::shared_ptr<ripple::SHAMapTreeNode> root_;
    std::uint32_t cowid_ = 1;

    using SharedPtrNodeStack = std::stack<std::pair<std::shared_ptr<SHAMapTreeNode>, SHAMapNodeID>>;

public:
    /** The depth of the hash map: data is only present in the leaves */
    static inline constexpr unsigned int leafDepth = 64;
    /** Number of children each non-leaf node has (the 'radix tree' part of the
     * map) */
    static inline constexpr unsigned int branchFactor = SHAMapInnerNode::branchFactor;

    SHAMap(SHAMapType t);

    bool
    addItem(SHAMapNodeType type, boost::intrusive_ptr<SHAMapItem const> item);

    bool
    addGiveItem(SHAMapNodeType type, boost::intrusive_ptr<SHAMapItem const> item);

    bool
    delItem(uint256 const& id);

    SHAMapHash
    getHash() const;

    // // save a copy if you have a temporary anyway
    bool
    updateGiveItem(SHAMapNodeType type, boost::intrusive_ptr<SHAMapItem const> item);

private:
    /** If there is only one leaf below this node, get its contents */
    boost::intrusive_ptr<SHAMapItem const> const&
    onlyBelow(SHAMapTreeNode*) const;

    /** Convert any modified nodes to shared. */
    int
    unshare();

    /** prepare a node to be modified before flushing */
    template <class Node>
    std::shared_ptr<Node>
    preFlushNode(std::shared_ptr<Node> node) const;

    int
    walkSubTree(bool doWrite);

    SHAMapLeafNode*
    walkTowardsKey(uint256 const& id, SharedPtrNodeStack* stack = nullptr) const;

    /** Unshare the node, allowing it to be modified */
    template <class Node>
    std::shared_ptr<Node>
    unshareNode(std::shared_ptr<Node>, SHAMapNodeID const& nodeID);

    /** Update hashes up to the root */
    void
    dirtyUp(SharedPtrNodeStack& stack, uint256 const& target, std::shared_ptr<SHAMapTreeNode> terminal);

    // Simple descent
    // Get a child of the specified node
    SHAMapTreeNode*
    descend(SHAMapInnerNode*, int branch) const;
    SHAMapTreeNode*
    descendThrow(SHAMapInnerNode*, int branch) const;
    std::shared_ptr<SHAMapTreeNode>
    descend(std::shared_ptr<SHAMapInnerNode> const&, int branch) const;
    std::shared_ptr<SHAMapTreeNode>
    descendThrow(std::shared_ptr<SHAMapInnerNode> const&, int branch) const;
};
}  // namespace ripple
