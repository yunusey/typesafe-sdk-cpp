#include "typesafe/question.hpp"

#include <format>

namespace typesafe
{

Json NoulCriteria::to_json() const
{
    Json map = Json::object();
    if (true_meaning)
    {
        map["true"] = *true_meaning;
    }
    if (false_meaning)
    {
        map["false"] = *false_meaning;
    }
    return map;
}

Question Question::noul(Json instructions)
{
    return Question(Noul{std::move(instructions), std::nullopt});
}

Question Question::noul_bare()
{
    return Question(Noul{std::nullopt, std::nullopt});
}

Question Question::with_noul_criteria(NoulCriteria criteria) const
{
    if (const auto *noul = std::get_if<Noul>(&data_))
    {
        return Question(Noul{noul->instructions, std::move(criteria)});
    }
    return *this;
}

Question Question::choice(Json instructions, std::vector<ChoiceCriterion> criteria)
{
    return Question(Choice{std::move(instructions), std::move(criteria)});
}

Question Question::score(Json instructions, std::vector<Json> criteria)
{
    return Question(Score{std::move(instructions), std::move(criteria)});
}

Question Question::raw(Json object)
{
    return Question(Raw{std::move(object)});
}

Result<Question> Question::from_value(Json value)
{
    if (value.is_object())
    {
        return Question(Raw{std::move(value)});
    }
    return std::unexpected(
        Error::sdk("Question must be a question object or a dictionary with a nonempty string \"type\"."));
}

namespace
{

Result<void> validate_raw(std::string_view name, const Json &map)
{
    auto type_it = map.find("type");
    bool valid_type = type_it != map.end() && type_it->is_string() && !type_it->get<std::string>().empty();
    if (!valid_type)
    {
        return std::unexpected(
            Error::sdk(std::format("Question \"{}\" must be a question object or a dictionary with a nonempty string "
                                   "\"type\".",
                                   name)));
    }
    std::string type_name = type_it->get<std::string>();
    if ((type_name == "choice" || type_name == "score") && !map.contains("criteria"))
    {
        return std::unexpected(Error::sdk(std::format("Question \"{}\" requires \"criteria\".", name)));
    }
    if (type_name == "score")
    {
        auto criteria = map.find("criteria");
        if (criteria != map.end() && criteria->is_array() && criteria->empty())
        {
            return std::unexpected(Error::sdk(
                std::format("Score question \"{}\" has no criteria; at least one score is required.", name)));
        }
    }
    return {};
}

} // namespace

Result<Json> Question::to_wire(std::string_view name) const
{
    if (const auto *noul = std::get_if<Noul>(&data_))
    {
        Json map = Json::object();
        map["type"] = "noul";
        if (noul->instructions)
        {
            map["instructions"] = *noul->instructions;
        }
        if (noul->criteria)
        {
            map["criteria"] = noul->criteria->to_json();
        }
        return map;
    }
    if (const auto *choice = std::get_if<Choice>(&data_))
    {
        Json map = Json::object();
        map["type"] = "choice";
        if (choice->instructions)
        {
            map["instructions"] = *choice->instructions;
        }
        Json criteria = Json::object();
        for (const auto &[label, description] : choice->criteria)
        {
            criteria[label] = description ? *description : Json(nullptr);
        }
        map["criteria"] = std::move(criteria);
        return map;
    }
    if (const auto *score = std::get_if<Score>(&data_))
    {
        if (score->criteria.empty())
        {
            return std::unexpected(Error::sdk(
                std::format("Score question \"{}\" has no criteria; at least one score is required.", name)));
        }
        Json map = Json::object();
        map["type"] = "score";
        if (score->instructions)
        {
            map["instructions"] = *score->instructions;
        }
        Json criteria = Json::array();
        for (const auto &item : score->criteria)
        {
            criteria.push_back(item);
        }
        map["criteria"] = std::move(criteria);
        return map;
    }
    const auto &raw = std::get<Raw>(data_);
    if (auto ok = validate_raw(name, raw.object); !ok)
    {
        return std::unexpected(ok.error());
    }
    return raw.object;
}

namespace detail
{

Result<Json> normalize_questions(const QuestionList &questions)
{
    Json out = Json::object();
    for (const auto &[name, question] : questions)
    {
        auto wire = question.to_wire(name);
        if (!wire)
        {
            return std::unexpected(wire.error());
        }
        out[name] = std::move(wire.value());
    }
    if (out.empty())
    {
        return std::unexpected(Error::sdk("At least one question is required."));
    }
    return out;
}

} // namespace detail

} // namespace typesafe
