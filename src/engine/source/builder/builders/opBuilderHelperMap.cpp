/* Copyright (C) 2015-2022, Wazuh Inc.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */
#include "opBuilderHelperMap.hpp"

#include <algorithm>
#include <optional>
#include <string>

#include <re2/re2.h>

#include "syntax.hpp"
#include <utils/stringUtils.hpp>

namespace
{

/**
 * @brief Tranform the string in `field` path in the base::Event `e` according to the
 * `op` definition and the `value` or the `refValue`
 *
 * @param field The field path to transform
 * @param op The operator to use:
 * - `u`: Upper case
 * - `l`: Lower case
 * @param e The base::Event that contains the field to transform
 * @param refValue The reference to the value user as source of the
 * transformation
 * @param value The value to use as source of the transformation
 * @return base::Event The base::Event with the field transformed
 * @throw std::logic_error if the `op` is not valid
 */
base::Event opBuilderHelperStringTransformation(const std::string field,
                                                char op,
                                                base::Event& e,
                                                std::optional<std::string> refValue,
                                                std::optional<std::string> value)
{

    // Get src field
    if (refValue.has_value())
    {
        // Get reference to json base::Event
        // TODO Remove try catch or if nullptr after fix get method of document
        // class
        // TODO Update to use proper reference
        const rapidjson::Value* refValueToCheck {};
        try
        {
            refValueToCheck = &e->getEventValue(refValue.value());
        }
        catch (std::exception& ex)
        {
            // TODO Check exception type
            return e;
        }

        if (refValueToCheck == nullptr || !refValueToCheck->IsString())
        {
            return e;
        }
        // If reache here, the refValueToCheck is a string
        value = std::string {refValueToCheck->GetString()};
    }

    // Operation
    switch (op)
    {
        case 'u':
            // Upper case
            std::transform(value.value().begin(),
                           value.value().end(),
                           value.value().begin(),
                           ::toupper);
            break;
        case 'l':
            // Lower case
            std::transform(value.value().begin(),
                           value.value().end(),
                           value.value().begin(),
                           ::tolower);
            break;
        default:
            // if raise here, then the source code is wrong
            throw std::logic_error("Invalid string transformation operator");
            break;
    }

    // Create and add string to base::Event
    try
    {
        e->getEvent()->set(
            field,
            rapidjson::Value(value.value().c_str(), e->getEventDocAllocator()).Move());
    }
    catch (std::exception& ex)
    {
        // TODO Check exception type
        return e;
    }

    return e;
}

/**
 * @brief Tranform the int in `field` path in the base::Event `e` according to the
 * `op` definition and the `value` or the `refValue`
 *
 * @param field The field path to transform
 * @param op The operator to use:
 * - `sum`: Sum
 * - `sub`: Subtract
 * - `mul`: Multiply
 * - `div`: Divide
 * @param e The base::Event that contains the field to transform
 * @param refValue The reference to the value user as source of the
 * transformation
 * @param value The value to use as source of the transformation
 * @return base::Event The base::Event with the field transformed
 * @throw std::logic_error if the `op` is not valid
 */
base::Event opBuilderHelperIntTransformation(const std::string field,
                                             std::string op,
                                             base::Event& e,
                                             std::optional<std::string> refValue,
                                             std::optional<int> value)
{
    // TODO Remove try catch or if nullptr after fix get method of document
    // class Get value to compare
    const rapidjson::Value* fieldValue {};
    try
    {
        fieldValue = &e->getEventValue(field);
    }
    catch (std::exception& ex)
    {
        // TODO Check exception type
        return e;
    }

    if (fieldValue == nullptr || !fieldValue->IsInt())
    {
        return e;
    }

    if (refValue.has_value())
    {
        // Get reference to json base::Event
        // TODO Remove try catch or if nullptr after fix get method of document
        // class
        const rapidjson::Value* refValueToCheck {};
        try
        {
            refValueToCheck = &e->getEventValue(refValue.value());
        }
        catch (std::exception& ex)
        {
            // TODO Check exception type
            return e;
        }

        if (refValueToCheck == nullptr || !refValueToCheck->IsInt())
        {
            return e;
        }
        value = refValueToCheck->GetInt();
    }

    // Operation
    // TODO: change to switch for performance
    if (op == "sum")
    {
        value = fieldValue->GetInt() + value.value();
    }
    else if (op == "sub")
    {
        value = fieldValue->GetInt() - value.value();
    }
    else if (op == "mul")
    {
        value = fieldValue->GetInt() * value.value();
    }
    else if (op == "div")
    {
        if (value.value() == 0)
        {
            return e;
        }
        value = fieldValue->GetInt() / value.value();
    }
    else
    {
        return e;
    }

    // Create and add string to base::Event
    try
    {
        e->getEvent()->set(field, rapidjson::Value(value.value()));
    }
    catch (std::exception& ex)
    {
        // TODO Check exception type
        return e;
    }
    return e;
}

} // namespace

namespace builder::internals::builders
{

using builder::internals::syntax::REFERENCE_ANCHOR;
//*************************************************
//*           String tranform                     *
//*************************************************

// <field>: +s_up/<str>|$<ref>
base::Lifter opBuilderHelperStringUP(const base::DocumentValue& def, types::TracerFn tr)
{
    // Get field key to check

    std::string key {json::formatJsonPath(def.MemberBegin()->name.GetString())};

    // Get the raw value of parameter
    if (!def.MemberBegin()->value.IsString())
    {
        throw std::runtime_error(
            "Invalid parameter type for s_up operator (str expected)");
    }

    // Parse parameters
    std::string parm {def.MemberBegin()->value.GetString()};
    auto parametersArr {utils::string::split(parm, '/')};
    if (parametersArr.size() != 2)
    {
        throw std::runtime_error("Invalid number of parameters for s_up operator");
    }

    std::optional<std::string> refExpStr {};
    std::optional<std::string> expectedStr {};

    // Check if is a reference to json base::Event
    if (parametersArr[1][0] == REFERENCE_ANCHOR)
    {
        refExpStr = json::formatJsonPath(parametersArr[1].substr(1));
    }
    else
    {
        expectedStr = parametersArr[1];
    }

    // Return Lifter
    return [=](base::Observable o) {
        // Append rxcpp operation
        return o.map([key, expectedStr, refExpStr](base::Event e) {
            return opBuilderHelperStringTransformation(
                key, 'u', e, refExpStr, expectedStr);
        });
    };
}

// <field>: +s_lo/<str>|$<ref>
base::Lifter opBuilderHelperStringLO(const base::DocumentValue& def, types::TracerFn tr)
{

    // Get field key to check
    std::string key {json::formatJsonPath(def.MemberBegin()->name.GetString())};

    // Get the raw value of parameter
    if (!def.MemberBegin()->value.IsString())
    {
        throw std::runtime_error(
            "Invalid parameter type for s_lo operator (str expected)");
    }

    // Parse parameters
    std::string parm {def.MemberBegin()->value.GetString()};
    auto parametersArr {utils::string::split(parm, '/')};
    if (parametersArr.size() != 2)
    {
        throw std::runtime_error("Invalid number of parameters for s_lo operator");
    }

    std::optional<std::string> refExpStr {};
    std::optional<std::string> expectedStr {};

    // Check if is a reference to json base::Event
    if (parametersArr[1][0] == REFERENCE_ANCHOR)
    {
        refExpStr = json::formatJsonPath(parametersArr[1].substr(1));
    }
    else
    {
        expectedStr = parametersArr[1];
    }

    // Return Lifter
    return [=](base::Observable o) {
        // Append rxcpp operation
        return o.map([key, expectedStr, refExpStr](base::Event e) {
            return opBuilderHelperStringTransformation(
                key, 'l', e, refExpStr, expectedStr);
        });
    };
}

// <field>: +s_trim/[begin | end | both]/char
base::Lifter opBuilderHelperStringTrim(const base::DocumentValue& def, types::TracerFn tr)
{

    // Get field path to trim
    std::string field {json::formatJsonPath(def.MemberBegin()->name.GetString())};

    // Get the raw value of parameter
    if (!def.MemberBegin()->value.IsString())
    {
        // Logical error
        throw std::runtime_error(
            "Invalid parameter type for s_trim operator (str expected)");
    }

    // Parse parameters
    std::string parm {def.MemberBegin()->value.GetString()};
    auto parametersArr {utils::string::split(parm, '/')};
    if (parametersArr.size() != 3)
    {
        throw std::runtime_error("Invalid number of parameters for s_trim operator");
    }

    // Get trim type
    char trimType =
        parametersArr[1] == "begin"
            ? 's'
            : parametersArr[1] == "end" ? 'e' : parametersArr[1] == "both" ? 'b' : '\0';
    if (trimType == '\0')
    {
        throw std::runtime_error("Invalid trim type for s_trim operator");
    }

    // get trim char
    std::string trimChar {parametersArr[2]};
    if (trimChar.size() != 1)
    {
        throw std::runtime_error("Invalid trim char for s_trim operator");
    }

    // Return Lifter
    return [=](base::Observable o) {
        // Append rxcpp operation
        return o.map([field, trimType, trimChar](base::Event e) {
            // Shoulbe short after refact, witout try catch
            // Get field value
            const rapidjson::Value* fieldValue;
            try
            {
                fieldValue = &e->getEventValue(field);
            }
            catch (std::exception& ex)
            {
                // TODO Check exception type
                return e;
            }

            // Check if field is a string
            if (fieldValue == nullptr || !fieldValue->IsString())
            {
                return e;
            }

            // Get string
            std::string strToTrim {fieldValue->GetString()};

            // Trim
            switch (trimType)
            {
                case 's':
                    // Trim begin
                    strToTrim.erase(0, strToTrim.find_first_not_of(trimChar));
                    break;
                case 'e':
                    // Trim end
                    strToTrim.erase(strToTrim.find_last_not_of(trimChar) + 1);
                    break;
                case 'b':
                    // Trim both
                    strToTrim.erase(0, strToTrim.find_first_not_of(trimChar));
                    strToTrim.erase(strToTrim.find_last_not_of(trimChar) + 1);
                    break;
                default:
                    // if raise here, then the source code is wrong
                    throw std::logic_error("Invalid trim type for s_trim operator");
                    break;
            }

            // Update base::Event
            try
            {
                e->getEvent()->set(
                    field,
                    rapidjson::Value(strToTrim.c_str(), e->getEventDocAllocator())
                        .Move());
            }
            catch (std::exception& ex)
            {
                // TODO Check exception type
                return e;
            }

            return e;
        });
    };
}

// <field>: +s_concat/<string1>|$<ref1>/<string2>|$<ref2>
base::Lifter opBuilderHelperStringConcat(const base::DocumentValue& def,
                                         types::TracerFn tr)
{

    // Get field path to concat
    std::string field {json::formatJsonPath(def.MemberBegin()->name.GetString())};

    // Get the raw value of parameter
    if (!def.MemberBegin()->value.IsString())
    {
        // Logical error
        throw std::runtime_error(
            "Invalid parameter type for s_concat operator (str expected)");
    }

    // Parse parameters
    std::string parm {def.MemberBegin()->value.GetString()};
    auto parametersArr {utils::string::split(parm, '/')};
    if (parametersArr.size() < 3)
    {
        throw std::runtime_error("Invalid number of parameters for s_concat operator");
    }

    // removing first element (helper function name)
    parametersArr.erase(parametersArr.begin());

    // check if parameters aren't empty
    for (auto param : parametersArr)
    {
        if (param.empty())
        {
            throw std::runtime_error("one parameter is an empty string");
        }
    }

    base::Document doc {def};
    std::string successTrace = fmt::format("{} s_concat Success", doc.str());
    std::string failureTrace = fmt::format("{} s_concat Failure", doc.str());

    // Return Lifter
    return [=, parametersArr = std::move(parametersArr), tr = std::move(tr)](
               base::Observable o) {
        // Append rxcpp operation
        return o.map([=, parametersArr = std::move(parametersArr), tr = std::move(tr)](
                         base::Event e) {
            std::string result {};

            for (auto parameter : parametersArr)
            {
                if (parameter.at(0) == REFERENCE_ANCHOR)
                {
                    auto param = json::formatJsonPath(parameter.substr(1));
                    try
                    {
                        const auto &value = e->getEvent()->get(param);
                        if (value.IsString())
                        {
                            result.append(value.GetString());
                        }
                        else
                        {
                            tr(failureTrace);
                            return e;
                        }
                    }
                    catch (std::exception& ex)
                    {
                        tr(failureTrace);
                        return e;
                    }
                }
                else
                {
                    result.append(parameter);
                }
            }

            try
            {
                tr(successTrace);
                e->getEvent()->set(
                    field,
                    rapidjson::Value(result.c_str(), e->getEventDocAllocator()).Move());
                return e;
            }
            catch (std::exception& ex)
            {
                return e;
            }
        });
    };
}

//*************************************************
//*           Int tranform                        *
//*************************************************

// field: +i_calc/[+|-|*|/]/val|$ref/
base::Lifter opBuilderHelperIntCalc(const base::DocumentValue& def, types::TracerFn tr)
{
    // Get field
    std::string field {json::formatJsonPath(def.MemberBegin()->name.GetString())};
    std::string rawValue {def.MemberBegin()->value.GetString()};

    std::vector<std::string> parameters {utils::string::split(rawValue, '/')};

    if (parameters.size() != 3)
    {
        throw std::runtime_error("Invalid parameters");
    }

    std::optional<std::string> refValue {};
    std::optional<int> value {};
    std::string op {parameters[1]};

    // TODO Parametrize this
    if (op != "sum" && op != "sub" && op != "mul" && op != "div")
    {
        throw std::runtime_error("Invalid operator");
    }

    if (op == "div")
    {
        if (parameters[2] == "0")
        {
            throw std::runtime_error("Division by zero");
        }
    }

    if (parameters[2][0] == REFERENCE_ANCHOR)
    {
        // Check case `+i_calc/op/$`
        refValue = json::formatJsonPath(parameters[2].substr(1, std::string::npos));
    }
    else
    {
        value = std::stoi(parameters[2]);
    }

    // Return Lifter
    return [field, op, refValue, value](base::Observable o) {
        // Append rxcpp operation
        return o.map([=](base::Event e) {
            return opBuilderHelperIntTransformation(field, op, e, refValue, value);
        });
    };
}

//*************************************************
//*             JSON tranform                     *
//*************************************************

// <key>: +delete_field
base::Lifter opBuilderHelperDeleteField(const base::DocumentValue& def,
                                        types::TracerFn tr)
{
    // Get the name of the field which will be deleted
    std::string key {json::formatJsonPath(def.MemberBegin()->name.GetString())};

    if (key.empty())
    {
        throw std::runtime_error("Key shouldn't be emoty for delete_field operator");
    }

    // Parse parameters
    std::string param {def.MemberBegin()->value.GetString()};
    auto parametersArr {utils::string::split(param, '/')};
    if (parametersArr.size() > 1)
    {
        throw std::runtime_error(
            "Invalid number of parameters for delete_field operator");
    }

    // Get the raw value of parameter
    if (!def.MemberBegin()->value.IsString())
    {
        // Logical error
        throw std::runtime_error(
            "Invalid parameter type for delete_field operator (str expected)");
    }

    base::Document doc {def};
    std::string successTrace = fmt::format("{} delete_field Success", doc.str());
    std::string failureTrace = fmt::format("{} delete_field Failure", doc.str());

    // Return Lifter
    return [=, tr = std::move(tr)](base::Observable o) {
        // Append rxcpp operation
        return o.map([=, tr = std::move(tr)](base::Event e) {
            try
            {
                if (e->getEvent()->erase(key))
                {
                    tr(successTrace);
                }
                else
                {
                    tr(failureTrace);
                }
            }
            catch (const std::exception& ex)
            {
                tr(failureTrace + ": " + ex.what());
            }

            return e;
        });
    };
}

//*************************************************
//*           Regex tranform                      *
//*************************************************

// field: +r_ext/_field/regexp/
base::Lifter opBuilderHelperRegexExtract(const base::DocumentValue& def,
                                         types::TracerFn tr)
{
    // Get fields
    std::string field {json::formatJsonPath(def.MemberBegin()->name.GetString())};
    std::string value {def.MemberBegin()->value.GetString()};

    std::vector<std::string> parameters {utils::string::split(value, '/')};
    if (parameters.size() != 3)
    {
        throw std::runtime_error("Invalid number of parameters");
    }

    std::string map_field {json::formatJsonPath(parameters[1])};

    auto regex_ptr {std::make_shared<RE2>(parameters[2])};
    if (!regex_ptr->ok())
    {
        const std::string err =
            "Error compiling regex '" + parameters[2] + "'. " + regex_ptr->error();
        throw std::runtime_error(err);
    }

    // Return Lifter
    return [field, regex_ptr, map_field](base::Observable o) {
        // Append rxcpp operation
        return o.map([=](base::Event e) {
            // TODO Remove try catch
            const rapidjson::Value* field_str {};
            try
            {
                field_str = &e->getEventValue(field);
            }
            catch (std::exception& ex)
            {
                // TODO Check exception type
                return e;
            }
            if (field_str != nullptr && field_str->IsString())
            {
                std::string match;
                if (RE2::PartialMatch(field_str->GetString(), *regex_ptr, &match))
                {
                    // Create and add string to base::Event
                    try
                    {
                        e->getEvent()->set(
                            map_field,
                            rapidjson::Value(match.c_str(), e->getEventDocAllocator())
                                .Move());
                    }
                    catch (std::exception& ex)
                    {
                        // TODO Check exception type
                        return e;
                    }

                    return e;
                }
            }
            return e;
        });
    };
}

} // namespace builder::internals::builders
