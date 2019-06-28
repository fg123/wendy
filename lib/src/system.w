/*
 * system.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides system functions
 */

struct System => [printCallStack, exec, getc, printBytecode, program, getImportedLibraries];
struct Program => [args];
System.printCallStack => (numLines) native printCallStack;
System.exec => (command) native exec;
System.getc => () native getc;
System.printBytecode => () native printBytecode;
System.getImportedLibraries => () native getImportedLibraries;
System.program = Program();
{
	// Prevent Global Scope Pollution
	let args => () native getProgramArgs;
	Program.args = args();
}

struct Process => (command, stdout, exit_code) [execute];
Process.init => (command) {
	this.command = command;
	this.stdout = [];
	this.exit_code = -1;
	ret this;
};

Process.execute => () {
	let internal_execute => (command) native process_execute;
	let result = internal_execute(this.command);
	this.stdout = result[1];
	this.exit_code = result[0];
};
