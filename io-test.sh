#!/bin/bash
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
		diff -c "${f%.in}.expect" file.tmp
		echo ============================
	fi
done
rm file.tmp
echo Tests Done
