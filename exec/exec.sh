#!/bin/bash

cd `dirname $0`/../bin

# 使用方法の表示関数
usage() {
    echo "Usage: $0 D E"
    echo "  X: 自然数 (正の整数)"
    echo "  E: 文字列"
    exit 1
}

# 引数の数を確認
if [ "$#" -ne 2 ]; then
    echo "Error: 引数の数が正しくありません。"
    usage
fi

# 引数を変数に割り当て
D="$1"
E="$2"

# X が自然数かどうかを検証
if ! [[ "$D" =~ ^[1-9][0-9]*$ ]]; then
    echo "Error: D は自然数でなければなりません。"
    exit 1
fi

# 現在の日時を取得 (例: 20250131_235959)
datetime=$(date '+%Y%m%d_%H%M%S')

# 入力ファイル名を構築
filename="../exec/${D}x${D}_${E}_${datetime}.txt"

# 配列を定義（ここでは例として固定の配列を使用）
# 必要に応じて配列の内容を変更してください
array=("0.004" "0.008" "0.012" "0.016" "0.020" "0.024" "0.028" "0.032" "0.036" "0.04" "0.044" "0.048")

# 出力ファイルを作成（存在しない場合）
touch "$filename"

# 配列の要素数だけループ
for element in "${array[@]}"; do
    # コマンドを実行し、標準出力をキャプチャ
    output=$(./noxim -config ../config_examples/bthesis.yaml -dimx $D -dimy $D -ecm $E -pir "$element" poisson)
    
    # 標準出力の後半14行を取得
    last14=$(echo "$output" | tail -n 14)
    
    # 取得した14行をファイルに追記
    echo "$last14" >> "$filename"
    
    # 実行の進行状況を表示（オプション）
    echo "Executed iteration $i, appended last 14 lines to $filename"
done

echo "全ての実行が完了しました。出力ファイル: $filename"