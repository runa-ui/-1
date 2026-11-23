ネットワークシステム 第9回：同時接続と性能の計測
第7回のチャットサーバ・クライアントを生かした実験A/B

【比較する条件】
実験A：サーバに追加待機なし（0ms）
実験B：SAYごとに共有mutexを保持して10ms sleep
AとBは同じ接続数・SAY回数で比較します。
BのsleepはCPU計算負荷や通信遅延の模擬ではなく、共有処理の順番待ちの模擬です。

【配布物】
chat-server09.cpp : 第7回サーバに待機・ログ・モード指定を追加
chat-client09.cpp : 第7回クライアントの対話操作を保持、--auto・ログを追加
run-load.py       : 複数クライアント起動とCSV集計
README.txt        : この説明
追加コードには [09 ADD] / [09 CHANGE] のコメントを付けています。
接続先は127.0.0.1:8081です。第7回の元ソースは変更していません。

【コンパイル】network09内で実行
g++ -std=c++17 -pthread chat-server09.cpp -o chat-server09
g++ -std=c++17 -pthread chat-client09.cpp -o chat-client09

【実験A：通常サーバ】
ターミナルA：
./chat-server09 0
ターミナルB：
python3 run-load.py A 1 10
python3 run-load.py A 4 10
python3 run-load.py A 8 10
各コマンドを3回ずつ実行してください。

【実験B：共有処理に待ち時間を追加】
ターミナルAでAのサーバをCtrl+Cで止めてから：
./chat-server09 10
ターミナルB：
python3 run-load.py B 1 10
python3 run-load.py B 4 10
python3 run-load.py B 8 10
Aと同じ条件・回数で測定してください。
※run-load.pyのA/Bはログの条件名であり、サーバの実装を切り替えません。
サーバ起動引数0/10と、runnerのA/Bを一致させてください。

【元の対話モード】
./chat-client09
JOIN hana
SAY hello
QUIT
第7回の送受信・画面表示を残しています。
自動送信時は ./chat-client09 --auto load0 10 A 1 のように起動できます（SAYは1接続あたり最大10回）。
引数は名前・SAY回数・条件(A/B)・最初の接続保持秒数(省略可)です。
各接続はJOIN後にSAY trial_0などを繰り返し、本人宛てのMSGを受信したら次へ進みます。

【ログ】
1. サーバ側：server-log09-ab.csv
mode,client,active_clients,mutex_wait_ms,sleep_ms
active_clientsはSAYの時点で参加登録済みの接続数。
mutex_wait_msは追加共有区間のロック取得までの待ち時間。
サーバを起動したカレントディレクトリに追記されます。

2. クライアント個別：results-ab/<実行フォルダ>/client-log-load*.csv
mode,client,seq,success,send_steady_ns,finish_steady_ns,rtt_ms
rtt_msはSAY送信開始から本人宛てMSGの受信までのアプリケーションレベルの時間。
TCPパケットのRTTとは別です。

3. 集計：results-ab/all-summary.csv
run,mode,clients,requests_per_client,attempted,success,failed,
avg_rtt_ms,p95_rtt_ms,elapsed_ms,throughput_req_s
run-load.pyが同時起動したクライアントのログを集計します。
throughput_req_sは成功したSAY件数/計測時間(秒)です。
別フォルダに記録した条件と混同しないようrun名を確認してください。

【topでの観察】
1. pgrep -fl chat-server09 でサーバPIDを探す
2. top -pid <PID> で#TH、MEM、%CPUを確認する
3. 接続数1・4・8を比較し、サーバ自身のMEMの変化を記録する

実行時間が短い場合：
python3 run-load.py A 8 10 --hold 5
python3 run-load.py B 8 10 --hold 5
--hold 5はJOINしてからSAYを送信するまで接続を約5秒維持する指定です。
待機中に#TH・MEM・%CPUを観察してください。
この待機時間はSAY→MSGの応答時間の計測には含めません。

【接続数からメモリを考える】
今回の自作サーバは1接続1スレッド方式です。
このMacの測定例ではスレッド用スタックの標準予約は512KiBでした。
8接続ならスタック予約は約4MiB、80接続なら約40MiBという概算になります。
ただし「仮想アドレス上のスタック予約量」と
「topに表示されるMEM（プロセスのメモリ使用量の指標）」は同じではありません。
実測から1接続のMEM増分を見積もる場合：
   (MEM_8 - MEM_1) / (8 - 1)
同じ条件で測り、人数が増えるほど必ず線形に増えるとは限らないことも考えます。

【共有待ちから処理能力を考える】
実験BはSAYごとにmutex内で10ms待ちます。
共有区間だけなら、1秒/0.010秒=最大約100SAY/秒が目安です。
実際には送信・OS・ログ処理もあるので保証値ではありません。
CPU使用率やメモリだけでなく、mutex待ち・配信回数と応答時間を併せて考察します。

【基本実習】
同時接続1・4・8 × 各10SAY × 各3回を、A/Bそれぞれで実施。
同じ人数のAとBで平均RTT・p95・スループットを比較。
topからサーバの#TH・MEM・%CPUを記録し、サーバログのmutex待ちも確認。
実測値に基づいて、性能変化の原因を考察してください。

【発展課題（任意）】
1. 第9回で追加したworkMutexの外にsleepを移すとどうなるか予想・測定。
   ※clientsMutex、sendMutexなど元の同期機構は変更しないこと。
2. 接続数に対するMEMを観察して80接続時の値を推定。
3. 配信数と共有処理のボトルネック、設計改善の可能性を検討。

【注意】
自分のMacのローカルサーバだけを測定対象にしてください。
公開API、学内共有サーバなどに負荷をかけないこと。
基本実習は接続数最大8、1接続あたりSAY10回で行います。負荷テストスクリプトもこの上限を超える指定を受け付けません。
run-load.pyの上限をむやみに広げないでください。

【ポート8081の確認】
8081番が別のプログラム（例：Webサーバ）で使われている場合、chat-server09は起動できません。
授業前に lsof -nP -iTCP:8081 -sTCP:LISTEN で状態を確認してください。
既存のサービスを無断で停止・変更しないでください。
