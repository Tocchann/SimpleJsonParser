// SimpleJsonParser.cpp : このファイルには 'main' 関数が含まれています。プログラム実行の開始と終了がそこで行われます。
//

#include <windows.h>
#include <stdio.h>

#include "ParseJSON.h"

#include <atlfile.h>
#include <atlconv.h>

#include <iostream>
#include <format>



static std::string LoadFile( const wchar_t* filePath )
{
	CAtlFile file;
	HRESULT hr = file.Create( filePath, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING );
	CW2AEX<MAX_PATH> cw2a( filePath, CP_ACP );
	LPCSTR filePathA = cw2a;
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

		auto result = Morrin::JSON::ParseJSON( jsonText, []( const Morrin::JSON::NotificationType& type, const std::string_view& value ) -> bool
		{
			// ここをトレースするだけでよい
			return true; // trueを返すと、さらに子要素のパースを続ける
		} );
		if( !result )
		{
			break;
		}
	}
	return 0;
}

// プログラムの実行: Ctrl + F5 または [デバッグ] > [デバッグなしで開始] メニュー
// プログラムのデバッグ: F5 または [デバッグ] > [デバッグの開始] メニュー

// 作業を開始するためのヒント: 
//    1. ソリューション エクスプローラー ウィンドウを使用してファイルを追加/管理します 
//   2. チーム エクスプローラー ウィンドウを使用してソース管理に接続します
//   3. 出力ウィンドウを使用して、ビルド出力とその他のメッセージを表示します
//   4. エラー一覧ウィンドウを使用してエラーを表示します
//   5. [プロジェクト] > [新しい項目の追加] と移動して新しいコード ファイルを作成するか、[プロジェクト] > [既存の項目の追加] と移動して既存のコード ファイルをプロジェクトに追加します
//   6. 後ほどこのプロジェクトを再び開く場合、[ファイル] > [開く] > [プロジェクト] と移動して .sln ファイルを選択します
