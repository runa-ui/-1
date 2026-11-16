ネットワークシステム08：天気APIと通信ログ（C++17 / libcurl）

【配布物】
weather-client.cpp  HTTPS GET とCSV通信ログのサンプル
location.txt        各自が緯度・経度を変更する設定ファイル（初期値は東京）
README.txt          このファイル

【利用する外部API】
Open-Meteo Forecast API: https://open-meteo.com/en/docs
個人・授業の非商用利用を想定した、APIキー不要の無料エンドポイントです。
商用利用、呼び出し制限、データの帰属表示は公式利用規約を確認してください。
出典：Weather data by Open-Meteo (CC BY 4.0) https://open-meteo.com/
「現在の天気」は気象モデルに基づく値で、観測所の実測値そのものではありません。

【公式サイトからAPI URLを取得する（授業で最初に行う）】
1. ブラウザで https://open-meteo.com/ を開き、上部の「API Docs」をクリック。
2. 「Weather Forecast API」の「Location and Time」で設定。
   Latitude=35.6762、Longitude=139.6503（まず東京で練習）。
   Timezone=Asia/Tokyoを選びます。
3. 「Current Weather」の次の3項目を選択。
   Temperature (2 m)、Relative Humidity (2 m)、Weather code
   ※「Hourly Weather Variables」は今回使いません。選択済みなら解除してください。
   ※生成URLに &hourly=... が含まれていたら、Hourly側のチェックを確認してください。
4. 「API Response」の「API URL」欄に生成されたURLが表示されます。
5. 「Open in new tab」で開き、JSONが表示されるか確認します。
6. URLをコピーし、macOSのターミナルで curl -i 'コピーしたURL' を実行します。
   URLのパラメータ（latitude、longitude、current、timezone）を確認しましょう。

※上記の東京はサイト操作の共通例です。実際にweather-clientを実行する前には、
  各自の自宅付近または学校付近の代表座標をlocation.txtへ設定してください。
※location.txtにはAPI URL全体を貼り付けません。「緯度 経度」の数値1行のみです。
※URL内のパラメータ順序などはサイトの生成結果と資料の例で異なる場合があります。

【準備】
macOS：通常はOS付属のlibcurlを使用できます。
ネットワーク管理者が外部の443番通信を制限している場合は、APIへアクセスできません。

【コンパイル】（network08ディレクトリ内）
g++ -std=c++17 weather-client.cpp -lcurl -o weather-client

【実行する前に：場所を設定】
1. location.txtをテキストエディタで開きます。
2. 末尾の「35.6762 139.6503」（東京の例）を、自宅付近または学校付近の
   「緯度 経度」に変更して保存してください。半角スペースで区切ります。
3. 緯度・経度は地図サービスで調べられます。
   ※自宅の正確な座標・住所は不要です。市役所や近くの駅などの代表地点で十分です。
   ※提出用ログにも緯度・経度が残るため、自宅のピンポイントな位置を使わないでください。
4. location.txtと実行ファイルは同じnetwork08フォルダに置いて実行してください。

【実行】
./weather-client                         # location.txtで設定した場所
./weather-client 43.0618 141.3545       # 札幌付近（今回だけ変更する例）
引数の緯度・経度を指定した場合はlocation.txtより優先します。
配布時の初期値は東京ですが、基本実習では必ず自宅付近か学校付近に変更してください。

【最初はcurlだけで確認】
curl -i "https://api.open-meteo.com/v1/forecast?latitude=35.6762&longitude=139.6503&current=temperature_2m,relative_humidity_2m,weather_code&timezone=Asia%2FTokyo"
※シェルではURL全体を引用符で囲み、&をコマンド区切りにしないこと。

【プログラムの内容】
1. location.txt（または起動時の引数）の緯度・経度からURLを作成し、libcurlを使ってHTTPS GETする。
2. curlの結果コード、HTTPステータス、所要時間、JSON本文を表示する。
3. weather-log.csvに1回の通信につき1行追加する。

【ログ列】
time_utc, latitude, longitude, curl_code, http_status, elapsed_ms, body_bytes
time_utcはUTC（協定世界時）。elapsed_msは通信の所要時間（ミリ秒）。
curl_codeはlibcurlの結果で、0はCURLE_OK。http_statusはHTTP応答のコード。
DNS/TLS/タイムアウトなどでHTTP応答が得られない場合はhttp_statusが0になります。
HTTP 400/500等でもcurl_codeが0になることがあります。
ファイルは実行したカレントディレクトリに出力されます。

【今回は簡略化したところ】
JSONを解析する外部ライブラリは使わず、受信本文をそのまま表示します。
ユーザー設定はlocation.txtの緯度・経度のみ。形式と範囲をチェックします。
ログの同時書き込みの排他制御はありません（単一プロセスを順次起動する実習用）。
外部APIの混雑、利用制限、DNS/TLS/プロキシ設定により失敗することがあります。
実際のAPIにアクセスできなくても、curlコード/HTTPコード/経過時間の違いは確認できます。

【実習】
基本：location.txtを自宅付近または学校付近の代表地点に変更して、3回実行。
      通信結果と時間を比較し、CSVログを確認する。実際の住所は書かない。
課題(1) 任意：JSONのcurrentから時刻・気温・湿度・天気コードを取り出して、場所名と単位を付けて読みやすく整形表示する。
      まずは1地点でよい。発展として東京・札幌など2地点の連続表示にも挑戦する。
      weather_codeから「くもり」等への変換や、体感気温・最高最低・風速・気圧・日の出入の追加は発展項目。
      体感気温などはcurrentに、最高最低・日の出入はdailyに変数の追加指定が必要。
      配布サンプルはJSON本文をそのまま表示する仕様のため、解析方法は自分で調べること。
      ※weather_codeは数値。地名も現在のForecast APIの応答からは自動取得されない。
課題(2) 中級・任意：Open-Meteo以外のWebサービスのAPIを1つ選び、C++からデータを取得して表示する。
      公式仕様で取得URL・パラメータ・認証や利用条件を調べ、リクエスト回数の制限を守る。
      例：逆ジオコーディングで緯度・経度から地名を取得し、課題(1)の天気表示と組み合わせる。
      ※ほかの分野のWeb APIでもよい。APIキーをソースや提出ログに記載しない。
課題(3) 中級・任意：CSVログから平均・最大所要時間、失敗件数を集計する。
※第7回のchat-server/chat-clientは変更しません。

【参照】
API仕様 https://open-meteo.com/en/docs
利用条件 https://open-meteo.com/en/terms
libcurl https://curl.se/libcurl/c/
