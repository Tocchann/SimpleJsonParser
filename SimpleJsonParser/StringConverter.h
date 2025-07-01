#pragma once

#include <string>
#include <string_view>

namespace StringConverter
{
/// <summary>
/// UNICODE��������w�肳�ꂽ�R�[�h�y�[�W�̕�����ɕϊ�����B
/// </summary>
/// <param name="str">�ϊ������� UNICODE ������</param>
/// <param name="codePage">�ϊ���̃R�[�h�y�[�W</param>
/// <returns></returns>
std::string __stdcall ToString( const std::wstring_view& str, UINT codePage );
inline std::string ToString( LPCWSTR str, UINT codePage )
{
	return ToString( std::wstring_view( str ), codePage );
}
/// <summary>
/// UNICODE �������UTF8�R�[�h�y�[�W�̕�����ɕϊ�����ϊ�����B
/// </summary>
/// <param name="str">�ϊ������� UNICODE ������</param>
/// <param name="codePage">�ϊ���̃R�[�h�y�[�W</param>
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
/// �w�肳�ꂽ�R�[�h�y�[�W�̕������ UNICODE ������ɕϊ�����B
/// </summary>
/// <param name="str">�ϊ�������������</param>
/// <param name="codePage">�ϊ�������������̃R�[�h�y�[�W</param>
/// <returns></returns>
std::wstring __stdcall ToWString( const std::string_view& u8str, UINT codePage = CP_UTF8 );
inline std::wstring ToWString( LPCSTR str, UINT codePage = CP_UTF8 )
{
	return ToWString( std::string_view( str ), codePage );
}
/// <summary>
/// IsUTF8BOM, IsUtf16LEBOM, IsUtf16BEBOM ���f�[�^��ǂݎ�����ۂ�BOM���X�L�b�v����K�v�����邩�̃`�F�b�N�֐�
/// IsUTF16BEBOM �̏ꍇ�́A�����R�[�h�̏�ʂƉ��ʂ����ւ���K�v������̂Œ��ӂ��K�v
/// </summary>
/// <param name="str">�`�F�b�N������</param>
/// <returns>Byte Order Mark �Ŏn�܂��Ă��邩�H</returns>
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
