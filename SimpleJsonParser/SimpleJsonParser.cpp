// SimpleJsonParser.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include "stdafx.h"
#include <stdio.h>

#include "JsonParser.h"
#include "StringConverter.h"

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
	if( FAILED( hr ) )
	{
		Trace( std::format( L"Failed to open file: {} (HRESULT: 0x{:08X})\n", filePath, hr ), std::wcerr );
		throw std::runtime_error( StringConverter::ToAnsi( std::format( L"Failed to open file: {} (HRESULT: 0x{:08X})", filePath, hr ) ) );
	}
	ULONGLONG fileSize = 0;
	if( FAILED( file.GetSize( fileSize ) ) )
	{
		Trace( std::format( L"Failed to get file size: {}\n", filePath ), std::wcerr );
		throw std::runtime_error( StringConverter::ToAnsi( std::format( L"Failed to get file size: {}", filePath ) ) );
	}
	std::string result;
	result.resize( static_cast<size_t>(fileSize) );
	if( FAILED( file.Read( result.data(), static_cast<DWORD>(result.length()) ) ) )
	{
		Trace( std::format( L"Failed to read file: {}\n", filePath ), std::wcerr );
		throw std::runtime_error( StringConverter::ToAnsi( std::format( L"Failed to read file: {}", filePath ) ) );
	}
	return result;
}
void DumpJson( const Morrin::JSON::JsonValue& value, int indent = 0 )
{
	const std::wstring indentStr( indent, ' ' );
	if( value.IsNull() )
	{
		std::wcout << indentStr << L"null" << std::endl;
	}
	else if( value.IsBoolean() )
	{
		std::wcout << indentStr << (value.AsBoolean() ? L"true" : L"false") << std::endl;
	}
	else if( value.IsInteger() )
	{
		std::wcout << indentStr << value.AsInteger() << std::endl;
	}
	else if( value.IsNumber() )
	{
		std::wcout << indentStr << value.AsNumber() << std::endl;
	}
	else if( value.IsString() )
	{
		std::wcout << indentStr << L'"' << value.AsString() << '"' << std::endl;
	}
	else if( value.IsArray() )
	{
		std::wcout << indentStr << L"[" << std::endl;
		for( const auto& item : value.AsArray() )
		{
			DumpJson( item, indent+1 );
		}
		std::wcout << indentStr << L"]" << std::endl;
	}
	else if( value.IsObject() )
	{
		std::wcout << indentStr << L"{" << std::endl;
		for( const auto& [key, val] : value.AsObject() )
		{
			std::wcout << indentStr << L"\"" << key << L"\": ";
			DumpJson( val, indent+1 );
		}
		std::wcout << indentStr << L"}" << std::endl;
	}
}

int wmain( int argc, wchar_t* argv[] )
{
	_wsetlocale( LC_ALL, L"" );

	// パラメータにセットされているファイル(JSON)をパースする
	for( int index = 0; index < argc; index++ )
	{
		Trace( std::format( L"argv[{}]: {}\n", index, argv[index] ) );
	}
	if( argc < 2 )
	{
		std::string json = R"({"name":"Copilot\u0041","age":2,"isAI":true,"skills":["C++","AI","JSON"],"nullValue":null})";
		auto root = Morrin::JSON::ParseJSON( json );
		DumpJson( root );
		return 0;
	}
	// コールバックで表示する
	for( int index = 1; index < argc; index++ )
	{
		std::string jsonText = LoadFile( argv[index] );
		Trace( std::format( L"Parsing JSON file: {}\n", argv[index] ) );
		auto root = Morrin::JSON::ParseJSON( jsonText );
		DumpJson( root );
	}
	return 0;
}
