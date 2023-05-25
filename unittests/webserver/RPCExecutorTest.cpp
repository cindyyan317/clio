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
#include <util/MockRPCEngine.h>
#include <util/MockReportingETL.h>
#include <webserver2/RPCExecutor.h>

#include <chrono>
#include <gtest/gtest.h>

using namespace std::chrono_literals;

constexpr static auto MINSEQ = 10;
constexpr static auto MAXSEQ = 30;

struct MockWsBase : public ServerNG::ConnectionBase
{
    std::string message;

    void
    send(std::shared_ptr<std::string> msg_type) override
    {
        message += std::string(msg_type->data());
    }

    void
    send(std::string&& msg, http::status status = http::status::ok) override
    {
        message += std::string(msg.data());
    }

    MockWsBase(util::TagDecoratorFactory const& factory) : ServerNG::ConnectionBase(factory, "localhost.fake.ip")
    {
    }
};

class WebRPCExecutorTest : public MockBackendTest
{
protected:
    void
    SetUp() override
    {
        MockBackendTest::SetUp();

        etl = std::make_shared<MockReportingETL>();
        rpcEngine = std::make_shared<MockAsyncRPCEngine>();
        tagFactory = std::make_shared<util::TagDecoratorFactory>(cfg);
        subManager = std::make_shared<SubscriptionManager>(cfg, mockBackendPtr);
    }

    void
    TearDown() override
    {
        MockBackendTest::TearDown();
    }

    std::shared_ptr<MockAsyncRPCEngine> rpcEngine;
    std::shared_ptr<MockReportingETL> etl;
    std::shared_ptr<SubscriptionManager> subManager;
    std::shared_ptr<util::TagDecoratorFactory> tagFactory;
    clio::Config cfg;
};

TEST_F(WebRPCExecutorTest, HTTPDefaultPath)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"({
                                            "method": "server_info"
                                        })")
                       .as_object();

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr result = "{}";
    static auto constexpr response = R"({
                                        "result":{
                                            "status":"success"
                                        },
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    rpcExecutor(std::move(request), nullptr, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, WsNormalPath)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"({
                                            "command": "server_info",
                                            "id": 99
                                        })")
                       .as_object();

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr result = "{}";
    static auto constexpr response = R"({
                                        "result":{
                                        },
                                        "id":99,
                                        "status":"success",
                                        "type":"response",
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    rpcExecutor(std::move(request), session, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, HTTPForwardedPath)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"({
                                         "method": "server_info"
                                    })")
                       .as_object();

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr result = R"({
                                        "result": {
                                            "index": 1
                                        },
                                        "forwarded": true
                                    })";
    static auto constexpr response = R"({
                                        "result":{
                                                "index": 1,
                                                "status": "success"
                                        },
                                        "forwarded": true,
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                            ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    rpcExecutor(std::move(request), nullptr, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, WsForwardedPath)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"({
                                            "command": "server_info",
                                            "id": 99
                                        })")
                       .as_object();

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr result = R"({
                                        "result": {
                                            "index": 1
                                        },
                                        "forwarded": true
                                    })";
    static auto constexpr response = R"({
                                        "result":{
                                            "index": 1
                                            },
                                        "forwarded": true,
                                        "id":99,
                                        "status":"success",
                                        "type":"response",
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    rpcExecutor(std::move(request), session, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, HTTPErrorPath)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    static auto constexpr response = R"({
                                        "result": {
                                            "error": "invalidParams",
                                            "error_code": 31,
                                            "error_message": "ledgerIndexMalformed",
                                            "status": "error",
                                            "type": "response",
                                            "request": {
                                                "method": "ledger",
                                                "params": [
                                                    {
                                                    "ledger_index": "xx"
                                                    }
                                                ]
                                            }
                                        },
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr requestJSON = R"({
                                            "method": "ledger",
                                            "params": [
                                                {
                                                "ledger_index": "xx"
                                                }
                                            ]
                                        })";
    auto request = boost::json::parse(requestJSON).as_object();
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(RPC::Status{RPC::RippledError::rpcINVALID_PARAMS, "ledgerIndexMalformed"}));
    EXPECT_CALL(*rpcEngine, notifyErrored("ledger")).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    rpcExecutor(std::move(request), nullptr, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, WsErrorPath)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    static auto constexpr response = R"({
                                        "id": "123",
                                        "error": "invalidParams",
                                        "error_code": 31,
                                        "error_message": "ledgerIndexMalformed",
                                        "status": "error",
                                        "type": "response",
                                        "request": {
                                            "command": "ledger",
                                            "ledger_index": "xx",
                                            "id": "123"
                                        },
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            }
                                        ]
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr requestJSON = R"({
                                            "command": "ledger",
                                            "ledger_index": "xx",
                                            "id": "123"
                                        })";
    auto request = boost::json::parse(requestJSON).as_object();
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(RPC::Status{RPC::RippledError::rpcINVALID_PARAMS, "ledgerIndexMalformed"}));
    EXPECT_CALL(*rpcEngine, notifyErrored("ledger")).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(45));

    rpcExecutor(std::move(request), session, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, HTTPNotReady)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"({
                                            "method": "server_info"
                                        })")
                       .as_object();

    static auto constexpr response = R"({
                                        "result":{
                                            "error":"notReady",
                                            "error_code":13,
                                            "error_message":"Not ready to handle this request.",
                                            "status":"error",
                                            "type":"response",
                                            "request":{
                                                "method":"server_info",
                                                "params":[
                                                    {
                                                    
                                                    }
                                                ]
                                            }
                                        }
                                    })";

    rpcExecutor(std::move(request), nullptr, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, WsNotReady)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"({
                                            "command": "server_info",
                                            "id": 99
                                        })")
                       .as_object();

    static auto constexpr response = R"({
                                        "error":"notReady",
                                        "error_code":13,
                                        "error_message":"Not ready to handle this request.",
                                        "status":"error",
                                        "type":"response",
                                        "id":99,
                                        "request":{
                                            "command":"server_info",
                                            "id":99
                                        }
                                    })";

    rpcExecutor(std::move(request), session, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, HTTPBadSyntax)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"({
                                            "method2": "server_info"
                                        })")
                       .as_object();

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr response = R"({
                                        "result":{
                                            "error":"badSyntax",
                                            "error_code":1,
                                            "error_message":"Syntax error.",
                                            "status":"error",
                                            "type":"response",
                                            "request":{
                                                "method2":"server_info",
                                                "params":[
                                                    {
                                                    
                                                    }
                                                ]
                                            }
                                        }
                                    })";

    rpcExecutor(std::move(request), nullptr, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, HTTPBadSyntaxWhenRequestSubscribe)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"({
                                            "method": "subscribe"
                                        })")
                       .as_object();

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr response = R"({
                                        "result":{
                                            "error":"badSyntax",
                                            "error_code":1,
                                            "error_message":"Syntax error.",
                                            "status":"error",
                                            "type":"response",
                                            "request":{
                                                "method":"subscribe",
                                                "params":[
                                                    {
                                                    
                                                    }
                                                ]
                                            }
                                        }
                                    })";

    rpcExecutor(std::move(request), nullptr, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, WsBadSyntax)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"(
    {
        "command2": "server_info",
        "id": 99
    })")
                       .as_object();

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr response = R"({
                                        "error":"badSyntax",
                                        "error_code":1,
                                        "error_message":"Syntax error.",
                                        "status":"error",
                                        "type":"response",
                                        "id":99,
                                        "request":{
                                            "command2":"server_info",
                                            "id":99
                                        }
                                    })";

    rpcExecutor(std::move(request), session, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, HTTPInternalError)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    static auto constexpr response = R"({
                                        "result": {
                                            "error":"internal",
                                            "error_code":73,
                                            "error_message":"Internal error.",
                                            "status":"error",
                                            "type":"response",
                                            "request":{
                                                "method": "ledger",
                                                "params": [
                                                    {

                                                    }
                                                ]
                                            }
                                        }
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr requestJSON = R"({
                                            "method": "ledger",
                                            "params": [
                                                {

                                                }
                                            ]
                                        })";
    auto request = boost::json::parse(requestJSON).as_object();
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_)).Times(1).WillOnce(testing::Throw(std::runtime_error("MyError")));

    rpcExecutor(std::move(request), nullptr, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, WsInternalError)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    static auto constexpr response = R"({
                                        "error":"internal",
                                        "error_code":73,
                                        "error_message":"Internal error.",
                                        "status":"error",
                                        "type":"response",
                                        "id":"123",
                                        "request":{
                                            "command":"ledger",
                                            "id":"123"
                                        }
                                    })";

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr requestJSON = R"({
                                            "command": "ledger",
                                            "id": "123"
                                        })";
    auto request = boost::json::parse(requestJSON).as_object();
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_)).Times(1).WillOnce(testing::Throw(std::runtime_error("MyError")));

    rpcExecutor(std::move(request), session, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, HTTPOutDated)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"({
                                            "method": "server_info"
                                        })")
                       .as_object();

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr result = "{}";
    static auto constexpr response = R"({
                                        "result":{
                                            "status":"success"
                                        },
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            },
                                            {
                                                "id":2002,
                                                "message":"This server may be out of date"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(61));

    rpcExecutor(std::move(request), nullptr, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, WsOutdated)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcExecutor = RPCExecutor<MockAsyncRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine, etl, subManager, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"({
                                            "command": "server_info",
                                            "id": 99
                                        })")
                       .as_object();

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr result = "{}";
    static auto constexpr response = R"({
                                        "result":{
                                        },
                                        "id":99,
                                        "status":"success",
                                        "type":"response",
                                        "warnings":[
                                            {
                                                "id":2001,
                                                "message":"This is a clio server. clio only serves validated data. If you want to talk to rippled, include 'ledger_index':'current' in your request"
                                            },
                                            {
                                                "id":2002,
                                                "message":"This server may be out of date"
                                            }
                                        ]
                                    })";
    EXPECT_CALL(*rpcEngine, buildResponse(testing::_))
        .WillOnce(testing::Return(boost::json::parse(result).as_object()));
    EXPECT_CALL(*rpcEngine, notifyComplete("server_info", testing::_)).Times(1);

    EXPECT_CALL(*etl, lastCloseAgeSeconds()).WillOnce(testing::Return(61));

    rpcExecutor(std::move(request), session, *session);
    std::this_thread::sleep_for(20ms);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
    std::cout << session->message << std::endl;
}

TEST_F(WebRPCExecutorTest, WsTooBusy)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcEngine2 = std::make_shared<MockRPCEngine>();
    auto rpcExecutor = RPCExecutor<MockRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine2, etl, subManager, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"({
                                            "command": "server_info",
                                            "id": 99
                                        })")
                       .as_object();

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr response =
        R"({"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"})";
    EXPECT_CALL(*rpcEngine2, post).WillOnce(testing::Return(false));
    rpcExecutor(std::move(request), session, *session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}

TEST_F(WebRPCExecutorTest, HTTPTooBusy)
{
    auto session = std::make_shared<MockWsBase>(*tagFactory);
    auto rpcEngine2 = std::make_shared<MockRPCEngine>();
    auto rpcExecutor = RPCExecutor<MockRPCEngine, MockReportingETL>(
        mockBackendPtr, rpcEngine2, etl, subManager, tagFactory->with(std::cref(session->tag())));
    auto request = boost::json::parse(R"({
                                            "method": "server_info"
                                        })")
                       .as_object();

    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max

    static auto constexpr response =
        R"({"error":"tooBusy","error_code":9,"error_message":"The server is too busy to help you now.","status":"error","type":"response"})";

    EXPECT_CALL(*rpcEngine2, post).WillOnce(testing::Return(false));
    rpcExecutor(std::move(request), nullptr, *session);
    EXPECT_EQ(boost::json::parse(session->message), boost::json::parse(response));
}
