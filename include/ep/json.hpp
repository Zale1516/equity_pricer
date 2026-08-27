#pragma once
#include <string>
#include <vector>
#include <map>

namespace ep::json {

class Value {
public:
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    bool               b = false;               // when type == Bool
    double             num = 0.0;               // when type == Num
    std::string        str;                     // when type == Str
    std::vector<Value> arr;                     // when type == Arr
    std::map<std::string, Value> obj;           // when type == Obj

    bool has(const std::string& k) const;               // object contains key k
    const Value& at(const std::string& k) const;        // object member (throws if missing)
    double as_num() const;                              // numeric value (throws if not a number)
    const std::string& as_str() const;                  // string value (throws if not a string)
    const std::vector<Value>& as_arr() const;           // array value (throws if not an array)
};

Value parse(const std::string& text);   // parse a JSON string
Value load(const std::string& path);    // parse a JSON file

} // namespace ep::json
