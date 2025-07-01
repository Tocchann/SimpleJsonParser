#include "stdafx.h"
#include "StringConverter.h"

using namespace std::string_view_literals;
using namespace std::string_literals;

namespace StringConverter
{
/// <summary>
/// UNICODE文字列を指定されたコードページの文字列に変換する。
/// </summary>
/// <param name="str">変換したい UNICODE 文字列</param>
/// <param name="codePage">変換後のコードページ</param>
/// <returns></returns>
std::string __stdcall ToString( const std::wstring_view& str, UINT codePage )
{
	if( str.empty() )
	{
		return ""s;
	}
	int needLength = WideCharToMultiByte( codePage, 0, str.data(), static_cast<int>(str.size()), nullptr, 0, nullptr, nullptr );
	std::string	result;
	result.resize( needLength );
	WideCharToMultiByte( codePage, 0, str.data(), static_cast<int>(str.size()), result.data(), static_cast<int>(result.size()), nullptr, nullptr );
	return result;
}
/// <summary>
/// 指定されたコードページの文字列を UNICODE 文字列に変換する。
/// </summary>
/// <param name="str">変換したい文字列</param>
/// <param name="codePage">変換したい文字列のコードページ</param>
/// <returns></returns>
std::wstring __stdcall ToWString( const std::string_view& str, UINT codePage/*= CP_UTF8*/ )
{
	if( str.empty() )
	{
		return L""s;
	}
	auto u16len = MultiByteToWideChar( codePage, 0, str.data(), static_cast<int>(str.size()), nullptr, 0 );
	std::wstring result;
	result.resize( u16len );
	MultiByteToWideChar( codePage, 0, str.data(), static_cast<int>(str.size()), result.data(), static_cast<int>(result.size()) );
	return result;
}
}
