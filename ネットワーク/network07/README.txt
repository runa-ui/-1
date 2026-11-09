ネットワークシステム 第7回：複数人チャット

【配布物】
chat-server.cpp   複数人へMSGを配信するサーバ
chat-client.cpp   入力中も受信を続けるクライアント
PROTOCOL.txt      第5・6回からの変更点と通信仕様
README.txt        このファイル

【環境】
macOS / Linux / WSL、C++17対応g++またはclang++。
接続先は127.0.0.1（同じPCのみ）、ポートは8081です。
第6回のchat-serverを動かしたままにせず、Ctrl+Cで終了してください。

【コンパイル】（ネットワークシステムのディレクトリから）
cd network07
g++ -std=c++17 -pthread chat-server.cpp -o chat-server
g++ -std=c++17 -pthread chat-client.cpp -o chat-client

※macOSでg++がclang++の場合もC++17でビルドできます。
※Jenkinsなどとポートが競合した場合、両方の.cppのPORTを同じ番号にして再ビルド。

【起動と操作】
ターミナルA： ./chat-server
ターミナルB： ./chat-client
ターミナルC： ./chat-client

Bで JOIN hana、Cで JOIN yuki と入力し、Enter。
Bで SAY hello、Cで SAY こんにちは と入力すると、両方に発言が届きます。
JOINした後は、SAYを省き、文章だけ入力しても送信できます。
終了するときは QUIT または /quit と入力します。
誰かが参加・退出すると「* ... joined / left」が表示されます。
参加者自身にもMSGが届きます。

【入力できる命令】
JOIN hana    名前を登録（英数字と_、1～16バイト）
SAY hello    発言（1～200バイト、日本語・空白可）
QUIT         終了
/join hana   JOINのショートカット
/quit        QUITのショートカット
こんにちは   JOIN後にそのまま打つとSAYとして送信

【実習の範囲】
・第5回：通信の区切りと要求／応答
・第6回：接続ごとのサーバスレッド
・第7回：全員への発言配信＋クライアントの受信スレッド

第5回のRENAME、第6回の課題(1)(2)は未実装でも動作します。
/users と /history は第7回の任意課題です。配布サーバではまだ対応しておらず、
ERR UNKNOWN_COMMANDが返ります。
入力中の他人の発言でプロンプトが乱れることがあります。
この表示の改善も任意課題です。

【注意事項】
通信は暗号化されず、ログイン認証はありません。学習用として同じPC内で使用してください。
複数人の同時送信・切断時に簡略化されている処理や、実運用上の限界があります。
ソースのビルドと実行確認が最低条件、追加課題の実装は加点要素です。
