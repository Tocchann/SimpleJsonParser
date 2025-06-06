//=============================================================================
//
//			�^�C�g���F	���x�D��^�f�R�[�h��pJSON�p�[�T�[ C++20��
//
//			�@�S���ҁF	���� �r�s
//
//=============================================================================
// �����E������
//	JSON�f�[�^�̏����̋K��� http://www.json.org/json-ja.html ���Q��(2018/11/05���_�̓��e�ɏ���)
//	�f�[�^���������Ȃ����A���炩�̗��R�Ńp�[�X�Ɏ��s�����ꍇ�́Astd::exception(�h���N���X)��O����������
#pragma once
#if !_HAS_CXX17
#error "JSON�p�[�T�[�𗘗p����ꍇ�́A�R���p�C���I�v�V������C++17�ȏ�ɐݒ肵�Ă�������(���� C++20)�B"
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
/// JSON�̒ʒm�^�C�v���`����񋓌^
/// </summary>
/// remarks
/// �L���X�g���Ďg����悤�ɂ��邽�߁A���e�����ƃ}�b�v����`�ɕύX
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
// �����������l�Ƃ��ĕs�������o�Ă��Ȃ�����
static_assert(NotificationType::StartObject > NotificationType::Null );

//	�p�[�X���ʂ̃R�[���o�b�N�󂯎�胁�\�b�h( bool func( Morrin::JSON::NotificationType type, const std::string_view& value ) )
using ParseCallBack = std::function<bool( NotificationType type, const std::string_view& value )> const;

// std::any �Ɋi�[�����f�[�^��`

// �����Ȃǂ̎Q�Ɨp�f�[�^
using JsonRefKeyType = std::string_view;


#ifdef FORCE_REFERENCE_VALUE
using JsonKeyType = JsonRefKeyType;
using JsonString = JsonRefKeyType;	// ���̂܂܎Q�Ƃ���̂ŁA�ϊ��������{���Ȃ�
#else
using JsonKeyType = std::string;
using JsonString = std::wstring;	// ������f�[�^�́A���K�����Ă���
#endif
using JsonObject = std::map<JsonKeyType, std::any>;
using JsonArray = std::vector<std::any>;	// �z��� std::any �Œl��ێ�����
//---------------
//	prottype
//---------------
bool __stdcall ParseJSON( const std::string_view& jsonText, const ParseCallBack& proc );

// �\�����f�[�^�Ƃ��ăp�[�X����f�[�^�́A�i�����\�Ƃ���B
std::any __stdcall ParseJSON( const std::string_view& jsonText, const concurrency::cancellation_token& token = concurrency::cancellation_token::none() );

// std::any �`���Ń��^�[������JSON�f�[�^���璼�ڒl���擾����
// searchKey �́A'/' �ŋ�؂��ăI�u�W�F�N�g�̃L�[���w�肷��B�z��̏ꍇ�̓C���f�b�N�X�𒼐ڋL�q����
// Ex) "key1/key2/0/key3/4"
std::any __stdcall GetValue( std::any obj, const JsonRefKeyType& searchKey );

//	string_view ���Q�Ƃ��Ă���e�L�X�g���A���G�X�P�[�v���� string �̏ꍇ�̕����R�[�h��UTF8�ɂȂ�
std::string __stdcall UnEscapeToString( const std::string_view& value );
std::wstring __stdcall UnEscapeToWstring( const std::string_view& value );

// JsonObject �ł́A���l�� int �݂̂Ƃ���
int __stdcall ConvertToInt( const std::string_view& value );

// �����R�[�h�̕ϊ�(UNICODE->�C�ӃR�[�h�y�[�W)
std::string __stdcall ToString( const std::wstring_view& str, UINT codePage );
inline std::string __stdcall ToString( LPCWSTR str, UINT codePage )
{
	return ToString( std::wstring_view( str ), codePage );
}
// UTF8�ւ̕ϊ�
inline std::string __stdcall ToUtf8( const std::wstring_view& str )
{
	return ToString( str, CP_UTF8 );
}
inline std::string ToUtf8( LPCWSTR str )
{
	return ToString( std::wstring_view( str ), CP_UTF8 );
}
// UNICODE ����C�ӃR�[�h�y�[�W�ɕϊ�����
std::wstring __stdcall ToWString( const std::string_view& u8str, UINT codePage = CP_UTF8 );
inline std::wstring ToWString( LPCSTR str, UINT codePage = CP_UTF8 )
{
	return ToWString( std::string_view( str ), codePage );
}
}
