// SPDX-License-Identifier: GPL-3.0-only
#include "galaxy/json_lite.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace galaxy::json {

namespace {
const std::string kEmptyString;
const std::vector<Value> kNoItems;
const std::vector<std::pair<std::string, Value>> kNoMembers;

constexpr int kMaxDepth = 64;

void append_utf8(std::string& out, std::uint32_t code) {
    if (code < 0x80u) {
        out += static_cast<char>(code);
    } else if (code < 0x800u) {
        out += static_cast<char>(0xC0u | (code >> 6));
        out += static_cast<char>(0x80u | (code & 0x3Fu));
    } else if (code < 0x10000u) {
        out += static_cast<char>(0xE0u | (code >> 12));
        out += static_cast<char>(0x80u | ((code >> 6) & 0x3Fu));
        out += static_cast<char>(0x80u | (code & 0x3Fu));
    } else {
        out += static_cast<char>(0xF0u | (code >> 18));
        out += static_cast<char>(0x80u | ((code >> 12) & 0x3Fu));
        out += static_cast<char>(0x80u | ((code >> 6) & 0x3Fu));
        out += static_cast<char>(0x80u | (code & 0x3Fu));
    }
}

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    std::optional<Value> run() {
        skip_space();
        std::optional<Value> value = parse_value(0);
        if (!value) return std::nullopt;
        skip_space();
        if (pos_ != text_.size()) return std::nullopt;
        return value;
    }

private:
    std::string_view text_;
    std::size_t pos_ = 0;

    bool at_end() const { return pos_ >= text_.size(); }
    char peek() const { return text_[pos_]; }

    void skip_space() {
        while (!at_end() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r')) ++pos_;
    }

    bool consume(std::string_view word) {
        if (text_.substr(pos_, word.size()) != word) return false;
        pos_ += word.size();
        return true;
    }

    std::optional<Value> parse_value(int depth) {
        if (depth > kMaxDepth || at_end()) return std::nullopt;
        switch (peek()) {
        case '{': return parse_object(depth);
        case '[': return parse_array(depth);
        case '"': {
            std::string text;
            if (!parse_string(text)) return std::nullopt;
            return Value::string(std::move(text));
        }
        case 't': return consume("true") ? std::optional<Value>(Value::boolean(true)) : std::nullopt;
        case 'f': return consume("false") ? std::optional<Value>(Value::boolean(false)) : std::nullopt;
        case 'n': return consume("null") ? std::optional<Value>(Value()) : std::nullopt;
        default: return parse_number();
        }
    }

    std::optional<Value> parse_number() {
        const std::size_t start = pos_;
        bool integral = true;
        if (!at_end() && peek() == '-') ++pos_;
        const std::size_t digits = pos_;
        while (!at_end() && peek() >= '0' && peek() <= '9') ++pos_;
        if (pos_ == digits) return std::nullopt;
        if (!at_end() && peek() == '.') {
            integral = false;
            ++pos_;
            const std::size_t fraction = pos_;
            while (!at_end() && peek() >= '0' && peek() <= '9') ++pos_;
            if (pos_ == fraction) return std::nullopt;
        }
        if (!at_end() && (peek() == 'e' || peek() == 'E')) {
            integral = false;
            ++pos_;
            if (!at_end() && (peek() == '+' || peek() == '-')) ++pos_;
            const std::size_t exponent = pos_;
            while (!at_end() && peek() >= '0' && peek() <= '9') ++pos_;
            if (pos_ == exponent) return std::nullopt;
        }
        const std::string literal(text_.substr(start, pos_ - start));
        if (integral) {
            errno = 0;
            char* end = nullptr;
            const long long value = std::strtoll(literal.c_str(), &end, 10);
            if (errno == 0 && end != nullptr && *end == '\0') return Value::integer(value);
        }
        return Value::number(std::strtod(literal.c_str(), nullptr));
    }

    bool parse_hex4(std::uint32_t& out) {
        if (pos_ + 4u > text_.size()) return false;
        out = 0;
        for (int i = 0; i < 4; ++i) {
            const char c = text_[pos_++];
            out <<= 4;
            if (c >= '0' && c <= '9') out |= static_cast<std::uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') out |= static_cast<std::uint32_t>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') out |= static_cast<std::uint32_t>(c - 'A' + 10);
            else return false;
        }
        return true;
    }

    bool parse_string(std::string& out) {
        if (at_end() || peek() != '"') return false;
        ++pos_;
        while (!at_end()) {
            const char c = text_[pos_++];
            if (c == '"') return true;
            if (c != '\\') {
                out += c;
                continue;
            }
            if (at_end()) return false;
            const char escape = text_[pos_++];
            switch (escape) {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u': {
                std::uint32_t code = 0;
                if (!parse_hex4(code)) return false;
                if (code >= 0xD800u && code < 0xDC00u) {
                    // A high surrogate: its low half follows as another \u escape.
                    std::uint32_t low = 0;
                    if (!consume("\\u") || !parse_hex4(low) || low < 0xDC00u || low > 0xDFFFu) return false;
                    code = 0x10000u + ((code - 0xD800u) << 10) + (low - 0xDC00u);
                } else if (code >= 0xDC00u && code <= 0xDFFFu) {
                    return false;
                }
                append_utf8(out, code);
                break;
            }
            default: return false;
            }
        }
        return false;
    }

    std::optional<Value> parse_array(int depth) {
        ++pos_;  // [
        Value array = Value::array();
        skip_space();
        if (!at_end() && peek() == ']') {
            ++pos_;
            return array;
        }
        for (;;) {
            skip_space();
            std::optional<Value> item = parse_value(depth + 1);
            if (!item) return std::nullopt;
            array.push(std::move(*item));
            skip_space();
            if (at_end()) return std::nullopt;
            if (peek() == ',') {
                ++pos_;
                continue;
            }
            if (peek() == ']') {
                ++pos_;
                return array;
            }
            return std::nullopt;
        }
    }

    std::optional<Value> parse_object(int depth) {
        ++pos_;  // {
        Value object = Value::object();
        skip_space();
        if (!at_end() && peek() == '}') {
            ++pos_;
            return object;
        }
        for (;;) {
            skip_space();
            std::string key;
            if (!parse_string(key)) return std::nullopt;
            skip_space();
            if (at_end() || peek() != ':') return std::nullopt;
            ++pos_;
            skip_space();
            std::optional<Value> member = parse_value(depth + 1);
            if (!member) return std::nullopt;
            object.set(std::move(key), std::move(*member));
            skip_space();
            if (at_end()) return std::nullopt;
            if (peek() == ',') {
                ++pos_;
                continue;
            }
            if (peek() == '}') {
                ++pos_;
                return object;
            }
            return std::nullopt;
        }
    }
};

}  // namespace

Value Value::boolean(bool value) {
    Value v;
    v.type_ = Type::Bool;
    v.bool_ = value;
    return v;
}

Value Value::integer(std::int64_t value) {
    Value v;
    v.type_ = Type::Number;
    v.is_integer_ = true;
    v.integer_ = value;
    v.number_ = static_cast<double>(value);
    return v;
}

Value Value::number(double value) {
    Value v;
    v.type_ = Type::Number;
    v.number_ = value;
    if (std::isfinite(value) && value == std::floor(value) && std::fabs(value) < 9.0e15) {
        v.is_integer_ = true;
        v.integer_ = static_cast<std::int64_t>(value);
    }
    return v;
}

Value Value::string(std::string value) {
    Value v;
    v.type_ = Type::String;
    v.string_ = std::move(value);
    return v;
}

Value Value::array() {
    Value v;
    v.type_ = Type::Array;
    return v;
}

Value Value::object() {
    Value v;
    v.type_ = Type::Object;
    return v;
}

bool Value::as_bool(bool fallback) const noexcept {
    return type_ == Type::Bool ? bool_ : fallback;
}

std::int64_t Value::as_int(std::int64_t fallback) const noexcept {
    if (type_ != Type::Number) return fallback;
    if (is_integer_) return integer_;
    if (!std::isfinite(number_) || std::fabs(number_) >= 9.2e18) return fallback;
    return static_cast<std::int64_t>(number_);
}

double Value::as_double(double fallback) const noexcept {
    return type_ == Type::Number ? number_ : fallback;
}

const std::string& Value::as_string() const noexcept {
    return type_ == Type::String ? string_ : kEmptyString;
}

const std::vector<Value>& Value::items() const noexcept {
    return type_ == Type::Array ? items_ : kNoItems;
}

const std::vector<std::pair<std::string, Value>>& Value::members() const noexcept {
    return type_ == Type::Object ? members_ : kNoMembers;
}

const Value* Value::find(std::string_view key) const noexcept {
    if (type_ != Type::Object) return nullptr;
    for (auto it = members_.rbegin(); it != members_.rend(); ++it) {
        if (it->first == key) return &it->second;
    }
    return nullptr;
}

Value& Value::push(Value value) {
    if (type_ == Type::Null) type_ = Type::Array;
    items_.push_back(std::move(value));
    return items_.back();
}

Value& Value::set(std::string key, Value value) {
    if (type_ == Type::Null) type_ = Type::Object;
    members_.emplace_back(std::move(key), std::move(value));
    return members_.back().second;
}

void Value::dump_to(std::string& out) const {
    switch (type_) {
    case Type::Null: out += "null"; break;
    case Type::Bool: out += bool_ ? "true" : "false"; break;
    case Type::Number: {
        char buffer[40];
        if (is_integer_) {
            std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(integer_));
            out += buffer;
        } else if (!std::isfinite(number_)) {
            out += "null";
        } else {
            std::snprintf(buffer, sizeof(buffer), "%.17g", number_);
            out += buffer;
        }
        break;
    }
    case Type::String: out += quote(string_); break;
    case Type::Array: {
        out += '[';
        bool first = true;
        for (const Value& item : items_) {
            if (!first) out += ',';
            first = false;
            item.dump_to(out);
        }
        out += ']';
        break;
    }
    case Type::Object: {
        out += '{';
        bool first = true;
        for (const auto& [key, member] : members_) {
            if (!first) out += ',';
            first = false;
            out += quote(key);
            out += ':';
            member.dump_to(out);
        }
        out += '}';
        break;
    }
    }
}

std::string Value::dump() const {
    std::string out;
    dump_to(out);
    return out;
}

std::optional<Value> parse(std::string_view text) {
    return Parser(text).run();
}

std::string quote(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 2u);
    out += '"';
    for (const char c : text) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20u) {
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "\\u%04x", static_cast<unsigned>(static_cast<unsigned char>(c)));
                out += buffer;
            } else {
                out += c;
            }
        }
    }
    out += '"';
    return out;
}

}  // namespace galaxy::json
