// �Ȉ�JSON�p�[�T�[

//---------------
//	Include Files
//---------------
#include "stdafx.h"

#include "ParseJSON.h"
#include <array>
#include <stack>
#include <format>
#include <stdexcept>

using namespace std::string_view_literals;
using namespace std::string_literals;

namespace Morrin::JSON
{

//---------------
//	defines
//---------------
struct LiteralValueDetail
{
	NotificationType type;
	std::string_view text;
};
struct EscapeCharDetail
{
	std::string_view::value_type escapeChar;
	std::string_view::value_type escapedChar;	// ���ۂ̕���
};
//---------------
//	static
//---------------
static const std::initializer_list<LiteralValueDetail> LiteralValues
{
	{ NotificationType::BooleanTrue, "true" },
	{ NotificationType::BooleanFalse, "false" },
	{ NotificationType::Null, "null" },
};
static const std::initializer_list<EscapeCharDetail> EscapeChars
{
	{ '"', '"' },
	{ '\\', '\\' },
	{ '/', '/' },
	{ 'b', '\b' },
	{ 'f', '\f' },
	{ 'n', '\n' },
	{ 'r', '\r' },
	{ 't', '\t' },
	{ 'u', 'u' },
};

static bool __stdcall ParseElement( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc );
static bool __stdcall ParseKeyValue( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc );
static bool __stdcall ParseValues( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc,
	char startChar, char endChar, const std::function<bool( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc )>& parseProc );
static bool __stdcall ParseString( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc );
static bool __stdcall ParseNumber( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc );

static std::string_view ParseStringValue( const std::string_view& rawData, std::string_view::size_type& offset );

///////////////////////////////////////////////////////////////////////////////
//	inline Functions
inline  bool IsWhiteSpace( std::string_view::value_type value )
{
	//	�z���C�g�X�y�[�X�Ƃ��ċK�肳��Ă���Bisspace �Ƃ͂�����ƈႤ�̂Œ�`�����i��
	switch( value )
	{
	case ' ':
	case '\t':
	case '\r':
	case '\n':
		return true;
	}
	return false;
}
inline std::string_view::size_type SkipWhiteSpace( const std::string_view& rawData, std::string_view::size_type offset )
{
	while( offset < rawData.length() && IsWhiteSpace( rawData[offset] ) )
	{
		++offset;
	}
	return offset;
}
inline std::string_view::value_type GetChar( const std::string_view& jsonText, std::string_view::size_type offset )
{
	//	�I�t�Z�b�g���͈͊O�Ȃ��O�𓊂���
	if( offset >= jsonText.length() )
	{
		throw std::out_of_range( std::format( "Offset {} is out of range for data length {}", offset, jsonText.length() ) );
	}
	return jsonText[offset];
}

///////////////////////////////////////////////////////////////////////////////
//	static Functions
static bool __stdcall ParseElement( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc )
{
	_ASSERTE( offset < jsonText.length() );
	offset = SkipWhiteSpace( jsonText, offset );
	// �v�f���������m�F����
	if( offset < jsonText.length() )
	{
		switch( GetChar( jsonText, offset ) )
		{
		// �I�u�W�F�N�g
		case '{':
			// �J�n��ʒm
			if( !proc( NotificationType::StartObject, jsonText.substr( offset++, 1 ) ) )
			{
				return false;
			}
			if( !ParseValues( jsonText, offset, proc, '{', '}', ParseKeyValue ) )
			{
				return false;
			}
			return proc( NotificationType::EndObject, jsonText.substr( offset++, 1 ) );	// �I����ʒm
		// �z��
		case '[':
			// �J�n��ʒm
			if( !proc( NotificationType::StartArray, jsonText.substr( offset++, 1 ) ) )
			{
				return false;
			}
			if( !ParseValues( jsonText, offset, proc, '[', ']', ParseElement ) )
			{
				return false;
			}
			return proc( NotificationType::EndArray, jsonText.substr( offset++, 1 ) );	// �I����ʒm
		// ������
		case '"':	return ParseString( jsonText, offset, proc );
		// ���̂ق�(true, false, null, ���l)
		default:
			// ���e�����l
			const auto& itr = std::find_if( LiteralValues.begin(), LiteralValues.end(),
				[&jsonText, offset]( const auto& value )
			{
				return jsonText.compare( offset, value.text.length(), value.text ) == 0;
			} );
			if( itr != LiteralValues.end() )
			{
				if( !proc( itr->type, itr->text ) )
				{
					return false;	// �R�[���o�b�N�� false ��Ԃ�����I��
				}
				offset += itr->text.length();	// �I�t�Z�b�g��i�߂�
				return true;
			}
			// ���l�^
			if( isdigit( GetChar( jsonText, offset ) ) || GetChar( jsonText, offset ) == '-' )
			{
				return ParseNumber( jsonText, offset, proc );
			}
			_ASSERTE( isdigit( GetChar( jsonText, offset ) ) || GetChar( jsonText, offset ) == '-' || GetChar( jsonText, offset ) == '+' );
		}
		// �����ɂ�����p�[�X�G���[
		throw new std::invalid_argument( std::format( "Parse error at offset {}: unexpected character '{}'", offset, GetChar( jsonText, offset ) ) );
	}
	// �����ɗ��Ă�c�����ɂ���񂾂����H
	return false;
}
// Object, Array �̋��ʃp�[�T�[
static bool __stdcall ParseValues( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc,
	char startChar, char endChar, const std::function<bool( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc )>& parseProc )
{
	_ASSERTE( GetChar( jsonText, offset ) == startChar );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	// �J�n��ʒm
	if( !proc( NotificationType::StartElement, jsonText.substr( offset++, 1 ) ) )
	{
		return false;
	}
	while( offset < jsonText.length() )
	{
		// ��v�f���p�[�X����
		if( !parseProc( jsonText, offset, proc ) )
		{
			return false;
		}
		// �󔒕���������\��������̂ŃX�L�b�v
		offset = SkipWhiteSpace( jsonText, offset );
		// �I�[�L��������O�Ƀf�[�^���Ȃ��Ȃ���(���蓾�Ȃ�)
		if( offset >= jsonText.length() )
		{
			throw new std::invalid_argument( std::format( "Parse error at offset {}: unexpected end of data", offset ) );
		}
		// �I�[�����������̂Ńu���b�N�I��(�K�������ɗ���c�͂�)
		if( GetChar( jsonText, offset ) == endChar )
		{
			break;
		}
		_ASSERTE( GetChar( jsonText, offset ) == ',' );	//	��؂蕶��������͂�
		if( GetChar( jsonText, offset ) != ',' )
		{
			throw new std::invalid_argument( std::format( "Parse error at offset {}: expected ',' but found '{}'", offset, GetChar( jsonText, offset ) ) );
		}
	}
	// �����ɂ͗��Ȃ�(����Ƃ�����A�G���[�`�F�b�N�����蔲���ė���ꍇ����)
	return true;
}

static bool __stdcall ParseKeyValue( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc )
{
	offset = SkipWhiteSpace( jsonText, offset );
	// object�u���b�N�I���
	if( GetChar( jsonText, offset ) == '}' )
	{
		return true;
	}
	_ASSERTE( GetChar( jsonText, offset ) == '"' );	//	�L�[�͕�����ł��邱�Ƃ�O��ɂ��Ă���
	auto key = ParseStringValue( jsonText, offset );
	offset  = SkipWhiteSpace( jsonText, offset );
	_ASSERTE( offset < jsonText.length() && GetChar( jsonText, offset ) == ':' );	//	�L�[�ƒl�̋�؂�̓R�����ł��邱�Ƃ�O��ɂ��Ă���
	if( offset >= jsonText.length() || GetChar( jsonText, offset ) != ':' )
	{
		throw new std::invalid_argument( std::format( "Parse error at offset {}: expected ':' but found '{}'", offset, GetChar( jsonText, offset ) ) );
	}
	offset++;
	if( !ParseElement( jsonText, offset, proc ) )
	{
		return false;	// �R�[���o�b�N�� false ��Ԃ�����I��
	}
	return true;
}
static bool __stdcall ParseString( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc )
{
	_ASSERTE( GetChar( jsonText, offset ) == '"' );	//	������̓_�u���N�H�[�g�ň͂܂�Ă��邱�Ƃ�O��ɂ��Ă���
	auto strValue = ParseStringValue( jsonText, offset );
	return proc( NotificationType::String, strValue );
}
static bool __stdcall ParseNumber( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc )
{
	// ���l�f�[�^�B���ۂ́A'0'(���̂P�����̐��l)�A'-'(���̐��l)�A'1'�`'9'(���̐��l)�̂����ꂩ�̂͂������ǁA+�������Ă�ꍇ��F�߂�
	_ASSERTE( offset < jsonText.length() && (isdigit( GetChar( jsonText, offset ) ) || GetChar( jsonText, offset ) == '-') );
	auto start = offset;
	if( GetChar( jsonText, offset ) == '-' )	// ���̐��l
	{
		++offset;
	}
	// �ŏ���0�̏ꍇ����'.'���A�����Ȃ����̂����ꂩ
	if( GetChar( jsonText, offset ) == '0' )
	{
		++offset;	// 0 �̏ꍇ�́A���̕����������łȂ����肱���ŏI���
	}
	else if( isdigit( GetChar( jsonText, offset ) ) )	// ���̐��l
	{
		// ���l����������i�߂�
		while( isdigit( GetChar( jsonText, offset ) ) )
		{
			++offset;
		}
	}
	bool needNext = false;
	// ���ɏo�Ă���̂́A'.' �܂��� 'e', 'E' �̂����ꂩ
	if( GetChar( jsonText, offset ) == '.' )	// �����_������ꍇ
	{
		++offset;	// �����_��i�߂�
		needNext = true;	// �����_�̌�͐������K�v
	}
	else if( GetChar( jsonText, offset ) == 'e' || GetChar( jsonText, offset ) == 'E' )	// �w����������ꍇ
	{
		++offset;	// �w�����̕�����i�߂�
		auto chkChar = GetChar( jsonText, offset );
		if( chkChar == '+' || chkChar == '-' )	// ����������ꍇ
		{
			++offset;	// ������i�߂�
		}
		needNext = true;	// �����_�̌�͐������K�v
	}
	if( needNext )	// �����_�܂��͎w����������ꍇ�A���ɐ������K�v
	{
		if( offset >= jsonText.length() || !isdigit( GetChar( jsonText, offset ) ) )	// �������Ȃ��Ƃ����Ȃ�
		{
			throw new std::invalid_argument( std::format( "Parse error at offset {}: expected digit after 'e' or 'E' but found '{}'", offset, GetChar( jsonText, offset ) ) );
		}
		while( isdigit( GetChar( jsonText, offset ) ) )	// ��������������i�߂�
		{
			++offset;
		}
	}
	// ���ۂ̐��l����؂�o��
	auto value = jsonText.substr( start, offset - start );
	return proc( NotificationType::Number, value );	// �R�[���o�b�N�ɒʒm
}
static std::string_view ParseStringValue( const std::string_view& jsonText, std::string_view::size_type& offset )
{
	_ASSERTE( GetChar( jsonText, offset ) );
	++offset;
	auto start = offset;	// �J�n�ʒu���L�^

	std::string_view::value_type currChar = '\0';
	while( offset < jsonText.length() && (currChar=GetChar( jsonText, offset )) != '"' )
	{
		// �G�X�P�[�v�������o�Ă����ꍇ
		if( currChar == '\\' )
		{
			currChar = GetChar( jsonText, ++offset );	// �G�X�P�[�v�����̎��̕������擾
			auto itr = std::find_if( EscapeChars.begin(), EscapeChars.end(), [currChar]( const auto& chars ){ return chars.escapeChar == currChar; } );
			if( itr != EscapeChars.end() )
			{
				// �����R�[�h�w��̏ꍇ
				if( itr->escapeChar == 'u' )
				{
					for( int step = 0; step < 4; ++step )	// 4����16�i����ǂݎ��
					{
						if( !isxdigit( GetChar( jsonText, ++offset ) ) )
						{
							// ����������O
							throw new std::invalid_argument( std::format( "Parse error at offset {}: expected hex digit after '\\u'", offset ) );
						}
					}
				}
				else
				{
					++offset;	// �G�X�P�[�v��������i�߂�
				}
			}
			else
			{
				// �G�X�P�[�v��������`����Ă��Ȃ��ꍇ�͗�O
				throw new std::invalid_argument( std::format( "Parse error at offset {}: unexpected escape character '{}'", offset, currChar ) );
			}
		}
		else
		{
			++offset;	// �G�X�P�[�v�����ł͂Ȃ��̂ł��̂܂ܐi�߂�
		}
	}
	_ASSERTE( GetChar( jsonText, offset ) == '"' );
	auto resultValue = jsonText.substr( start, offset-start );
	++offset;
	return resultValue;	// �؂�o�����������Ԃ�
}
///////////////////////////////////////////////////////////////////////////////
//	export Functions
bool __stdcall ParseJSON( const std::string_view& jsonText, ParseCallBack&& proc )
{
	std::string_view::size_type offset = 0;
	// �����J�n��ʒm(�R�[���o�b�N���̏������𑣂���悤�ɂ��Ă���)
	if( jsonText.empty() || !proc( NotificationType::StartParse, jsonText ) )
	{
		return false;
	}
	while( offset < jsonText.length() )
	{
		if( !ParseElement( jsonText, offset, proc ) )
		{
			break;
		}
	}
	//	�I����ʒm�Boffset �̈ʒu�Ŕ���\�Ȃ̂ŕK�v������΂�������邱�ƂƂ���
	_ASSERTE( offset <= jsonText.length() );
	//	�I�[�o�[���Ă���Ɨ�O���o��̂ŁA���[�𒴂��Ȃ��悤�ɂ���(�v���O�����I�ɂ͖����𒴂��邱�Ƃ͂Ȃ��͂��Ȃ̂����B�B�B)
	if( offset > jsonText.length() )	offset = jsonText.length();
	// �����I����ʒm���ďI���
	return proc( NotificationType::EndParse, jsonText.substr( offset ) );	//	�Ȃɂ��c���Ă�Ƃ����z��őS�����荞��ł��(����Ƀl�X�g�I�ȏ����������Ă��悢�Ƃ�������)
}
std::any __stdcall ParseJSON( const std::string_view& jsonText, const concurrency::cancellation_token& token/* = concurrency::cancellation_token::none()*/ )
{
	std::any json;
	return json;
}
}
