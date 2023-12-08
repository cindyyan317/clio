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

#include "feed/ForwardFeed.h"
#include "util/MockWsBase.h"
#include "util/Taggable.h"
#include "util/config/Config.h"

#include <gtest/gtest.h>

#include <memory>

TEST(SubForwardFeed, TEST)
{
    ForwardFeed forwardFeed;
    util::Config cfg;
    util::TagDecoratorFactory tagDecoratorFactory{cfg};
    {
        std::shared_ptr<web::ConnectionBase> wsConnection = std::make_shared<MockSession>(tagDecoratorFactory);

        auto slot = std::make_shared<WsSessionSlot>(wsConnection);

        forwardFeed.sub(slot);

        EXPECT_EQ(forwardFeed.count(), 1);

        forwardFeed.sub(slot);

        EXPECT_EQ(forwardFeed.count(), 1);

        forwardFeed.publish("test");
        forwardFeed.publish("123");

        MockSession* session = dynamic_cast<MockSession*>(wsConnection.get());
        EXPECT_EQ(session->message, "test123");

        std::shared_ptr<web::ConnectionBase> wsConnection2 = std::make_shared<MockSession>(tagDecoratorFactory);
        auto slot2 = std::make_shared<WsSessionSlot>(wsConnection2);

        forwardFeed.sub(slot2);
        EXPECT_EQ(forwardFeed.count(), 2);
    }

    EXPECT_EQ(forwardFeed.count(), 0);
}
