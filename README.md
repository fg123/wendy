# WendyScript C Interpreter
## Language and Interpreter created by Felix Guo

The syntax for WendyScript can be found at [felixguo.me](http://felixguo.me/wendy).

The repository is a WendyScript interpreter written in C. 

The interpreter can be built by using the build.sh file found in the /src folder.

> ./build.sh

This script will run a `make clean`, `make` as well as execute a variety of input-output tests in the /tests folder.
This ensures the build of WendyScript works as intended.

If a test fails, the produced output will be shown, as well as the output dumped to a file.tmp in the /tests folder. Tests can easily be added by creating a set of .in and .expect files in the test folder. `build.sh` will run every .in file through Wendy and check it against the corresponding .expect file.
