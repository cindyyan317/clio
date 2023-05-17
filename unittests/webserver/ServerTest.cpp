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
#include <util/TestHttpSyncClient.h>
#include <webserver2/Server.h>

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>

constexpr static auto JSONData = R"JSON(
    {
        "server":{
            "ip":"0.0.0.0",
            "port":8888
        },
        "dos_guard": {
            "max_fetches": 100,
            "sweep_interval": 1,
            "max_connections": 2,
            "max_requests": 3,
            "whitelist": ["127.0.0.1"]
        }
    }
)JSON";

class WebServerTest : public AsyncAsioContextTest
{
protected:
    clio::Config cfg{boost::json::parse(JSONData)};
    clio::IntervalSweepHandler sweepHandler = clio::IntervalSweepHandler{cfg, ctx};
    // DOSGuard will destory before io_context, which means http session will get invalid dosguard
    // So we need to call stop() before DOSGuard destruction
    clio::DOSGuard dosGuard = clio::DOSGuard{cfg, sweepHandler};
};

class EchoExecutor
{
public:
    std::tuple<http::status, std::string>
    operator()(http::request<http::string_body>&& req)
    {
        std::cout << "req:" << req.body() << std::endl;
        return std::make_tuple(http::status::ok, req.body());
    }
};

TEST_F(WebServerTest, Server)
{
    EchoExecutor e;
    auto server = ServerNG::make_HttpServer(cfg, ctx, std::nullopt, dosGuard, e);
    auto const res = HttpSyncClient::syncPost("localhost", "8888", "Hello");
    std::cout << "Received: " << res << std::endl;
    EXPECT_EQ(res, "Hello");
    stop();
    std::cout << "end test" << std::endl;
}
