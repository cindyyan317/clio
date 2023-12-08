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

#include "web/interface/ConnectionBase.h"

#include <functional>
#include <memory>
#include <string>

namespace feed::impl {

class WsSessionSlot {
    std::reference_wrapper<std::shared_ptr<web::ConnectionBase>> wsConnection_;

public:
    using FuncType = void(std::shared_ptr<std::string> const&);

    WsSessionSlot(std::shared_ptr<web::ConnectionBase>& wsConnection) : wsConnection_(wsConnection)
    {
    }
    ~WsSessionSlot() = default;

    std::shared_ptr<web::ConnectionBase>
    getWsConnection() const
    {
        return wsConnection_;
    }

    void
    operator()(std::shared_ptr<std::string> const& msg)
    {
        wsConnection_.get()->send(msg);
    }

    bool
    operator==(WsSessionSlot const& other) const
    {
        return wsConnection_.get() == other.wsConnection_.get();
    }

    void
    onDisconnect(web::ConnectionBase::OnDisconnectSlot const& onDisconnect)
    {
        wsConnection_.get()->onDisconnect.connect(onDisconnect);
    }
};
}  // namespace feed::impl
