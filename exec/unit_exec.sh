cd `dirname $0`/../bin

# コマンドを実行し、標準出力をキャプチャ
output=$(./noxim -config ../config_examples/presentation.yaml -routing $1 -ecm $2 -pir $3 poisson 2>&1)

# de-comment in measure error ratios
# echo "$output" | grep -e '^% Total' -e '^% E' -e '^% F' -e '^% R'
# de-comment int measure seinou
echo -e "ra: ${1}, ecm: ${2}, pir: ${3}\n$output" | grep -e '^ra:' -e '^% G' -e '^% N'
echo