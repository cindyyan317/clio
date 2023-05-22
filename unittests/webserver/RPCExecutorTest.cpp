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

#include <rpc/common/impl/HandlerProvider.h>
#include <util/Fixtures.h>
#include <webserver2/RPCExecutor.h>

#include <gtest/gtest.h>

class WebRPCExecutorTest : public MockBackendTest
{
protected:
    void
    SetUp() override
    {
        MockBackendTest::SetUp();
        work.emplace(ctx);  // make sure ctx does not stop on its own
        runner.emplace([this] { ctx.run(); });
        auto workQueue = WorkQueue::make_WorkQueue(cfg);
        auto counters = RPC::Counters::make_Counters(workQueue);
        auto ledgers = NetworkValidatedLedgers::make_ValidatedLedgers();
        subManager = SubscriptionManager::make_SubscriptionManager(cfg, mockBackendPtr);
        auto balancer = ETLLoadBalancer::make_ETLLoadBalancer(cfg, ctx, mockBackendPtr, subManager, ledgers);
        etl = ReportingETL::make_ReportingETL(cfg, ctx, mockBackendPtr, subManager, balancer, ledgers);
        auto const handlerProvider = std::make_shared<RPC::detail::ProductionHandlerProvider const>(
            mockBackendPtr, subManager, balancer, etl, counters);
        // ctx for dosguard
        boost::asio::io_context ctxSync;
        clio::IntervalSweepHandler sweepHandler = clio::IntervalSweepHandler{cfg, ctxSync};
        clio::DOSGuard dosGuard = clio::DOSGuard{cfg, sweepHandler};
        rpcEngine = RPC::RPCEngine::make_RPCEngine(
            cfg, mockBackendPtr, subManager, balancer, etl, dosGuard, workQueue, counters, handlerProvider);

        tagFactory = std::make_shared<util::TagDecoratorFactory>(cfg);
    }

    void
    TearDown() override
    {
        work.reset();
        ctx.stop();
        if (runner->joinable())
            runner->join();
        MockBackendTest::TearDown();
    }

    std::shared_ptr<RPC::RPCEngine> rpcEngine;
    std::shared_ptr<SubscriptionManager> subManager;
    std::shared_ptr<ReportingETL> etl;
    std::shared_ptr<util::TagDecoratorFactory> tagFactory;
    clio::Config cfg;
    boost::asio::io_context ctx;

private:
    std::optional<boost::asio::io_service::work> work;
    std::optional<std::thread> runner;
};

class MockETL
{
public:
    MOCK_METHOD(void, lastCloseAgeSeconds, (std::uint32_t), (const));
};

struct MockWsBase : public ServerNG::WsBase
{
    std::string message;

    void
    send(std::shared_ptr<Message> msg_type) override
    {
        message += std::string(msg_type->data());
    }

    MockWsBase(util::TagDecoratorFactory const& factory) : ServerNG::WsBase(factory, "localhost.fake.ip")
    {
    }
};

TEST_F(WebRPCExecutorTest, test1)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);

    auto rpcExecutor = RPCExecutor(mockBackendPtr, rpcEngine, etl, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"(
    {
        "method": "server_info"
    })")
                       .as_object();
    rpcExecutor(
        std::move(request), [](std::string, http::status) {}, session, *session);
}
