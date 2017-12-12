#!/bin/bash

# Builds the Standard Libraries
echo Compiling Wendy Library Files
mkdir -p bin/wendy-lib
rm -f bin/wendy-lib/*

# Compile Dependency Files
filestems=()

# Dependency Resolver can only work within the directory for now
cd lib/src/
for f in *.w ; do
	../../bin/wendy "$f" --dependencies > ${f%.w}.d
	filestems+=(${f%.w})
done

build_order="$(../../tools/dependency-resolve ${filestems[*]})"

while read -r line; do
	# $line is the stem to build
	../../bin/wendy $line.w -c
	cp -R $line.wc ../../bin/wendy-lib/
done <<< "$build_order"

echo Compiled and Copied
