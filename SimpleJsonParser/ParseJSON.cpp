// 簡易JSONパーサー

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
	std::string_view::value_type escapedChar;	// 実際の文字
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
	//	ホワイトスペースとして規定されている。isspace とはちょっと違うので定義を厳格化
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
	//	オフセットが範囲外なら例外を投げる
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
	// 要素が何かを確認する
	if( offset < jsonText.length() )
	{
		switch( GetChar( jsonText, offset ) )
		{
		// オブジェクト
		case '{':
			// 開始を通知
			if( !proc( NotificationType::StartObject, jsonText.substr( offset++, 1 ) ) )
			{
				return false;
			}
			if( !ParseValues( jsonText, offset, proc, '{', '}', ParseKeyValue ) )
			{
				return false;
			}
			return proc( NotificationType::EndObject, jsonText.substr( offset++, 1 ) );	// 終了を通知
		// 配列
		case '[':
			// 開始を通知
			if( !proc( NotificationType::StartArray, jsonText.substr( offset++, 1 ) ) )
			{
				return false;
			}
			if( !ParseValues( jsonText, offset, proc, '[', ']', ParseElement ) )
			{
				return false;
			}
			return proc( NotificationType::EndArray, jsonText.substr( offset++, 1 ) );	// 終了を通知
		// 文字列
		case '"':	return ParseString( jsonText, offset, proc );
		// そのほか(true, false, null, 数値)
		default:
			// リテラル値
			const auto& itr = std::find_if( LiteralValues.begin(), LiteralValues.end(),
				[&jsonText, offset]( const auto& value )
			{
				return jsonText.compare( offset, value.text.length(), value.text ) == 0;
			} );
			if( itr != LiteralValues.end() )
			{
				if( !proc( itr->type, itr->text ) )
				{
					return false;	// コールバックが false を返したら終了
				}
				offset += itr->text.length();	// オフセットを進める
				return true;
			}
			// 数値型
			if( isdigit( GetChar( jsonText, offset ) ) || GetChar( jsonText, offset ) == '-' )
			{
				return ParseNumber( jsonText, offset, proc );
			}
			_ASSERTE( isdigit( GetChar( jsonText, offset ) ) || GetChar( jsonText, offset ) == '-' || GetChar( jsonText, offset ) == '+' );
		}
		// ここにきたらパースエラー
		throw new std::invalid_argument( std::format( "Parse error at offset {}: unexpected character '{}'", offset, GetChar( jsonText, offset ) ) );
	}
	// 末尾に来てる…ここにくるんだっけ？
	return false;
}
// Object, Array の共通パーサー
static bool __stdcall ParseValues( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc,
	char startChar, char endChar, const std::function<bool( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc )>& parseProc )
{
	_ASSERTE( GetChar( jsonText, offset ) == startChar );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	// 開始を通知
	if( !proc( NotificationType::StartElement, jsonText.substr( offset++, 1 ) ) )
	{
		return false;
	}
	while( offset < jsonText.length() )
	{
		// 一要素をパースする
		if( !parseProc( jsonText, offset, proc ) )
		{
			return false;
		}
		// 空白文字がある可能性もあるのでスキップ
		offset = SkipWhiteSpace( jsonText, offset );
		// 終端記号が来る前にデータがなくなった(あり得ない)
		if( offset >= jsonText.length() )
		{
			throw new std::invalid_argument( std::format( "Parse error at offset {}: unexpected end of data", offset ) );
		}
		// 終端文字が来たのでブロック終了(必ずここに来る…はず)
		if( GetChar( jsonText, offset ) == endChar )
		{
			break;
		}
		_ASSERTE( GetChar( jsonText, offset ) == ',' );	//	区切り文字が来るはず
		if( GetChar( jsonText, offset ) != ',' )
		{
			throw new std::invalid_argument( std::format( "Parse error at offset {}: expected ',' but found '{}'", offset, GetChar( jsonText, offset ) ) );
		}
	}
	// ここには来ない(来るとしたら、エラーチェックをすり抜けて来る場合だけ)
	return true;
}

static bool __stdcall ParseKeyValue( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc )
{
	offset = SkipWhiteSpace( jsonText, offset );
	// objectブロック終わり
	if( GetChar( jsonText, offset ) == '}' )
	{
		return true;
	}
	_ASSERTE( GetChar( jsonText, offset ) == '"' );	//	キーは文字列であることを前提にしている
	auto key = ParseStringValue( jsonText, offset );
	offset  = SkipWhiteSpace( jsonText, offset );
	_ASSERTE( offset < jsonText.length() && GetChar( jsonText, offset ) == ':' );	//	キーと値の区切りはコロンであることを前提にしている
	if( offset >= jsonText.length() || GetChar( jsonText, offset ) != ':' )
	{
		throw new std::invalid_argument( std::format( "Parse error at offset {}: expected ':' but found '{}'", offset, GetChar( jsonText, offset ) ) );
	}
	offset++;
	if( !ParseElement( jsonText, offset, proc ) )
	{
		return false;	// コールバックが false を返したら終了
	}
	return true;
}
static bool __stdcall ParseString( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc )
{
	_ASSERTE( GetChar( jsonText, offset ) == '"' );	//	文字列はダブルクォートで囲まれていることを前提にしている
	auto strValue = ParseStringValue( jsonText, offset );
	return proc( NotificationType::String, strValue );
}
static bool __stdcall ParseNumber( const std::string_view& jsonText, std::string_view::size_type offset, const ParseCallBack& proc )
{
	// 数値データ。実際は、'0'(正の１未満の数値)、'-'(負の数値)、'1'～'9'(正の数値)のいずれかのはずだけど、+が入ってる場合を認める
	_ASSERTE( offset < jsonText.length() && (isdigit( GetChar( jsonText, offset ) ) || GetChar( jsonText, offset ) == '-') );
	auto start = offset;
	if( GetChar( jsonText, offset ) == '-' )	// 負の数値
	{
		++offset;
	}
	// 最初が0の場合次は'.'か、何もないかのいずれか
	if( GetChar( jsonText, offset ) == '0' )
	{
		++offset;	// 0 の場合は、次の文字が数字でない限りここで終わる
	}
	else if( isdigit( GetChar( jsonText, offset ) ) )	// 正の数値
	{
		// 数値が続く限り進める
		while( isdigit( GetChar( jsonText, offset ) ) )
		{
			++offset;
		}
	}
	bool needNext = false;
	// 次に出てくるのは、'.' または 'e', 'E' のいずれか
	if( GetChar( jsonText, offset ) == '.' )	// 小数点がある場合
	{
		++offset;	// 小数点を進める
		needNext = true;	// 小数点の後は数字が必要
	}
	else if( GetChar( jsonText, offset ) == 'e' || GetChar( jsonText, offset ) == 'E' )	// 指数部がある場合
	{
		++offset;	// 指数部の文字を進める
		auto chkChar = GetChar( jsonText, offset );
		if( chkChar == '+' || chkChar == '-' )	// 符号がある場合
		{
			++offset;	// 符号を進める
		}
		needNext = true;	// 小数点の後は数字が必要
	}
	if( needNext )	// 小数点または指数部がある場合、次に数字が必要
	{
		if( offset >= jsonText.length() || !isdigit( GetChar( jsonText, offset ) ) )	// 数字がないといけない
		{
			throw new std::invalid_argument( std::format( "Parse error at offset {}: expected digit after 'e' or 'E' but found '{}'", offset, GetChar( jsonText, offset ) ) );
		}
		while( isdigit( GetChar( jsonText, offset ) ) )	// 数字が続く限り進める
		{
			++offset;
		}
	}
	// 実際の数値部を切り出す
	auto value = jsonText.substr( start, offset - start );
	return proc( NotificationType::Number, value );	// コールバックに通知
}
static std::string_view ParseStringValue( const std::string_view& jsonText, std::string_view::size_type& offset )
{
	_ASSERTE( GetChar( jsonText, offset ) );
	++offset;
	auto start = offset;	// 開始位置を記録

	std::string_view::value_type currChar = '\0';
	while( offset < jsonText.length() && (currChar=GetChar( jsonText, offset )) != '"' )
	{
		// エスケープ文字が出てきた場合
		if( currChar == '\\' )
		{
			currChar = GetChar( jsonText, ++offset );	// エスケープ文字の次の文字を取得
			auto itr = std::find_if( EscapeChars.begin(), EscapeChars.end(), [currChar]( const auto& chars ){ return chars.escapeChar == currChar; } );
			if( itr != EscapeChars.end() )
			{
				// 文字コード指定の場合
				if( itr->escapeChar == 'u' )
				{
					for( int step = 0; step < 4; ++step )	// 4桁の16進数を読み取る
					{
						if( !isxdigit( GetChar( jsonText, ++offset ) ) )
						{
							// そもそも例外
							throw new std::invalid_argument( std::format( "Parse error at offset {}: expected hex digit after '\\u'", offset ) );
						}
					}
				}
				else
				{
					++offset;	// エスケープ文字分を進める
				}
			}
			else
			{
				// エスケープ文字が定義されていない場合は例外
				throw new std::invalid_argument( std::format( "Parse error at offset {}: unexpected escape character '{}'", offset, currChar ) );
			}
		}
		else
		{
			++offset;	// エスケープ文字ではないのでそのまま進める
		}
	}
	_ASSERTE( GetChar( jsonText, offset ) == '"' );
	auto resultValue = jsonText.substr( start, offset-start );
	++offset;
	return resultValue;	// 切り出した文字列を返す
}
///////////////////////////////////////////////////////////////////////////////
//	export Functions
bool __stdcall ParseJSON( const std::string_view& jsonText, ParseCallBack&& proc )
{
	std::string_view::size_type offset = 0;
	// 処理開始を通知(コールバック側の初期化を促せるようにしておく)
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
	//	終了を通知。offset の位置で判定可能なので必要があればそれを見ることとする
	_ASSERTE( offset <= jsonText.length() );
	//	オーバーしていると例外が出るので、末端を超えないようにする(プログラム的には末尾を超えることはないはずなのだが。。。)
	if( offset > jsonText.length() )	offset = jsonText.length();
	// 処理終了を通知して終わる
	return proc( NotificationType::EndParse, jsonText.substr( offset ) );	//	なにか残ってるという想定で全部送り込んでやる(さらにネスト的な処理があってもよいという判定)
}
std::any __stdcall ParseJSON( const std::string_view& jsonText, const concurrency::cancellation_token& token/* = concurrency::cancellation_token::none()*/ )
{
	std::any json;
	return json;
}
}
