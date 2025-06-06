//=============================================================================
//
//			タイトル：	速度優先型デコード専用JSONパーサー C++20版
//
//			　担当者：	高萩 俊行
//
//=============================================================================
// 説明・諸注意
//	JSONデータの書式の規定は http://www.json.org/json-ja.html を参照(2018/11/05時点の内容に準拠)
//	データが正しくない等、何らかの理由でパースに失敗した場合は、std::exception(派生クラス)例外が発生する
#pragma once
#if !_HAS_CXX17
#error "JSONパーサーを利用する場合は、コンパイラオプションをC++17以上に設定してください(推奨 C++20)。"
#endif
//---------------
//	Include Files
//---------------
#include <any>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include <ppl.h>

namespace Morrin::JSON
{
//---------------
//	defines
//---------------
/// <summary>
/// JSONの通知タイプを定義する列挙型
/// </summary>
/// remarks
/// キャストして使えるようにするため、リテラルとマップする形に変更
enum class NotificationType : char
{
	StartParse,
	EndParse,

	Key,
	String,
	Number,
	BooleanTrue,
	BooleanFalse,
	Null,

	StartObject = '{',
	EndObject = '}',
	
	StartArray = '[',
	EndArray = ']',
};
// そもそもが値として不整合が出ていないこと
static_assert(NotificationType::StartObject > NotificationType::Null );

//	パース結果のコールバック受け取りメソッド( bool func( Morrin::JSON::NotificationType type, const std::string_view& value ) )
using ParseCallBack = std::function<bool( NotificationType type, const std::string_view& value )> const;

// std::any に格納されるデータ定義

// 検索などの参照用データ
using JsonRefKeyType = std::string_view;


#ifdef FORCE_REFERENCE_VALUE
using JsonKeyType = JsonRefKeyType;
using JsonString = JsonRefKeyType;	// そのまま参照するので、変換処理も施さない
#else
using JsonKeyType = std::string;
using JsonString = std::wstring;	// 文字列データは、正規化しておく
#endif
using JsonObject = std::map<JsonKeyType, std::any>;
using JsonArray = std::vector<std::any>;	// 配列は std::any で値を保持する
//---------------
//	prottype
//---------------
bool __stdcall ParseJSON( const std::string_view& jsonText, const ParseCallBack& proc );

// 構造化データとしてパースするデータは、永続化可能とする。
std::any __stdcall ParseJSON( const std::string_view& jsonText, const concurrency::cancellation_token& token = concurrency::cancellation_token::none() );

// std::any 形式でリターンしたJSONデータから直接値を取得する
// searchKey は、'/' で区切ってオブジェクトのキーを指定する。配列の場合はインデックスを直接記述する
// Ex) "key1/key2/0/key3/4"
std::any __stdcall GetValue( std::any obj, const JsonRefKeyType& searchKey );

//	string_view が参照しているテキストをアンエスケープする string の場合の文字コードはUTF8になる
std::string __stdcall UnEscapeToString( const std::string_view& value );
std::wstring __stdcall UnEscapeToWstring( const std::string_view& value );

// JsonObject では、数値は int のみとする
int __stdcall ConvertToInt( const std::string_view& value );

// 文字コードの変換(UNICODE->任意コードページ)
std::string __stdcall ToString( const std::wstring_view& str, UINT codePage );
inline std::string __stdcall ToString( LPCWSTR str, UINT codePage )
{
	return ToString( std::wstring_view( str ), codePage );
}
// UTF8への変換
inline std::string __stdcall ToUtf8( const std::wstring_view& str )
{
	return ToString( str, CP_UTF8 );
}
inline std::string ToUtf8( LPCWSTR str )
{
	return ToString( std::wstring_view( str ), CP_UTF8 );
}
// UNICODE から任意コードページに変換する
std::wstring __stdcall ToWString( const std::string_view& u8str, UINT codePage = CP_UTF8 );
inline std::wstring ToWString( LPCSTR str, UINT codePage = CP_UTF8 )
{
	return ToWString( std::string_view( str ), codePage );
}
}
