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

#include <log/Logger.h>
#include <webserver2/HttpSession.h>
#include <webserver2/SslHttpSession.h>
#include <webserver2/Type.h>

namespace ServerNG {

template <template <class> class PlainSession, template <class> class SslSession, ServerCallback Callback>
class Detector : public std::enable_shared_from_this<Detector<PlainSession, SslSession, Callback>>
{
    using std::enable_shared_from_this<Detector<PlainSession, SslSession, Callback>>::shared_from_this;

    clio::Logger log_{"WebServer"};
    boost::asio::io_context& ioc_;
    boost::beast::tcp_stream stream_;
    std::optional<std::reference_wrapper<ssl::context>> ctx_;
    util::TagDecoratorFactory const& tagFactory_;
    clio::DOSGuard& dosGuard_;
    Callback& callback_;
    boost::beast::flat_buffer buffer_;

public:
    Detector(
        boost::asio::io_context& ioc,
        tcp::socket&& socket,
        std::optional<std::reference_wrapper<ssl::context>> ctx,
        util::TagDecoratorFactory const& tagFactory,
        clio::DOSGuard& dosGuard,
        Callback& callback)
        : ioc_(ioc)
        , stream_(std::move(socket))
        , ctx_(ctx)
        , tagFactory_(tagFactory)
        , dosGuard_(dosGuard)
        , callback_(callback)
    {
    }

    inline void
    fail(boost::system::error_code ec, char const* message)
    {
        if (ec == net::ssl::error::stream_truncated)
            return;

        log_.info() << "Detector failed (" << message << "): " << ec.message();
    }

    // Launch the detector
    void
    run()
    {
        // Set the timeout.
        boost::beast::get_lowest_layer(stream_).expires_after(std::chrono::seconds(30));
        // Detect a TLS handshake
        async_detect_ssl(stream_, buffer_, boost::beast::bind_front_handler(&Detector::onDetect, shared_from_this()));
    }

    void
    onDetect(boost::beast::error_code ec, bool result)
    {
        if (ec)
            return fail(ec, "detect");

        // would not create session if can not get ip
        std::string ip;
        try
        {
            ip = stream_.socket().remote_endpoint().address().to_string();
        }
        catch (std::exception const&)
        {
            return fail(ec, "cannot get remote endpoint");
        }

        if (result)
        {
            if (!ctx_)
                return fail(ec, "ssl not supported by this server");
            // Launch SSL session
            std::make_shared<SslSession<Callback>>(
                ioc_, stream_.release_socket(), ip, *ctx_, tagFactory_, dosGuard_, callback_, std::move(buffer_))
                ->run();
            return;
        }
        std::cout << "Plain session" << std::endl;
        // Launch plain session
        std::make_shared<PlainSession<Callback>>(
            ioc_, stream_.release_socket(), ip, tagFactory_, dosGuard_, callback_, std::move(buffer_))
            ->run();
    }
};

template <template <class> class PlainSession, template <class> class SslSession, ServerCallback Callback>
class Server : public std::enable_shared_from_this<Server<PlainSession, SslSession, Callback>>
{
    using std::enable_shared_from_this<Server<PlainSession, SslSession, Callback>>::shared_from_this;

    clio::Logger log_{"WebServer"};
    boost::asio::io_context& ioc_;
    std::optional<std::reference_wrapper<ssl::context>> ctx_;
    util::TagDecoratorFactory tagFactory_;
    clio::DOSGuard& dosGuard_;
    Callback& callback_;
    tcp::acceptor acceptor_;

public:
    Server(
        boost::asio::io_context& ioc,
        std::optional<std::reference_wrapper<ssl::context>> ctx,
        tcp::endpoint endpoint,
        util::TagDecoratorFactory tagFactory,
        clio::DOSGuard& dosGuard,
        Callback& callback)
        : ioc_(ioc)
        , ctx_(ctx)
        , tagFactory_(std::move(tagFactory))
        , dosGuard_(dosGuard)
        , callback_(callback)
        , acceptor_(net::make_strand(ioc))
    {
        boost::beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
            return;

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if (ec)
            return;

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if (ec)
        {
            log_.error() << "Failed to bind to endpoint: " << endpoint << ". message: " << ec.message();
            throw std::runtime_error("Failed to bind to specified endpoint");
        }

        // Start listening for connections
        acceptor_.listen(net::socket_base::max_listen_connections, ec);
        if (ec)
        {
            log_.error() << "Failed to listen at endpoint: " << endpoint << ". message: " << ec.message();
            throw std::runtime_error("Failed to listen at specified endpoint");
        }
    }

    // Start accepting incoming connections
    void
    run()
    {
        doAccept();
    }

private:
    void
    doAccept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_), boost::beast::bind_front_handler(&Server::onAccept, shared_from_this()));
    }

    void
    onAccept(boost::beast::error_code ec, tcp::socket socket)
    {
        if (!ec)
        {
            auto ctxRef = ctx_ ? std::optional<std::reference_wrapper<ssl::context>>{ctx_.value()} : std::nullopt;
            // Create the detector session and run it
            std::make_shared<Detector<PlainSession, SslSession, Callback>>(
                ioc_, std::move(socket), ctxRef, tagFactory_, dosGuard_, callback_)
                ->run();
        }

        // Accept another connection
        doAccept();
    }
};

template <class Executor>
using HttpServer = Server<HttpSession, SslHttpSession, Executor>;

template <class Executor>
static std::shared_ptr<HttpServer<Executor>>
make_HttpServer(
    clio::Config const& config,
    boost::asio::io_context& ioc,
    std::optional<std::reference_wrapper<ssl::context>> sslCtx,
    clio::DOSGuard& dosGuard,
    Executor& callback)
{
    static clio::Logger log{"WebServer"};
    if (!config.contains("server"))
        return nullptr;

    auto const serverConfig = config.section("server");
    auto const address = boost::asio::ip::make_address(serverConfig.value<std::string>("ip"));
    auto const port = serverConfig.value<unsigned short>("port");

    auto server = std::make_shared<HttpServer<Executor>>(
        ioc,
        sslCtx,
        boost::asio::ip::tcp::endpoint{address, port},
        util::TagDecoratorFactory(config),
        dosGuard,
        callback);

    server->run();
    return server;
}

}  // namespace ServerNG
