//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include <boost/signals2.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

template <typename Slot, typename Topic = void>
class Subscribers {
    using SlotPtr = std::shared_ptr<Slot>;
    using SlotFunc = decltype(Slot::operator());
    std::unordered_map<Topic, boost::signals2::signal<SlotFunc>> subscribersSignalMap_;
    std::unordered_map<SlotPtr, std::unordered_set<Topic>> subscribersTopicMap_;

public:
    Subscribers() = default;
    virtual ~Subscribers() = default;

    void
    sub(SlotPtr const& slot, Topic const& topic)
    {
        // If the slot is already subscribed to the topic, do nothing
        if (subscribersTopicMap_.contains(slot) && subscribersTopicMap_[slot].contains(topic))
            return;

        subscribersSignalMap_[topic].connect(*slot);
        subscribersTopicMap_[slot].insert(topic);
    }

    void
    unsub(SlotPtr const& slot, Topic const& topic)
    {
        // If the slot is not subscribed to the topic, do nothing
        if (!subscribersTopicMap_.contains(slot) || !subscribersTopicMap_[slot].contains(topic))
            return;

        subscribersSignalMap_[topic].disconnect(*slot);
        subscribersTopicMap_[slot].erase(topic);
    }

    std::uint64_t
    count()
    {
        return 0;
    }

    void
    publish(Topic const& topic, std::shared_ptr<std::string> const& msg)
    {
        if (subscribersTopicMap_.contains(topic))
            subscribersSignalMap_[topic](msg);
    }
};

template <typename Slot>
class Subscribers<Slot, void> {
    using SlotPtr = std::shared_ptr<Slot>;

    boost::signals2::signal<typename Slot::FuncType> subscribersSignal_;
    std::unordered_set<SlotPtr> subscribers_;

public:
    Subscribers() = default;
    virtual ~Subscribers() = default;

    void
    sub(SlotPtr const& slot)
    {
        // If the slot is already subscribed, do nothing
        if (subscribers_.contains(slot))
            return;

        subscribersSignal_.connect(*slot);
        subscribers_.insert(slot);
    }

    void
    unsub(SlotPtr const& slot)
    {
        // If the slot is not subscribed, do nothing
        if (!subscribers_.contains(slot))
            return;

        subscribersSignal_.disconnect(*slot);
        subscribers_.erase(slot);
    }

    std::uint64_t
    count()
    {
        return subscribers_.size();
    }

    void
    publish(std::shared_ptr<std::string> const& msg)
    {
        subscribersSignal_(msg);
    }
};
