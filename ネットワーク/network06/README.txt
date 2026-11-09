ネットワークシステム 第6回：複数接続するチャットサーバ

【ファイル】
chat-server-sequential.cpp  1人ずつ処理する比較用サーバ
chat-server.cpp             接続ごとにスレッドで処理するサーバ
protocol-client.cpp         第5回と同じ通信仕様のクライアント

【動作環境】
macOS / Linux / WSL。C++17対応のg++またはclang++。
配布用ポート番号は8081、接続先は127.0.0.1（同じPCだけ）です。

【コンパイル】
「ネットワークシステム」フォルダにいる場合：
  cd network06
  g++ -std=c++17 chat-server-sequential.cpp -o chat-server-sequential
  g++ -std=c++17 -pthread chat-server.cpp -o chat-server
  g++ -std=c++17 protocol-client.cpp -o protocol-client

【起動】
前のサーバが動作している場合はCtrl+Cで停止してください。
端末A： ./chat-server-sequential
端末B： ./protocol-client
端末C： ./protocol-client

端末Bで JOIN hana を実行した後、QUITせずそのままにします。
端末Cで JOIN yuki を実行すると、1人ずつ処理するサーバでは返答待ちになります。
※端末Cに Connected と出ても、サーバがJOINを処理したとは限りません。
端末Bで QUIT すると、端末Cへ OK JOIN yuki が返ることを確認できます。

次に端末AをCtrl+Cで止め、端末B・Cも終了させてから、端末Aで以下を起動します。
  ./chat-server
再び端末B・Cで ./protocol-client を実行し、Bを接続したままCがJOINできることを確認します。

クライアントで使う命令：
  JOIN hana
  SAY hello
  QUIT
（名前と文章は必要に応じて変更できます。）
サーバの停止はCtrl+C。

【実行時の注意】
8081番を他のプログラム（例：Jenkins）が使用している場合、サーバがbindで失敗します。
確認用に別ポートを使う場合は、3つの.cppにあるPORT定数を同じ番号へ変更して再コンパイルしてください。
配布版はすべて8081に統一しています。

この版では複数人が同時に接続できますが、SAYのMSGは送信者本人に返します。
ほかの接続先へメッセージを配信する処理や、非同期表示は第7回以降に扱います。
