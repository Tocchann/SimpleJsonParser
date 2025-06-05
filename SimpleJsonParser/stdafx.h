// 本来なら、そのプロジェクトのプリコンパイルヘッダーが含まれる。
// ParseJSON.h で参照するためのダミーヘッダー(ほかのVCプロジェクトでそのままコピーできるようにしておく(最新はpch.hだけどねｗ))
#pragma once

#define STRICT
#include <windows.h>

// C++20以前でビルドする設定にした場合に std::format の代わりの処理をおこなう段取りで利用する({} も使えないから#if で切り分けがいるけどねｗ)
#include <atlbase.h>
#include <atlstr.h>
