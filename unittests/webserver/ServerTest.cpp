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
        },
        "ssl_cert_file": "/Users/cyan/openssl/server.crt",
        "ssl_key_file": "/Users/cyan/openssl/server.key"
    }
)JSON";

std::optional<ssl::context>
parseCertsForTest(clio::Config const& config)
{
    if (!config.contains("ssl_cert_file") || !config.contains("ssl_key_file"))
        return {};

    auto certFilename = config.value<std::string>("ssl_cert_file");
    auto keyFilename = config.value<std::string>("ssl_key_file");

    std::ifstream readCert(certFilename, std::ios::in | std::ios::binary);
    if (!readCert)
        return {};

    std::stringstream contents;
    contents << readCert.rdbuf();
    std::string cert = contents.str();

    std::ifstream readKey(keyFilename, std::ios::in | std::ios::binary);
    if (!readKey)
        return {};

    contents.str("");
    contents << readKey.rdbuf();
    readKey.close();
    std::string key = contents.str();

    ssl::context ctx{ssl::context::tlsv12};
    ctx.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2);
    ctx.use_certificate_chain(boost::asio::buffer(cert.data(), cert.size()));
    ctx.use_private_key(boost::asio::buffer(key.data(), key.size()), boost::asio::ssl::context::file_format::pem);

    return ctx;
}

class WebServerTest : public NoLoggerFixture
{
protected:
    WebServerTest()
    {
        work.emplace(ctx);  // make sure ctx does not stop on its own
        runner.emplace([this] { ctx.run(); });
    }

    ~WebServerTest()
    {
        work.reset();
        ctx.stop();
        if (runner->joinable())
            runner->join();
    }

    // this ctx is for dos timer
    boost::asio::io_context ctxSync;
    clio::Config cfg{boost::json::parse(JSONData)};
    clio::IntervalSweepHandler sweepHandler = clio::IntervalSweepHandler{cfg, ctxSync};
    clio::DOSGuard dosGuard = clio::DOSGuard{cfg, sweepHandler};
    // this ctx is for http server
    boost::asio::io_context ctx;

private:
    std::optional<boost::asio::io_service::work> work;
    std::optional<std::thread> runner;
};

class EchoExecutor
{
public:
    std::tuple<http::status, std::string>
    operator()(boost::json::object&& req)
    {
        std::cout << "req:" << req << std::endl;
        return std::make_tuple(http::status::ok, boost::json::serialize(req));
    }
};

TEST_F(WebServerTest, Http)
{
    EchoExecutor e;
    auto server = ServerNG::make_HttpServer(cfg, ctx, std::nullopt, dosGuard, e);
    auto const res = HttpSyncClient::syncPost("localhost", "8888", R"({"Hello":1})");
    std::cout << "Received: " << res << std::endl;
    EXPECT_EQ(res, R"({"Hello":1})");
    std::cout << "end test" << std::endl;
}

TEST_F(WebServerTest, Ws)
{
    EchoExecutor e;
    auto server = ServerNG::make_HttpServer(cfg, ctx, std::nullopt, dosGuard, e);
    WebSocketSyncClient wsClient;
    wsClient.connect("localhost", "8888");
    auto const res = wsClient.syncPost(R"({"Hello":1})");
    std::cout << "Received: " << res << std::endl;
    EXPECT_EQ(res, R"({"Hello":1})");
    wsClient.disconnect();
    std::cout << "end test" << std::endl;
}

TEST_F(WebServerTest, DISABLED_Https)
{
    EchoExecutor e;
    auto sslCtx = parseCertsForTest(cfg);
    auto const ctxSslRef = sslCtx ? std::optional<std::reference_wrapper<ssl::context>>{sslCtx.value()} : std::nullopt;

    auto server = ServerNG::make_HttpServer(cfg, ctx, ctxSslRef, dosGuard, e);
    auto const res = HttpsSyncClient::syncPost("localhost", "8888", R"({"Hello":1})");
    std::cout << "Received: " << res << std::endl;
    EXPECT_EQ(res, R"({"Hello":1})");
    std::cout << "end test" << std::endl;
}
