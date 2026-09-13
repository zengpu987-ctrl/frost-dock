#pragma once

#include <cctype>
#include <cstdlib>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace json {

struct Value {
    enum Type { Null, Bool, Number, String, Arr, Obj } type = Null;
    bool boolean = false;
    double number = 0;
    std::string string;
    std::vector<Value> array;
    std::map<std::string, Value> object;

    const Value* find(const std::string& key) const {
        auto it = object.find(key);
        return it == object.end() ? nullptr : &it->second;
    }

    std::string getString(const char* key, const std::string& def = "") const {
        const Value* v = find(key);
        return (v && v->type == String) ? v->string : def;
    }

    double getNumber(const char* key, double def = 0) const {
        const Value* v = find(key);
        return (v && v->type == Number) ? v->number : def;
    }

    bool getBool(const char* key, bool def = false) const {
        const Value* v = find(key);
        return (v && v->type == Bool) ? v->boolean : def;
    }
};

class Parser {
public:
    explicit Parser(const std::string& text) : s_(text), i_(0) {}

    Value parse() {
        Value v = parseValue();
        skipWs();
        if (i_ != s_.size()) {
            throw std::runtime_error("trailing data after JSON value");
        }
        return v;
    }

private:
    const std::string& s_;
    std::size_t i_;

    void skipWs() {
        while (i_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[i_]))) {
            i_++;
        }
    }

    char peek() const { return i_ < s_.size() ? s_[i_] : '\0'; }
    char take() { return s_[i_++]; }

    void expect(char c) {
        skipWs();
        if (peek() != c) {
            throw std::runtime_error("unexpected character in JSON");
        }
        i_++;
    }

    Value parseValue() {
        skipWs();
        char c = peek();
        if (c == '{') return parseObject();
        if (c == '[') return parseArray();
        if (c == '"') {
            Value v;
            v.type = Value::String;
            v.string = parseString();
            return v;
        }
        if (c == 't') return parseLiteral("true", Value::Bool, true);
        if (c == 'f') return parseLiteral("false", Value::Bool, false);
        if (c == 'n') return parseLiteral("null", Value::Null, false);
        return parseNumber();
    }

    Value parseLiteral(const char* word, Value::Type type, bool boolean) {
        for (const char* p = word; *p; p++) {
            if (peek() != *p) {
                throw std::runtime_error("invalid JSON literal");
            }
            i_++;
        }
        Value v;
        v.type = type;
        v.boolean = boolean;
        return v;
    }

    Value parseNumber() {
        std::size_t start = i_;
        if (peek() == '-') i_++;
        while (std::isdigit(static_cast<unsigned char>(peek()))) i_++;
        if (peek() == '.') {
            i_++;
            while (std::isdigit(static_cast<unsigned char>(peek()))) i_++;
        }
        if (peek() == 'e' || peek() == 'E') {
            i_++;
            if (peek() == '+' || peek() == '-') i_++;
            while (std::isdigit(static_cast<unsigned char>(peek()))) i_++;
        }
        Value v;
        v.type = Value::Number;
        v.number = std::strtod(s_.substr(start, i_ - start).c_str(), nullptr);
        return v;
    }

    std::string parseString() {
        expect('"');
        std::string out;
        while (true) {
            char c = take();
            if (c == '"') break;
            if (c == '\\') {
                char e = take();
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'u': {
                        // Decode a single UTF-16 code unit (BMP only, enough for config paths).
                        if (i_ + 4 > s_.size()) {
                            throw std::runtime_error("bad unicode escape");
                        }
                        unsigned code = 0;
                        for (int k = 0; k < 4; k++) {
                            char h = take();
                            code <<= 4;
                            if (h >= '0' && h <= '9') code |= h - '0';
                            else if (h >= 'a' && h <= 'f') code |= h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') code |= h - 'A' + 10;
                            else throw std::runtime_error("bad hex escape");
                        }
                        if (code < 0x80) {
                            out += static_cast<char>(code);
                        } else if (code < 0x800) {
                            out += static_cast<char>(0xC0 | (code >> 6));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            out += static_cast<char>(0xE0 | (code >> 12));
                            out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            out += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: throw std::runtime_error("bad escape");
                }
            } else {
                out += c;
            }
        }
        return out;
    }

    Value parseArray() {
        expect('[');
        Value v;
        v.type = Value::Arr;
        skipWs();
        if (peek() == ']') {
            i_++;
            return v;
        }
        while (true) {
            v.array.push_back(parseValue());
            skipWs();
            char c = take();
            if (c == ']') break;
            if (c != ',') throw std::runtime_error("expected ',' or ']'");
        }
        return v;
    }

    Value parseObject() {
        expect('{');
        Value v;
        v.type = Value::Obj;
        skipWs();
        if (peek() == '}') {
            i_++;
            return v;
        }
        while (true) {
            skipWs();
            if (peek() != '"') throw std::runtime_error("expected object key");
            std::string key = parseString();
            expect(':');
            v.object[key] = parseValue();
            skipWs();
            char c = take();
            if (c == '}') break;
            if (c != ',') throw std::runtime_error("expected ',' or '}'");
        }
        return v;
    }
};

inline Value parse(const std::string& text) { return Parser(text).parse(); }

} // namespace json
