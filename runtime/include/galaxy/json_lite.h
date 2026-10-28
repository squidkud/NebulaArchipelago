// SPDX-License-Identifier: GPL-3.0-only
// A small JSON reader and writer, for the Archipelago client's messages (galaxy/archipelago.h). It reads what the
// protocol sends (objects, arrays, strings, numbers, booleans, null) and writes what the client sends back.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace galaxy::json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Value() = default;
    [[nodiscard]] static Value boolean(bool value);
    [[nodiscard]] static Value integer(std::int64_t value);
    [[nodiscard]] static Value number(double value);
    [[nodiscard]] static Value string(std::string value);
    [[nodiscard]] static Value array();
    [[nodiscard]] static Value object();

    [[nodiscard]] Type type() const noexcept { return type_; }
    [[nodiscard]] bool is_null() const noexcept { return type_ == Type::Null; }
    [[nodiscard]] bool is_number() const noexcept { return type_ == Type::Number; }
    [[nodiscard]] bool is_string() const noexcept { return type_ == Type::String; }
    [[nodiscard]] bool is_array() const noexcept { return type_ == Type::Array; }
    [[nodiscard]] bool is_object() const noexcept { return type_ == Type::Object; }

    // The value as that type, or `fallback` when it is another type.
    [[nodiscard]] bool as_bool(bool fallback = false) const noexcept;
    [[nodiscard]] std::int64_t as_int(std::int64_t fallback = 0) const noexcept;
    [[nodiscard]] double as_double(double fallback = 0.0) const noexcept;
    // Empty when the value is not a string.
    [[nodiscard]] const std::string& as_string() const noexcept;

    // An array's items; empty when the value is not an array.
    [[nodiscard]] const std::vector<Value>& items() const noexcept;
    // An object's members in the order they were read; empty when the value is not an object.
    [[nodiscard]] const std::vector<std::pair<std::string, Value>>& members() const noexcept;
    // The object's member of that name (the last when repeated); null when absent or the value is not an object.
    [[nodiscard]] const Value* find(std::string_view key) const noexcept;

    // Building: append to an array (turns a null value into one), set an object's member (likewise).
    Value& push(Value value);
    Value& set(std::string key, Value value);

    // The value as JSON text, compact.
    [[nodiscard]] std::string dump() const;

private:
    void dump_to(std::string& out) const;

    Type type_ = Type::Null;
    bool bool_ = false;
    bool is_integer_ = false;
    std::int64_t integer_ = 0;
    double number_ = 0.0;
    std::string string_;
    std::vector<Value> items_;
    std::vector<std::pair<std::string, Value>> members_;
};

// Parses one JSON document (nothing may follow it but white space); nullopt when it is malformed or nested deeper than
// 64 levels.
[[nodiscard]] std::optional<Value> parse(std::string_view text);

// `text` as a JSON string literal, quotes included.
[[nodiscard]] std::string quote(std::string_view text);

}  // namespace galaxy::json
