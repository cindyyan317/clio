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

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace websocket = boost::beast::websocket;
using tcp = boost::asio::ip::tcp;

namespace ServerNG {
// Echoes back all received WebSocket messages
template <class Callback>
class PlainWsSession : public WsSession<PlainWsSession, Callback>
{
    websocket::stream<boost::beast::tcp_stream> ws_;

public:
    // Take ownership of the socket
    explicit PlainWsSession(
        boost::asio::io_context& ioc,
        boost::asio::ip::tcp::socket&& socket,
        std::optional<std::string> ip,
        util::TagDecoratorFactory const& tagFactory,
        clio::DOSGuard& dosGuard,
        Callback& callback,
        boost::beast::flat_buffer&& buffer)
        : WsSession<PlainWsSession, Callback>(ioc, ip, tagFactory, dosGuard, callback, std::move(buffer))
        , ws_(std::move(socket))
    {
    }

    websocket::stream<boost::beast::tcp_stream>&
    ws()
    {
        return ws_;
    }

    //??? do we need this
    std::optional<std::string>
    ip()
    {
        return this->ip_;
    }

    ~PlainWsSession() = default;
};

template <class Callback>
class WsUpgrader : public std::enable_shared_from_this<WsUpgrader<Callback>>
{
    boost::asio::io_context& ioc_;
    boost::beast::tcp_stream http_;
    boost::optional<http::request_parser<http::string_body>> parser_;
    boost::beast::flat_buffer buffer_;
    util::TagDecoratorFactory const& tagFactory_;
    clio::DOSGuard& dosGuard_;
    http::request<http::string_body> req_;
    std::optional<std::string> ip_;
    Callback callback_;

public:
    WsUpgrader(
        boost::asio::io_context& ioc,
        boost::beast::tcp_stream&& stream,
        std::optional<std::string> ip,
        util::TagDecoratorFactory const& tagFactory,
        clio::DOSGuard& dosGuard,
        Callback& callback,
        boost::beast::flat_buffer&& b,
        http::request<http::string_body> req)
        : ioc_(ioc)
        , http_(std::move(stream))
        , buffer_(std::move(b))
        , tagFactory_(tagFactory)
        , dosGuard_(dosGuard)
        , req_(std::move(req))
        , ip_(ip)
        , callback_(callback)
    {
    }

    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.

        net::dispatch(
            http_.get_executor(),
            boost::beast::bind_front_handler(&WsUpgrader<Callback>::doUpgrade, this->shared_from_this()));
    }

private:
    void
    doUpgrade()
    {
        parser_.emplace();

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser_->body_limit(10000);

        // Set the timeout.
        boost::beast::get_lowest_layer(http_).expires_after(std::chrono::seconds(30));

        onUpgrade();
    }

    void
    onUpgrade()
    {
        // See if it is a WebSocket Upgrade
        if (!websocket::is_upgrade(req_))
            return;

        // Disable the timeout.
        // The websocket::stream uses its own timeout settings.
        boost::beast::get_lowest_layer(http_).expires_never();

        std::make_shared<PlainWsSession<Callback>>(
            ioc_, http_.release_socket(), ip_, tagFactory_, dosGuard_, callback_, std::move(buffer_))
            ->run(std::move(req_));
    }
};

}  // namespace ServerNG
