#pragma once
#include <string_view>
#include <functional>
#include <any>
#include <map>
#include <vector>
#include <string>
#include <variant>
#include <ppl.h>

namespace Morrin::JSON
{
class JsonValue;
using JsonObject = std::map<std::wstring, JsonValue>;
using JsonArray = std::vector<JsonValue>;
using JsonKeyType = std::wstring;

class JsonValue : public std::variant<std::wstring, double, int, bool, std::nullptr_t, JsonObject, JsonArray>
{
public:
    using variant::variant; // Enable construction from any type in the variant

    // Helper methods to check the type
    bool IsString() const { return std::holds_alternative<std::wstring>(*this); }
    bool IsNumber() const { return std::holds_alternative<double>( *this ); }
    bool IsInteger() const { return std::holds_alternative<int>(*this); }
    bool IsBoolean() const{ return std::holds_alternative<bool>( *this ); }
    bool IsNull() const{ return std::holds_alternative<std::nullptr_t>( *this ); }
    bool IsObject() const{ return std::holds_alternative<JsonObject>( *this ); }
    bool IsArray() const { return std::holds_alternative<JsonArray>( *this ); }

    template<typename Type> Type Get() const{ return std::get<Type>(*this); }

    std::wstring AsString() const { return Get<std::wstring>(); }
	double AsNumber() const { return Get<double>(); }
	int AsInteger() const { return Get<int>(); }
	bool AsBoolean() const { return Get<bool>(); }
    std::nullptr_t AsNull() const { return Get<std::nullptr_t>(); }
    JsonObject AsObject() const { return Get<JsonObject>(); }
    JsonArray AsArray() const { return Get<JsonArray>(); }
};

JsonValue __stdcall ParseJSON(const std::string_view& jsonText, const concurrency::cancellation_token& token = concurrency::cancellation_token::none());

class OperationCanceledException : public std::runtime_error
{
public:
    using _Mybase = runtime_error;

    explicit OperationCanceledException( const std::string& _Message ) : _Mybase( _Message.c_str() ) {}

    explicit OperationCanceledException( const char* _Message ) : _Mybase( _Message ) {}
};
}
