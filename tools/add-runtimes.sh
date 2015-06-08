#!/bin/sh
#
# Script to take a movies list and add an extra field on the end that
# contains the length of the movie in seconds.
#
# It uses the ffprobe utility from ffmpeg to get the duration.
#
# This just outputs to stdout

IFS="|"

if [ $# -ne 2 ]; then
	echo "Usage: add-runtimes.sh <movies list> <movies dir>"
	exit 1
fi

cd $2

while read -a movie
do
	echo -n "${movie[0]}|${movie[1]}|${movie[2]}|${movie[3]}"
	runtime=$(ffprobe -i ${movie[1]} -show_entries format=duration -v quiet -of csv="p=0")
	if [ $? -ne 0 ]; then
		echo
	else
		echo -n "|"
		expr ${runtime} : '\([0-9]*\)' + 1
	fi
done < $1

exit 0
