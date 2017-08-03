/* system.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides system functions
 */

struct System => [printCallStack, printFreeMemory, examineMemory, exec, getc];

System.printCallStack => (numLines) native printCallStack;
System.printFreeMemory => () native printFreeMemory;
System.examineMemory => (from, to) native examineMemory;
System.exec => (command) native exec;
System.getc => () native getc;