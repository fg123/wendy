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
# echo Running Tests with Optimize Local Flag...
# for f in tests/*.in ; do
#	bin/wendy "$f" --optimize-locals > file.tmp
#	if diff "${f%.in}.expect" file.tmp > /dev/null ; then
#		echo Test $(basename $f):optimize-locals passed.
#	else
#		cp file.tmp "${f%.in}.err"
#		echo Test $(basename $f):optimize-locals failed.
#		diff -c "${f%.in}.expect" file.tmp
#		echo ============================
#	fi
# done
rm file.tmp
echo Tests Done
