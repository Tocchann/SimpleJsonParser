#include "stdafx.h"
#include "StringConverter.h"

using namespace std::string_view_literals;
using namespace std::string_literals;

namespace StringConverter
{
/// <summary>
/// UNICODE��������w�肳�ꂽ�R�[�h�y�[�W�̕�����ɕϊ�����B
/// </summary>
/// <param name="str">�ϊ������� UNICODE ������</param>
/// <param name="codePage">�ϊ���̃R�[�h�y�[�W</param>
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
/// �w�肳�ꂽ�R�[�h�y�[�W�̕������ UNICODE ������ɕϊ�����B
/// </summary>
/// <param name="str">�ϊ�������������</param>
/// <param name="codePage">�ϊ�������������̃R�[�h�y�[�W</param>
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
