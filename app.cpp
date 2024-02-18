//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: Advanced server
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/strand.hpp>
#include <boost/make_unique.hpp>
#include <boost/optional.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <unordered_map>
#include <chrono>

#include "sqlite3.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace beast = boost::beast;                 // from <boost/beast.hpp>
namespace http = beast::http;                   // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;         // from <boost/beast/websocket.hpp>
namespace net = boost::asio;                    // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>

std::unordered_map<std::string, std::string> userTokens;

// Return a reasonable mime type based on the extension of a file.
beast::string_view
mime_type(beast::string_view path)
{
    using beast::iequals;
    auto const ext = [&path]
    {
        auto const pos = path.rfind(".");
        if(pos == beast::string_view::npos)
            return beast::string_view{};
        return path.substr(pos);
    }();
    if(iequals(ext, ".htm"))  return "text/html";
    if(iequals(ext, ".html")) return "text/html";
    if(iequals(ext, ".php"))  return "text/html";
    if(iequals(ext, ".css"))  return "text/css";
    if(iequals(ext, ".txt"))  return "text/plain";
    if(iequals(ext, ".js"))   return "application/javascript";
    if(iequals(ext, ".json")) return "application/json";
    if(iequals(ext, ".xml"))  return "application/xml";
    if(iequals(ext, ".swf"))  return "application/x-shockwave-flash";
    if(iequals(ext, ".flv"))  return "video/x-flv";
    if(iequals(ext, ".png"))  return "image/png";
    if(iequals(ext, ".jpe"))  return "image/jpeg";
    if(iequals(ext, ".jpeg")) return "image/jpeg";
    if(iequals(ext, ".jpg"))  return "image/jpeg";
    if(iequals(ext, ".gif"))  return "image/gif";
    if(iequals(ext, ".bmp"))  return "image/bmp";
    if(iequals(ext, ".ico"))  return "image/vnd.microsoft.icon";
    if(iequals(ext, ".tiff")) return "image/tiff";
    if(iequals(ext, ".tif"))  return "image/tiff";
    if(iequals(ext, ".svg"))  return "image/svg+xml";
    if(iequals(ext, ".svgz")) return "image/svg+xml";
    return "application/text";
}

// Append an HTTP rel-path to a local filesystem path.
// The returned path is normalized for the platform.
std::string
path_cat(
    beast::string_view base,
    beast::string_view path)
{
    if(base.empty())
        return std::string(path);
    std::string result(base);
#ifdef BOOST_MSVC
    char constexpr path_separator = '\\';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
    for(auto& c : result)
        if(c == '/')
            c = path_separator;
#else
    char constexpr path_separator = '/';
    if(result.back() == path_separator)
        result.resize(result.size() - 1);
    result.append(path.data(), path.size());
#endif
    return result;
}

// This function produces an HTTP response for the given
// request. The type of the response object depends on the
// contents of the request, so the interface requires the
// caller to pass a generic lambda for receiving the response.
template<
    class Body, class Allocator,
    class Send>
void
handle_request(
    beast::string_view doc_root,
    http::request<Body, http::basic_fields<Allocator>>&& req,
    Send&& send)
{

    // Returns a bad request response
    auto const bad_request =
    [&req](beast::string_view why)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(why);
        res.prepare_payload();
        return res;
    };

    // Returns a not found response
    auto const not_found =
    [&req](beast::string_view target)
    {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "The resource '" + std::string(target) + "' was not found.";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const server_error =
    [&req](beast::string_view what)
    {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "text/html");
        res.keep_alive(req.keep_alive());
        res.body() = "An error occurred: '" + std::string(what) + "'";
        res.prepare_payload();
        return res;
    };

    // Returns a server error response
    auto const json_ok =
    [&req](beast::string_view what)
    {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(what);
        res.prepare_payload();
        return res;
    };

    auto const json_bad_request =
    [&req](beast::string_view what)
    {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(what);
        res.prepare_payload();
        return res;
    };

    auto const json_internal_error =
    [&req](beast::string_view what)
    {
        http::response<http::string_body> res{http::status::internal_server_error, req.version()};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, "application/json");
        res.keep_alive(req.keep_alive());
        res.body() = std::string(what);
        res.prepare_payload();
        return res;
    };

    // Make sure we can handle the method
    if( req.method() != http::verb::get &&
        req.method() != http::verb::head && 
        req.method() != http::verb::post)
        return send(bad_request("Unknown HTTP-method"));

    // Request path must be absolute and not contain "..".
    if( req.target().empty() ||
        req.target()[0] != '/' ||
        req.target().find("..") != beast::string_view::npos)
        return send(bad_request("Illegal request-target"));

    // Build the path to the requested file
    // std::cout << "target is '" << req.target() << "'" << std::endl;
    
    std::vector<std::string> route;
    std::stringstream ss(std::string(req.target()));
    std::string w;
    int i = 0;
    while (!ss.eof()) {
        std::getline(ss, w, '/');
        std::cout << w << std::endl;
        if (i > 0 || w != "")
            route.push_back(w);
        i++;
    }

    // std::cout << "route size()= " << route.size() << std::endl;
    if(route.size() == 2 && route[0] == "getfeed") {
        auto uname = route[1];

        // std::cout << "getfeed req.body()= " << req.body() << std::endl;
        json jsn = json::parse(req.body());
        // std::cout << "getfeed jsn.dump()= " << jsn.dump() << std::endl;
        if (jsn.contains("user_token")) {
            auto tok = jsn["user_token"];
            if(userTokens.count(uname) > 0) {
                if(userTokens[uname] == tok) {
                    sqlite3 *pDb = nullptr;
                    sqlite3_open_v2("/tmp/simsoms.db", 
                                &pDb,
                                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 
                                nullptr);
                    sqlite3_stmt* stmt;
                    const int maxPosts = 10;
                    std::string qy = std::string("SELECT author, content, post_time_utc FROM posts WHERE author IN ") +
                        std::string(" (SELECT sub_to FROM subs WHERE username='") + uname + std::string("'") +
                        std::string("  UNION SELECT '") + uname + std::string("')") +
                        std::string(" ORDER BY post_time_utc DESC LIMIT ") + std::to_string(maxPosts);

                    sqlite3_prepare(pDb, qy.c_str(), qy.size(), &stmt, nullptr);
                    bool done = false, err = false, firstResult = true;
                    int row = 0;
                    std::string result = "{ \"posts\": [";
                    while (!done) {
                        switch (sqlite3_step (stmt)) {
                        case SQLITE_ROW: {
                            auto bytes = sqlite3_column_bytes(stmt, 0);
                            auto text  = sqlite3_column_text(stmt, 1);
                            auto num_cols = sqlite3_column_count(stmt);
                            // std::cout << "count " << row << " num_cols " << num_cols << " text " << text << " bytes" << bytes << std::endl;
                            //result += std::string(text);
                            
                            //std::cout << "\t";
                            if(firstResult) 
                                firstResult = false;
                            else
                                result += ", ";
                            result += "[";
                            for (int k = 0; k < num_cols; k++)
                                {
                                    //std::cout << " " << sqlite3_column_name(stmt, k);
                                    switch (sqlite3_column_type(stmt, k))
                                    {
                                    case (SQLITE3_TEXT): {
                                        auto r = std::string((const char*)sqlite3_column_text(stmt, k));
                                        result = result +  "\"" + r + "\"";
                                        //std::cout << " " << r;
                                        break;
                                    }
                                    case (SQLITE_INTEGER): {
                                        auto r = std::to_string(sqlite3_column_int(stmt, k));
                                        result = result + "\"" + r + "\"";
                                        //std::cout << " " << r;
                                        break;
                                    }
                                    case (SQLITE_FLOAT): {
                                        auto r = std::to_string(sqlite3_column_double(stmt, k));
                                        result = result + r;
                                        //std::cout << " " << r;
                                        break;
                                    }
                                    default:
                                        std::cout << " (unkonwn datatype)";
                                        break;
                                    }
                                    if (k < num_cols-1)
                                        result = result + ", ";
                                }
                            row++;
                            result += "]";
                            break;
                        }
                        case SQLITE_DONE:
                            done = true;
                            break;
                        default:
                            done = true;
                            err = true;
                            std::cerr << "Uxpeected return from sqlite3_step() at " << __LINE__ << std::endl;
                            break;
                        }
                    }
                    sqlite3_finalize(stmt);
                    sqlite3_close(pDb);
                    result += "]}";
                    // std::cout << result << std::endl;

                    return send(json_ok(result));
                }
                else
                    return send(json_bad_request("{error: 'user token mismatch}'"));    
            } else 
                return send(json_bad_request("{error: 'not logged in'}"));
        } 
        else
            return send(json_bad_request("{error:'no \'user_token\' present in request'}"));    
    }
    else if(route.size() == 2 && route[0] == "login") {
        // std::cout << "login";

        auto uname = route[1];
        json j = json::parse(req.body());
        if (j.contains("passwd")) {
            // std::cout << "passwd: '" << j["passwd"] << "'" << std::endl;
            const int nChars = 40;
            const char begin = 'a';
            const char end = 'z';
            const int rng = end-begin+1;

            std::string tok(nChars, 0);

            for (int i=0; i<nChars; i++) {
                tok[i] = begin + rand() % rng;
            }
            // std::cout << "tok= " << tok << std::endl;
            std::string result = std::string("{\"usertoken\":\"") + tok + std::string("\"}");

            userTokens[uname] = tok;
            return send(json_ok(result));
        } 
        else {
            return send(json_bad_request("{error:'no \'passwd\' present in request'}"));    
        }

        // return send(not_found(req.target()));
    }
    else if(route.size() == 2 && route[0] == "postmsg") {
        auto uname = route[1];

        // std::cout << "getfeed req.body()= " << req.body() << std::endl;
        json jsn = json::parse(req.body());
        // std::cout << "getfeed jsn.dump()= " << jsn.dump() << std::endl;
        if (jsn.contains("user_token")) {
            auto tok = jsn["user_token"];
            if(userTokens.count(uname) > 0) {
                if(userTokens[uname] == tok) {
                    auto msg = std::string(jsn["msg"]);
                    std::cout << "postmsg msg=" << msg << std::endl;

                    std::chrono::milliseconds time_ms = std::chrono::duration_cast< std::chrono::milliseconds >(
                        std::chrono::system_clock::now().time_since_epoch());
                    sqlite3 *pDb = nullptr;
                    sqlite3_open_v2("/tmp/simsoms.db", 
                                &pDb,
                                SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 
                                nullptr);
                    sqlite3_stmt* stmt;
                    const int maxPosts = 10;
                    std::string qy = std::string("INSERT INTO posts (author, content, post_time_utc) VALUES ($a, $c, $t)");

                    sqlite3_prepare(pDb, qy.c_str(), qy.size(), &stmt, nullptr);

                    sqlite3_bind_text(stmt, 1, uname.c_str(), uname.length(), SQLITE_STATIC);
                    sqlite3_bind_text(stmt, 2, msg.c_str(), msg.length(), SQLITE_STATIC);
                    sqlite3_bind_double(stmt, 3, time_ms.count() /(double)1000);
                    
                    auto err = sqlite3_step(stmt);
                    sqlite3_finalize(stmt);
                    sqlite3_close(pDb);

                    if(err != SQLITE_DONE) {
                        std::cout << "sqlite3_step() did not return SQLITE_DONE" << std::endl;
                        return send(json_internal_error("\"result\":\"sqlite3_step() returned " + 
                            std::to_string(err) + " expected " + std::to_string(SQLITE_DONE) +
                            + " (SQLITE_DONE)\"" + "}"));
                    }
                    // std::cout << result << std::endl;
                }
                else
                    return send(json_bad_request("{error: 'user token mismatch}'"));    
            } else 
                return send(json_bad_request("{error: 'not logged in'}"));
        } 
        else
            return send(json_bad_request("{error:'no \'user_token\' present in request'}"));    
        return send(json_ok("{\"result\":\"ok\"}"));
    }

    else {
        std::string path = path_cat(doc_root, req.target());
        if(req.target().back() == '/')
            path.append("index.html");
    
        // Attempt to open the file
        beast::error_code ec;
        http::file_body::value_type body;
        body.open(path.c_str(), beast::file_mode::scan, ec);

        // Handle the case where the file doesn't exist
        if(ec == beast::errc::no_such_file_or_directory)
            return send(not_found(req.target()));

        // Handle an unknown error
        if(ec)
            return send(server_error(ec.message()));

        // Cache the size since we need it after the move
        auto const size = body.size();

        // Respond to HEAD request
        if(req.method() == http::verb::head)
        {
            http::response<http::empty_body> res{http::status::ok, req.version()};
            res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(http::field::content_type, mime_type(path));
            res.content_length(size);
            res.keep_alive(req.keep_alive());
            return send(std::move(res));
        }

        // Respond to GET request
        http::response<http::file_body> res{
            std::piecewise_construct,
            std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, req.version())};
        res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
        res.set(http::field::content_type, mime_type(path));
        res.content_length(size);
        res.keep_alive(req.keep_alive());
        return send(std::move(res));
    }
}

//------------------------------------------------------------------------------

// Report a failure
void
fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Echoes back all received WebSocket messages
class websocket_session : public std::enable_shared_from_this<websocket_session>
{
    websocket::stream<beast::tcp_stream> ws_;
    beast::flat_buffer buffer_;

public:
    // Take ownership of the socket
    explicit
    websocket_session(tcp::socket&& socket)
        : ws_(std::move(socket))
    {
    }

    // Start the asynchronous accept operation
    template<class Body, class Allocator>
    void
    do_accept(http::request<Body, http::basic_fields<Allocator>> req)
    {
        // Set suggested timeout settings for the websocket
        ws_.set_option(
            websocket::stream_base::timeout::suggested(
                beast::role_type::server));

        // Set a decorator to change the Server of the handshake
        ws_.set_option(websocket::stream_base::decorator(
            [](websocket::response_type& res)
            {
                res.set(http::field::server,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                        " advanced-server");
            }));

        // Accept the websocket handshake
        ws_.async_accept(
            req,
            beast::bind_front_handler(
                &websocket_session::on_accept,
                shared_from_this()));
    }

private:
    void
    on_accept(beast::error_code ec)
    {
        if(ec)
            return fail(ec, "accept");

        // Read a message
        do_read();
    }

    void
    do_read()
    {
        // Read a message into our buffer
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &websocket_session::on_read,
                shared_from_this()));
    }

    void
    on_read(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the websocket_session was closed
        if(ec == websocket::error::closed)
            return;

        if(ec)
            fail(ec, "read");

        // Echo the message
        ws_.text(ws_.got_text());
        ws_.async_write(
            buffer_.data(),
            beast::bind_front_handler(
                &websocket_session::on_write,
                shared_from_this()));
    }

    void
    on_write(
        beast::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");

        // Clear the buffer
        buffer_.consume(buffer_.size());

        // Do another read
        do_read();
    }
};

//------------------------------------------------------------------------------

// Handles an HTTP server connection
class http_session : public std::enable_shared_from_this<http_session>
{
    // This queue is used for HTTP pipelining.
    class queue
    {
        enum
        {
            // Maximum number of responses we will queue
            limit = 8
        };

        // The type-erased, saved work item
        struct work
        {
            virtual ~work() = default;
            virtual void operator()() = 0;
        };

        http_session& self_;
        std::vector<std::unique_ptr<work>> items_;

    public:
        explicit
        queue(http_session& self)
            : self_(self)
        {
            static_assert(limit > 0, "queue limit must be positive");
            items_.reserve(limit);
        }

        // Returns `true` if we have reached the queue limit
        bool
        is_full() const
        {
            return items_.size() >= limit;
        }

        // Called when a message finishes sending
        // Returns `true` if the caller should initiate a read
        bool
        on_write()
        {
            BOOST_ASSERT(! items_.empty());
            auto const was_full = is_full();
            items_.erase(items_.begin());
            if(! items_.empty())
                (*items_.front())();
            return was_full;
        }

        // Called by the HTTP handler to send a response.
        template<bool isRequest, class Body, class Fields>
        void
        operator()(http::message<isRequest, Body, Fields>&& msg)
        {
            // This holds a work item
            struct work_impl : work
            {
                http_session& self_;
                http::message<isRequest, Body, Fields> msg_;

                work_impl(
                    http_session& self,
                    http::message<isRequest, Body, Fields>&& msg)
                    : self_(self)
                    , msg_(std::move(msg))
                {
                }

                void
                operator()()
                {
                    http::async_write(
                        self_.stream_,
                        msg_,
                        beast::bind_front_handler(
                            &http_session::on_write,
                            self_.shared_from_this(),
                            msg_.need_eof()));
                }
            };

            // Allocate and store the work
            items_.push_back(
                boost::make_unique<work_impl>(self_, std::move(msg)));

            // If there was no previous work, start this one
            if(items_.size() == 1)
                (*items_.front())();
        }
    };

    beast::tcp_stream stream_;
    beast::flat_buffer buffer_;
    std::shared_ptr<std::string const> doc_root_;
    queue queue_;

    // The parser is stored in an optional container so we can
    // construct it from scratch it at the beginning of each new message.
    boost::optional<http::request_parser<http::string_body>> parser_;

public:
    // Take ownership of the socket
    http_session(
        tcp::socket&& socket,
        std::shared_ptr<std::string const> const& doc_root)
        : stream_(std::move(socket))
        , doc_root_(doc_root)
        , queue_(*this)
    {  }

    // Start the session
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(
            stream_.get_executor(),
            beast::bind_front_handler(
                &http_session::do_read,
                this->shared_from_this()));
    }

private:
    void
    do_read()
    {
        // Construct a new parser for each message
        parser_.emplace();

        // Apply a reasonable limit to the allowed size
        // of the body in bytes to prevent abuse.
        parser_->body_limit(10000);

        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(30));

        // Read a request using the parser-oriented interface
        http::async_read(
            stream_,
            buffer_,
            *parser_,
            beast::bind_front_handler(
                &http_session::on_read,
                shared_from_this()));
    }

    void
    on_read(beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream)
            return do_close();

        if(ec)
            return fail(ec, "read");

        // See if it is a WebSocket Upgrade
        if(websocket::is_upgrade(parser_->get()))
        {
            // Create a websocket session, transferring ownership
            // of both the socket and the HTTP request.
            std::make_shared<websocket_session>(
                stream_.release_socket())->do_accept(parser_->release());
            return;
        }

        // Send the response
        handle_request(*doc_root_, parser_->release(), queue_);

        // If we aren't at the queue limit, try to pipeline another request
        if(! queue_.is_full())
            do_read();
    }

    void
    on_write(bool close, beast::error_code ec, std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");

        if(close)
        {
            // This means we should close the connection, usually because
            // the response indicated the "Connection: close" semantic.
            return do_close();
        }

        // Inform the queue that a write completed
        if(queue_.on_write())
        {
            // Read another request
            do_read();
        }
    }

    void
    do_close()
    {
        // Send a TCP shutdown
        beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

        // At this point the connection is closed gracefully
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    std::shared_ptr<std::string const> doc_root_;

public:
    listener(
        net::io_context& ioc,
        tcp::endpoint endpoint,
        std::shared_ptr<std::string const> const& doc_root)
        : ioc_(ioc)
        , acceptor_(net::make_strand(ioc))
        , doc_root_(doc_root)
    {
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if(ec)
        {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(net::socket_base::reuse_address(true), ec);
        if(ec)
        {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if(ec)
        {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            net::socket_base::max_listen_connections, ec);
        if(ec)
        {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void
    run()
    {
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        net::dispatch(
            acceptor_.get_executor(),
            beast::bind_front_handler(
                &listener::do_accept,
                this->shared_from_this()));
    }

private:
    void
    do_accept()
    {
        // The new connection gets its own strand
        acceptor_.async_accept(
            net::make_strand(ioc_),
            beast::bind_front_handler(
                &listener::on_accept,
                shared_from_this()));
    }

    void
    on_accept(beast::error_code ec, tcp::socket socket)
    {
        if(ec)
        {
            fail(ec, "accept");
        }
        else
        {
            // Create the http session and run it
            std::make_shared<http_session>(
                std::move(socket),
                doc_root_)->run();
        }

        // Accept another connection
        do_accept();
    }
};

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    auto address = net::ip::make_address("127.0.0.1");
    auto port = static_cast<unsigned short>(3000);
    auto doc_root = std::make_shared<std::string>(".");
    int threads = 1;

    // Check command line arguments.
    if (argc == 5)
    {
        address = net::ip::make_address(argv[1]);
        port = static_cast<unsigned short>(std::atoi(argv[2]));
        doc_root = std::make_shared<std::string>(argv[3]);
        threads = std::max<int>(1, std::atoi(argv[4]));
    } else if (argc != 1) {
        std::cerr <<
            "Usage (1): app \n"
            "   Uses 127.0.0.1 port 3000 docroot '.' and threads 1 \n"
            "Usage (2): app <address> <port> <doc_root> <threads>\n" <<
            "Example:\n" <<
            "    app 0.0.0.0 8080 . 1\n";
        return EXIT_FAILURE;
    }
    

    std::cout << "Listening on " << address << ":" << port;
    std::cout << " doc root " << *doc_root << " threads " << threads << std::endl;
    // The io_context is required for all I/O
    net::io_context ioc{threads};

  
    // Create and launch a listening port
    std::make_shared<listener>(
        ioc,
        tcp::endpoint{address, port},
        doc_root)->run();

    // Capture SIGINT and SIGTERM to perform a clean shutdown
    net::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait(
        [&](beast::error_code const&, int)
        {
            // Stop the `io_context`. This will cause `run()`
            // to return immediately, eventually destroying the
            // `io_context` and all of the sockets in it.
            ioc.stop();
        });

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back(
        [&ioc]
        {
            ioc.run();
        });
    ioc.run();

    // (If we get here, it means we got a SIGINT or SIGTERM)

    // Block until all the threads exit
    for(auto& t : v)
        t.join();

    return EXIT_SUCCESS;
}