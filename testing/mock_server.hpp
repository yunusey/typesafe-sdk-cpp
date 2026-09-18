#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "typesafe/common.hpp"

namespace typesafe::testing
{

class ResponseTemplate
{
  public:
    explicit ResponseTemplate(int status) : status_(status)
    {
    }

    ResponseTemplate &set_body_json(const Json &body);
    ResponseTemplate &set_body_text(std::string body);
    ResponseTemplate &insert_header(std::string name, std::string value);

    [[nodiscard]] int status() const
    {
        return status_;
    }
    [[nodiscard]] const std::string &body() const
    {
        return body_;
    }
    [[nodiscard]] const std::vector<std::pair<std::string, std::string>> &headers() const
    {
        return headers_;
    }

  private:
    int status_;
    std::string body_;
    std::vector<std::pair<std::string, std::string>> headers_;
};

struct RecordedRequest
{
    std::string method;
    std::string path;
    std::string query;
    std::string body;
    Headers headers;

    [[nodiscard]] std::optional<std::string> header(std::string_view name) const
    {
        return headers.get(name);
    }
};

class MockServer
{
  public:
    static std::unique_ptr<MockServer> start();
    ~MockServer();

    MockServer(const MockServer &) = delete;
    MockServer &operator=(const MockServer &) = delete;

    [[nodiscard]] std::string uri() const;
    [[nodiscard]] std::vector<RecordedRequest> received_requests() const;

    struct HeaderMatcher
    {
        std::string name;
        std::string value;
    };

    struct Stub
    {
        std::string method;
        std::string path;
        std::vector<HeaderMatcher> header_matchers;
        ResponseTemplate response{200};
        int remaining = -1; // -1 = unlimited
    };

    class StubBuilder
    {
      public:
        StubBuilder &and_header(std::string name, std::string value);
        StubBuilder &respond_with(ResponseTemplate response);
        StubBuilder &up_to_n_times(int times);
        void mount();

      private:
        friend class MockServer;
        StubBuilder(MockServer &server, std::string method, std::string path);
        MockServer *server_;
        Stub stub_;
    };

    StubBuilder given(std::string method, std::string path);

  private:
    MockServer(int listen_fd, int port);
    void serve();
    void add_stub(Stub stub);
    std::optional<ResponseTemplate> match(const RecordedRequest &request);

    int listen_fd_;
    int port_;
    std::thread thread_;
    bool running_ = true;

    mutable std::mutex mutex_;
    std::vector<Stub> stubs_;
    std::vector<RecordedRequest> requests_;
};

} // namespace typesafe::testing
