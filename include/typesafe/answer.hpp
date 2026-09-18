#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "typesafe/common.hpp"
#include "typesafe/error.hpp"

namespace typesafe
{

struct NoulAnswer
{
    double noul{}; // probability, not a bool
    bool operator==(const NoulAnswer &) const = default;
};

struct ChoiceAnswer
{
    std::string choice;
    double confidence{};
    std::map<std::string, double> probabilities;
    bool operator==(const ChoiceAnswer &) const = default;
};

struct ScoreAnswer
{
    double score{};
    double confidence{};
    std::map<int, Json> legend;
    std::map<int, double> probabilities;
    bool operator==(const ScoreAnswer &) const = default;
};

class Answer
{
  public:
    Answer(NoulAnswer answer) : data_(std::move(answer))
    {
    } // NOLINT(*-explicit-*)
    Answer(ChoiceAnswer answer) : data_(std::move(answer))
    {
    } // NOLINT(*-explicit-*)
    Answer(ScoreAnswer answer) : data_(std::move(answer))
    {
    } // NOLINT(*-explicit-*)

    [[nodiscard]] const NoulAnswer *as_noul() const
    {
        return std::get_if<NoulAnswer>(&data_);
    }
    [[nodiscard]] const ChoiceAnswer *as_choice() const
    {
        return std::get_if<ChoiceAnswer>(&data_);
    }
    [[nodiscard]] const ScoreAnswer *as_score() const
    {
        return std::get_if<ScoreAnswer>(&data_);
    }

    bool operator==(const Answer &) const = default;

  private:
    std::variant<NoulAnswer, ChoiceAnswer, ScoreAnswer> data_;
};

class AnswerMap
{
  public:
    void insert(std::string name, Answer answer)
    {
        items_.emplace_back(std::move(name), std::move(answer));
    }
    [[nodiscard]] std::size_t size() const
    {
        return items_.size();
    }
    [[nodiscard]] bool empty() const
    {
        return items_.empty();
    }
    [[nodiscard]] const Answer *find(std::string_view name) const
    {
        for (const auto &[key, value] : items_)
        {
            if (key == name)
            {
                return &value;
            }
        }
        return nullptr;
    }
    [[nodiscard]] auto begin() const
    {
        return items_.begin();
    }
    [[nodiscard]] auto end() const
    {
        return items_.end();
    }

    bool operator==(const AnswerMap &) const = default;

  private:
    std::vector<std::pair<std::string, Answer>> items_;
};

struct Usage
{
    std::optional<std::int64_t> input_tokens;
    std::optional<std::int64_t> output_tokens;
    bool operator==(const Usage &) const = default;
};

struct ModelMetadata
{
    std::string name;
    std::string description;
    std::string release_date;
    bool operator==(const ModelMetadata &) const = default;
};

class ListModelsResponse
{
  public:
    std::vector<ModelMetadata> models;

    ListModelsResponse() = default;
    ListModelsResponse(std::vector<ModelMetadata> models, std::optional<std::string> request_id, std::string raw_body)
        : models(std::move(models)), request_id_(std::move(request_id)), raw_body_(std::move(raw_body))
    {
    }

    [[nodiscard]] Result<std::string> request_id() const;
    [[nodiscard]] const std::string &raw_body() const
    {
        return raw_body_;
    }

  private:
    std::optional<std::string> request_id_;
    std::string raw_body_;
};

class SystemOneResponse
{
  public:
    std::string model;
    Usage usage;
    AnswerMap answers;

    SystemOneResponse() = default;
    SystemOneResponse(std::string model, Usage usage, AnswerMap answers, std::optional<std::string> request_id,
                      std::string raw_body)
        : model(std::move(model)), usage(usage), answers(std::move(answers)), request_id_(std::move(request_id)),
          raw_body_(std::move(raw_body))
    {
    }

    [[nodiscard]] Result<std::string> request_id() const;
    [[nodiscard]] const std::string &raw_body() const
    {
        return raw_body_;
    }

    [[nodiscard]] Result<NoulAnswer> noul(std::string_view name) const;
    [[nodiscard]] Result<ChoiceAnswer> choice(std::string_view name) const;
    [[nodiscard]] Result<ScoreAnswer> score(std::string_view name) const;

  private:
    std::optional<std::string> request_id_;
    std::string raw_body_;
};

namespace detail
{

Result<SystemOneResponse> decode_system_one(int status, Headers headers, std::optional<std::string> endpoint,
                                            std::string body);
Result<ListModelsResponse> decode_models(int status, Headers headers, std::optional<std::string> endpoint,
                                         std::string body);

} // namespace detail

} // namespace typesafe
