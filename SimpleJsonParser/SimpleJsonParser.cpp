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


static std::string LoadFile( const wchar_t* filePath )
{
	CAtlFile file;
	HRESULT hr = file.Create( filePath, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING );
	std::string filePathA = Morrin::JSON::ToString( filePath, CP_ACP );
	if( FAILED( hr ) )
	{
		std::wcerr << std::format( L"Failed to open file: {} (HRESULT: 0x{:08X})\n", filePath, hr );
		throw std::runtime_error( std::format( "Failed to open file: {} (HRESULT: 0x{:08X})", filePathA, hr ) );
	}
	ULONGLONG fileSize = 0;
	if( FAILED( file.GetSize( fileSize ) ) )
	{
		std::wcerr << std::format( L"Failed to get file size: {}\n", filePath );
		throw std::runtime_error( std::format( "Failed to get file size: {}", filePathA ) );
	}
	std::string result;
	result.resize(fileSize);
	if( FAILED( file.Read( result.data(), static_cast<DWORD>(result.length()) ) ) )
	{
		std::wcerr << std::format( L"Failed to read file: {}\n", filePath );
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
		std::wcout << L"argv[" << index << L"]: " << argv[index] << L"\n";
	}
	// コールバックで表示する
	for( int index = 1; index < argc; index++ )
	{
		std::string jsonText = LoadFile( argv[index] );

		Morrin::JSON::JsonKeyType firstKeyName;
		// 簡易パーサーでストレートな変換を試す(テストコードなのでこのまま展開出来ていればよい)
		std::wcout << L"Direct Parse of JSON:" << std::endl;
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
			std::wcout << msg;
			OutputDebugString( msg.c_str() );
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
		switch( jsonText[0] )
		{
		case '{':	_ASSERTE( obj.type() == typeid(Morrin::JSON::JsonObject) );	break;
		case '[':	_ASSERTE( obj.type() == typeid(Morrin::JSON::JsonArray) );	break;
		}
		// 何が来てるかわからないけど最初のオブジェクトのキーを取得してみる
		bool getVersion = false;
		if ( firstKeyName == "LaunchPackage" )
		{
			getVersion = true;
			firstKeyName = "LaunchList/3/MsiPackage/Version";
		}
		auto firstObj = Morrin::JSON::GetValue( obj, firstKeyName );
		if ( getVersion )
		{
			_ASSERTE( firstObj.has_value() && firstObj.type() == typeid(Morrin::JSON::JsonString) );
			auto version = Morrin::JSON::UnEscapeToWstring( std::any_cast<Morrin::JSON::JsonString>( firstObj ) );
			_ASSERTE( version.empty() == false );
			std::wcout << std::format( L"Version: `{}`\n", version );

		}
		else
		{
			_ASSERTE( firstObj.has_value() && firstObj.type() == typeid(Morrin::JSON::JsonObject) );
		}
	}
	return 0;
}
