/* system.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides system functions
 */

struct System => [printCallStack, printFreeMemory, examineMemory];

System.printCallStack => (numLines) native printCallStack;
System.printFreeMemory => () native printFreeMemory;
System.examineMemory => (from, to) native examineMemory;