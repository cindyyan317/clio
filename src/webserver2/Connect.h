//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include <log/Logger.h>
#include <util/Taggable.h>
namespace ServerNG {

struct Connection : public util::Taggable
{
    clio::Logger log{"WebServer"};
    clio::Logger perfLog{"Performance"};
    std::optional<std::string> ipMaybe;

    Connection(util::TagDecoratorFactory const& tagFactory) : Taggable(tagFactory)
    {
    }

    Connection(util::TagDecoratorFactory const& tagFactory, std::optional<std::string> ip)
        : Taggable(tagFactory), ipMaybe(ip)
    {
    }
};
}  // namespace ServerNG
