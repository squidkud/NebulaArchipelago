// The small JSON reader and writer (galaxy/json_lite.h).
#include "galaxy/json_lite.h"

#include <cstdio>
#include <string>

namespace {
int s_failures = 0;
void expect(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++s_failures;
    }
}
}  // namespace

int main() {
    namespace json = galaxy::json;

    {
        const auto v = json::parse(R"( {"cmd":"Connected","slot":3,"ok":true,"none":null,"list":[1,2,3],"nested":{"a":-5,"b":1.5}} )");
        expect(v.has_value() && v->is_object(), "an object parses");
        expect(v->find("cmd") != nullptr && v->find("cmd")->as_string() == "Connected", "a string member");
        expect(v->find("slot")->as_int() == 3, "an integer member");
        expect(v->find("ok")->as_bool(), "a boolean member");
        expect(v->find("none")->is_null(), "a null member");
        expect(v->find("list")->items().size() == 3u && v->find("list")->items()[2].as_int() == 3, "an array member");
        expect(v->find("nested")->find("a")->as_int() == -5, "a negative integer");
        expect(v->find("nested")->find("b")->as_double() == 1.5, "a fraction");
        expect(v->find("missing") == nullptr, "an absent member is null");
        expect(v->find("cmd")->as_int(7) == 7 && v->find("slot")->as_string().empty(), "other types give the fallback");
    }
    {
        // Ids of the size Archipelago uses stay exact.
        const auto v = json::parse("[1397508865, 9007199254740991]");
        expect(v && v->items()[0].as_int() == 1397508865LL, "an item id");
        expect(v && v->items()[1].as_int() == 9007199254740991LL, "the largest exact id");
    }
    {
        const auto v = json::parse(R"(["a\"b\\c\n\u00e9\u20ac\ud83d\ude00"])");
        expect(v && v->items()[0].as_string() == "a\"b\\c\n\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80", "escapes and surrogate pairs");
    }
    expect(!json::parse("").has_value(), "empty text is refused");
    expect(!json::parse("{").has_value(), "an unclosed object is refused");
    expect(!json::parse("[1,]").has_value(), "a trailing comma is refused");
    expect(!json::parse("{\"a\":1} x").has_value(), "text after the document is refused");
    expect(!json::parse("\"\\ud83d\"").has_value(), "a lone surrogate is refused");
    expect(!json::parse("tru").has_value(), "a broken literal is refused");
    expect(!json::parse("-").has_value(), "a bare sign is refused");
    expect(json::parse("  [ ]  ").has_value(), "white space around an empty array");
    {
        std::string deep(100, '[');
        deep += std::string(100, ']');
        expect(!json::parse(deep).has_value(), "nesting past 64 levels is refused");
        std::string ok(60, '[');
        ok += std::string(60, ']');
        expect(json::parse(ok).has_value(), "nesting to 60 levels is read");
    }
    {
        json::Value message = json::Value::object();
        message.set("cmd", json::Value::string("LocationChecks"));
        json::Value ids = json::Value::array();
        ids.push(json::Value::integer(1397508865));
        ids.push(json::Value::integer(1397508866));
        message.set("locations", std::move(ids));
        message.set("note", json::Value::string("a \"quoted\"\ttab"));
        message.set("flag", json::Value::boolean(false));
        message.set("half", json::Value::number(0.5));
        message.set("empty", json::Value());
        const std::string text = message.dump();
        expect(text == R"({"cmd":"LocationChecks","locations":[1397508865,1397508866],"note":"a \"quoted\"\ttab","flag":false,"half":0.5,"empty":null})",
               "the writer's output");
        const auto back = json::parse(text);
        expect(back && back->dump() == text, "what is written reads back the same");
    }
    expect(json::quote("a\x01") == "\"a\\u0001\"", "a control character is escaped");
    expect(json::Value::number(3.0).dump() == "3", "a whole double writes as an integer");

    if (s_failures == 0) std::printf("json_lite: all tests passed\n");
    return s_failures == 0 ? 0 : 1;
}
