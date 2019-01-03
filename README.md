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
The syntax for WendyScript can be found at [wendy.felixguo.me](http://wendy.felixguo.me).

WendyScript can be compiled and run online at [wendy.felixguo.me/code](http://wendy.felixguo.me/code).

Technical implementation details can be found at the [wiki](https://github.com/fg123/wendy/wiki).

The source can be built by simply running:
```
make
```
