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

#include <util/Fixtures.h>
#include <webserver2/Server.h>

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>

constexpr static auto JSONData = R"JSON(
    {
        "dos_guard": {
            "max_fetches": 100,
            "sweep_interval": 1,
            "max_connections": 2,
            "max_requests": 3,
            "whitelist": ["127.0.0.1"]
        }
    }
)JSON";

class WebServerTest : public SyncAsioContextTest
{
public:
    void
    SetUp() override
    {
    }

    void
    TearDown() override
    {
    }

protected:
    clio::Config cfg{boost::json::parse(JSONData)};
    clio::IntervalSweepHandler sweepHandler = clio::IntervalSweepHandler{cfg, ctx};
    clio::DOSGuard dosGuard = clio::DOSGuard{cfg, sweepHandler};
};

class MockSession1
{
};

class MockSession2
{
};

class Executor
{
};

TEST_F(WebServerTest, Server)
{
    Executor e;
    auto server = ServerNG::make_HttpServer(cfg, ctx, std::nullopt, dosGuard, e);
}
