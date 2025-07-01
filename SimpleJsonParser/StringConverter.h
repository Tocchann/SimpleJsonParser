#pragma once

#include <string>
#include <string_view>

namespace StringConverter
{
/// <summary>
/// UNICODE文字列を指定されたコードページの文字列に変換する。
/// </summary>
/// <param name="str">変換したい UNICODE 文字列</param>
/// <param name="codePage">変換後のコードページ</param>
/// <returns></returns>
std::string __stdcall ToString( const std::wstring_view& str, UINT codePage );
inline std::string ToString( LPCWSTR str, UINT codePage )
{
	return ToString( std::wstring_view( str ), codePage );
}
/// <summary>
/// UNICODE 文字列をUTF8コードページの文字列に変換する変換する。
/// </summary>
/// <param name="str">変換したい UNICODE 文字列</param>
/// <param name="codePage">変換後のコードページ</param>
/// <returns></returns>
inline std::string ToUtf8( const std::wstring_view& str )
{
	return ToString( str, CP_UTF8 );
}
inline std::string ToUtf8( LPCWSTR str )
{
	return ToString( std::wstring_view( str ), CP_UTF8 );
}
inline std::string ToAnsi( const std::wstring_view& str )
{
	return ToString( str, CP_ACP );
}
inline std::string ToAnsi( LPCWSTR str )
{
	return ToString( std::wstring_view( str ), CP_ACP );
}

/// <summary>
/// 指定されたコードページの文字列を UNICODE 文字列に変換する。
/// </summary>
/// <param name="str">変換したい文字列</param>
/// <param name="codePage">変換したい文字列のコードページ</param>
/// <returns></returns>
std::wstring __stdcall ToWString( const std::string_view& u8str, UINT codePage = CP_UTF8 );
inline std::wstring ToWString( LPCSTR str, UINT codePage = CP_UTF8 )
{
	return ToWString( std::string_view( str ), codePage );
}
/// <summary>
/// IsUTF8BOM, IsUtf16LEBOM, IsUtf16BEBOM 生データを読み取った際のBOMをスキップする必要があるかのチェック関数
/// IsUTF16BEBOM の場合は、文字コードの上位と下位を入れ替える必要があるので注意が必要
/// </summary>
/// <param name="str">チェック文字列</param>
/// <returns>Byte Order Mark で始まっているか？</returns>
inline bool IsUtf8BOM( const std::string_view& str )
{
	return str.size() >= 3 && str[0] == '\xEF' && str[1] == '\xBB' && str[2] == '\xBF';
}
inline bool IsUtf16LEBOM( const std::wstring_view& str )
{
	return str.size() >= 1 && str[0] == L'\xFFFE';
}
inline bool IsUtf16BEBOM( const std::wstring_view& str )
{
	return str.size() >= 1 && str[0] == L'\xFEFF';
}
}
