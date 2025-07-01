#include "stdafx.h"
#include "JsonParser.h"
#include "StringConverter.h"
#include <iostream> // For std::cerr and std::endl

namespace Morrin::JSON
{
class JsonParser
{
private:
	std::string_view jsonText;
	concurrency::cancellation_token token;
	size_t currentIndex;

	std::string_view::value_type Current() const;
	void Advance();
	void SkipWhitespace();
	bool Match( std::string_view str );
	JsonValue ParseValue();
	JsonValue ParseObject();
	JsonValue ParseArray();
	std::wstring ParseString();
	JsonValue ParseNumber();
	JsonValue ParseBoolean();
	JsonValue ParseNull();

	void CheckCancellation() const
	{
		if( token.is_canceled() )
		{
			OperationCanceledException( "Operation canceled during JSON parsing." );
		}
	}

public:
	JsonParser( const std::string_view& jsonText, const concurrency::cancellation_token& token );
	JsonValue Parse();
};


JsonParser::JsonParser( const std::string_view& jsonText, const concurrency::cancellation_token& token )
	: jsonText( jsonText ), token( token ), currentIndex( 0 )
{
}

std::string_view::value_type JsonParser::Current() const
{
	return currentIndex < jsonText.size() ? jsonText[currentIndex] : '\0';
}

void JsonParser::Advance()
{
	if( currentIndex < jsonText.size() )
		++currentIndex;
}

void JsonParser::SkipWhitespace()
{
	while( currentIndex < jsonText.size() && std::isspace( Current() ) )
	{
		Advance();
	}
}
bool JsonParser::Match( std::string_view str )
{
	if( jsonText.substr( currentIndex, str.size() ) == str )
	{
		currentIndex += str.size();
		return true;
	}
	return false;
}

JsonValue JsonParser::ParseValue()
{
	CheckCancellation();
	SkipWhitespace();
	auto c = Current();

	switch( c )
	{
	case '{':	return ParseObject();
	case '[':	return ParseArray();
	case '"':	return ParseString();
	case 't':
	case 'f':	return ParseBoolean();
	case 'n':	return ParseNull();
	default:
		if( std::isdigit( c ) || c == '-' )
		{
			return ParseNumber();
		}
		throw std::runtime_error( "Invalid JSON value" );
	}
}

JsonValue JsonParser::ParseObject()
{
	JsonObject obj;
	Advance(); // Skip '{'

	while( Current() != '}' )
	{
		SkipWhitespace();
		std::wstring key = ParseString();
		SkipWhitespace();
		if( Current() != ':' ) throw std::runtime_error( "Expected ':' in JSON object" );
		Advance();
		JsonValue value = ParseValue();
		obj[key] = value;
		SkipWhitespace();

		if( Current() == ',' ) Advance();
		else if( Current() != '}' ) throw std::runtime_error( "Expected ',' or '}' in JSON object" );
	}

	Advance(); // Skip '}'
	return JsonValue( obj );
}

JsonValue JsonParser::ParseArray()
{
	JsonArray arr;
	Advance(); // Skip '['
	SkipWhitespace();

	while( Current() != ']' )
	{
		arr.push_back( ParseValue() );
		SkipWhitespace();

		if( Current() == ',' ) Advance();
		else if( Current() != ']' ) throw std::runtime_error( "Expected ',' or ']' in JSON array" );
	}

	Advance(); // Skip ']'
	return JsonValue( arr );
}

std::wstring JsonParser::ParseString()
{
	if( Current() != '"' )
	{
		throw std::runtime_error( "Expected '\"' at the start of JSON string" );
	}
	Advance(); // Skip '"'

	auto start = currentIndex;
	std::wstring str;
	std::string_view::value_type ch = '\0';
	while( (ch=Current()) != '"' )
	{
		// エスケープ処理
		if( ch == '\\' )
		{
			if( currentIndex != start && start != std::string_view::npos )
			{
				str += StringConverter::ToWString( jsonText.substr( start, currentIndex-start ) );
			}
			start = std::string_view::npos;
			Advance();
			ch = Current();
			switch( ch )
			{
			case '"': str += L'"'; Advance(); break;
			case '\\': str += L'\\'; Advance(); break;
			case '/': str += L'/'; Advance(); break;
			case 'b': str += L'\b'; Advance(); break;
			case 'f': str += L'\f'; Advance(); break;
			case 'n': str += L'\n'; Advance(); break;
			case 'r': str += L'\r'; Advance(); break;
			case 't': str += L'\t'; Advance(); break;
			case 'u':
				Advance(); // Skip 'u'
				if( currentIndex + 4 > jsonText.size() )
				{
					throw std::runtime_error( "Invalid Unicode escape sequence in JSON string" );
				}
				// Unicodeエスケープシーケンスを処理
				wchar_t codepoint = 0;
				for( int i = 0; i < 4; ++i )
				{
					ch = Current();
					Advance();
					codepoint <<= 4;
					if( std::isxdigit( ch ) )
					{
						if( std::isdigit( ch ) )
						{
							codepoint |= ch - '0';
						}
						else if( std::isupper( ch ) )
						{
							codepoint |= ch - 'A' + 10;
						}
						else
						{
							codepoint |= ch - 'a' + 10;
						}
					}
					else
					{
						throw std::runtime_error( "Invalid Unicode character at position " + std::to_string( currentIndex + i ) );
					}
				}
				str += codepoint; // Add the decoded Unicode character to the string
				break;
			}
		}
		else
		{
			if( start == std::string_view::npos )
			{
				start = currentIndex; // Set start position if not already set
			}
			Advance();
		}
	}
	if( start != std::string_view::npos )
	{
		str += StringConverter::ToWString( jsonText.substr( start, currentIndex - start ) );
	}
	_ASSERTE( Current() == '"' ); // Ensure we are at the end of the string
	Advance(); // Skip '"'
	return str;
}

JsonValue JsonParser::ParseNumber()
{
	SkipWhitespace();
	auto start = currentIndex;
	if( Current() == '-' ) // Handle negative numbers
	{
		Advance();
	}
	while( std::isdigit( Current() ) )
	{
		Advance();
	}
	// 小数点が出てきた場合は、doubleで処理する。
	if( Current() == '.' ) // Handle decimal point
	{
		Advance();
		while( std::isdigit( Current() ) )
		{
			Advance();
		}
		double value = std::stod( std::string( jsonText.substr( start, currentIndex - start ) ) );
		return JsonValue( value );
	}
	// 小数点が出てこない場合は整数として処理する（のちの効率重視）
	int value = std::stoi( std::string( jsonText.substr( start, currentIndex - start ) ) );
	return JsonValue( value );
}

JsonValue JsonParser::ParseBoolean()
{
	if( Match( "true" ) )
	{
		return JsonValue( true );
	}
	else if( Match( "false" ) )
	{
		return JsonValue( false );
	}
	throw std::runtime_error( "Invalid JSON boolean" );
}

JsonValue JsonParser::ParseNull()
{
	if( Match( "null" ) )
	{
		return JsonValue( nullptr );
	}
	throw std::runtime_error( "Invalid JSON null" );
}

JsonValue JsonParser::Parse()
{
	CheckCancellation();
	if( StringConverter::IsUtf8BOM( jsonText ) )
	{
		// UTF-8 BOMがある場合は、BOMをスキップする
		currentIndex += 3;
	}
	return ParseValue();
}

JsonValue __stdcall ParseJSON( const std::string_view& jsonText, const concurrency::cancellation_token& token )
{
    JsonParser parser(jsonText, token);
    return parser.Parse();
}
}
