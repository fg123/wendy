# WendyScript C Compiler and VM
## Created by Felix Guo

The syntax for WendyScript can be found at [felixguo.me/wendy](http://felixguo.me/wendy).

WendyScript code can be run online at [felixguo.me/#wendyScript](http://felixguo.me/#wendyScript).

Technical implementation details can be found at the [wiki](https://github.com/fg123/wendy/wiki).

This can be built by using the build.sh file.

> ./build.sh

This script will run a `make clean`, `make` as well as execute a variety of input-output tests in the /tests folder.
This ensures the build of WendyScript works as intended.

If a test fails, the produced output will be shown, as well as the output dumped to a file.tmp in the /tests folder. Tests can easily be added by creating a set of .in and .expect files in the test folder. `build.sh` will run every .in file through Wendy and check it against the corresponding .expect file.
