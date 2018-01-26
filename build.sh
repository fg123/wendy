#!/bin/bash

mkdir -p bin
mkdir -p build

buildOnly=0
release=0
for var in "$@"; do
	if [ "$var" = "-b" ]; then
		buildOnly=1
	elif [ "$var" = "-r" ]; then
		release=1
	fi
done

#make clean -s
if (( $release == 1)); then
	echo Building WendyScript with Release
	make -s release=-DRELEASE
else
	echo Building WendyScript...
	make -s
fi
echo Build Done.

bash ./build-libraries.sh

if (( $buildOnly == 0 )); then
	echo Running Tests...

	for f in tests/*.err ; do
		rm -f $f
	done

	for f in tests/*.in ; do
		bin/wendy "$f" > file.tmp
		if diff "${f%.in}.expect" file.tmp > /dev/null ; then
			echo Test $(basename $f) passed.
		else
			cp file.tmp "${f%.in}.err"
			echo Test $(basename $f) failed.
			echo Produced Output:
			echo ============================
			cat file.tmp
			echo ============================
			echo Expected Output:
			echo ============================
			cat "${f%.in}.expect"
			echo ============================
		fi
	done

	rm file.tmp

	echo Tests Done
fi
echo Finished!
