

#include "shamap/SHAMap.hpp"

#include "shamap/SHAMapAccountStateLeafNode.hpp"
#include "shamap/SHAMapLeafNode.hpp"
#include "shamap/SHAMapTxLeafNode.hpp"
#include "shamap/SHAMapTxPlusMetaLeafNode.hpp"

namespace ripple {

[[nodiscard]] std::shared_ptr<SHAMapLeafNode>
makeTypedLeaf(SHAMapNodeType type, boost::intrusive_ptr<SHAMapItem const> item, std::uint32_t owner)
{
    if (type == SHAMapNodeType::tnTRANSACTION_NM)
        return std::make_shared<SHAMapTxLeafNode>(std::move(item), owner);

    if (type == SHAMapNodeType::tnTRANSACTION_MD)
        return std::make_shared<SHAMapTxPlusMetaLeafNode>(std::move(item), owner);

    if (type == SHAMapNodeType::tnACCOUNT_STATE)
        return std::make_shared<SHAMapAccountStateLeafNode>(std::move(item), owner);

    LogicError(
        "Attempt to create leaf node of unknown type " +
        std::to_string(static_cast<std::underlying_type_t<SHAMapNodeType>>(type))
    );
}

SHAMap::SHAMap(SHAMapType t) : type_(t)
{
    root_ = std::make_shared<ripple::SHAMapInnerNode>(cowid_);
}

bool
SHAMap::addItem(SHAMapNodeType type, boost::intrusive_ptr<SHAMapItem const> item)
{
    return addGiveItem(type, std::move(item));
}

bool
SHAMap::addGiveItem(SHAMapNodeType type, boost::intrusive_ptr<SHAMapItem const> item)
{
    assert(state_ != SHAMapState::Immutable);
    assert(type != SHAMapNodeType::tnINNER);

    // add the specified item, does not update
    uint256 tag = item->key();

    SharedPtrNodeStack stack;
    walkTowardsKey(tag, &stack);

    if (stack.empty())
        Throw<SHAMapMissingNode>(type_, tag);

    auto [node, nodeID] = stack.top();
    stack.pop();

    if (node->isLeaf()) {
        auto leaf = std::static_pointer_cast<SHAMapLeafNode>(node);
        if (leaf->peekItem()->key() == tag)
            return false;
    }
    node = unshareNode(std::move(node), nodeID);
    if (node->isInner()) {
        // easy case, we end on an inner node
        auto inner = std::static_pointer_cast<SHAMapInnerNode>(node);
        int branch = selectBranch(nodeID, tag);
        assert(inner->isEmptyBranch(branch));
        inner->setChild(branch, makeTypedLeaf(type, std::move(item), cowid_));
    } else {
        // this is a leaf node that has to be made an inner node holding two
        // items
        auto leaf = std::static_pointer_cast<SHAMapLeafNode>(node);
        auto otherItem = leaf->peekItem();
        assert(otherItem && (tag != otherItem->key()));

        node = std::make_shared<SHAMapInnerNode>(node->cowid());

        unsigned int b1, b2;

        while ((b1 = selectBranch(nodeID, tag)) == (b2 = selectBranch(nodeID, otherItem->key()))) {
            stack.push({node, nodeID});

            // we need a new inner node, since both go on same branch at this
            // level
            nodeID = nodeID.getChildNodeID(b1);
            node = std::make_shared<SHAMapInnerNode>(cowid_);
        }

        // we can add the two leaf nodes here
        assert(node->isInner());

        auto inner = static_cast<SHAMapInnerNode*>(node.get());
        inner->setChild(b1, makeTypedLeaf(type, std::move(item), cowid_));
        inner->setChild(b2, makeTypedLeaf(type, std::move(otherItem), cowid_));
    }

    dirtyUp(stack, tag, node);
    return true;
}

bool
SHAMap::updateGiveItem(SHAMapNodeType type, boost::intrusive_ptr<SHAMapItem const> item)
{
    // can't change the tag but can change the hash
    uint256 tag = item->key();

    assert(state_ != SHAMapState::Immutable);

    SharedPtrNodeStack stack;
    walkTowardsKey(tag, &stack);

    if (stack.empty())
        Throw<SHAMapMissingNode>(type_, tag);

    auto node = std::dynamic_pointer_cast<SHAMapLeafNode>(stack.top().first);
    auto nodeID = stack.top().second;
    stack.pop();

    if (!node || (node->peekItem()->key() != tag)) {
        assert(false);
        return false;
    }

    if (node->getType() != type) {
        // JLOG(journal_.fatal()) << "SHAMap::updateGiveItem: cross-type change!";
        return false;
    }

    node = unshareNode(std::move(node), nodeID);

    if (node->setItem(item))
        dirtyUp(stack, tag, node);

    return true;
}

bool
SHAMap::delItem(uint256 const& id)
{
    // delete the item with this ID
    assert(state_ != SHAMapState::Immutable);

    SharedPtrNodeStack stack;
    walkTowardsKey(id, &stack);

    if (stack.empty())
        Throw<SHAMapMissingNode>(type_, id);

    auto leaf = std::dynamic_pointer_cast<SHAMapLeafNode>(stack.top().first);
    stack.pop();

    if (!leaf || (leaf->peekItem()->key() != id))
        return false;

    SHAMapNodeType type = leaf->getType();

    // What gets attached to the end of the chain
    // (For now, nothing, since we deleted the leaf)
    std::shared_ptr<SHAMapTreeNode> prevNode;

    while (!stack.empty()) {
        auto node = std::static_pointer_cast<SHAMapInnerNode>(stack.top().first);
        SHAMapNodeID nodeID = stack.top().second;
        stack.pop();

        node = unshareNode(std::move(node), nodeID);
        node->setChild(selectBranch(nodeID, id), std::move(prevNode));

        if (!nodeID.isRoot()) {
            // we may have made this a node with 1 or 0 children
            // And, if so, we need to remove this branch
            int const bc = node->getBranchCount();
            if (bc == 0) {
                // no children below this branch
                prevNode.reset();
            } else if (bc == 1) {
                // If there's only one item, pull up on the thread
                auto item = onlyBelow(node.get());

                if (item) {
                    for (int i = 0; i < branchFactor; ++i) {
                        if (!node->isEmptyBranch(i)) {
                            node->setChild(i, nullptr);
                            break;
                        }
                    }

                    prevNode = makeTypedLeaf(type, item, node->cowid());
                } else {
                    prevNode = std::move(node);
                }
            } else {
                // This node is now the end of the branch
                prevNode = std::move(node);
            }
        }
    }

    return true;
}

SHAMapHash
SHAMap::getHash() const
{
    auto hash = root_->getHash();
    if (hash.isZero()) {
        const_cast<SHAMap&>(*this).unshare();
        hash = root_->getHash();
    }
    return hash;
}

static boost::intrusive_ptr<SHAMapItem const> const no_item;

boost::intrusive_ptr<SHAMapItem const> const&
SHAMap::onlyBelow(SHAMapTreeNode* node) const
{
    // If there is only one item below this node, return it

    while (!node->isLeaf()) {
        SHAMapTreeNode* nextNode = nullptr;
        auto inner = static_cast<SHAMapInnerNode*>(node);
        for (int i = 0; i < branchFactor; ++i) {
            if (!inner->isEmptyBranch(i)) {
                if (nextNode)
                    return no_item;

                nextNode = descendThrow(inner, i);
            }
        }

        if (!nextNode) {
            assert(false);
            return no_item;
        }

        node = nextNode;
    }

    // An inner node must have at least one leaf
    // below it, unless it's the root_
    auto const leaf = static_cast<SHAMapLeafNode const*>(node);
    assert(leaf->peekItem() || (leaf == root_.get()));
    return leaf->peekItem();
}

int
SHAMap::unshare()
{
    // Don't share nodes with parent map
    return walkSubTree(false);
}

SHAMapLeafNode*
SHAMap::walkTowardsKey(uint256 const& id, SharedPtrNodeStack* stack) const
{
    assert(stack == nullptr || stack->empty());
    auto inNode = root_;
    SHAMapNodeID nodeID;

    while (inNode->isInner()) {
        if (stack != nullptr)
            stack->push({inNode, nodeID});

        auto const inner = std::static_pointer_cast<SHAMapInnerNode>(inNode);
        auto const branch = selectBranch(nodeID, id);
        if (inner->isEmptyBranch(branch))
            return nullptr;

        inNode = descendThrow(inner, branch);
        nodeID = nodeID.getChildNodeID(branch);
    }

    if (stack != nullptr)
        stack->push({inNode, nodeID});
    return static_cast<SHAMapLeafNode*>(inNode.get());
}

template <class Node>
std::shared_ptr<Node>
SHAMap::unshareNode(std::shared_ptr<Node> node, SHAMapNodeID const& nodeID)
{
    // make sure the node is suitable for the intended operation (copy on write)
    assert(node->cowid() <= cowid_);
    if (node->cowid() != cowid_) {
        // have a CoW
        assert(state_ != SHAMapState::Immutable);
        node = std::static_pointer_cast<Node>(node->clone(cowid_));
        if (nodeID.isRoot())
            root_ = node;
    }
    return node;
}

void
SHAMap::dirtyUp(SharedPtrNodeStack& stack, uint256 const& target, std::shared_ptr<SHAMapTreeNode> child)
{
    // walk the tree up from through the inner nodes to the root_
    // update hashes and links
    // stack is a path of inner nodes up to, but not including, child
    // child can be an inner node or a leaf

    assert((state_ != SHAMapState::Synching) && (state_ != SHAMapState::Immutable));
    assert(child && (child->cowid() == cowid_));

    while (!stack.empty()) {
        auto node = std::dynamic_pointer_cast<SHAMapInnerNode>(stack.top().first);
        SHAMapNodeID nodeID = stack.top().second;
        stack.pop();
        assert(node != nullptr);

        int branch = selectBranch(nodeID, target);
        assert(branch >= 0);

        node = unshareNode(std::move(node), nodeID);
        node->setChild(branch, std::move(child));

        child = std::move(node);
    }
}

std::shared_ptr<SHAMapTreeNode>
SHAMap::descendThrow(std::shared_ptr<SHAMapInnerNode> const& parent, int branch) const
{
    std::shared_ptr<SHAMapTreeNode> ret = descend(parent, branch);

    if (!ret && !parent->isEmptyBranch(branch))
        Throw<SHAMapMissingNode>(type_, parent->getChildHash(branch));

    return ret;
}

std::shared_ptr<SHAMapTreeNode>
SHAMap::descend(std::shared_ptr<SHAMapInnerNode> const& parent, int branch) const
{
    std::shared_ptr<SHAMapTreeNode> node = parent->getChild(branch);
    // if (node || !backed_)
    return node;

    // node = fetchNode(parent->getChildHash(branch));
    // if (!node)
    //     return nullptr;

    // node = parent->canonicalizeChild(branch, std::move(node));
    // return node;
}

SHAMapTreeNode*
SHAMap::descendThrow(SHAMapInnerNode* parent, int branch) const
{
    SHAMapTreeNode* ret = descend(parent, branch);

    if (!ret && !parent->isEmptyBranch(branch))
        Throw<SHAMapMissingNode>(type_, parent->getChildHash(branch));

    return ret;
}

SHAMapTreeNode*
SHAMap::descend(SHAMapInnerNode* parent, int branch) const
{
    SHAMapTreeNode* ret = parent->getChildPointer(branch);
    //   if (ret || !backed_)
    return ret;

    // std::shared_ptr<SHAMapTreeNode> node = fetchNodeNT(parent->getChildHash(branch));
    // if (!node)
    //     return nullptr;

    // node = parent->canonicalizeChild(branch, std::move(node));
    // return node.get();
}

// We can't modify an inner node someone else might have a
// pointer to because flushing modifies inner nodes -- it
// makes them point to canonical/shared nodes.
template <class Node>
std::shared_ptr<Node>
SHAMap::preFlushNode(std::shared_ptr<Node> node) const
{
    // A shared node should never need to be flushed
    // because that would imply someone modified it
    assert(node->cowid() != 0);

    if (node->cowid() != cowid_) {
        // Node is not uniquely ours, so unshare it before
        // possibly modifying it
        node = std::static_pointer_cast<Node>(node->clone(cowid_));
    }
    return node;
}

int
SHAMap::walkSubTree(bool doWrite)
{
    // assert(!doWrite || backed_);

    int flushed = 0;

    if (!root_ || (root_->cowid() == 0))
        return flushed;

    if (root_->isLeaf()) {  // special case -- root_ is leaf
        root_ = preFlushNode(std::move(root_));
        root_->updateHash();
        root_->unshare();

        // if (doWrite)
        //     root_ = writeNode(t, std::move(root_));

        return 1;
    }

    auto node = std::static_pointer_cast<SHAMapInnerNode>(root_);

    if (node->isEmpty()) {  // replace empty root with a new empty root
        root_ = std::make_shared<SHAMapInnerNode>(0);
        return 1;
    }

    // Stack of {parent,index,child} pointers representing
    // inner nodes we are in the process of flushing
    using StackEntry = std::pair<std::shared_ptr<SHAMapInnerNode>, int>;
    std::stack<StackEntry, std::vector<StackEntry>> stack;

    node = preFlushNode(std::move(node));

    int pos = 0;

    // We can't flush an inner node until we flush its children
    while (1) {
        while (pos < branchFactor) {
            if (node->isEmptyBranch(pos)) {
                ++pos;
            } else {
                // No need to do I/O. If the node isn't linked,
                // it can't need to be flushed
                int branch = pos;
                auto child = node->getChild(pos++);

                if (child && (child->cowid() != 0)) {
                    // This is a node that needs to be flushed

                    child = preFlushNode(std::move(child));

                    if (child->isInner()) {
                        // save our place and work on this node

                        stack.emplace(std::move(node), branch);
                        // The semantics of this changes when we move to c++-20
                        // Right now no move will occur; With c++-20 child will
                        // be moved from.
                        node = std::static_pointer_cast<SHAMapInnerNode>(std::move(child));
                        pos = 0;
                    } else {
                        // flush this leaf
                        ++flushed;

                        assert(node->cowid() == cowid_);
                        child->updateHash();
                        child->unshare();

                        // if (doWrite)
                        //     child = writeNode(t, std::move(child));

                        node->shareChild(branch, child);
                    }
                }
            }
        }

        // update the hash of this inner node
        node->updateHashDeep();

        // This inner node can now be shared
        node->unshare();

        // if (doWrite)
        //     node = std::static_pointer_cast<SHAMapInnerNode>(writeNode(t, std::move(node)));

        ++flushed;

        if (stack.empty())
            break;

        auto parent = std::move(stack.top().first);
        pos = stack.top().second;
        stack.pop();

        // Hook this inner node to its parent
        assert(parent->cowid() == cowid_);
        parent->shareChild(pos, node);

        // Continue with parent's next child, if any
        node = std::move(parent);
        ++pos;
    }

    // Last inner node is the new root_
    root_ = std::move(node);

    return flushed;
}

}  // namespace ripple
