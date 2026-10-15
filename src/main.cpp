#include "headers.h"

#include <boost/asio.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <iostream>
#include <print>
#include <string_view>

using boost::asio::async_read_until;
using boost::asio::awaitable;
using boost::asio::buffer;
using boost::asio::co_spawn;
using boost::asio::dynamic_buffer;
using boost::asio::io_service;
using boost::asio::transfer_at_least;
using boost::asio::use_awaitable;
using boost::asio::ip::tcp;
using boost::system::error_code;

constexpr std::string_view delimiter = "\r\n\r\n";
constexpr std::size_t max_header_size = 64 * 1024;
constexpr size_t chunk_size = 8192;

awaitable<void> session(tcp::socket client_socket, io_service &io_service) {

    tcp::socket server_socket(io_service);

    try {
        std::string request;
        const auto bytes_transferred = co_await async_read_until(
            client_socket, dynamic_buffer(request, max_header_size), delimiter, use_awaitable);

        std::string_view request_headers(request.data(), bytes_transferred);
        std::string_view request_body(request.data() + bytes_transferred, request.size() - bytes_transferred);
        auto [host, port] = findHostPort(request_headers);
        if (host.empty()) {
            throw std::runtime_error("Request has no host set");
        }

        tcp::resolver resolver(io_service);
        auto endpoints = co_await resolver.async_resolve(host, port, use_awaitable);
        co_await boost::asio::async_connect(server_socket, endpoints, use_awaitable);

        co_await boost::asio::async_write(server_socket, buffer(request_headers), use_awaitable);

        std::string response;
        const auto bytes_transferred_response = co_await async_read_until(
            server_socket, dynamic_buffer(response, max_header_size), delimiter, use_awaitable);

        auto content_length = findContentLength(response);

        if (!request_body.empty()) {
            co_await boost::asio::async_write(server_socket, buffer(request_body), use_awaitable);
        }

        co_await boost::asio::async_write(client_socket, buffer(response), use_awaitable);

        std::size_t received = response.length() - bytes_transferred_response;

        if (content_length.has_value()) {
            std::array<char, chunk_size> chunk;
            while (received < content_length.value()) {
                std::size_t toRead = std::min<std::size_t>(chunk_size, content_length.value() - received);

                std::size_t bytes_transferred_chunk =
                    co_await boost::asio::async_read(server_socket, buffer(chunk.data(), toRead), use_awaitable);
                received += bytes_transferred_chunk;

                co_await async_write(client_socket, buffer(chunk, bytes_transferred_chunk), use_awaitable);
            }
        } else {
            boost::system::error_code ec;
            std::array<char, chunk_size> chunk;
            for (;;) {
                auto bytes_transferred_chunk = co_await server_socket.async_read_some(
                    buffer(chunk), boost::asio::redirect_error(use_awaitable, ec));
                if (bytes_transferred_chunk > 0) {
                    co_await boost::asio::async_write(client_socket, buffer(chunk.data(), bytes_transferred_chunk),
                                                      use_awaitable);
                }
                if (ec) {
                    break;
                }
            }
        }
    } catch (const boost::system::system_error &e) {
        if (e.code() == boost::asio::error::not_found) {
            std::println(std::cerr, "Request header too large (exceeded {} bytes)", max_header_size);
        } else {
            std::println(std::cerr, "Session failed: {}", e.what());
        }
    } catch (const std::exception &e) {
        std::println(std::cerr, "Session failed: {}", e.what());
    } catch (...) {
        std::println(std::cerr, "Session failed: unknown exception");
    }

    if (client_socket.is_open()) {
        client_socket.shutdown(tcp::socket::shutdown_both);
        client_socket.close();
    }
    if (server_socket.is_open()) {
        server_socket.shutdown(tcp::socket::shutdown_both);
        server_socket.close();
    }
}

class Server {
public:
    Server(io_service &io_service, short port)
        : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)), socket_(io_service) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(socket_, [this](error_code ec) {
            if (!ec) {
                co_spawn(io_service_, session(std::move(socket_), io_service_), boost::asio::detached);
            } else {
                std::println(std::cerr, "Connection failed: {}", ec.message());
            }
            do_accept();
        });
    }

    io_service &io_service_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;
};

int main(int argc, char *argv[]) {
    try {
        if (argc != 2) {
            std::println(std::cerr, "Usage: proxy_server <listen_port>");
            return 1;
        }

        io_service io_service(1);
        Server server(io_service, std::atoi(argv[1]));
        io_service.run();
    } catch (const std::exception &e) {
        std::println(std::cerr, "Exception:{}", e.what());
    }
}
