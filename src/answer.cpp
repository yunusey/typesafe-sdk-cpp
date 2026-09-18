#include "typesafe/answer.hpp"

#include <charconv>
#include <expected>
#include <format>

#include "typesafe/constants.hpp"

namespace typesafe
{

namespace
{

template <class T> using Decoded = std::expected<T, std::string>; // error is a schema path

std::optional<std::string> header_request_id(const Headers &headers)
{
    return headers.get(detail::kRequestIdHeader);
}

Decoded<std::optional<std::int64_t>> optional_i64(const Json &object, std::string_view path, std::string_view key)
{
    auto it = object.find(key);
    if (it == object.end() || it->is_null())
    {
        return std::optional<std::int64_t>{};
    }
    if (it->is_number_integer() || it->is_number_unsigned())
    {
        return std::optional<std::int64_t>{it->get<std::int64_t>()};
    }
    return std::unexpected(std::string(path));
}

Decoded<Usage> decode_usage(const Json &value)
{
    if (!value.is_object())
    {
        return std::unexpected(std::string("usage"));
    }
    auto input = optional_i64(value, "usage.input_tokens", "input_tokens");
    if (!input)
    {
        return std::unexpected(input.error());
    }
    auto output = optional_i64(value, "usage.output_tokens", "output_tokens");
    if (!output)
    {
        return std::unexpected(output.error());
    }
    return Usage{input.value(), output.value()};
}

Decoded<std::string> required_str(const Json &object, const std::string &path, std::string_view key)
{
    auto it = object.find(key);
    if (it == object.end() || !it->is_string())
    {
        return std::unexpected(path);
    }
    return it->get<std::string>();
}

Decoded<ModelMetadata> decode_model(std::size_t index, const Json &value)
{
    if (!value.is_object())
    {
        return std::unexpected(std::format("models[{}]", index));
    }
    auto name = required_str(value, std::format("models[{}].name", index), "name");
    if (!name)
    {
        return std::unexpected(name.error());
    }
    auto description = required_str(value, std::format("models[{}].description", index), "description");
    if (!description)
    {
        return std::unexpected(description.error());
    }
    auto release_date = required_str(value, std::format("models[{}].release_date", index), "release_date");
    if (!release_date)
    {
        return std::unexpected(release_date.error());
    }
    return ModelMetadata{name.value(), description.value(), release_date.value()};
}

Decoded<std::map<int, Json>> int_content_map(const Json *value, const std::string &path)
{
    if (value == nullptr || !value->is_object())
    {
        return std::unexpected(path);
    }
    std::map<int, Json> out;
    for (const auto &[key, item] : value->items())
    {
        int index = 0;
        auto [ptr, ec] = std::from_chars(key.data(), key.data() + key.size(), index);
        if (ec != std::errc{} || ptr != key.data() + key.size())
        {
            return std::unexpected(path);
        }
        out[index] = item;
    }
    return out;
}

Decoded<std::map<int, double>> int_f64_map(const Json *value, const std::string &path)
{
    if (value == nullptr || !value->is_object())
    {
        return std::unexpected(path);
    }
    std::map<int, double> out;
    for (const auto &[key, item] : value->items())
    {
        int index = 0;
        auto [ptr, ec] = std::from_chars(key.data(), key.data() + key.size(), index);
        if (ec != std::errc{} || ptr != key.data() + key.size())
        {
            return std::unexpected(path);
        }
        if (!item.is_number())
        {
            return std::unexpected(std::format("{}.{}", path, key));
        }
        out[index] = item.get<double>();
    }
    return out;
}

const Json *find_ptr(const Json &object, std::string_view key)
{
    auto it = object.find(key);
    return it == object.end() ? nullptr : &*it;
}

Decoded<AnswerMap> decode_answers(const Json &map)
{
    AnswerMap answers;
    for (const auto &[name, raw] : map.items())
    {
        if (!raw.is_object())
        {
            return std::unexpected(std::format("answers.{}.type", name));
        }
        auto type_it = raw.find("type");
        if (type_it == raw.end() || !type_it->is_string())
        {
            return std::unexpected(std::format("answers.{}.type", name));
        }
        std::string tag = type_it->get<std::string>();
        if (tag == "noul")
        {
            const Json *noul = find_ptr(raw, "noul");
            if (noul == nullptr || !noul->is_number())
            {
                return std::unexpected(std::format("answers.{}.noul", name));
            }
            answers.insert(name, NoulAnswer{noul->get<double>()});
        }
        else if (tag == "choice")
        {
            const Json *choice = find_ptr(raw, "choice");
            if (choice == nullptr || !choice->is_string())
            {
                return std::unexpected(std::format("answers.{}.choice", name));
            }
            const Json *confidence = find_ptr(raw, "confidence");
            if (confidence == nullptr || !confidence->is_number())
            {
                return std::unexpected(std::format("answers.{}.confidence", name));
            }
            const Json *probabilities = find_ptr(raw, "probabilities");
            if (probabilities == nullptr || !probabilities->is_object())
            {
                return std::unexpected(std::format("answers.{}.probabilities", name));
            }
            std::map<std::string, double> probs;
            for (const auto &[key, value] : probabilities->items())
            {
                if (!value.is_number())
                {
                    return std::unexpected(std::format("answers.{}.probabilities.{}", name, key));
                }
                probs[key] = value.get<double>();
            }
            answers.insert(name, ChoiceAnswer{choice->get<std::string>(), confidence->get<double>(), std::move(probs)});
        }
        else if (tag == "score")
        {
            const Json *score = find_ptr(raw, "score");
            if (score == nullptr || !score->is_number())
            {
                return std::unexpected(std::format("answers.{}.score", name));
            }
            const Json *confidence = find_ptr(raw, "confidence");
            if (confidence == nullptr || !confidence->is_number())
            {
                return std::unexpected(std::format("answers.{}.confidence", name));
            }
            auto legend = int_content_map(find_ptr(raw, "legend"), std::format("answers.{}.legend", name));
            if (!legend)
            {
                return std::unexpected(legend.error());
            }
            auto probs = int_f64_map(find_ptr(raw, "probabilities"), std::format("answers.{}.probabilities", name));
            if (!probs)
            {
                return std::unexpected(probs.error());
            }
            answers.insert(name, ScoreAnswer{score->get<double>(), confidence->get<double>(), std::move(legend.value()),
                                             std::move(probs.value())});
        }
        // Unknown answer types are ignored; the raw body still carries them.
    }
    return answers;
}

} // namespace

Result<std::string> ListModelsResponse::request_id() const
{
    if (request_id_)
    {
        return *request_id_;
    }
    return std::unexpected(Error::sdk("The response did not include a request ID."));
}

Result<std::string> SystemOneResponse::request_id() const
{
    if (request_id_)
    {
        return *request_id_;
    }
    return std::unexpected(Error::sdk("The response did not include a request ID."));
}

Result<NoulAnswer> SystemOneResponse::noul(std::string_view name) const
{
    if (const Answer *answer = answers.find(name))
    {
        if (const NoulAnswer *noul = answer->as_noul())
        {
            return *noul;
        }
    }
    return std::unexpected(Error::sdk(std::format("No noul answer named \"{}\".", name)));
}

Result<ChoiceAnswer> SystemOneResponse::choice(std::string_view name) const
{
    if (const Answer *answer = answers.find(name))
    {
        if (const ChoiceAnswer *choice = answer->as_choice())
        {
            return *choice;
        }
    }
    return std::unexpected(Error::sdk(std::format("No choice answer named \"{}\".", name)));
}

Result<ScoreAnswer> SystemOneResponse::score(std::string_view name) const
{
    if (const Answer *answer = answers.find(name))
    {
        if (const ScoreAnswer *score = answer->as_score())
        {
            return *score;
        }
    }
    return std::unexpected(Error::sdk(std::format("No score answer named \"{}\".", name)));
}

namespace detail
{

Result<SystemOneResponse> decode_system_one(int status, Headers headers, std::optional<std::string> endpoint,
                                            std::string body)
{
    auto fail = [&](std::string path) {
        return std::unexpected(validation_error(status, deserialize_body(body), headers, endpoint, std::move(path)));
    };

    Json parsed = Json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_discarded() || !parsed.is_object())
    {
        return fail("model");
    }
    auto model_it = parsed.find("model");
    if (model_it == parsed.end() || !model_it->is_string())
    {
        return fail("model");
    }
    std::string model = model_it->get<std::string>();

    Usage usage;
    if (auto usage_it = parsed.find("usage"); usage_it != parsed.end())
    {
        auto decoded = decode_usage(*usage_it);
        if (!decoded)
        {
            return fail(decoded.error());
        }
        usage = decoded.value();
    }

    AnswerMap answers;
    if (auto answers_it = parsed.find("answers"); answers_it != parsed.end())
    {
        if (!answers_it->is_object())
        {
            return fail("answers");
        }
        auto decoded = decode_answers(*answers_it);
        if (!decoded)
        {
            return fail(decoded.error());
        }
        answers = std::move(decoded.value());
    }

    auto request_id = header_request_id(headers);
    return SystemOneResponse(std::move(model), usage, std::move(answers), std::move(request_id), std::move(body));
}

Result<ListModelsResponse> decode_models(int status, Headers headers, std::optional<std::string> endpoint,
                                         std::string body)
{
    auto fail = [&](std::string path) {
        return std::unexpected(validation_error(status, deserialize_body(body), headers, endpoint, std::move(path)));
    };

    Json parsed = Json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (parsed.is_discarded())
    {
        return fail("models");
    }
    auto models_it = parsed.is_object() ? parsed.find("models") : parsed.end();
    if (models_it == parsed.end() || !models_it->is_array())
    {
        return fail("models");
    }
    std::vector<ModelMetadata> models;
    std::size_t index = 0;
    for (const auto &item : *models_it)
    {
        auto decoded = decode_model(index, item);
        if (!decoded)
        {
            return fail(decoded.error());
        }
        models.push_back(std::move(decoded.value()));
        ++index;
    }

    auto request_id = header_request_id(headers);
    return ListModelsResponse(std::move(models), std::move(request_id), std::move(body));
}

} // namespace detail

} // namespace typesafe
