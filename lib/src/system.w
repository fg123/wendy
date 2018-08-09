/* system.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides system functions
 */

struct System => [printCallStack, printFreeMemory, examineMemory, exec, getc, printBytecode, gc, program, getImportedLibraries];
struct Program => [args];
struct GarbageCollector => [collect];

GarbageCollector.collect => () native garbageCollect;
System.printCallStack => (numLines) native printCallStack;
System.printFreeMemory => () native printFreeMemory;
System.examineMemory => (from, to) native examineMemory;
System.exec => (command) native exec;
System.getc => () native getc;
System.printBytecode => () native printBytecode;
System.getImportedLibraries => () native getImportedLibraries;
System.gc => () GarbageCollector;
System.program = Program();
{
	// Prevent Global Scope Pollution
	let args => () native getProgramArgs;
	Program.args = args();
}
