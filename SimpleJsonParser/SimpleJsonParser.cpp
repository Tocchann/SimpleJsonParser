// SimpleJsonParser.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include "stdafx.h"
#include <stdio.h>

#include "ParseJSON.h"

#include <atlfile.h>
#include <atlconv.h>

#include <iostream>
#include <format>
#include <sstream>

inline static void Trace( const std::wstring& msg, std::wostream& out = std::wcout )
{
	OutputDebugString( msg.c_str() );
	out << msg;
}

static std::string LoadFile( const wchar_t* filePath )
{
	CAtlFile file;
	HRESULT hr = file.Create( filePath, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING );
	std::string filePathA = Morrin::JSON::ToString( filePath, CP_ACP );
	if( FAILED( hr ) )
	{
		Trace( std::format( L"Failed to open file: {} (HRESULT: 0x{:08X})\n", filePath, hr ), std::wcerr );
		throw std::runtime_error( std::format( "Failed to open file: {} (HRESULT: 0x{:08X})", filePathA, hr ) );
	}
	ULONGLONG fileSize = 0;
	if( FAILED( file.GetSize( fileSize ) ) )
	{
		Trace( std::format( L"Failed to get file size: {}\n", filePath ), std::wcerr );
		throw std::runtime_error( std::format( "Failed to get file size: {}", filePathA ) );
	}
	std::string result;
	result.resize( static_cast<size_t>(fileSize) );
	if( FAILED( file.Read( result.data(), static_cast<DWORD>(result.length()) ) ) )
	{
		Trace( std::format( L"Failed to read file: {}\n", filePath ), std::wcerr );
		throw std::runtime_error( std::format( "Failed to read file: {}", filePathA ) );
	}
	return result;
}
int wmain( int argc, wchar_t* argv[] )
{
	wchar_t szLocale[MAX_PATH];
	GetLocaleInfo( GetThreadLocale(), LOCALE_SABBREVLANGNAME, szLocale, MAX_PATH );
	_wsetlocale( LC_ALL, szLocale );

	// パラメータにセットされているファイル(JSON)をパースする
	for( int index = 0; index < argc; index++ )
	{
		Trace( std::format( L"argv[{}]: {}\n", index, argv[index] ) );
	}
	// コールバックで表示する
	for( int index = 1; index < argc; index++ )
	{
		std::string jsonText = LoadFile( argv[index] );

		std::string firstKeyName;
		// 簡易パーサーでストレートな変換を試す(テストコードなのでこのまま展開出来ていればよい)
		Trace( std::format( L"Parsing JSON file: {}\n", argv[index] ) );
		auto result = Morrin::JSON::ParseJSON( jsonText, [&firstKeyName]( const Morrin::JSON::NotificationType& type, const std::string_view& value ) -> bool
		{
			std::wstring typeText;
			switch( type )
			{
			case Morrin::JSON::NotificationType::StartParse:	typeText = L"StartParse"; break;
			case Morrin::JSON::NotificationType::EndParse:		typeText = L"EndParse"; break;
			case Morrin::JSON::NotificationType::StartObject:	typeText = L"StartObject"; break;
			case Morrin::JSON::NotificationType::EndObject:		typeText = L"EndObject"; break;
			case Morrin::JSON::NotificationType::StartArray:	typeText = L"StartArray"; break;
			case Morrin::JSON::NotificationType::EndArray:		typeText = L"EndArray"; break;
			case Morrin::JSON::NotificationType::Key:			typeText = L"Key"; break;
			case Morrin::JSON::NotificationType::String:		typeText = L"String"; break;
			case Morrin::JSON::NotificationType::Number:		typeText = L"Number"; break;
			case Morrin::JSON::NotificationType::BooleanTrue:	typeText = L"BooleanTrue"; break;
			case Morrin::JSON::NotificationType::BooleanFalse:	typeText = L"BooleanFalse"; break;
			case Morrin::JSON::NotificationType::Null:			typeText = L"Null"; break;
			default:											typeText = std::format( L"Unknown NotificationType: {}", static_cast<int>(type) );	break;
			}
			std::wstring msg;
			if( type == Morrin::JSON::NotificationType::StartParse )
			{
				msg = std::format( L"Value:`{}`,\t\tNotificationType::{}, \n", Morrin::JSON::ToWString( value ), typeText );
			}
			else
			{
				msg = std::format( L"Value:`{}`,\t\tNotificationType::{}, \n", Morrin::JSON::UnEscapeToWstring( value ), typeText );
			}
			if( firstKeyName.empty() && type == Morrin::JSON::NotificationType::Key )
			{
#ifdef FORCE_REFERENCE_VALUE
				firstKeyName = value;
#else
				firstKeyName = Morrin::JSON::UnEscapeToString( value );
#endif
			}
			Trace( msg );
			// ここをトレースするだけでよい
			return true; // trueを返すと、さらに子要素のパースを続ける
		} );
		_ASSERTE( result );
		if( !result )
		{
			break;
		}
		// こっちは純粋にデバッグ実行程度しかしないユニットテストにしてないのでまぁ面倒ごとはいらないでしょうｗ
		auto obj = Morrin::JSON::ParseJSON( jsonText );
		_ASSERTE( obj.has_value() );
		int firstCharPos = 0;
		if ( static_cast<BYTE>(jsonText[0]) == 0xEF && static_cast<BYTE>(jsonText[1]) == 0xBB && static_cast<BYTE>(jsonText[2]) == 0xBF )
		{
			firstCharPos = 3;
		}
		switch( jsonText[firstCharPos] )
		{
		case '{':	_ASSERTE( obj.type() == typeid(Morrin::JSON::JsonObject) );	break;
		case '[':	_ASSERTE( obj.type() == typeid(Morrin::JSON::JsonArray) );	break;
		}
		// 何が来てるかわからないけど最初のオブジェクトのキーを取得してみる
		bool getVersion = false;
		if ( firstKeyName == "LaunchPackage" )
		{
			auto arr = Morrin::JSON::GetValue( obj, "LaunchList" );
			_ASSERTE( arr.has_value() && arr.type() == typeid(Morrin::JSON::JsonArray) );
			auto arrObj = std::any_cast<Morrin::JSON::JsonArray>(arr);

			// MsiPackageを探す
			for ( int index = 0; index < arrObj.size(); ++index )
			{
				_ASSERTE( arrObj[index].has_value() && arrObj[index].type() == typeid(Morrin::JSON::JsonObject) );
				auto jsonObj = std::any_cast<Morrin::JSON::JsonObject>( arrObj[index] );
				if ( jsonObj.find( "MsiPackage" ) != jsonObj.end() )
				{
					firstKeyName = std::format( "LaunchList/{}/MsiPackage/Version", index );
					break;
				}
			}
			getVersion = true;
		}
		Trace( std::format( L"GetValue(`{}`)\n", Morrin::JSON::UnEscapeToWstring( firstKeyName ) ) );
		auto searchObj = Morrin::JSON::GetValue( obj, firstKeyName );
		if ( getVersion )
		{
			_ASSERTE( searchObj.has_value() && searchObj.type() == typeid(Morrin::JSON::JsonString) );
#ifdef FORCE_REFERENCE_VALUE
			auto version = Morrin::JSON::UnEscapeToWstring( std::any_cast<Morrin::JSON::JsonString>( firstObj ) );
#else
			auto version = std::any_cast<Morrin::JSON::JsonString>(searchObj);
#endif
			_ASSERTE( version.empty() == false );
			Trace( std::format( L"Version: `{}`\n", version ) );
		}
		else
		{
			_ASSERTE( searchObj.has_value() );
			Trace( std::format( L"obj[{0}].type() == {1}", Morrin::JSON::ToWString( firstKeyName ), Morrin::JSON::ToWString( searchObj.type().name() ) ) );
		}
	}
	return 0;
}
