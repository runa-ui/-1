ネットワークシステム 第5回：チャットの通信仕様

【ファイル】
PROTOCOL.txt          今回決める通信仕様（必ず最初に確認）
protocol-server.cpp   1接続ずつ処理する確認用サーバ
protocol-client.cpp   JOIN・SAY・QUITを入力して送信するクライアント
framing-demo.cpp      recvの分割・結合を模擬する短いプログラム（通信しない）

【環境・コンパイル】
macOS / Linux / 用意済みのWSL向けです。C++17対応のg++（またはclang++）を使います。
「ネットワークシステム」フォルダから次を実行：
  cd network05
  g++ -std=c++17 protocol-server.cpp -o protocol-server
  g++ -std=c++17 protocol-client.cpp -o protocol-client
  g++ -std=c++17 framing-demo.cpp -o framing-demo

【最初に試す：区切りの確認】
  ./framing-demo
これはネットワークを使わず、受信データが複数のかたまりになった場合を模擬します。
「JOIN ha」だけでは1行完成しません。次のかたまりにある改行で1行を取り出します。
1回のrecvと1メッセージが対応するとは限らないのがポイントです。

【最初の実行】
前回のサーバが動いていたらCtrl+Cで停止してください。
端末A（network05フォルダ）： ./protocol-server
端末B（network05フォルダ）： ./protocol-client

端末Bに、以下の順に入力してEnter：
  JOIN hana
  SAY hello
  SAY こんにちは
  QUIT

想定されるサーバの応答：
  OK JOIN hana
  MSG hana hello
  MSG hana こんにちは
  BYE

端末Bが終了しても、サーバは次の接続を待ち受けます。
終了はサーバの端末でCtrl+C。
通信は同じPCの127.0.0.1:8080のみ（別PCからは接続不可）。
8080番を別のプログラムが使用中なら、サーバのbindが失敗します。
その場合は競合するプログラムを止めるか、実験用に両方の.cppのPORTを同じ別番号へ変更します。
配布版のPORTは8080で統一しています。

【今回の通信仕様（概略）】
・UTF-8のテキストを使い、1行（末尾のLF=\n）を1つのメッセージとします。
・要求：JOIN 名前 ／ SAY 文章 ／ QUIT
・応答：OK JOIN 名前 ／ MSG 名前 文章 ／ BYE ／ ERR 理由
・JOINの名前は英数字・_ の1～16バイト。登録後はSAYを送れます。
・SAYの文章は1～200バイト。文章には空白・日本語を使用できます（改行不可）。
・1行は改行を除き最大256バイト。超過時はERR TOO_LONGを返して切断します。
・SAYの文章が200バイトを超えた場合はERR TEXT_TOO_LONGを返し、接続を継続します。
・SAYのMSGは今は送信者自身に返すだけ。複数人への配信ではありません。
・不正な命令はERRで返し、通常は接続を維持します。詳細はPROTOCOL.txt。

