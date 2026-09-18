#include "mock_server.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cstring>
#include <format>
#include <stdexcept>

namespace typesafe::testing
{

ResponseTemplate &ResponseTemplate::set_body_json(const Json &body)
{
    body_ = body.dump();
    insert_header("content-type", "application/json");
    return *this;
}

ResponseTemplate &ResponseTemplate::set_body_text(std::string body)
{
    body_ = std::move(body);
    return *this;
}

ResponseTemplate &ResponseTemplate::insert_header(std::string name, std::string value)
{
    headers_.emplace_back(std::move(name), std::move(value));
    return *this;
}

MockServer::StubBuilder::StubBuilder(MockServer &server, std::string method, std::string path) : server_(&server)
{
    stub_.method = std::move(method);
    stub_.path = std::move(path);
}

MockServer::StubBuilder &MockServer::StubBuilder::and_header(std::string name, std::string value)
{
    stub_.header_matchers.push_back({std::move(name), std::move(value)});
    return *this;
}

MockServer::StubBuilder &MockServer::StubBuilder::respond_with(ResponseTemplate response)
{
    stub_.response = std::move(response);
    return *this;
}

MockServer::StubBuilder &MockServer::StubBuilder::up_to_n_times(int times)
{
    stub_.remaining = times;
    return *this;
}

void MockServer::StubBuilder::mount()
{
    server_->add_stub(std::move(stub_));
}

MockServer::StubBuilder MockServer::given(std::string method, std::string path)
{
    return StubBuilder(*this, std::move(method), std::move(path));
}

void MockServer::add_stub(Stub stub)
{
    std::lock_guard<std::mutex> lock(mutex_);
    stubs_.push_back(std::move(stub));
}

std::unique_ptr<MockServer> MockServer::start()
{
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        throw std::runtime_error("mock server: socket() failed");
    }
    int reuse = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
    addr.sin_port = 0; // ephemeral
    if (::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        ::close(fd);
        throw std::runtime_error("mock server: bind() failed");
    }
    if (::listen(fd, 16) < 0)
    {
        ::close(fd);
        throw std::runtime_error("mock server: listen() failed");
    }
    socklen_t len = sizeof(addr);
    if (::getsockname(fd, reinterpret_cast<sockaddr *>(&addr), &len) < 0)
    {
        ::close(fd);
        throw std::runtime_error("mock server: getsockname() failed");
    }
    int port = ::ntohs(addr.sin_port);

    auto server = std::unique_ptr<MockServer>(new MockServer(fd, port));
    server->thread_ = std::thread(&MockServer::serve, server.get());
    return server;
}

MockServer::MockServer(int listen_fd, int port) : listen_fd_(listen_fd), port_(port)
{
}

MockServer::~MockServer()
{
    {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    if (thread_.joinable())
    {
        thread_.join();
    }
    ::close(listen_fd_);
}

std::string MockServer::uri() const
{
    return std::format("http://127.0.0.1:{}", port_);
}

std::vector<RecordedRequest> MockServer::received_requests() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return requests_;
}

namespace
{

bool is_running(std::mutex &mutex, const bool &flag)
{
    std::lock_guard<std::mutex> lock(mutex);
    return flag;
}

std::string trim(std::string_view raw)
{
    std::size_t begin = 0;
    std::size_t end = raw.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(raw[begin])))
    {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(raw[end - 1])))
    {
        --end;
    }
    return std::string(raw.substr(begin, end - begin));
}

std::optional<RecordedRequest> read_request(int fd)
{
    std::string buffer;
    char tmp[4096];
    while (buffer.find("\r\n\r\n") == std::string::npos)
    {
        ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0)
        {
            return std::nullopt;
        }
        buffer.append(tmp, static_cast<std::size_t>(n));
    }
    std::size_t header_end = buffer.find("\r\n\r\n");
    std::string head = buffer.substr(0, header_end);
    std::string rest = buffer.substr(header_end + 4);

    RecordedRequest request;
    std::size_t line_start = 0;
    bool first_line = true;
    std::size_t content_length = 0;
    while (line_start <= head.size())
    {
        std::size_t line_end = head.find("\r\n", line_start);
        std::string line =
            head.substr(line_start, line_end == std::string::npos ? std::string::npos : line_end - line_start);
        if (first_line)
        {
            first_line = false;
            // METHOD SP TARGET SP HTTP/1.1
            auto sp1 = line.find(' ');
            auto sp2 = line.find(' ', sp1 == std::string::npos ? sp1 : sp1 + 1);
            if (sp1 != std::string::npos && sp2 != std::string::npos)
            {
                request.method = line.substr(0, sp1);
                std::string target = line.substr(sp1 + 1, sp2 - sp1 - 1);
                auto q = target.find('?');
                if (q == std::string::npos)
                {
                    request.path = target;
                }
                else
                {
                    request.path = target.substr(0, q);
                    request.query = target.substr(q + 1);
                }
            }
        }
        else if (!line.empty())
        {
            auto colon = line.find(':');
            if (colon != std::string::npos)
            {
                std::string name = trim(line.substr(0, colon));
                std::string value = trim(line.substr(colon + 1));
                if (Headers::iequals(name, "content-length"))
                {
                    content_length = static_cast<std::size_t>(std::stoul(value));
                }
                request.headers.append(std::move(name), std::move(value));
            }
        }
        if (line_end == std::string::npos)
        {
            break;
        }
        line_start = line_end + 2;
    }

    while (rest.size() < content_length)
    {
        ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
        if (n <= 0)
        {
            break;
        }
        rest.append(tmp, static_cast<std::size_t>(n));
    }
    request.body = rest.substr(0, content_length);
    return request;
}

const char *reason_phrase(int status)
{
    switch (status)
    {
    case 200:
        return "OK";
    case 400:
        return "Bad Request";
    case 401:
        return "Unauthorized";
    case 403:
        return "Forbidden";
    case 404:
        return "Not Found";
    case 422:
        return "Unprocessable Entity";
    case 429:
        return "Too Many Requests";
    case 500:
        return "Internal Server Error";
    case 503:
        return "Service Unavailable";
    default:
        return "Status";
    }
}

void write_response(int fd, const ResponseTemplate &response)
{
    std::string out = std::format("HTTP/1.1 {} {}\r\n", response.status(), reason_phrase(response.status()));
    for (const auto &[name, value] : response.headers())
    {
        out += std::format("{}: {}\r\n", name, value);
    }
    out += std::format("Content-Length: {}\r\n", response.body().size());
    out += "Connection: close\r\n\r\n";
    out += response.body();

    std::size_t sent = 0;
    while (sent < out.size())
    {
        ssize_t n = ::send(fd, out.data() + sent, out.size() - sent, 0);
        if (n <= 0)
        {
            break;
        }
        sent += static_cast<std::size_t>(n);
    }
}

} // namespace

std::optional<ResponseTemplate> MockServer::match(const RecordedRequest &request)
{
    std::lock_guard<std::mutex> lock(mutex_);
    requests_.push_back(request);
    for (auto &stub : stubs_)
    {
        if (stub.remaining == 0)
        {
            continue;
        }
        if (stub.method != request.method || stub.path != request.path)
        {
            continue;
        }
        bool headers_ok = true;
        for (const auto &matcher : stub.header_matchers)
        {
            auto value = request.headers.get(matcher.name);
            if (!value || *value != matcher.value)
            {
                headers_ok = false;
                break;
            }
        }
        if (!headers_ok)
        {
            continue;
        }
        if (stub.remaining > 0)
        {
            --stub.remaining;
        }
        return stub.response;
    }
    return std::nullopt;
}

void MockServer::serve()
{
    while (is_running(mutex_, running_))
    {
        pollfd pfd{listen_fd_, POLLIN, 0};
        int ready = ::poll(&pfd, 1, 100);
        if (ready <= 0)
        {
            continue;
        }
        int client = ::accept(listen_fd_, nullptr, nullptr);
        if (client < 0)
        {
            continue;
        }
        auto request = read_request(client);
        if (request)
        {
            auto response = match(*request);
            if (response)
            {
                write_response(client, *response);
            }
            else
            {
                write_response(client, ResponseTemplate(404).set_body_text("no stub"));
            }
        }
        ::close(client);
    }
}

} // namespace typesafe::testing
