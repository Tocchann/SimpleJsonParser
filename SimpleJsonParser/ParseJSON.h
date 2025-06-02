// �Ȉ�JSON�p�[�T�[

//---------------
//	Include Files
//---------------
#include <string_view>
#include <string>
#include <map>
#include <vector>
#include <any>
#include <functional>

#include <ppl.h>

namespace Morrin::JSON
{

//---------------
//	defines
//---------------
enum class NotificationType
{
	StartParse,
	EndParse,

	StartObject = '{',
	EndObject = '}',
	
	StartArray = '[',
	EndArray = ']',

	Key = ':',
	String = '"',
	Number = '0',
	BooleanTrue = 't',
	BooleanFalse = 'f',
	Null = 'n',
};



using ParseCallBack = std::function<bool( NotificationType type, const std::string_view& value )> const;

using JsonRefKeyType = std::string_view;	// �L�[�� UTF8 �̂܂�
using JsonKeyType = std::string;	// �L�[�� UTF8 �̂܂�
using JsonObject = std::map<JsonKeyType, std::any>;
using JsonArray = std::vector<std::any>;	// �z��� std::any �Œl��ێ�����
using JsonString = std::wstring;	// ������f�[�^�́A���K�����Ă���
//---------------
//	prottype
//---------------
bool __stdcall ParseJSON( const std::string_view& jsonText, const ParseCallBack& proc );

// �\�����f�[�^�Ƃ��ăp�[�X����f�[�^�́A�i�����\�Ƃ���B
std::any __stdcall ParseJSON( const std::string_view& jsonText, const concurrency::cancellation_token& token = concurrency::cancellation_token::none() );
std::any APIENTRY GetValue( std::any obj, const JsonRefKeyType& searchKey );

//	string_view ���Q�Ƃ��Ă���e�L�X�g���A���G�X�P�[�v����
std::string __stdcall UnEscapeToString( const std::string_view& value );
std::wstring __stdcall UnEscapeToWstring( const std::string_view& value );

// JsonObject �ł́A���l�� int �݂̂Ƃ���
int __stdcall ConvertToInt( const std::string_view& value );

// UTF-8 �ւ̕ϊ�
std::string __stdcall ToString( const std::wstring_view& str, UINT codePage );
inline std::string __stdcall ToString( LPCWSTR str, UINT codePage )
{
	return ToString( std::wstring_view( str ), codePage );
}
inline std::string __stdcall ToUtf8( const std::wstring_view& str )
{
	return ToString( str, CP_UTF8 );
}
inline std::string ToUtf8( LPCWSTR str )
{
	return ToString( std::wstring_view( str ), CP_UTF8 );
}
std::wstring __stdcall ToWString( const std::string_view& u8str, UINT codePage = CP_UTF8 );
inline std::wstring ToWString( LPCSTR str, UINT codePage = CP_UTF8 )
{
	return ToWString( std::string_view( str ), codePage );
}

}
