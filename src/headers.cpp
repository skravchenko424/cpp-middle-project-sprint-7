#include "headers.h"

#include <algorithm>
#include <charconv>
#include <ranges>
#include <string_view>

using namespace std::string_view_literals;

using Callback = std::function<void(std::string_view, std::string_view)>;

constexpr auto line_delimiter = "\r\n"sv;
constexpr auto headers_end_delimiter = "\r\n\r\n"sv;
constexpr auto header_delimiter = ":"sv;
constexpr auto port_delimiter = ":"sv;
constexpr auto default_port = "80";
constexpr size_t max_content_length = 5 * 1024 * 1024;

bool compareLowerCase(std::string_view left, std::string_view right) {
    return std::ranges::equal(
        left, right, [](unsigned char lhs, unsigned char rhs) { return std::tolower(lhs) == std::tolower(rhs); });
}

void iterHeaders(std::string_view req, Callback &&callback) {
    auto headers_end = req.find(headers_end_delimiter);
    if (headers_end == std::string_view::npos)
        return;

    auto headers = req.substr(0, headers_end);
    for (auto line : headers | std::views::split(line_delimiter)) {
        auto line_sv = std::string_view(line);
        auto sep_pos = line_sv.find(header_delimiter);
        if (sep_pos == std::string_view::npos)
            continue;

        auto name = line_sv.substr(0, sep_pos);
        auto value = line_sv.substr(sep_pos + 1);
        while (!value.empty() && value[0] == ' ')
            value.remove_prefix(1);

        callback(name, value);
    }
}

std::pair<std::string, std::string> findHostPort(std::string_view req) {
    std::string host;
    std::string port = default_port;

    iterHeaders(req, [&](std::string_view name, std::string_view value) {
        if (compareLowerCase("host", name)) {
            auto delimiter_position = value.find(port_delimiter);
            if (delimiter_position != std::string_view::npos) {
                host = value.substr(0, delimiter_position);
                port = value.substr(delimiter_position + 1);
            } else {
                host = value;
                port = default_port;
            }
        }
    });
    return {host, port};
}

std::optional<size_t> findContentLength(std::string_view rsp) {
    std::optional<size_t> maybe_content_length;
    iterHeaders(rsp, [&](std::string_view name, std::string_view value) {
        if (compareLowerCase(name, "content-length")) {
            size_t length = 0;
            auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), length);
            if (ec == std::errc{} && length <= max_content_length) {
                maybe_content_length = length;
            }
        }
    });
    return maybe_content_length;
}
