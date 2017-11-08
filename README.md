<p align="center" >
<img src="https://raw.githubusercontent.com/fg123/wendy/master/docs/logo.png" height="74px" alt="WendyScript" title="WendyScript">
</p>

**WendyScript** is a dynamically typed, imperative, bytecode-compiled programming language.

**WendyScript** supports first class functions and closures, structure based objects, easy list manipulation, and an easy to learn syntax.

Here is the code to print a list of odd numbers from 1 to 100:
```
for i in 1->100 if i % 2 == 1 i
```
or more clearly:
```
for i in 1->100 {
    if (i % 2 == 1) {
        i
    }
}
```
In fact, you can even use the short form keywords and it becomes:
```
#i:1->100?i%2==1i
```
The syntax for WendyScript can be found at [felixguo.me/wendy](http://felixguo.me/wendy).

WendyScript can be compiled and run online at [felixguo.me/#wendyScript](http://felixguo.me/#wendyScript) courtesy of the University of Waterloo.

Technical implementation details can be found at the [wiki](https://github.com/fg123/wendy/wiki).

The source can be built by using the build.sh file.
```
./build.sh
```
This script will run a `make clean`, `make` as well as execute a variety of input-output tests in the `/tests` folder.
This ensures the build of WendyScript works as intended.
