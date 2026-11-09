ネットワークシステム 第4回：遅延・タイムアウト・再試行

【準備】
macOS / Linux / 用意済みのWSL向け。Windowsのコマンドプロンプト単体では動きません。
ZIPを展開し、network04フォルダを端末で開きます。第3回のサーバは終了してください。

g++ -std=c++17 delay-server.cpp -o delay-server
g++ -std=c++17 timeout-client.cpp -o timeout-client

【最初の実行】
端末A： ./delay-server 0
端末B： ./timeout-client 2000
端末Bで hello と入力してEnter。両方の表示を見てください。
サーバは待受を続けます。停止は端末AでCtrl+Cです。
サーバの待ち時間を変えるときは、停止してから起動し直します。

【コマンドの引数】
./delay-server [応答前の待ち時間ms] [reply または close]
初期値は0、reply。待ち時間は0〜10000ms。
replyは受信した分を返す。closeは待った後、返答せず閉じます。

./timeout-client [受信制限時間ms] [最大試行回数]
初期値は1000ms、1回。制限時間は1〜10000ms、回数は1〜3回。
最大試行回数は初回を含みます。2を指定すると再試行は最大1回です。
受信タイムアウト時だけ、1秒待って新しい接続で同じ文字列を送ります。
接続失敗、送信失敗、応答なしの切断等は再試行しません。

【基本実習1】
サーバの待ち時間を0 / 500 / 1500にして比較します。
クライアントは ./timeout-client 2000 に固定。
条件ごとにサーバを停止・再起動し、同じ文字列を送ります。
設定値とElapsed、受信した文字列をメモします。

【基本実習2】
1. ./delay-server 3000 と ./timeout-client 1000
2. ./delay-server 0 close と ./timeout-client 1000
3. ./delay-server 0 と ./timeout-client 1000
各条件の前にサーバを停止・再起動します。
Receive timeout / Peer closed without reply / Receivedを比較します。

【再試行の観察】
サーバを起動し直し ./delay-server 3000
クライアントは ./timeout-client 1000 2
クライアントが終了した後も、数秒間サーバの表示を見ます。
同じ文字列でProcessed #1と#2が出ることを確認します。
サーバは1接続ずつ処理するため、2回目も時間切れになる場合があります。
Processedはこの教材の処理カウンタで、実際の購入などは行いません。

【自由実習】
今日の内容から確かめたいことを決め、予想してから試してください。
試した内容と結果、分かったことを短くまとめてください。
基本実習の続き、コードの読み直し、質問や相談でも構いません。

【表示の意味と範囲】
Elapsed：接続後のsend直前から、1回のrecvが戻るまでの経過時間（ms）。
入力時間と接続時間は含みません。全文受信完了の時間とも限りません。
Processed：受信したデータを処理した回数。応答前に増えます。再起動すると0に戻ります。
Reply handed to OS：OSへ送信データを渡せた状態。相手アプリが受け取った保証はありません。
Receive timeout：この受信待ちでデータを得られなかった状態。サーバの処理結果は不明です。
Peer closed without reply：応答データを受ける前に相手が送信を終了しました。

【教材の制限】
第3回simple版と同様、1回のsend/recvだけを扱います。短い文字列で試してください。
TCPは送信単位を保ちません。1回で全文を受け取れる保証はありません。
空入力は再入力、上限は1024バイト（文字数ではありません）。
部分送信時はその接続の処理を終了し、残りの再送は実装していません。
サーバは順番に処理します。スレッドによる同時処理は行いません。
SO_RCVTIMEOは受信操作用の設定で、connect・send・入力待ち・要求全体の期限ではありません。
タイムアウトの後、クライアントが明示的にcloseします。サーバの処理は取り消されません。
エコー以外の状態を変える操作へ流用する場合は、重複防止など別の設計が必要です。

【困ったとき】
Address already in use：前のサーバが動いていないか確認。
Connection refused：サーバの起動とPORTの一致を確認。
返答が遅い：待ち時間の設定、前の要求が処理中かを確認。
比較し直すときはサーバをCtrl+Cで止め、起動し直してください。

【確認状況】
整理版はmacOSでコンパイル・リンク確認済み（通信の動作確認は実行環境の制限により未実施）。
変更前版はLinuxで通信動作を確認済みです。macOS / Linux / WSL向けのPOSIXソケット教材です。
参考： https://man7.org/linux/man-pages/man7/socket.7.html
       https://man7.org/linux/man-pages/man2/recv.2.html
【第3回とのdiff比較（第4回整理版）】
このnetwork04は第3回の関数構成に寄せています。第3回のソースは変更していません。
ネットワークシステム フォルダで実行：
  diff -u network03/simple-server.cpp network04/delay-server.cpp
  diff -u network03/simple-client.cpp network04/timeout-client.cpp
（diffは違いがあると終了コード1を返しますが、正常な動作です。）
まずは、#includeの追加 → 引数の取得 → while/for → sleep_for → SO_RCVTIMEO → 受信結果の判定を確認すると理解しやすくなります。
送信・受信は第3回と同じく各1回で、アプリケーション上のメッセージ境界は未実装です。
第4回のサーバは接続を繰り返し処理するため、送信失敗時にも次の接続に進むようsendMessageの戻り値だけ変更しています。

【変更前ファイル】
network04/.backup-before-diff-cleanup/ に今回整理する直前の3ファイルを保存しています。