//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012, 2013 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <ripple/basics/random.h>
#include <shamap/SHAMap.h>
#include <shamap/SHAMapSyncFilter.h>

namespace ripple {

void
SHAMap::visitLeaves(std::function<void(boost::intrusive_ptr<SHAMapItem const> const& item)> const& leafFunction) const
{
    visitNodes([&leafFunction](SHAMapTreeNode& node) {
        if (!node.isInner())
            leafFunction(static_cast<SHAMapLeafNode&>(node).peekItem());
        return true;
    });
}

void
SHAMap::visitNodes(std::function<bool(SHAMapTreeNode&)> const& function) const
{
    if (!root_)
        return;

    function(*root_);

    if (!root_->isInner())
        return;

    using StackEntry = std::pair<int, std::shared_ptr<SHAMapInnerNode>>;
    std::stack<StackEntry, std::vector<StackEntry>> stack;

    auto node = std::static_pointer_cast<SHAMapInnerNode>(root_);
    int pos = 0;

    while (true) {
        while (pos < 16) {
            if (!node->isEmptyBranch(pos)) {
                std::shared_ptr<SHAMapTreeNode> child = descendNoStore(node, pos);
                if (!function(*child))
                    return;

                if (child->isLeaf())
                    ++pos;
                else {
                    // If there are no more children, don't push this node
                    while ((pos != 15) && (node->isEmptyBranch(pos + 1)))
                        ++pos;

                    if (pos != 15) {
                        // save next position to resume at
                        stack.push(std::make_pair(pos + 1, std::move(node)));
                    }

                    // descend to the child's first position
                    node = std::static_pointer_cast<SHAMapInnerNode>(child);
                    pos = 0;
                }
            } else {
                ++pos;  // move to next position
            }
        }

        if (stack.empty())
            break;

        std::tie(pos, node) = stack.top();
        stack.pop();
    }
}

void
SHAMap::visitDifferences(SHAMap const* have, std::function<bool(SHAMapTreeNode const&)> const& function) const
{
    // Visit every node in this SHAMap that is not present
    // in the specified SHAMap
    if (!root_)
        return;

    if (root_->getHash().isZero())
        return;

    if (have && (root_->getHash() == have->root_->getHash()))
        return;

    if (root_->isLeaf()) {
        auto leaf = std::static_pointer_cast<SHAMapLeafNode>(root_);
        if (!have || !have->hasLeafNode(leaf->peekItem()->key(), leaf->getHash()))
            function(*root_);
        return;
    }
    // contains unexplored non-matching inner node entries
    using StackEntry = std::pair<SHAMapInnerNode*, SHAMapNodeID>;
    std::stack<StackEntry, std::vector<StackEntry>> stack;

    stack.push({static_cast<SHAMapInnerNode*>(root_.get()), SHAMapNodeID{}});

    while (!stack.empty()) {
        auto const [node, nodeID] = stack.top();
        stack.pop();

        // 1) Add this node to the pack
        if (!function(*node))
            return;

        // 2) push non-matching child inner nodes
        for (int i = 0; i < 16; ++i) {
            if (!node->isEmptyBranch(i)) {
                auto const& childHash = node->getChildHash(i);
                SHAMapNodeID childID = nodeID.getChildNodeID(i);
                auto next = descendThrow(node, i);

                if (next->isInner()) {
                    if (!have || !have->hasInnerNode(childID, childHash))
                        stack.push({static_cast<SHAMapInnerNode*>(next), childID});
                } else if (!have || !have->hasLeafNode(static_cast<SHAMapLeafNode*>(next)->peekItem()->key(), childHash)) {
                    if (!function(*next))
                        return;
                }
            }
        }
    }
}

/** Does this map have this inner node?
 */
bool
SHAMap::hasInnerNode(SHAMapNodeID const& targetNodeID, SHAMapHash const& targetNodeHash) const
{
    auto node = root_.get();
    SHAMapNodeID nodeID;

    while (node->isInner() && (nodeID.getDepth() < targetNodeID.getDepth())) {
        int branch = selectBranch(nodeID, targetNodeID.getNodeID());
        auto inner = static_cast<SHAMapInnerNode*>(node);
        if (inner->isEmptyBranch(branch))
            return false;

        node = descendThrow(inner, branch);
        nodeID = nodeID.getChildNodeID(branch);
    }

    return (node->isInner()) && (node->getHash() == targetNodeHash);
}

/** Does this map have this leaf node?
 */
bool
SHAMap::hasLeafNode(uint256 const& tag, SHAMapHash const& targetNodeHash) const
{
    auto node = root_.get();
    SHAMapNodeID nodeID;

    if (!node->isInner())  // only one leaf node in the tree
        return node->getHash() == targetNodeHash;

    do {
        int branch = selectBranch(nodeID, tag);
        auto inner = static_cast<SHAMapInnerNode*>(node);
        if (inner->isEmptyBranch(branch))
            return false;  // Dead end, node must not be here

        if (inner->getChildHash(branch) == targetNodeHash)  // Matching leaf, no need to retrieve it
            return true;

        node = descendThrow(inner, branch);
        nodeID = nodeID.getChildNodeID(branch);
    } while (node->isInner());

    return false;  // If this was a matching leaf, we would have caught it
                   // already
}

bool
SHAMap::verifyProofPath(uint256 const& rootHash, uint256 const& key, std::vector<Blob> const& path)
{
    if (path.empty() || path.size() > 65)
        return false;

    SHAMapHash hash{rootHash};
    try {
        for (auto rit = path.rbegin(); rit != path.rend(); ++rit) {
            auto const& blob = *rit;
            auto node = SHAMapTreeNode::makeFromWire(makeSlice(blob));
            if (!node)
                return false;
            node->updateHash();
            if (node->getHash() != hash)
                return false;

            auto depth = std::distance(path.rbegin(), rit);
            if (node->isInner()) {
                auto nodeId = SHAMapNodeID::createID(depth, key);
                hash = static_cast<SHAMapInnerNode*>(node.get())->getChildHash(selectBranch(nodeId, key));
            } else {
                // should exhaust all the blobs now
                return depth + 1 == path.size();
            }
        }
    } catch (std::exception const&) {
        // the data in the path may come from the network,
        // exception could be thrown when parsing the data
        return false;
    }
    return false;
}

}  // namespace ripple
