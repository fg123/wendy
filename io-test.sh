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
echo Running Tests with Optimize Flag...
for f in tests/*.in ; do
	bin/wendy "$f" --optimize > file.tmp
	if diff "${f%.in}.expect" file.tmp > /dev/null ; then
		echo Test $(basename $f):optimize passed.
	else
		cp file.tmp "${f%.in}.err"
		echo Test $(basename $f):optimize failed.
		diff -c "${f%.in}.expect" file.tmp
		echo ============================
	fi
done
echo Running SortCompare Tests...
for f in tests/*.in2 ; do
	bin/wendy "$f" > file.tmp
	if diff <(sort "${f%.in2}.expect") <(file.tmp) > /dev/null ; then
		echo Test $(basename $f) passed.
	else
		cp file.tmp "${f%.in2}.err"
		echo Test $(basename $f) failed.
		diff -c <(sort "${f%.in2}.expect") <(sort file.tmp)
		echo ============================
	fi
done
rm file.tmp
echo Tests Done
