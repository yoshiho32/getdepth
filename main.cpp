﻿//
// メインプログラム
//

// メッセージボックスの表示の準備
#if defined(_WIN32)
#  include <Windows.h>
#  include <atlstr.h>  
#endif

// アプリケーション本体
#include "GgApplication.h"

//
// メインプログラム
//
int main() try
{
  // アプリケーション本体
  GgApplication app(4, 3);

  // アプリケーションを実行する
  app.run();
}

catch (const std::exception &e)
{
  // エラーメッセージを表示する
#if defined(_WIN32)
  const CStringW message(e.what());
  MessageBox(NULL, LPCWSTR(message), TEXT("getDepth"), MB_OK | MB_ICONERROR);
#else
  std::cerr << e.what() << "\n\n[Type enter key] ";
  std::cin.get();
#endif

  // ブログラムを終了する
  return EXIT_FAILURE;
}
