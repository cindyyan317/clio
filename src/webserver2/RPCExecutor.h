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

#include <rpc/Factories.h>
#include <rpc/RPCHelpers.h>

#include <webserver2/details/WsBase.h>

#include <boost/json/parse.hpp>

template <class Engine, class ETL>
class RPCExecutor
{
    std::shared_ptr<BackendInterface const> backend_;
    std::shared_ptr<Engine> rpcEngine_;
    std::shared_ptr<ETL const> etl_;
    // subscription manager holds the shared_ptr of this class
    std::weak_ptr<SubscriptionManager> subscriptions_;
    util::TagDecoratorFactory const& tagFactory_;

public:
    RPCExecutor(
        std::shared_ptr<BackendInterface const> backend,
        std::shared_ptr<Engine> rpcEngine,
        std::shared_ptr<ETL const> etl,
        std::shared_ptr<SubscriptionManager> subscriptions,
        util::TagDecoratorFactory const& tagFactory)
        : backend_(backend), rpcEngine_(rpcEngine), etl_(etl), subscriptions_(subscriptions), tagFactory_(tagFactory)
    {
    }

    void
    operator()(boost::json::object&& req, std::shared_ptr<Server::ConnectionBase> const& ws)
    {
        if (!rpcEngine_->post(
                [request = std::move(req), ws, this](boost::asio::yield_context yc) {
                    handleRequest(yc, std::move(request), ws);
                },
                ws->clientIp))
        {
            ws->send(
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcTOO_BUSY)), boost::beast::http::status::ok);
        }
    }

    void
    operator()(boost::beast::error_code ec, std::shared_ptr<Server::ConnectionBase> const& ws)
    {
        if (auto manager = subscriptions_.lock(); manager)
            manager->cleanup(ws);
    }

private:
    void
    handleRequest(
        boost::asio::yield_context& yc,
        boost::json::object const&& request,
        std::shared_ptr<Server::ConnectionBase> ws)
    {
        ws->perfLog.debug() << ws->tag() << "Received request from work queue: " << request;
        ws->log.info() << ws->tag() << "Received request from work queue : " << request;

        auto const id = request.contains("id") ? request.at("id") : nullptr;

        auto const composeError = [&](auto const& error) -> boost::json::object {
            auto e = RPC::makeError(error);
            if (!id.is_null())
                e["id"] = id;
            e["request"] = request;
            if (ws->upgraded)
            {
                return e;
            }
            else
            {
                return boost::json::object{{"result", e}};
            }
        };
        try
        {
            auto const range = backend_->fetchLedgerRange();
            // for the error happened before the handler, we don't attach the clio warning
            if (!range)
                return ws->send(
                    boost::json::serialize(composeError(RPC::RippledError::rpcNOT_READY)),
                    boost::beast::http::status::ok);

            auto context = ws->upgraded
                ? RPC::make_WsContext(yc, request, ws, tagFactory_.with(ws->tag()), *range, ws->clientIp)
                : RPC::make_HttpContext(yc, request, tagFactory_.with(ws->tag()), *range, ws->clientIp);
            if (!context)
            {
                ws->perfLog.warn() << ws->tag() << "Could not create RPC context";
                return ws->send(
                    boost::json::serialize(composeError(RPC::RippledError::rpcBAD_SYNTAX)),
                    boost::beast::http::status::ok);
            }

            boost::json::object response;
            auto [v, timeDiff] = util::timed([&]() { return rpcEngine_->buildResponse(*context); });

            auto us = std::chrono::duration<int, std::milli>(timeDiff);
            RPC::logDuration(*context, us);

            if (auto const status = std::get_if<RPC::Status>(&v))
            {
                rpcEngine_->notifyErrored(context->method);
                response = std::move(composeError(*status));
                ws->perfLog.debug() << ws->tag() << "Encountered error: " << boost::json::serialize(response);
            }
            else
            {
                // This can still technically be an error. Clio counts forwarded
                // requests as successful.

                rpcEngine_->notifyComplete(context->method, us);
                auto& result = std::get<boost::json::object>(v);
                auto const isForwarded = result.contains("forwarded") && result.at("forwarded").is_bool() &&
                    result.at("forwarded").as_bool();

                // if the result is forwarded - just use it as is
                // the response has "result" inside
                // but keep all default fields in the response too.
                if (isForwarded)
                {
                    for (auto const& [k, v] : result)
                        response.insert_or_assign(k, v);
                }
                else
                {
                    response["result"] = result;
                }
                // for ws , there is additional field "status" in response
                // otherwise , the "status" is in the "result" field
                if (ws->upgraded)
                {
                    if (!id.is_null())
                        response["id"] = id;
                    if (!response.contains("error"))
                        response["status"] = "success";
                    response["type"] = "response";
                }
                else
                {
                    if (response.contains("result") && !response["result"].as_object().contains("error"))
                        response["result"].as_object()["status"] = "success";
                }
            }

            boost::json::array warnings;
            warnings.emplace_back(RPC::makeWarning(RPC::warnRPC_CLIO));

            auto const lastCloseAge = etl_->lastCloseAgeSeconds();
            if (lastCloseAge >= 60)
                warnings.emplace_back(RPC::makeWarning(RPC::warnRPC_OUTDATED));

            response["warnings"] = warnings;
            ws->send(boost::json::serialize(response), boost::beast::http::status::ok);
        }
        catch (std::exception const& e)
        {
            ws->perfLog.error() << ws->tag() << "Caught exception : " << e.what();
            return ws->send(
                boost::json::serialize(composeError(RPC::RippledError::rpcINTERNAL)),
                boost::beast::http::status::internal_server_error);
        }
    }
};
