#include "typesafe/client.hpp"

#include <chrono>
#include <format>
#include <random>
#include <thread>

#include "http.hpp"
#include "logging.hpp"
#include "request.hpp"
#include "typesafe/constants.hpp"
#include "typesafe/json.hpp"

namespace typesafe
{

namespace
{

double jitter_sample()
{
    static thread_local std::mt19937_64 engine{std::random_device{}()};
    std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(engine);
}

} // namespace

ClientBuilder<NoApiKey> Client::builder()
{
    return ClientBuilder<NoApiKey>();
}

Result<Client> Client::from_env()
{
    auto api_key = detail::resolve_env(std::nullopt, kApiKeyEnv, std::nullopt);
    if (!api_key)
    {
        return std::unexpected(Error::sdk(
            std::format("No API key was provided. Pass api_key or set the {} environment variable.", kApiKeyEnv)));
    }
    return builder().api_key(*api_key).build();
}

Result<Client> Client::create(std::string api_key)
{
    return builder().api_key(std::move(api_key)).build();
}

Result<Client> Client::from_builder(detail::BuilderFields fields)
{
    detail::logging_setup();
    if (fields.retry)
    {
        if (auto ok = fields.retry->validate(); !ok)
        {
            return std::unexpected(ok.error());
        }
    }
    std::string api_key = fields.api_key.value();
    auto config =
        detail::Config::resolve(std::move(api_key), fields.base_url, fields.model, fields.timeout, fields.headers);
    if (!config)
    {
        return std::unexpected(config.error());
    }
    RetryPolicy retry = fields.retry.value_or(RetryPolicy{});
    return Client(std::move(config.value()), std::move(retry));
}

Result<SystemOneResponse> Client::system_one(Json state, detail::QuestionList questions) const
{
    return system_one_opts(std::move(state), std::move(questions), SystemOneOpts{});
}

Result<SystemOneResponse> Client::system_one_opts(Json state, detail::QuestionList questions, SystemOneOpts opts) const
{
    auto validated_state = to_state(std::move(state));
    if (!validated_state)
    {
        return std::unexpected(validated_state.error());
    }

    Json body = Json::object();
    body["state"] = std::move(validated_state.value());
    body["model"] = opts.model.value_or(config_.default_model);

    auto questions_json = detail::normalize_questions(questions);
    if (!questions_json)
    {
        return std::unexpected(questions_json.error());
    }
    body["questions"] = std::move(questions_json.value());
    body = detail::merge_extra_body(std::move(body), opts.extra_body);

    RetryPolicy local;
    const RetryPolicy *retry = &retry_;
    if (opts.retry)
    {
        if (auto ok = opts.retry->validate(); !ok)
        {
            return std::unexpected(ok.error());
        }
        local = *opts.retry;
        retry = &local;
    }

    auto raw = send("POST", detail::kSystemOnePath, &body, opts.timeout, opts.extra_headers, *retry);
    if (!raw)
    {
        return std::unexpected(raw.error());
    }
    return detail::decode_system_one(raw->status, std::move(raw->headers),
                                     std::optional<std::string>(std::move(raw->endpoint)), std::move(raw->body));
}

Result<ListModelsResponse> Client::models() const
{
    return models_opts(ModelsOpts{});
}

Result<ListModelsResponse> Client::models_opts(ModelsOpts opts) const
{
    RetryPolicy local;
    const RetryPolicy *retry = &retry_;
    if (opts.retry)
    {
        if (auto ok = opts.retry->validate(); !ok)
        {
            return std::unexpected(ok.error());
        }
        local = *opts.retry;
        retry = &local;
    }

    auto raw = send("GET", detail::kModelsPath, nullptr, opts.timeout, opts.extra_headers, *retry);
    if (!raw)
    {
        return std::unexpected(raw.error());
    }
    return detail::decode_models(raw->status, std::move(raw->headers),
                                 std::optional<std::string>(std::move(raw->endpoint)), std::move(raw->body));
}

Result<Client::RawResponse> Client::send(std::string_view method, std::string_view path, const Json *body,
                                         std::optional<Duration> timeout, const Headers &extra_headers,
                                         const RetryPolicy &retry) const
{
    auto prepared = detail::prepare(config_, method, path, body, timeout, extra_headers);
    if (!prepared)
    {
        return std::unexpected(prepared.error());
    }

    const auto started = std::chrono::steady_clock::now();
    std::uint32_t attempt = 0;
    std::string endpoint = detail::format_endpoint(prepared->method, prepared->url);

    while (true)
    {
        Headers headers = prepared->headers;
        detail::set_retry_count(headers, attempt);

        if (detail::log_level() >= detail::LogLevel::Debug)
        {
            auto redacted = detail::redact(headers);
            std::string joined;
            for (const auto &[name, value] : redacted)
            {
                joined += std::format("{}={} ", name, value);
            }
            detail::log_message(detail::LogLevel::Debug,
                                std::format("{} {} -> headers: {}", prepared->method, prepared->url, joined));
        }

        auto exec = detail::http_execute(prepared->method, prepared->url, headers, prepared->body, prepared->timeout);

        Error error = Error::sdk("unreachable");
        if (exec)
        {
            if (exec->status >= 200 && exec->status < 300)
            {
                return RawResponse{exec->status, std::move(exec->headers), endpoint, std::move(exec->body)};
            }
            error = detail::api_error(exec->status, deserialize_body(exec->body), exec->headers, endpoint);
        }
        else
        {
            error = exec.error();
        }

        if (retry.retryable(error) && attempt < retry.max_retries)
        {
            detail::log_message(detail::LogLevel::Info,
                                std::format("{} {} retry {}", prepared->method, prepared->url, attempt + 1));
            Duration wait = retry.wait(attempt + 1, error, jitter_sample());
            if (retry.timeout)
            {
                auto elapsed = std::chrono::duration_cast<Duration>(std::chrono::steady_clock::now() - started);
                if (elapsed + wait >= *retry.timeout)
                {
                    return std::unexpected(error);
                }
            }
            std::this_thread::sleep_for(wait);
            ++attempt;
            continue;
        }
        return std::unexpected(error);
    }
}

} // namespace typesafe
