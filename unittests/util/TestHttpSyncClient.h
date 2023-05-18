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

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/http.hpp>

#include <string>

struct HttpSyncClient
{
    static std::string
    syncPost(std::string const& host, std::string const& port, std::string const& body)
    {
        boost::asio::io_context ioc;

        // These objects perform our I/O
        net::ip::tcp::resolver resolver(ioc);
        boost::beast::tcp_stream stream(ioc);

        // Look up the domain name
        auto const results = resolver.resolve(host, port);

        // Make the connection on the IP address we get from a lookup
        stream.connect(results);

        http::request<http::string_body> req{http::verb::post, "/", 10};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req.body() = std::string(body);
        req.prepare_payload();
        // Send the HTTP request to the remote host
        http::write(stream, req);

        // This buffer is used for reading and must be persisted
        boost::beast::flat_buffer buffer;

        // Declare a container to hold the response
        http::response<http::string_body> res;

        // Receive the HTTP response
        http::read(stream, buffer, res);
        // Gracefully close the socket
        boost::beast::error_code ec;
        stream.socket().shutdown(tcp::socket::shutdown_both, ec);

        return std::string(res.body());
    }
};

class WebSocketSyncClient
{
    // The io_context is required for all I/O
    net::io_context ioc_;

    // These objects perform our I/O
    tcp::resolver resolver_{ioc_};
    websocket::stream<tcp::socket> ws_{ioc_};

public:
    void
    connect(std::string const& host, std::string const& port)
    {
        // Look up the domain name
        auto const results = resolver_.resolve(host, port);

        // Make the connection on the IP address we get from a lookup
        auto const ep = net::connect(ws_.next_layer(), results);

        // Update the host_ string. This will provide the value of the
        // Host HTTP header during the WebSocket handshake.
        // See https://tools.ietf.org/html/rfc7230#section-5.4
        auto const hostPort = host + ':' + std::to_string(ep.port());

        // Set a decorator to change the User-Agent of the handshake
        ws_.set_option(websocket::stream_base::decorator([](websocket::request_type& req) {
            req.set(http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
        }));

        // Perform the websocket handshake
        ws_.handshake(hostPort, "/");
    }

    void
    disconnect()
    {
        ws_.close(websocket::close_code::normal);
    }

    std::string
    syncPost(std::string const& body)
    {
        // Send the message
        ws_.write(net::buffer(std::string(body)));

        // This buffer will hold the incoming message
        boost::beast::flat_buffer buffer;

        // Read a message into our buffer
        ws_.read(buffer);

        return boost::beast::buffers_to_string(buffer.data());
    }
};
