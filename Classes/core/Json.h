// Small dynamic JSON value (mirrors Dart's Map<String,dynamic>/List<dynamic>) parsed and written with rapidjson.
// Object keys keep insertion order so request bodies are byte-stable for tests and move hashes.
#pragma once
#include <cstdint>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace nx {

class Json {
public:
    enum class Type { Null, Bool, Int, Double, String, Array, Object };
    using Array = std::vector<Json>;
    using Object = std::vector<std::pair<std::string, Json>>;

    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool b) : type_(Type::Bool), b_(b) {}
    Json(int v) : type_(Type::Int), i_(v) {}
    Json(int64_t v) : type_(Type::Int), i_(v) {}
    Json(double v) : type_(Type::Double), d_(v) {}
    Json(const char* s) : type_(Type::String), s_(s) {}
    Json(std::string s) : type_(Type::String), s_(std::move(s)) {}
    Json(Array a) : type_(Type::Array), a_(std::make_shared<Array>(std::move(a))) {}
    Json(Object o) : type_(Type::Object), o_(std::make_shared<Object>(std::move(o))) {}

    static Json array() { return Json(Array{}); }
    static Json object() { return Json(Object{}); }
    static Json obj(std::initializer_list<std::pair<std::string, Json>> kv) {
        Json j = object();
        for (auto& p : kv) j.set(p.first, p.second);
        return j;
    }
    template <class T>
    static Json arr(const std::vector<T>& v) {
        Json j = array();
        for (auto& x : v) j.push(Json(x));
        return j;
    }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isBool() const { return type_ == Type::Bool; }
    bool isNumber() const { return type_ == Type::Int || type_ == Type::Double; }
    bool isInt() const { return type_ == Type::Int; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    bool asBool(bool def = false) const { return type_ == Type::Bool ? b_ : def; }
    int64_t asInt(int64_t def = 0) const {
        if (type_ == Type::Int) return i_;
        if (type_ == Type::Double) return static_cast<int64_t>(d_);
        return def;
    }
    double asDouble(double def = 0) const {
        if (type_ == Type::Double) return d_;
        if (type_ == Type::Int) return static_cast<double>(i_);
        return def;
    }
    const std::string& asString() const;
    /// Dart-style '$value' interpolation (null -> "null", numbers as text).
    std::string str() const;

    // Array
    size_t size() const;
    const Json& operator[](size_t i) const;
    const Json& operator[](int i) const { return (*this)[static_cast<size_t>(i < 0 ? size_t(-1) : size_t(i))]; }
    Json& push(Json v);
    const Array& items() const;

    // Object
    bool has(const std::string& k) const;
    const Json& operator[](const std::string& k) const;
    const Json& operator[](const char* k) const { return (*this)[std::string(k)]; }
    Json& set(const std::string& k, Json v);
    const Object& entries() const;

    std::string dump() const;
    static Json parse(const std::string& text, bool* ok = nullptr);

    bool operator==(const Json& o) const;
    bool operator!=(const Json& o) const { return !(*this == o); }

private:
    Type type_ = Type::Null;
    bool b_ = false;
    int64_t i_ = 0;
    double d_ = 0;
    std::string s_;
    std::shared_ptr<Array> a_;
    std::shared_ptr<Object> o_;
    void detach();
};

}  // namespace nx
