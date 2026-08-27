#include "ep/json.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>

namespace ep::json {

bool Value::has(const std::string& k) const { return type == Obj && obj.count(k) > 0; }

const Value& Value::at(const std::string& k) const {
    if (type != Obj) throw std::runtime_error("json: not an object for key '" + k + "'");
    auto it = obj.find(k);
    if (it == obj.end()) throw std::runtime_error("json: missing key '" + k + "'");
    return it->second;
}

double Value::as_num() const { if (type != Num) throw std::runtime_error("json: not a number"); return num; }
const std::string& Value::as_str() const { if (type != Str) throw std::runtime_error("json: not a string"); return str; }
const std::vector<Value>& Value::as_arr() const { if (type != Arr) throw std::runtime_error("json: not an array"); return arr; }

namespace {

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}
    Value parse() { skip(); Value v = value(); skip(); return v; }
private:
    const std::string& s_; size_t i_ = 0;
    [[noreturn]] void err(const std::string& m) { throw std::runtime_error("json parse error @" + std::to_string(i_) + ": " + m); }
    void skip() { while (i_ < s_.size() && (s_[i_]==' '||s_[i_]=='\t'||s_[i_]=='\n'||s_[i_]=='\r')) ++i_; }
    char peek() { return i_ < s_.size() ? s_[i_] : '\0'; }

    Value value() {
        skip();
        char c = peek();
        if (c == '{') return object();
        if (c == '[') return array();
        if (c == '"') { Value v; v.type = Value::Str; v.str = string(); return v; }
        if (c == 't' || c == 'f') return boolean();
        if (c == 'n') { expect("null"); return Value{}; }
        return number();
    }
    void expect(const char* lit) { for (const char* p = lit; *p; ++p) { if (peek() != *p) err(std::string("expected ") + lit); ++i_; } }
    Value boolean() { Value v; v.type = Value::Bool; if (peek()=='t'){expect("true");v.b=true;} else {expect("false");v.b=false;} return v; }

    Value number() {
        size_t start = i_;
        while (i_ < s_.size()) { char c = s_[i_]; if ((c>='0'&&c<='9')||c=='-'||c=='+'||c=='.'||c=='e'||c=='E') ++i_; else break; }
        if (i_ == start) err("bad number");
        Value v; v.type = Value::Num; v.num = std::strtod(s_.substr(start, i_-start).c_str(), nullptr); return v;
    }
    std::string string() {
        if (peek() != '"') err("expected string"); ++i_;
        std::string out;
        while (i_ < s_.size()) {
            char c = s_[i_++];
            if (c == '"') return out;
            if (c == '\\') {
                char e = s_[i_++];
                switch (e) {
                    case '"': out += '"'; break;  case '\\': out += '\\'; break; case '/': out += '/'; break;
                    case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break; case 'f': out += '\f'; break;
                    case 'u': i_ += 4; out += '?'; break;
                    default: out += e; break;
                }
            } else out += c;
        }
        err("unterminated string");
    }
    Value array() {
        Value v; v.type = Value::Arr; ++i_; skip();
        if (peek() == ']') { ++i_; return v; }
        while (true) { v.arr.emplace_back(value()); skip(); char c = peek(); if (c==',') {++i_; skip(); continue;} if (c==']'){++i_; break;} err("expected , or ]"); }
        return v;
    }
    Value object() {
        Value v; v.type = Value::Obj; ++i_; skip();
        if (peek() == '}') { ++i_; return v; }
        while (true) {
            skip(); std::string k = string(); skip();
            if (peek() != ':') err("expected :"); ++i_;
            v.obj[k] = value(); skip();
            char c = peek(); if (c==',') {++i_; continue;} if (c=='}'){++i_; break;} err("expected , or }");
        }
        return v;
    }
};

} // namespace

Value parse(const std::string& text) { return Parser(text).parse(); }

Value load(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open json: " + path);
    std::stringstream ss; ss << f.rdbuf();
    return parse(ss.str());
}

} // namespace ep::json
