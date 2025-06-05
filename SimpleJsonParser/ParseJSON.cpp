// 簡易JSONパーサー

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
		// ホワイトスペースとして規定されている。isspace とはちょっと違うので定義を厳格化
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
	const std::string_view& m_jsonText;	// JSON テキスト
	std::string_view::size_type m_offset;	// 現在のオフセット
	const ParseCallBack& m_callback;	// コールバック関数
};
//---------------
//	prototype
//---------------
inline std::any& AddValue( std::any* anyObj, const JsonKeyType& keyName, std::any&& value )
{
	std::any& parentObj = *anyObj;
	// 何かしら値を持っている&& 親オブジェクトはJsonObjectまたはJsonArrayになっている(他はあり得ないパターン)
	_ASSERTE( parentObj.has_value() && (parentObj.type() == typeid(JsonObject) || parentObj.type() == typeid(JsonArray)) );
	// JsonObject の場合はキーとペアで保存する(ルート要素はダミーをいれているので空にならないようになっている)
	if( parentObj.type() == typeid(JsonObject) )
	{
		auto& jsonObj = std::any_cast<JsonObject&>(parentObj);
		auto pair = std::make_pair( keyName, value );
		jsonObj.insert( pair );
		return jsonObj[keyName];
	}
	// JsonArray の場合は配列要素として追加する(keyNameは無視)
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
	// スタックに追加する
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
	// 構造化したデータとして保持する
	std::stack<std::any*> stack;	//	参照(ポインタ)を持たせることでコピーを防ぐ
	JsonKeyType rootName = "RootObject";
	JsonKeyType keyName = rootName;
	JsonObject rootObj;
	std::any topObj = std::make_any<JsonObject>( rootObj );	// ルートオブジェクトを格納するための仮想親オブジェクト
	stack.push( &topObj );

	auto result = ParseJSON( jsonText, [&]( const Morrin::JSON::NotificationType& type, const std::string_view& value ) -> bool
	{
		if( token.is_canceled() )
		{
			// キャンセルされたら、falseを返す
			return false;
		}
		switch( type )
		{
		case Morrin::JSON::NotificationType::Key:			keyName = UnEscapeToString( value );		break;
		case Morrin::JSON::NotificationType::StartObject:	PushValueT( stack, keyName, JsonObject() );	break;
		case Morrin::JSON::NotificationType::StartArray:	PushValueT( stack, keyName, JsonArray() );	break;
		case Morrin::JSON::NotificationType::EndObject:
		case Morrin::JSON::NotificationType::EndArray:		stack.pop();	break;
		// 値型はそのまま追加するだけでよいはず…
		case Morrin::JSON::NotificationType::String:		AddValue( stack.top(), keyName, std::make_any<JsonString>( UnEscapeToWstring(value) ) );	break;
		case Morrin::JSON::NotificationType::Number:		AddValue( stack.top(), keyName, std::make_any<int>( ConvertToInt( value ) ) );				break;
		case Morrin::JSON::NotificationType::BooleanTrue:	AddValue( stack.top(), keyName, std::make_any<bool>( true ) );								break;
		case Morrin::JSON::NotificationType::BooleanFalse:	AddValue( stack.top(), keyName, std::make_any<bool>( false ) );							break;
		case Morrin::JSON::NotificationType::Null:			AddValue( stack.top(), keyName, std::any() );												break;
		case Morrin::JSON::NotificationType::StartParse:
		case Morrin::JSON::NotificationType::EndParse:		break;	//	パース開始と終了は何もしないので無視(defaultに行かないようにブロック)
		default:
			// 想定外は例外を飛ばす
#if _HAS_CXX20
			throw new std::runtime_error( std::format( "Unknown NotificationType: {}", static_cast<int>(type) ) );
#else
			throw new std::runtime_error( SPRINTF( "Unknown NotificationType: %d", static_cast<int>(type) ) );
#endif
		}
		return true; // trueを返すと、さらに子要素のパースを続ける	)
	} );
	// スタック状態にかかわりなくルートが取れるようになっていないと困る
	auto& top = std::any_cast<JsonObject&>(*stack.top());
	//auto top = std::any_cast<JsonObject>(topObj);
	return top[rootName];
}
std::any APIENTRY GetValue( std::any obj, const JsonRefKeyType& searchKey )
{
	_ASSERTE( obj.has_value() );
	// 検索要素が何もない場合は、objをそのまま返す
	if( searchKey.empty() )
	{
		return obj;
	}
	// データなしを返す。多少面倒でもゼロアロケーションは必須
	JsonRefKeyType::size_type start = 0;
	while( obj.has_value() && start < searchKey.length() )
	{
		// データがある限り頑張って潜る(再帰でも同じだけどここはループで処理する)
		auto last = searchKey.find( '/', start ); // 見つからなければ npos でその場合は substr で残り全部になる
		JsonRefKeyType key{ searchKey.substr( start, last != searchKey.npos ? last-start : last ) };
		// 検索キーが抽出できた(//とか入れている場合は単純スキップする。)
		_ASSERTE( key.empty() == false );	//	そもそも空要素で検出されることはないはず…
		if( !key.empty() )
		{
			// JsonArray(実体はstd::vector)
			if( isdigit( key[0] ) )
			{
				// string_view 版 atoi に相当するものがないので自力で回す(ここは、0以上の正数しか来ないので、単純ループでよい)
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
			// JsonObject(実体は、std::map)
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
		// これ以上検索するものがない
		if( last == searchKey.npos || last+1 >= searchKey.length() )
		{
			break;
		}
		// 次の要素を相手に探す
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
		//	エスケープ記号
		if( value[pos] == '\\' )
		{
			SetPrevStr( value, pos, prevPos, resultStr );
			prevPos = std::string_view::npos;	//	エスケープ文字をセットしたので、以前の位置をクリアー
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
				// エスケープ文字以外が来たらそのままスキップ(無視する)
				if( itr != EscapeChars.end() )
				{
					//	エスケープ文字が定義されているので、変換する
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
// テキストのアンエスケープ(速度重視のため、共通化しない)
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
	// 処理開始を通知(コールバック側の初期化を促せるようにしておく)
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
	//	終了を通知。m_offset の位置で判定可能なので必要があればそれを見ることとする
	_ASSERTE( m_offset <= m_jsonText.length() );
	//	オーバーしていると例外が出るので、末端を超えないようにする(プログラム的には末尾を超えることはないはずなのだが。。。)
	if( m_offset > m_jsonText.length() )	m_offset = m_jsonText.length();
	// 処理終了を通知して終わる
	return m_callback( NotificationType::EndParse, m_jsonText.substr( m_offset ) );
}
	
bool JsonParser::ParseElement()
{
	_ASSERTE( m_offset < m_jsonText.length() );
	SkipWhiteSpace();
	// 要素が何かを確認する
	if( m_offset < m_jsonText.length() )
	{
		switch( GetChar() )
		{
		// オブジェクト
		case '{':
			// 開始を通知
			return ParseValues( NotificationType::StartObject, NotificationType::EndObject, &JsonParser::ParseKeyValue );
		// 配列
		case '[':
			return ParseValues( NotificationType::StartArray, NotificationType::EndArray, &JsonParser::ParseElement );
		// 文字列
		case '"':	return ParseString();
		// リテラル(true, false, null), 数値
		default:
			// リテラル値
			const auto& itr = std::find_if( LiteralValues.begin(), LiteralValues.end(), [&]( const auto& value )
			{
				return m_jsonText.compare( m_offset, value.text.length(), value.text ) == 0;
			} );
			if( itr != LiteralValues.end() )
			{
				if( !m_callback( itr->type, itr->text ) )
				{
					return false;	// コールバックが false を返したら終了
				}
				m_offset += itr->text.length();	// オフセットを進める
				return true;
			}
			// 数値型
			if( isdigit( GetChar() ) || GetChar() == '-' )
			{
				return ParseNumber();
			}
			// ここに来てる時点でパースエラー
			break;
		}
		// ここにきたらパースエラー
#if _HAS_CXX20
		throw new std::invalid_argument( std::format( "Parse error at m_offset {}: unexpected character '{}'", m_offset, GetChar() ) );
#else
		throw new std::invalid_argument( SPRINTF( "Parse error at m_offset %u: unexpected character '%c'", m_offset, GetChar() ) );
#endif
	}
	// 末尾に来てる…ここにくるんだっけ？
	return false;
}
// Object, Array の共通パーサー
bool JsonParser::ParseValues( NotificationType start, NotificationType end, bool (JsonParser::* parseProc)() )
{
	_ASSERTE( GetChar() == static_cast<char>(start) );	//	手前でスキップしないので、ここで再度値チェックできるようにしておく
	if( !m_callback( start, m_jsonText.substr( m_offset, 1 ) ) )
	{
		return false;
	}
	while( m_offset+1 < m_jsonText.length() )
	{
		++m_offset;	// 直前要素(最初なら '{' or '[', 途中なら ',')をスキップ
		// 一要素をパースする
		if( !(this->*parseProc)() )
		{
			return false;
		}
		// 空白文字がある可能性もあるのでスキップ
		SkipWhiteSpace();
		// 終端記号が来る前にデータがなくなった(あり得ない)
		if( m_offset >= m_jsonText.length() )
		{
#if _HAS_CXX20
			throw new std::invalid_argument( std::format( "Parse error at m_offset {}: unexpected end of data", m_offset ) );
#else
			throw new std::invalid_argument( SPRINTF( "Parse error at m_offset %u: unexpected end of data", m_offset ) );
#endif
		}
		// 終端文字が来たのでブロック終了(必ずここに来る…はず)
		if( GetChar() == static_cast<char>(end) )
		{
			break;
		}
		_ASSERTE( GetChar() == ',' );	//	区切り文字が来るはず
		if( GetChar() != ',' )
		{
#if _HAS_CXX20
			throw new std::invalid_argument( std::format( "Parse error at m_offset {}: expected ',' but found '{}'", m_offset, GetChar() ) );
#else
			throw new std::invalid_argument( SPRINTF( "Parse error at m_offset %u: expected ',' but found '%c'", m_offset, GetChar() ) );
#endif
		}
	}
	// ここには来ない(来るとしたら、エラーチェックをすり抜けて来る場合だけ)
	return m_callback( end, m_jsonText.substr( m_offset++, 1 ) );	// 終了を通知
}

bool JsonParser::ParseKeyValue()
{
	SkipWhiteSpace();
	// objectブロック終わり
	if( GetChar() == '}' )
	{
		return true;
	}
	_ASSERTE( GetChar() == '"' );	//	キーは文字列であることを前提にしている
	auto key = ParseStringValue();
	if( !m_callback( NotificationType::Key, key ) )
	{
		return false;
	}
	SkipWhiteSpace();
	_ASSERTE( m_offset < m_jsonText.length() && GetChar() == ':' );	//	キーと値の区切りはコロンであることを前提にしている
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
		return false;	// コールバックが false を返したら終了
	}
	return true;
}
bool JsonParser::ParseString()
{
	_ASSERTE( GetChar() == '"' );	//	文字列はダブルクォートで囲まれていることを前提にしている
	auto strValue = ParseStringValue();
	return m_callback( NotificationType::String, strValue );
}
bool JsonParser::ParseNumber()
{
	// 数値データ。実際は、'0'(正の１未満の数値)、'-'(負の数値)、'1'～'9'(正の数値)のいずれかのはずだけど、+が入ってる場合を認める
	_ASSERTE( m_offset < m_jsonText.length() && (isdigit( GetChar() ) || GetChar() == '-') );
	auto start = m_offset;
	if( GetChar() == '-' )	// 負の数値
	{
		++m_offset;
	}
	// 最初が0の場合次は'.'か、何もないかのいずれか
	if( GetChar() == '0' )
	{
		++m_offset;	// 0 の場合は、次の文字が数字でない限りここで終わる
	}
	else if( isdigit( GetChar() ) )	// 正の数値
	{
		// 数値が続く限り進める
		while( isdigit( GetChar() ) )
		{
			++m_offset;
		}
	}
	bool needNext = false;
	// 次に出てくるのは、'.' または 'e', 'E' のいずれか
	if( GetChar() == '.' )	// 小数点がある場合
	{
		++m_offset;	// 小数点を進める
		needNext = true;	// 小数点の後は数字が必要
	}
	else if( GetChar() == 'e' || GetChar() == 'E' )	// 指数部がある場合
	{
		++m_offset;	// 指数部の文字を進める
		auto chkChar = GetChar();
		if( chkChar == '+' || chkChar == '-' )	// 符号がある場合
		{
			++m_offset;	// 符号を進める
		}
		needNext = true;	// 小数点の後は数字が必要
	}
	if( needNext )	// 小数点または指数部がある場合、次に数字が必要
	{
		if( m_offset >= m_jsonText.length() || !isdigit( GetChar() ) )	// 数字がないといけない
		{
#if _HAS_CXX20
			throw new std::invalid_argument( std::format( "Parse error at m_offset {}: expected digit after 'e' or 'E' but found '{}'", m_offset, GetChar() ) );
#else
			throw new std::invalid_argument( SPRINTF( "Parse error at m_offset %u: expected digit after 'e' or 'E' but found '%c'", m_offset, GetChar() ) );
#endif
		}
		while( isdigit( GetChar() ) )	// 数字が続く限り進める
		{
			++m_offset;
		}
	}
	// 実際の数値部を切り出す
	auto value = m_jsonText.substr( start, m_offset - start );
	return m_callback( NotificationType::Number, value );	// コールバックに通知
}
std::string_view JsonParser::ParseStringValue()
{
	_ASSERTE( GetChar() );
	++m_offset;
	auto start = m_offset;	// 開始位置を記録

	std::string_view::value_type currChar = '\0';
	while( m_offset < m_jsonText.length() && (currChar=GetChar()) != '"' )
	{
		// エスケープ文字が出てきた場合
		if( currChar == '\\' )
		{
			// エスケープ文字の次の文字を取得
			++m_offset;
			currChar = GetChar();
			auto itr = std::find_if( EscapeChars.begin(), EscapeChars.end(), [currChar]( const auto& chars )
			{
				return chars.escapeChar == currChar;
			} );
			if( itr != EscapeChars.end() )
			{
				// 文字コード指定の場合
				if( itr->escapeChar == 'u' )
				{
					for( int step = 0; step < 4; ++step )	// 4桁の16進数を読み取る
					{
						++m_offset;
						if( !isxdigit( GetChar() ) )
						{
							// そもそも例外
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
					++m_offset;	// エスケープ文字分を進める
				}
			}
			else
			{
				// エスケープ文字が定義されていない場合は例外
#if _HAS_CXX20
				throw new std::invalid_argument( std::format( "Parse error at m_offset {}: unexpected escape character '{}'", m_offset, currChar ) );
#else
				throw new std::invalid_argument( SPRINTF( "Parse error at m_offset %u: unexpected escape character '%c'", m_offset, currChar ) );
#endif
			}
		}
		else
		{
			++m_offset;	// エスケープ文字ではないのでそのまま進める
		}
	}
	_ASSERTE( GetChar() == '"' );
	auto resultValue = m_jsonText.substr( start, m_offset-start );
	++m_offset;
	return resultValue;	// 切り出した文字列を返す
}

}
