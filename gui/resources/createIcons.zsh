#!/bin/zsh

NAME=$(basename $1 .png); DIR="$NAME.iconset"

mkdir -pv $DIR

for m r in 'n' '' '((n+1))' '@2x'; do
    for n in $(seq 4 9 | grep -v 6); do
        p=$((2**$m)); q=$((2**$n))
        OUT="$DIR/icon_${q}x${q}${r}.png"
        sips -z $p $p $1 --out $OUT
    done
done

iconutil --convert icns $DIR

