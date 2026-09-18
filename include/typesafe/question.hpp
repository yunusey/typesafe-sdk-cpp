#pragma once

#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "typesafe/common.hpp"
#include "typesafe/error.hpp"

namespace typesafe
{

struct NoulCriteria
{
    std::optional<Json> true_meaning;
    std::optional<Json> false_meaning;

    NoulCriteria &yes(Json value)
    {
        true_meaning = std::move(value);
        return *this;
    }
    NoulCriteria &no(Json value)
    {
        false_meaning = std::move(value);
        return *this;
    }

    // Wire criteria is {"true": ..., "false": ...}.
    [[nodiscard]] Json to_json() const;
};

using ChoiceCriterion = std::pair<std::string, std::optional<Json>>;

class Question
{
  public:
    static Question noul(Json instructions);
    static Question noul_bare();
    [[nodiscard]] Question with_noul_criteria(NoulCriteria criteria) const; // no-op on other variants

    static Question choice(Json instructions, std::vector<ChoiceCriterion> criteria);
    static Question score(Json instructions, std::vector<Json> criteria);

    static Question raw(Json object);
    static Result<Question> from_value(Json value); // error if not an object

    [[nodiscard]] Result<Json> to_wire(std::string_view name) const;

  private:
    struct Noul
    {
        std::optional<Json> instructions;
        std::optional<NoulCriteria> criteria;
    };
    struct Choice
    {
        std::optional<Json> instructions;
        std::vector<ChoiceCriterion> criteria;
    };
    struct Score
    {
        std::optional<Json> instructions;
        std::vector<Json> criteria;
    };
    struct Raw
    {
        Json object;
    };

    using Data = std::variant<Noul, Choice, Score, Raw>;
    explicit Question(Data data) : data_(std::move(data))
    {
    }
    Data data_;
};

namespace detail
{

using QuestionList = std::vector<std::pair<std::string, Question>>;

Result<Json> normalize_questions(const QuestionList &questions);

} // namespace detail

} // namespace typesafe
