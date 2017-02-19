#!/bin/sh
echo Building WendyScript...
make clean -s
make -s
echo Build Done.
echo Running Tests...

for f in ../tests/*.err ; do
	rm $f
done


for f in ../tests/*.in ; do
	../bin/wendy "$f" > file.tmp
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

