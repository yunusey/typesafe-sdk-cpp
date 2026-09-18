#pragma once

#include <optional>
#include <string>
#include <utility>

#include "typesafe/answer.hpp"
#include "typesafe/common.hpp"
#include "typesafe/config.hpp"
#include "typesafe/error.hpp"
#include "typesafe/question.hpp"
#include "typesafe/retry.hpp"

namespace typesafe
{

struct NoApiKey
{
};
struct ApiKeySet
{
};

struct SystemOneOpts
{
    std::optional<std::string> model;
    std::optional<RetryPolicy> retry;
    std::optional<Duration> timeout;
    Headers extra_headers;
    std::optional<Json> extra_body; // shallow last-write-wins over the body
};

struct ModelsOpts
{
    std::optional<RetryPolicy> retry;
    std::optional<Duration> timeout;
    Headers extra_headers;
};

namespace detail
{

struct BuilderFields
{
    std::optional<std::string> api_key;
    std::optional<std::string> model;
    std::optional<RetryPolicy> retry;
    std::optional<Duration> timeout;
    Headers headers;
    std::optional<std::string> base_url;
};

} // namespace detail

template <class State> class ClientBuilder;

class Client
{
  public:
    static ClientBuilder<NoApiKey> builder();
    static Result<Client> from_env();
    static Result<Client> create(std::string api_key);

    [[nodiscard]] Result<SystemOneResponse> system_one(Json state, detail::QuestionList questions) const;
    [[nodiscard]] Result<SystemOneResponse> system_one_opts(Json state, detail::QuestionList questions,
                                                            SystemOneOpts opts) const;
    [[nodiscard]] Result<ListModelsResponse> models() const;
    [[nodiscard]] Result<ListModelsResponse> models_opts(ModelsOpts opts) const;

  private:
    template <class State> friend class ClientBuilder;

    Client(detail::Config config, RetryPolicy retry) : config_(std::move(config)), retry_(std::move(retry))
    {
    }

    static Result<Client> from_builder(detail::BuilderFields fields);

    struct RawResponse
    {
        int status;
        Headers headers;
        std::string endpoint;
        std::string body;
    };

    [[nodiscard]] Result<RawResponse> send(std::string_view method, std::string_view path, const Json *body,
                                           std::optional<Duration> timeout, const Headers &extra_headers,
                                           const RetryPolicy &retry) const;

    detail::Config config_;
    RetryPolicy retry_;
};

// build() exists only on ClientBuilder<ApiKeySet>.
template <class State> class ClientBuilder
{
  public:
    [[nodiscard]] ClientBuilder<ApiKeySet> api_key(std::string api_key) const
    {
        detail::BuilderFields next = fields_;
        next.api_key = std::move(api_key);
        return ClientBuilder<ApiKeySet>(std::move(next));
    }

    [[nodiscard]] ClientBuilder model(std::string model) const
    {
        ClientBuilder next = *this;
        next.fields_.model = std::move(model);
        return next;
    }

    [[nodiscard]] ClientBuilder retry(RetryPolicy retry) const
    {
        ClientBuilder next = *this;
        next.fields_.retry = std::move(retry);
        return next;
    }

    [[nodiscard]] ClientBuilder timeout(Duration timeout) const
    {
        ClientBuilder next = *this;
        next.fields_.timeout = timeout;
        return next;
    }

    [[nodiscard]] ClientBuilder base_url(std::string base_url) const
    {
        ClientBuilder next = *this;
        next.fields_.base_url = std::move(base_url);
        return next;
    }

    [[nodiscard]] ClientBuilder header(std::string name, std::string value) const
    {
        ClientBuilder next = *this;
        next.fields_.headers.set(std::move(name), std::move(value));
        return next;
    }

    [[nodiscard]] ClientBuilder headers(Headers headers) const
    {
        ClientBuilder next = *this;
        next.fields_.headers = std::move(headers);
        return next;
    }

    [[nodiscard]] Result<Client> build() const
        requires std::same_as<State, ApiKeySet>
    {
        return Client::from_builder(fields_);
    }

  private:
    friend class Client;
    template <class U> friend class ClientBuilder;

    ClientBuilder() = default;
    explicit ClientBuilder(detail::BuilderFields fields) : fields_(std::move(fields))
    {
    }

    detail::BuilderFields fields_;
};

} // namespace typesafe
