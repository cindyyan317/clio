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

#pragma once

#include <webserver2/HttpBase.h>

namespace ServerNG {

using tcp = boost::asio::ip::tcp;

// Handles an HTTP server connection
template <ServerCallback Callback>
class HttpSession : public HttpBase<HttpSession, Callback>, public std::enable_shared_from_this<HttpSession<Callback>>
{
    boost::beast::tcp_stream stream_;

public:
    // Take ownership of the socket
    explicit HttpSession(
        boost::asio::io_context& ioc,
        tcp::socket&& socket,
        std::string const& ip,
        util::TagDecoratorFactory const& tagFactory,
        clio::DOSGuard& dosGuard,
        Callback& callback,
        boost::beast::flat_buffer buffer)
        : HttpBase<HttpSession, Callback>(ioc, ip, tagFactory, dosGuard, callback, std::move(buffer))
        , stream_(std::move(socket))
    {
        this->dosGuard().increment(ip);
    }

    ~HttpSession()
    {
        if (not this->upgraded_)
            this->dosGuard().decrement(this->clientIp);
    }

    boost::beast::tcp_stream&
    stream()
    {
        return stream_;
    }

    boost::beast::tcp_stream
    releaseStream()
    {
        return std::move(stream_);
    }

    // Start the asynchronous operation
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this HttpSession. Although not strictly
        // necessary for single-threaded contexts, this example code is written
        // to be thread-safe by default.
        boost::asio::dispatch(
            stream_.get_executor(),
            boost::beast::bind_front_handler(&HttpBase<HttpSession, Callback>::doRead, this->shared_from_this()));
    }

    void
    doClose()
    {
        // Send a TCP shutdown
        boost::beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        // At this point the connection is closed gracefully
    }

    void
    upgrade()
    {
        std::make_shared<WsUpgrader<Callback>>(
            this->ioc_,
            std::move(stream_),
            this->clientIp,
            this->tagFactory_,
            this->dosGuard_,
            this->callback_,
            std::move(this->buffer_),
            std::move(this->req_))
            ->run();
    }
};
}  // namespace ServerNG
