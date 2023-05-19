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

#include <log/Logger.h>
#include <main/Build.h>
#include <util/Profiler.h>
#include <webserver/DOSGuard.h>
#include <webserver2/Connect.h>
#include <webserver2/PlainWsSession.h>
#include <webserver2/SslWsSession.h>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/config.hpp>
#include <boost/json.hpp>

#include <memory>
#include <string>

// TODO: consider removing those - visible to anyone including this header
namespace http = boost::beast::http;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;
namespace ServerNG {
static std::string defaultResponse =
    "<!DOCTYPE html><html><head><title>"
    " Test page for reporting mode</title></head><body><h1>"
    " Test</h1><p>This page shows xrpl reporting http(s) "
    "connectivity is working.</p></body></html>";

// From Boost Beast examples http_server_flex.cpp
template <template <class> class Derived, class Callback>
class HttpBase : public Connection
{
    // using Callback = typename Derived::CallbackType;

    // Access the derived class, this is part of
    // the Curiously Recurring Template Pattern idiom.
    Derived<Callback>&
    derived()
    {
        return static_cast<Derived<Callback>&>(*this);
    }

    struct send_lambda
    {
        HttpBase& self_;

        explicit send_lambda(HttpBase& self) : self_(self)
        {
        }

        template <bool isRequest, class Body, class Fields>
        void
        operator()(http::message<isRequest, Body, Fields>&& msg) const
        {
            if (self_.dead())
                return;

            // The lifetime of the message has to extend
            // for the duration of the async operation so
            // we use a shared_ptr to manage it.
            auto sp = std::make_shared<http::message<isRequest, Body, Fields>>(std::move(msg));

            // Store a type-erased version of the shared
            // pointer in the class to keep it alive.
            self_.res_ = sp;

            // Write the response
            http::async_write(
                self_.derived().stream(),
                *sp,
                boost::beast::bind_front_handler(
                    &HttpBase::onWrite, self_.derived().shared_from_this(), sp->need_eof()));
        }
    };

    boost::system::error_code ec_;
    std::shared_ptr<void> res_;
    send_lambda lambda_;

protected:
    boost::beast::flat_buffer buffer_;
    bool upgraded_ = false;
    boost::asio::io_context& ioc_;
    http::request<http::string_body> req_;
    clio::DOSGuard& dosGuard_;
    util::TagDecoratorFactory const& tagFactory_;
    Callback& callback_;

    bool
    dead()
    {
        return ec_ != boost::system::error_code{};
    }

    inline void
    httpFail(boost::beast::error_code ec, char const* what)
    {
        // ssl::error::stream_truncated, also known as an SSL "short read",
        // indicates the peer closed the connection without performing the
        // required closing handshake (for example, Google does this to
        // improve performance). Generally this can be a security issue,
        // but if your communication protocol is self-terminated (as
        // it is with both HTTP and WebSocket) then you may simply
        // ignore the lack of close_notify.
        //
        // https://github.com/boostorg/beast/issues/38
        //
        // https://security.stackexchange.com/questions/91435/how-to-handle-a-malicious-ssl-tls-shutdown
        //
        // When a short read would cut off the end of an HTTP message,
        // Beast returns the error boost::beast::http::error::partial_message.
        // Therefore, if we see a short read here, it has occurred
        // after the message has been completed, so it is safe to ignore it.

        if (ec == net::ssl::error::stream_truncated)
            return;

        if (!ec_ && ec != boost::asio::error::operation_aborted)
        {
            ec_ = ec;
            perfLog.info() << tag() << ": " << what << ": " << ec.message();
            boost::beast::get_lowest_layer(derived().stream()).socket().close(ec);
        }
    }

public:
    HttpBase(
        boost::asio::io_context& ioc,
        util::TagDecoratorFactory const& tagFactory,
        clio::DOSGuard& dosGuard,
        Callback& callback,
        boost::beast::flat_buffer buffer)
        : Connection(tagFactory)
        , lambda_(*this)
        , buffer_(std::move(buffer))
        , ioc_(ioc)
        , dosGuard_(dosGuard)
        , tagFactory_(tagFactory)
        , callback_(callback)
    {
        perfLog.debug() << tag() << "http session created";
    }

    virtual ~HttpBase()
    {
        perfLog.debug() << tag() << "http session closed";
    }

    clio::DOSGuard&
    dosGuard()
    {
        return dosGuard_;
    }

    void
    doRead()
    {
        if (dead())
            return;
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        boost::beast::get_lowest_layer(derived().stream()).expires_after(std::chrono::seconds(30));

        // Read a request
        http::async_read(
            derived().stream(),
            buffer_,
            req_,
            boost::beast::bind_front_handler(&HttpBase::onRead, derived().shared_from_this()));
    }

    void
    onRead(boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if (ec == http::error::end_of_stream)
            return derived().doClose();

        if (ec)
            return httpFail(ec, "read");

        if (!ipMaybe)
        {
            return;
        }

        if (boost::beast::websocket::is_upgrade(req_))
        {
            upgraded_ = true;
            // Disable the timeout.
            // The websocket::stream uses its own timeout settings.
            boost::beast::get_lowest_layer(derived().stream()).expires_never();
            return derived().upgrade();
        }

        auto const httpResponse = [&](http::status status, std::string content_type, std::string message) {
            http::response<http::string_body> res{status, req_.version()};
            res.set(http::field::server, "clio-server-" + Build::getClioVersionString());
            res.set(http::field::content_type, content_type);
            res.keep_alive(req_.keep_alive());
            res.body() = std::string(message);
            res.prepare_payload();
            return res;
        };

        // to avoid overwhelm work queue, the request limit check should be
        // before posting to queue the web socket creation will be guarded via
        // connection limit
        if (!dosGuard_.request(ipMaybe.value()))
        {
            return lambda_(httpResponse(http::status::service_unavailable, "text/plain", "Server is overloaded"));
        }

        log.info() << tag() << "Received request from ip = " << *ipMaybe << " - posting to WorkQueue";
        if (req_.method() == http::verb::get && req_.body() == "")
        {
            return lambda_(httpResponse(http::status::ok, "text/html", defaultResponse));
        }
        if (req_.method() != http::verb::post)
        {
            return lambda_(httpResponse(http::status::bad_request, "text/html", "Expected a POST request"));
        }

        perfLog.debug() << tag() << "http received request from work queue: " << req_.body();
        try
        {
            auto request = boost::json::parse(req_.body()).as_object();
            callback_(
                std::move(request),
                [&, self = derived().shared_from_this()](auto result, auto errCode) {
                    if (!self->dosGuard_.add(*ipMaybe, result.size()))
                    {
                        auto jsonResponse = boost::json::parse(result).as_object();
                        jsonResponse["warning"] = "load";
                        jsonResponse["warnings"].as_array().push_back(RPC::makeWarning(RPC::warnRPC_RATE_LIMIT));
                        // reserialize when we need to include this warning
                        result = boost::json::serialize(jsonResponse);
                    }
                    self->lambda_(httpResponse(errCode, "application/json", result));
                },
                nullptr,
                *this);
            return;
        }
        catch (std::runtime_error const& e)
        {
            return lambda_(httpResponse(
                http::status::ok,
                "application/json",
                boost::json::serialize(RPC::makeError(RPC::RippledError::rpcBAD_SYNTAX))));
        }
    }

    void
    onWrite(bool close, boost::beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if (ec)
            return httpFail(ec, "write");

        if (close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return derived().doClose();
        }

        // We're done with the response so delete it
        res_ = nullptr;
        // Read another request
        doRead();
    }
};

}  // namespace ServerNG