#include "core/Json.h"

#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <cmath>
#include <cstdio>

namespace nx {

namespace {
const Json kNull;
const std::string kEmpty;
const Json::Array kEmptyArr;
const Json::Object kEmptyObj;

Json fromRapid(const rapidjson::Value& v) {
    if (v.IsNull()) return Json();
    if (v.IsBool()) return Json(v.GetBool());
    if (v.IsInt64()) return Json(static_cast<int64_t>(v.GetInt64()));
    if (v.IsUint64()) return Json(static_cast<double>(v.GetUint64()));
    if (v.IsNumber()) return Json(v.GetDouble());
    if (v.IsString()) return Json(std::string(v.GetString(), v.GetStringLength()));
    if (v.IsArray()) {
        Json a = Json::array();
        for (auto& e : v.GetArray()) a.push(fromRapid(e));
        return a;
    }
    Json o = Json::object();
    for (auto& m : v.GetObject()) o.set(std::string(m.name.GetString(), m.name.GetStringLength()), fromRapid(m.value));
    return o;
}

void write(const Json& j, rapidjson::Writer<rapidjson::StringBuffer>& w) {
    switch (j.type()) {
        case Json::Type::Null: w.Null(); break;
        case Json::Type::Bool: w.Bool(j.asBool()); break;
        case Json::Type::Int: w.Int64(j.asInt()); break;
        case Json::Type::Double: {
            double d = j.asDouble();
            if (std::isfinite(d)) w.Double(d); else w.Null();
            break;
        }
        case Json::Type::String: w.String(j.asString().c_str(), static_cast<rapidjson::SizeType>(j.asString().size())); break;
        case Json::Type::Array:
            w.StartArray();
            for (auto& e : j.items()) write(e, w);
            w.EndArray();
            break;
        case Json::Type::Object:
            w.StartObject();
            for (auto& kv : j.entries()) {
                w.Key(kv.first.c_str(), static_cast<rapidjson::SizeType>(kv.first.size()));
                write(kv.second, w);
            }
            w.EndObject();
            break;
    }
}
}  // namespace

const std::string& Json::asString() const { return type_ == Type::String ? s_ : kEmpty; }

std::string Json::str() const {
    switch (type_) {
        case Type::Null: return "null";
        case Type::Bool: return b_ ? "true" : "false";
        case Type::Int: return std::to_string(i_);
        case Type::String: return s_;
        case Type::Double: {
            if (std::floor(d_) == d_ && std::fabs(d_) < 1e15) {
                char b[64];
                std::snprintf(b, sizeof b, "%.1f", d_);
                return b;
            }
            char b[64];
            std::snprintf(b, sizeof b, "%.17g", d_);
            return b;
        }
        default: return dump();
    }
}

size_t Json::size() const {
    if (type_ == Type::Array) return a_->size();
    if (type_ == Type::Object) return o_->size();
    return 0;
}
const Json& Json::operator[](size_t i) const {
    if (type_ != Type::Array || i >= a_->size()) return kNull;
    return (*a_)[i];
}
void Json::detach() {
    if (a_ && a_.use_count() > 1) a_ = std::make_shared<Array>(*a_);
    if (o_ && o_.use_count() > 1) o_ = std::make_shared<Object>(*o_);
}
Json& Json::push(Json v) {
    if (type_ != Type::Array) { *this = array(); }
    detach();
    a_->push_back(std::move(v));
    return *this;
}
const Json::Array& Json::items() const { return type_ == Type::Array ? *a_ : kEmptyArr; }
bool Json::has(const std::string& k) const {
    if (type_ != Type::Object) return false;
    for (auto& kv : *o_) if (kv.first == k) return true;
    return false;
}
const Json& Json::operator[](const std::string& k) const {
    if (type_ != Type::Object) return kNull;
    for (auto& kv : *o_) if (kv.first == k) return kv.second;
    return kNull;
}
Json& Json::set(const std::string& k, Json v) {
    if (type_ != Type::Object) { *this = object(); }
    detach();
    for (auto& kv : *o_) {
        if (kv.first == k) { kv.second = std::move(v); return *this; }
    }
    o_->emplace_back(k, std::move(v));
    return *this;
}
const Json::Object& Json::entries() const { return type_ == Type::Object ? *o_ : kEmptyObj; }

std::string Json::dump() const {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> w(sb);
    write(*this, w);
    return std::string(sb.GetString(), sb.GetSize());
}

Json Json::parse(const std::string& text, bool* ok) {
    rapidjson::Document d;
    d.Parse(text.c_str(), text.size());
    if (d.HasParseError()) {
        if (ok) *ok = false;
        return Json();
    }
    if (ok) *ok = true;
    return fromRapid(d);
}

bool Json::operator==(const Json& o) const {
    if (isNumber() && o.isNumber()) return asDouble() == o.asDouble();
    if (type_ != o.type_) return false;
    switch (type_) {
        case Type::Null: return true;
        case Type::Bool: return b_ == o.b_;
        case Type::String: return s_ == o.s_;
        case Type::Array: return *a_ == *o.a_;
        case Type::Object: {
            if (o_->size() != o.o_->size()) return false;
            for (auto& kv : *o_) {
                if (!o.has(kv.first) || !(kv.second == o[kv.first])) return false;
            }
            return true;
        }
        default: return false;
    }
}

}  // namespace nx
