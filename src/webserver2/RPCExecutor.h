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

#include <webserver2/WsBase.h>

#include <boost/json/parse.hpp>

#include <iostream>

class RPCExecutor
{
    std::shared_ptr<BackendInterface const> backend_;
    std::shared_ptr<RPC::RPCEngine> rpcEngine_;
    std::shared_ptr<ReportingETL const> etl_;

public:
    void
    operator()(
        boost::json::object&& req,
        std::function<void(std::string, http::status)> cb,
        std::shared_ptr<ServerNG::WsBase> ws,
        util::TagDecoratorFactory const& tagFactory,
        std::string const& clientIP,
        clio::Logger& perfLog,
        util::Taggable const& taggable)
    {
        std::cout << "req:" << req << std::endl;
        cb(boost::json::serialize(req), http::status::ok);

        if (!rpcEngine_->post(
                [&, this](boost::asio::yield_context yc) {
                    handleRequest(yc, std::move(req), cb, ws, tagFactory, clientIP, perfLog, taggable);
                },
                clientIP))
        {
            cb(boost::json::serialize(RPC::makeError(RPC::RippledError::rpcTOO_BUSY)), http::status::ok);
        }
    }

    void
    operator()(boost::beast::error_code ec, std::shared_ptr<ServerNG::WsBase> ws)
    {
    }

private:
    void
    handleRequest(
        boost::asio::yield_context& yc,
        boost::json::object&& request,
        std::function<void(std::string, http::status)> cb,
        std::shared_ptr<ServerNG::WsBase> const& ws,
        util::TagDecoratorFactory const& tagFactory,
        std::string const& clientIP,
        clio::Logger& perfLog,
        util::Taggable const& taggable)
    {
        if (!request.contains("params"))
            request["params"] = boost::json::array({boost::json::object{}});

        try
        {
            auto const range = backend_->fetchLedgerRange();
            if (!range)
                cb(boost::json::serialize(RPC::makeError(RPC::RippledError::rpcNOT_READY)), http::status::ok);

            auto context = RPC::make_HttpContext(yc, request, tagFactory, *range, clientIP);
            if (!context)
                cb(boost::json::serialize(RPC::makeError(RPC::RippledError::rpcBAD_SYNTAX)), http::status::ok);

            boost::json::object response;
            auto [v, timeDiff] = util::timed([&]() { return rpcEngine_->buildResponse(*context); });

            auto us = std::chrono::duration<int, std::milli>(timeDiff);
            RPC::logDuration(*context, us);

            if (auto const status = std::get_if<RPC::Status>(&v))
            {
                rpcEngine_->notifyErrored(context->method);
                auto error = RPC::makeError(*status);
                error["request"] = request;
                response["result"] = error;
                perfLog.debug() << taggable.tag() << "Encountered error: " << boost::json::serialize(response);
            }
            else
            {
                // This can still technically be an error. Clio counts forwarded
                // requests as successful.

                rpcEngine_->notifyComplete(context->method, us);

                auto result = std::get<boost::json::object>(v);
                if (result.contains("result") && result.at("result").is_object())
                    result = result.at("result").as_object();

                if (!result.contains("error"))
                    result["status"] = "success";

                response["result"] = result;
            }

            boost::json::array warnings;
            warnings.emplace_back(RPC::makeWarning(RPC::warnRPC_CLIO));

            auto const lastCloseAge = etl_->lastCloseAgeSeconds();
            if (lastCloseAge >= 60)
                warnings.emplace_back(RPC::makeWarning(RPC::warnRPC_OUTDATED));

            response["warnings"] = warnings;
            return cb(boost::json::serialize(response), http::status::ok);
        }
        catch (std::exception const& e)
        {
            perfLog.error() << taggable.tag() << "Caught exception : " << e.what();
            return cb(
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcINTERNAL)),
                http::status::internal_server_error);
        }
    }
};
