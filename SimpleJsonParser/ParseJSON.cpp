// �Ȉ�JSON�p�[�T�[

//---------------
//	Include Files
//---------------
#include "stdafx.h"

#include "ParseJSON.h"
#include <array>
#include <stack>
#include <stdexcept>
#if _HAS_CXX20
#include <format>
#endif
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
//---------------
//	class
//---------------
#if !_HAS_CXX20
template <class... _Types>
std::string SPRINTF( _In_z_ _Printf_format_string_ const char* _fmt, ... )
{
	CStringA text;
	va_list argList;
	va_start( argList, _fmt );
	text.FormatV( _fmt, argList );
	va_end( argList );
	std::string result{ text };
	return result;
}
#endif

class JsonParser
{
public:
	JsonParser( const std::string_view& jsonText, const ParseCallBack& callback )
		: m_jsonText( jsonText )
		, m_offset( 0 )
		, m_callback( callback )
	{
	}
	bool Parse();

private:
	bool ParseElement();
	bool ParseKeyValue();
	bool ParseValues( NotificationType start, NotificationType end, bool (JsonParser::*parseProc)() );
	bool ParseString();
	bool ParseNumber();

	void SkipWhiteSpace()
	{
		while( m_offset < m_jsonText.length() && IsWhiteSpace( m_jsonText[m_offset] ) )
		{
			++m_offset;
		}
	}
	std::string_view::value_type GetChar() const
	{
		if( m_offset >= m_jsonText.length() )
		{
#if _HAS_CXX20
			throw std::out_of_range( std::format( "m_offset {} is out of range for data length {}", m_offset, m_jsonText.length() ) );
#else
			throw std::out_of_range( SPRINTF( "m_offset %u is out of range for data length %u", m_offset, m_jsonText.length() ) );
#endif
		}
		return m_jsonText[m_offset];
	}
	std::string_view ParseStringValue();

	bool IsWhiteSpace( std::string_view::value_type value ) const
	{
		// �z���C�g�X�y�[�X�Ƃ��ċK�肳��Ă���Bisspace �Ƃ͂�����ƈႤ�̂Œ�`�����i��
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
private:
	const std::string_view& m_jsonText;	// JSON �e�L�X�g
	std::string_view::size_type m_offset;	// ���݂̃I�t�Z�b�g
	const ParseCallBack& m_callback;	// �R�[���o�b�N�֐�
};
//---------------
//	prototype
//---------------
inline std::any& AddValue( std::any* anyObj, const JsonKeyType& keyName, std::any&& value )
{
	std::any& parentObj = *anyObj;
	// ��������l�������Ă���&& �e�I�u�W�F�N�g��JsonObject�܂���JsonArray�ɂȂ��Ă���(���͂��蓾�Ȃ��p�^�[��)
	_ASSERTE( parentObj.has_value() && (parentObj.type() == typeid(JsonObject) || parentObj.type() == typeid(JsonArray)) );
	// JsonObject �̏ꍇ�̓L�[�ƃy�A�ŕۑ�����(���[�g�v�f�̓_�~�[������Ă���̂ŋ�ɂȂ�Ȃ��悤�ɂȂ��Ă���)
	if( parentObj.type() == typeid(JsonObject) )
	{
		auto& jsonObj = std::any_cast<JsonObject&>(parentObj);
		auto pair = std::make_pair( keyName, value );
		jsonObj.insert( pair );
		return jsonObj[keyName];
	}
	// JsonArray �̏ꍇ�͔z��v�f�Ƃ��Ēǉ�����(keyName�͖���)
	else if( parentObj.type() == typeid(JsonArray) )
	{
		auto& jsonArray = std::any_cast<JsonArray&>(parentObj);
		jsonArray.push_back( value );
		return jsonArray.back();
	}
	return value;
}
template <typename Type>
inline void PushValueT( std::stack<std::any*>& stack, const JsonKeyType& keyName, Type&& value )
{
	std::any add = std::make_any<Type>( value );
	std::any& anyObj = AddValue( stack.top(), keyName, std::forward<std::any>( add ) );
	// �X�^�b�N�ɒǉ�����
	stack.push( &anyObj );
}

///////////////////////////////////////////////////////////////////////////////
//	export Functions
bool __stdcall ParseJSON( const std::string_view& jsonText, const ParseCallBack& proc )
{
	JsonParser parser( jsonText, proc );
	return parser.Parse();
}
std::any __stdcall ParseJSON( const std::string_view& jsonText, const concurrency::cancellation_token& token/* = concurrency::cancellation_token::none()*/ )
{
	// �\���������f�[�^�Ƃ��ĕێ�����
	std::stack<std::any*> stack;	//	�Q��(�|�C���^)���������邱�ƂŃR�s�[��h��
	JsonKeyType rootName = "RootObject";
	JsonKeyType keyName = rootName;
	JsonObject rootObj;
	std::any topObj = std::make_any<JsonObject>( rootObj );	// ���[�g�I�u�W�F�N�g���i�[���邽�߂̉��z�e�I�u�W�F�N�g
	stack.push( &topObj );

	auto result = ParseJSON( jsonText, [&]( const Morrin::JSON::NotificationType& type, const std::string_view& value ) -> bool
	{
		if( token.is_canceled() )
		{
			// �L�����Z�����ꂽ��Afalse��Ԃ�
			return false;
		}
		switch( type )
		{
		case Morrin::JSON::NotificationType::Key:			keyName = UnEscapeToString( value );		break;
		case Morrin::JSON::NotificationType::StartObject:	PushValueT( stack, keyName, JsonObject() );	break;
		case Morrin::JSON::NotificationType::StartArray:	PushValueT( stack, keyName, JsonArray() );	break;
		case Morrin::JSON::NotificationType::EndObject:
		case Morrin::JSON::NotificationType::EndArray:		stack.pop();	break;
		// �l�^�͂��̂܂ܒǉ����邾���ł悢�͂��c
		case Morrin::JSON::NotificationType::String:		AddValue( stack.top(), keyName, std::make_any<JsonString>( UnEscapeToWstring(value) ) );	break;
		case Morrin::JSON::NotificationType::Number:		AddValue( stack.top(), keyName, std::make_any<int>( ConvertToInt( value ) ) );				break;
		case Morrin::JSON::NotificationType::BooleanTrue:	AddValue( stack.top(), keyName, std::make_any<bool>( true ) );								break;
		case Morrin::JSON::NotificationType::BooleanFalse:	AddValue( stack.top(), keyName, std::make_any<bool>( false ) );							break;
		case Morrin::JSON::NotificationType::Null:			AddValue( stack.top(), keyName, std::any() );												break;
		case Morrin::JSON::NotificationType::StartParse:
		case Morrin::JSON::NotificationType::EndParse:		break;	//	�p�[�X�J�n�ƏI���͉������Ȃ��̂Ŗ���(default�ɍs���Ȃ��悤�Ƀu���b�N)
		default:
			// �z��O�͗�O���΂�
#if _HAS_CXX20
			throw new std::runtime_error( std::format( "Unknown NotificationType: {}", static_cast<int>(type) ) );
#else
			throw new std::runtime_error( SPRINTF( "Unknown NotificationType: %d", static_cast<int>(type) ) );
#endif
		}
		return true; // true��Ԃ��ƁA����Ɏq�v�f�̃p�[�X�𑱂���	)
	} );
	// �X�^�b�N��Ԃɂ������Ȃ����[�g������悤�ɂȂ��Ă��Ȃ��ƍ���
	auto& top = std::any_cast<JsonObject&>(*stack.top());
	//auto top = std::any_cast<JsonObject>(topObj);
	return top[rootName];
}
std::any APIENTRY GetValue( std::any obj, const JsonRefKeyType& searchKey )
{
	_ASSERTE( obj.has_value() );
	// �����v�f�������Ȃ��ꍇ�́Aobj�����̂܂ܕԂ�
	if( searchKey.empty() )
	{
		return obj;
	}
	// �f�[�^�Ȃ���Ԃ��B�����ʓ|�ł��[���A���P�[�V�����͕K�{
	JsonRefKeyType::size_type start = 0;
	while( obj.has_value() && start < searchKey.length() )
	{
		// �f�[�^���������撣���Đ���(�ċA�ł����������ǂ����̓��[�v�ŏ�������)
		auto last = searchKey.find( '/', start ); // ������Ȃ���� npos �ł��̏ꍇ�� substr �Ŏc��S���ɂȂ�
		JsonRefKeyType key{ searchKey.substr( start, last != searchKey.npos ? last-start : last ) };
		// �����L�[�����o�ł���(//�Ƃ�����Ă���ꍇ�͒P���X�L�b�v����B)
		_ASSERTE( key.empty() == false );	//	����������v�f�Ō��o����邱�Ƃ͂Ȃ��͂��c
		if( !key.empty() )
		{
			// JsonArray(���̂�std::vector)
			if( isdigit( key[0] ) )
			{
				// string_view �� atoi �ɑ���������̂��Ȃ��̂Ŏ��͂ŉ�(�����́A0�ȏ�̐����������Ȃ��̂ŁA�P�����[�v�ł悢)
				JsonRefKeyType::size_type pos = 1;
				JsonArray::size_type index = 0;
				while( pos < key.length() && isdigit( key[pos] ) )
				{
					index = index*10 + key[pos]-'0';
					pos++;
				}
				const auto& arr = std::any_cast<const JsonArray&>( obj );
				if( index < arr.size() )
				{
					obj = arr[index];
				}
				else
				{
					obj = std::any();
				}
			}
			// JsonObject(���̂́Astd::map)
			else
			{
				const auto& jsonObj = std::any_cast<const JsonObject&>(obj);
				//auto itr = jsonObj.find( JsonKeyType(key) );
				auto itr = std::find_if( jsonObj.begin(), jsonObj.end(), [&]( const auto& pair ){ return pair.first == key; } );
				_ASSERTE( itr != jsonObj.end() );
				if( itr != jsonObj.end() )
				{
					obj = itr->second;
				}
				else
				{
					obj = std::any();
				}
			}
		}
		// ����ȏ㌟��������̂��Ȃ�
		if( last == searchKey.npos || last+1 >= searchKey.length() )
		{
			break;
		}
		// ���̗v�f�𑊎�ɒT��
		start = last+1;
	}
	return obj;
}
template <class ResultStringType> ResultStringType UnEscape(
	const std::string_view& value,
	std::function<void(const std::string_view& value, std::string_view::size_type pos, std::string_view::size_type prevPos, ResultStringType& resultStr)>&& SetPrevStr,
	std::function<ResultStringType(const std::wstring& u16str )>&& ConvertToResultStr )
{
	std::string_view::size_type pos = 0;
	ResultStringType resultStr;
	auto prevPos = pos;
	std::wstring u16str;
	while( pos < value.length() )
	{
		//	�G�X�P�[�v�L��
		if( value[pos] == '\\' )
		{
			SetPrevStr( value, pos, prevPos, resultStr );
			prevPos = std::string_view::npos;	//	�G�X�P�[�v�������Z�b�g�����̂ŁA�ȑO�̈ʒu���N���A�[
			++pos;
			if( value[pos] == 'u' )
			{
				++pos;
				wchar_t ch = 0;
				for( int cnt = 0; cnt < 4 && isxdigit( value[pos] ); ++cnt, ++pos )
				{
					if( isdigit( value[pos] ) )
					{
						ch = (ch << 4) + (value[pos] - '0');
					}
					else
					{
						ch = (ch<<4) + ((toupper( value[pos] ) - 'A') + 0xA);
					}
				}
				u16str += ch;
			}
			else
			{
				if( !u16str.empty() )
				{
					resultStr += ConvertToResultStr( u16str );
					u16str.clear();
				}
				auto itr = std::find_if( EscapeChars.begin(), EscapeChars.end(), [&]( const auto& escapeChar )
				{
					return escapeChar.escapeChar == value[pos];
				} );
				// �G�X�P�[�v�����ȊO�������炻�̂܂܃X�L�b�v(��������)
				if( itr != EscapeChars.end() )
				{
					//	�G�X�P�[�v��������`����Ă���̂ŁA�ϊ�����
					resultStr += itr->escapedChar;
					++pos;
				}
			}
		}
		else
		{
			if( !u16str.empty() )
			{
				resultStr += ConvertToResultStr( u16str );
				u16str.clear();
			}
			if( prevPos == std::string_view::npos )
			{
				prevPos = pos;
			}
			++pos;
		}
	}
	if( !u16str.empty() )
	{
		resultStr += ConvertToResultStr( u16str );
		u16str.clear();
	}
	SetPrevStr( value, pos, prevPos, resultStr );
	return resultStr;
}
// �e�L�X�g�̃A���G�X�P�[�v(���x�d���̂��߁A���ʉ����Ȃ�)
std::string __stdcall UnEscapeToString( const std::string_view& value )
{
	return UnEscape<std::string>( value,
		[]( const auto& value, auto pos, auto prevPos, auto& resultStr )
		{
			if( prevPos != std::string_view::npos )
			{
				auto parseStr = value.substr( prevPos, pos-prevPos );
				resultStr += parseStr;
			}
		},
		[]( const auto& u16str ){ return ToUtf8( u16str ); }
	);
}
std::wstring __stdcall UnEscapeToWstring( const std::string_view& value )
{
	return UnEscape<std::wstring>( value,
		[]( const auto& value, auto pos, auto prevPos, auto& resultStr )
		{
			if( prevPos != std::string_view::npos )
			{
				auto parseStr = value.substr( prevPos, pos-prevPos );
				resultStr += ToWString( parseStr, CP_UTF8 );
			}
		},
		[]( const auto& u16str ){ return u16str; }
	);
}
int __stdcall ConvertToInt( const std::string_view& value )
{
	if( value.empty()|| value == "0"sv )
	{
		return 0;
	}
	auto pos = value.begin();
	int flag = 1;
	if( *pos == '-' )
	{
		flag = -1;
		++pos;
	}
	int numValue = 0;
	for( ; pos != value.end() && isdigit( *pos ); ++pos )
	{
		numValue = numValue*10 + *pos-'0';
	}
	return numValue * flag;
}
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
///////////////////////////////////////////////////////////////////////////////
//	JsonParser
bool JsonParser::Parse()
{
	m_offset = 0;
	// �����J�n��ʒm(�R�[���o�b�N���̏������𑣂���悤�ɂ��Ă���)
	if( m_jsonText.empty() || !m_callback( NotificationType::StartParse, m_jsonText ) )
	{
		return false;
	}
	while( m_offset < m_jsonText.length() )
	{
		if( !ParseElement() )
		{
			break;
		}
	}
	//	�I����ʒm�Bm_offset �̈ʒu�Ŕ���\�Ȃ̂ŕK�v������΂�������邱�ƂƂ���
	_ASSERTE( m_offset <= m_jsonText.length() );
	//	�I�[�o�[���Ă���Ɨ�O���o��̂ŁA���[�𒴂��Ȃ��悤�ɂ���(�v���O�����I�ɂ͖����𒴂��邱�Ƃ͂Ȃ��͂��Ȃ̂����B�B�B)
	if( m_offset > m_jsonText.length() )	m_offset = m_jsonText.length();
	// �����I����ʒm���ďI���
	return m_callback( NotificationType::EndParse, m_jsonText.substr( m_offset ) );
}
	
bool JsonParser::ParseElement()
{
	_ASSERTE( m_offset < m_jsonText.length() );
	SkipWhiteSpace();
	// �v�f���������m�F����
	if( m_offset < m_jsonText.length() )
	{
		switch( GetChar() )
		{
		// �I�u�W�F�N�g
		case '{':
			// �J�n��ʒm
			return ParseValues( NotificationType::StartObject, NotificationType::EndObject, &JsonParser::ParseKeyValue );
		// �z��
		case '[':
			return ParseValues( NotificationType::StartArray, NotificationType::EndArray, &JsonParser::ParseElement );
		// ������
		case '"':	return ParseString();
		// ���e����(true, false, null), ���l
		default:
			// ���e�����l
			const auto& itr = std::find_if( LiteralValues.begin(), LiteralValues.end(), [&]( const auto& value )
			{
				return m_jsonText.compare( m_offset, value.text.length(), value.text ) == 0;
			} );
			if( itr != LiteralValues.end() )
			{
				if( !m_callback( itr->type, itr->text ) )
				{
					return false;	// �R�[���o�b�N�� false ��Ԃ�����I��
				}
				m_offset += itr->text.length();	// �I�t�Z�b�g��i�߂�
				return true;
			}
			// ���l�^
			if( isdigit( GetChar() ) || GetChar() == '-' )
			{
				return ParseNumber();
			}
			// �����ɗ��Ă鎞�_�Ńp�[�X�G���[
			break;
		}
		// �����ɂ�����p�[�X�G���[
#if _HAS_CXX20
		throw new std::invalid_argument( std::format( "Parse error at m_offset {}: unexpected character '{}'", m_offset, GetChar() ) );
#else
		throw new std::invalid_argument( SPRINTF( "Parse error at m_offset %u: unexpected character '%c'", m_offset, GetChar() ) );
#endif
	}
	// �����ɗ��Ă�c�����ɂ���񂾂����H
	return false;
}
// Object, Array �̋��ʃp�[�T�[
bool JsonParser::ParseValues( NotificationType start, NotificationType end, bool (JsonParser::* parseProc)() )
{
	_ASSERTE( GetChar() == static_cast<char>(start) );	//	��O�ŃX�L�b�v���Ȃ��̂ŁA�����ōēx�l�`�F�b�N�ł���悤�ɂ��Ă���
	if( !m_callback( start, m_jsonText.substr( m_offset, 1 ) ) )
	{
		return false;
	}
	while( m_offset+1 < m_jsonText.length() )
	{
		++m_offset;	// ���O�v�f(�ŏ��Ȃ� '{' or '[', �r���Ȃ� ',')���X�L�b�v
		// ��v�f���p�[�X����
		if( !(this->*parseProc)() )
		{
			return false;
		}
		// �󔒕���������\��������̂ŃX�L�b�v
		SkipWhiteSpace();
		// �I�[�L��������O�Ƀf�[�^���Ȃ��Ȃ���(���蓾�Ȃ�)
		if( m_offset >= m_jsonText.length() )
		{
#if _HAS_CXX20
			throw new std::invalid_argument( std::format( "Parse error at m_offset {}: unexpected end of data", m_offset ) );
#else
			throw new std::invalid_argument( SPRINTF( "Parse error at m_offset %u: unexpected end of data", m_offset ) );
#endif
		}
		// �I�[�����������̂Ńu���b�N�I��(�K�������ɗ���c�͂�)
		if( GetChar() == static_cast<char>(end) )
		{
			break;
		}
		_ASSERTE( GetChar() == ',' );	//	��؂蕶��������͂�
		if( GetChar() != ',' )
		{
#if _HAS_CXX20
			throw new std::invalid_argument( std::format( "Parse error at m_offset {}: expected ',' but found '{}'", m_offset, GetChar() ) );
#else
			throw new std::invalid_argument( SPRINTF( "Parse error at m_offset %u: expected ',' but found '%c'", m_offset, GetChar() ) );
#endif
		}
	}
	// �����ɂ͗��Ȃ�(����Ƃ�����A�G���[�`�F�b�N�����蔲���ė���ꍇ����)
	return m_callback( end, m_jsonText.substr( m_offset++, 1 ) );	// �I����ʒm
}

bool JsonParser::ParseKeyValue()
{
	SkipWhiteSpace();
	// object�u���b�N�I���
	if( GetChar() == '}' )
	{
		return true;
	}
	_ASSERTE( GetChar() == '"' );	//	�L�[�͕�����ł��邱�Ƃ�O��ɂ��Ă���
	auto key = ParseStringValue();
	if( !m_callback( NotificationType::Key, key ) )
	{
		return false;
	}
	SkipWhiteSpace();
	_ASSERTE( m_offset < m_jsonText.length() && GetChar() == ':' );	//	�L�[�ƒl�̋�؂�̓R�����ł��邱�Ƃ�O��ɂ��Ă���
	if( m_offset >= m_jsonText.length() || GetChar() != ':' )
	{
#if _HAS_CXX20
		throw new std::invalid_argument( std::format( "Parse error at m_offset {}: expected ':' but found '{}'", m_offset, GetChar() ) );
#else
		throw new std::invalid_argument( SPRINTF( "Parse error at m_offset %u: expected ':' but found '%c'", m_offset, GetChar() ) );
#endif
	}
	++m_offset;
	if( !ParseElement() )
	{
		return false;	// �R�[���o�b�N�� false ��Ԃ�����I��
	}
	return true;
}
bool JsonParser::ParseString()
{
	_ASSERTE( GetChar() == '"' );	//	������̓_�u���N�H�[�g�ň͂܂�Ă��邱�Ƃ�O��ɂ��Ă���
	auto strValue = ParseStringValue();
	return m_callback( NotificationType::String, strValue );
}
bool JsonParser::ParseNumber()
{
	// ���l�f�[�^�B���ۂ́A'0'(���̂P�����̐��l)�A'-'(���̐��l)�A'1'�`'9'(���̐��l)�̂����ꂩ�̂͂������ǁA+�������Ă�ꍇ��F�߂�
	_ASSERTE( m_offset < m_jsonText.length() && (isdigit( GetChar() ) || GetChar() == '-') );
	auto start = m_offset;
	if( GetChar() == '-' )	// ���̐��l
	{
		++m_offset;
	}
	// �ŏ���0�̏ꍇ����'.'���A�����Ȃ����̂����ꂩ
	if( GetChar() == '0' )
	{
		++m_offset;	// 0 �̏ꍇ�́A���̕����������łȂ����肱���ŏI���
	}
	else if( isdigit( GetChar() ) )	// ���̐��l
	{
		// ���l����������i�߂�
		while( isdigit( GetChar() ) )
		{
			++m_offset;
		}
	}
	bool needNext = false;
	// ���ɏo�Ă���̂́A'.' �܂��� 'e', 'E' �̂����ꂩ
	if( GetChar() == '.' )	// �����_������ꍇ
	{
		++m_offset;	// �����_��i�߂�
		needNext = true;	// �����_�̌�͐������K�v
	}
	else if( GetChar() == 'e' || GetChar() == 'E' )	// �w����������ꍇ
	{
		++m_offset;	// �w�����̕�����i�߂�
		auto chkChar = GetChar();
		if( chkChar == '+' || chkChar == '-' )	// ����������ꍇ
		{
			++m_offset;	// ������i�߂�
		}
		needNext = true;	// �����_�̌�͐������K�v
	}
	if( needNext )	// �����_�܂��͎w����������ꍇ�A���ɐ������K�v
	{
		if( m_offset >= m_jsonText.length() || !isdigit( GetChar() ) )	// �������Ȃ��Ƃ����Ȃ�
		{
#if _HAS_CXX20
			throw new std::invalid_argument( std::format( "Parse error at m_offset {}: expected digit after 'e' or 'E' but found '{}'", m_offset, GetChar() ) );
#else
			throw new std::invalid_argument( SPRINTF( "Parse error at m_offset %u: expected digit after 'e' or 'E' but found '%c'", m_offset, GetChar() ) );
#endif
		}
		while( isdigit( GetChar() ) )	// ��������������i�߂�
		{
			++m_offset;
		}
	}
	// ���ۂ̐��l����؂�o��
	auto value = m_jsonText.substr( start, m_offset - start );
	return m_callback( NotificationType::Number, value );	// �R�[���o�b�N�ɒʒm
}
std::string_view JsonParser::ParseStringValue()
{
	_ASSERTE( GetChar() );
	++m_offset;
	auto start = m_offset;	// �J�n�ʒu���L�^

	std::string_view::value_type currChar = '\0';
	while( m_offset < m_jsonText.length() && (currChar=GetChar()) != '"' )
	{
		// �G�X�P�[�v�������o�Ă����ꍇ
		if( currChar == '\\' )
		{
			// �G�X�P�[�v�����̎��̕������擾
			++m_offset;
			currChar = GetChar();
			auto itr = std::find_if( EscapeChars.begin(), EscapeChars.end(), [currChar]( const auto& chars )
			{
				return chars.escapeChar == currChar;
			} );
			if( itr != EscapeChars.end() )
			{
				// �����R�[�h�w��̏ꍇ
				if( itr->escapeChar == 'u' )
				{
					for( int step = 0; step < 4; ++step )	// 4����16�i����ǂݎ��
					{
						++m_offset;
						if( !isxdigit( GetChar() ) )
						{
							// ����������O
#if _HAS_CXX20
							throw new std::invalid_argument( std::format( "Parse error at m_offset {}: expected hex digit after '\\u'", m_offset ) );
#else
							throw new std::invalid_argument( SPRINTF( "Parse error at m_offset %u: expected hex digit after '\\u'", m_offset ) );
#endif
						}
					}
				}
				else
				{
					++m_offset;	// �G�X�P�[�v��������i�߂�
				}
			}
			else
			{
				// �G�X�P�[�v��������`����Ă��Ȃ��ꍇ�͗�O
#if _HAS_CXX20
				throw new std::invalid_argument( std::format( "Parse error at m_offset {}: unexpected escape character '{}'", m_offset, currChar ) );
#else
				throw new std::invalid_argument( SPRINTF( "Parse error at m_offset %u: unexpected escape character '%c'", m_offset, currChar ) );
#endif
			}
		}
		else
		{
			++m_offset;	// �G�X�P�[�v�����ł͂Ȃ��̂ł��̂܂ܐi�߂�
		}
	}
	_ASSERTE( GetChar() == '"' );
	auto resultValue = m_jsonText.substr( start, m_offset-start );
	++m_offset;
	return resultValue;	// �؂�o�����������Ԃ�
}

}
