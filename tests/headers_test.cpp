#include "headers.h"
#include <gtest/gtest.h>

#include <map>
#include <string>
#include <vector>

TEST(iterHeaders, Empty) {
    std::vector<std::pair<std::string, std::string>> collected;
    iterHeaders("", [&](std::string_view n, std::string_view v) { collected.emplace_back(n, v); });
    EXPECT_TRUE(collected.empty());
}

TEST(iterHeaders, SkipRequestLine) {
    std::vector<std::pair<std::string, std::string>> collected;
    iterHeaders("GET / HTTP/1.1\r\nHost: example.com",
                [&](std::string_view n, std::string_view v) { collected.emplace_back(n, v); });
    EXPECT_TRUE(collected.empty());

    collected.clear();
    iterHeaders("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n",
                [&](std::string_view n, std::string_view v) { collected.emplace_back(n, v); });
    ASSERT_EQ(collected.size(), 1u);
    EXPECT_EQ(collected[0].first, "Host");
    EXPECT_EQ(collected[0].second, "example.com");
}

TEST(iterHeaders, SingleHeader) {
    std::vector<std::pair<std::string, std::string>> collected;
    iterHeaders("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n",
                [&](std::string_view n, std::string_view v) { collected.emplace_back(n, v); });
    ASSERT_EQ(collected.size(), 1u);
    EXPECT_EQ(collected[0].first, "Host");
    EXPECT_EQ(collected[0].second, "example.com");
}

TEST(iterHeaders, MultipleHeaders) {
    std::string req = "GET / HTTP/1.1\r\n"
                      "Host: example.com\r\n"
                      "User-Agent: test\r\n"
                      "Accept: */*\r\n"
                      "\r\n";

    std::map<std::string, std::string> headers;
    iterHeaders(req, [&](std::string_view n, std::string_view v) { headers[std::string(n)] = std::string(v); });

    ASSERT_EQ(headers.size(), 3u);
    EXPECT_EQ(headers["Host"], "example.com");
    EXPECT_EQ(headers["User-Agent"], "test");
    EXPECT_EQ(headers["Accept"], "*/*");
}

TEST(iterHeaders, MultipleSameHeaders) {
    std::string req = "GET / HTTP/1.1\r\n"
                      "X-Test: one\r\n"
                      "X-Test: two\r\n"
                      "\r\n";

    std::vector<std::string> values;
    iterHeaders(req, [&](std::string_view n, std::string_view v) {
        if (n == "X-Test")
            values.emplace_back(v);
    });

    ASSERT_EQ(values.size(), 2u);
    EXPECT_EQ(values[0], "one");
    EXPECT_EQ(values[1], "two");
}

TEST(findHostPort, Simple) {
    std::string req = "GET / HTTP/1.1\r\n"
                      "Host: example.com:8080\r\n"
                      "\r\n";

    auto [host, port] = findHostPort(req);
    EXPECT_EQ(host, "example.com");
    EXPECT_EQ(port, "8080");
}

TEST(findHostPort, NoHost) {
    std::string req = "GET / HTTP/1.1\r\n"
                      "User-Agent: test\r\n"
                      "\r\n";

    auto [host, port] = findHostPort(req);
    EXPECT_TRUE(host.empty());
    EXPECT_EQ(port, "80");
}

TEST(findContentLength, Simple) {
    std::string rsp = "HTTP/1.1 200 OK\r\n"
                      "Content-Length: 1234\r\n"
                      "\r\n";

    auto len = findContentLength(rsp);
    ASSERT_TRUE(len.has_value());
    EXPECT_EQ(len.value(), 1234u);
}

TEST(findContentLength, NoContentLength) {
    std::string rsp = "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/plain\r\n"
                      "\r\n";

    auto len = findContentLength(rsp);
    EXPECT_FALSE(len.has_value());
}
