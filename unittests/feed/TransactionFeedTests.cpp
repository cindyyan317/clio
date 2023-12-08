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

#include "feed/impl/TransactionFeedImplV1.h"
#include "util/MockWsBase.h"
#include "util/Taggable.h"
#include "util/TestObject.h"
#include "util/config/Config.h"

#include <gtest/gtest.h>

#include <memory>

using namespace feed::impl;

TEST(SubTransactionFeed, TEST)
{
    TransactionFeedImplV1 txFeedV1;
    util::Config cfg;
    util::TagDecoratorFactory tagDecoratorFactory{cfg};
    {
        std::shared_ptr<web::ConnectionBase> wsConnection = std::make_shared<MockSession>(tagDecoratorFactory);

        auto slot = std::make_shared<WsSessionSlot>(wsConnection);

        txFeedV1.sub(slot);
        txFeedV1.sub(slot, GetAccountIDWithString("rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"));
        txFeedV1.sub(slot, GetAccountIDWithString("rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn"));
        txFeedV1.sub(slot, GetAccountIDWithString("rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"));

        EXPECT_EQ(txFeedV1.transactionsStreamCount(), 1);
        EXPECT_EQ(txFeedV1.accountStreamCount(), 2);
    }
    EXPECT_EQ(txFeedV1.transactionsStreamCount(), 0);
    EXPECT_EQ(txFeedV1.accountStreamCount(), 0);
}
