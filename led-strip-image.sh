#!/bin/sh
# camera exposure of 10 to 15 seconds

numofleds=32
reps=10

usage(){
  printf "Usage: led-strip-image.sh image.png \n"
  exit 1
}
[ "$1" = "" ] && usage
fn="$1"
[ -s "$fn" ] || usage

set $(identify "$fn" | awk '{split($4,a,"[x+]"); print a[1],a[2]}') || usage
if expr $1 \< $2 >/dev/null ; then
  printf "Portrait image will be rotated for downward motion.\n"
else
  printf "Landscape or square.\n"
fi

rm -f tmp.ppm
convert "$fn" -background black -alpha remove -flatten -alpha off -rotate "-90<" -scale ${numofleds}x tmp.ppm
./led_strip_ppm $reps tmp.ppm
