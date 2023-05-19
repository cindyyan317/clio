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

#include <webserver2/HttpBase.h>

namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

namespace ServerNG {
// Handles an HTTPS server connection
template <class Callback>
class SslHttpSession : public HttpBase<SslHttpSession, Callback>,
                       public std::enable_shared_from_this<SslHttpSession<Callback>>
{
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;

public:
    // Take ownership of the socket
    explicit SslHttpSession(
        boost::asio::io_context& ioc,
        tcp::socket&& socket,
        ssl::context& ctx,
        util::TagDecoratorFactory const& tagFactory,
        clio::DOSGuard& dosGuard,
        Callback& callback,
        boost::beast::flat_buffer buffer)
        : HttpBase<SslHttpSession, Callback>(ioc, tagFactory, dosGuard, callback, std::move(buffer))
        , stream_(std::move(socket), ctx)
    {
        try
        {
            this->ipMaybe = stream_.next_layer().socket().remote_endpoint().address().to_string();
            this->dosGuard().increment(*(this->ipMaybe));
        }
        catch (std::exception const&)
        {
        }
    }

    ~SslHttpSession()
    {
        if (this->ipMaybe and not this->upgraded_)
            this->dosGuard().decrement(*(this->ipMaybe));
    }

    boost::beast::ssl_stream<boost::beast::tcp_stream>&
    stream()
    {
        return stream_;
    }
    boost::beast::ssl_stream<boost::beast::tcp_stream>
    releaseStream()
    {
        return std::move(stream_);
    }

    // Start the asynchronous operation
    void
    run()
    {
        auto self = this->shared_from_this();
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session.
        net::dispatch(stream_.get_executor(), [self]() {
            // Set the timeout.
            boost::beast::get_lowest_layer(self->stream()).expires_after(std::chrono::seconds(30));

            // Perform the SSL handshake
            // Note, this is the buffered version of the handshake.
            self->stream_.async_handshake(
                ssl::stream_base::server,
                self->buffer_.data(),
                boost::beast::bind_front_handler(&SslHttpSession<Callback>::onHandshake, self));
        });
    }

    void
    onHandshake(boost::beast::error_code ec, std::size_t bytes_used)
    {
        if (ec)
            return this->httpFail(ec, "handshake");

        this->buffer_.consume(bytes_used);

        this->doRead();
    }

    void
    doClose()
    {
        // Set the timeout.
        boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));

        // Perform the SSL shutdown
        stream_.async_shutdown(boost::beast::bind_front_handler(&SslHttpSession::onShutdown, this->shared_from_this()));
    }

    void
    onShutdown(boost::beast::error_code ec)
    {
        if (ec)
            return this->httpFail(ec, "shutdown");

        // At this point the connection is closed gracefully
    }

    void
    upgrade()
    {
        std::make_shared<SslWsUpgrader<Callback>>(
            this->ioc_,
            std::move(stream_),
            this->ipMaybe,
            this->tagFactory_,
            this->dosGuard_,
            this->callback_,
            std::move(this->buffer_),
            std::move(this->req_))
            ->run();
    }
};
}  // namespace ServerNG