#include <string_view>
#include <string>
#include <map>
#include <vector>
#include <any>
#include <functional>

#include <ppl.h>
namespace Morrin::JSON
{
enum class NotificationType
{
	StartParse,
	EndParse,

	StartElement,
	EndElement,

	StartObject,
	EndObject,
	
	StartArray,
	EndArray,

	Key,
	String,
	Number,
	BooleanTrue,
	BooleanFalse,
	Null,
};
using ParseCallBack = std::function<bool( NotificationType type, const std::string_view& value )> const;

using JsonKeyType = std::string;	// キーは UTF8 のまま
using JsonObject = std::map<JsonKeyType, std::any>;
using JsonArray = std::vector<std::any>;	// 配列は std::any で値を保持する

bool __stdcall ParseJSON( const std::string_view& jsonText, ParseCallBack&& proc );
std::any __stdcall ParseJSON( const std::string_view& jsonText, const concurrency::cancellation_token& token = concurrency::cancellation_token::none() );
}
