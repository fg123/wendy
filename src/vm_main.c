#include "memory.h"
#include "error.h"
#include "execpath.h"
#include "global.h"
#include "native.h"
#include "vm.h"
#include "data.h"
#include "imports.h"
#include <string.h>
#include <stdio.h>

// Barebones VM entry point

void invalid_usage(void) {
	printf("usage: wvm [wendy bytecode] [options] [arguments to program] \n\n");
	printf("    [wendy bytecode]     : is either a compiled WendyScript file, a raw source file, or a source string to run.\n\n");
	printf("Arguments to program can be read using the system module.\n\n");
	printf("Options:\n");
	printf("    -h, --help        : shows this message.\n");
	printf("    --nogc            : disables garbage-collection.\n");
	printf("    --trace-vm        : traces each VM instruction.\n");
	printf("    --trace-refcnt    : traces each ref-count action.\n");
	printf("    -v, --verbose     : displays information about memory state on error.\n");
	printf("    --sandbox         : runs the VM in sandboxed mode, ie. no file access and no native execution calls.\n");
	safe_exit(1);
}

// The first non-valid option is typically the file name / source string.
// The other non-valid options are the arguments.
// Returns true if user prompted for help.
bool process_options(char** options, int len, char** source) {
	int i;
	bool has_encountered_invalid = false;
	*source = NULL;
	for (i = 0; i < len; i++) {
		if (streq("-v", options[i]) ||
			streq("--verbose", options[i])) {
			set_settings_flag(SETTINGS_VERBOSE);
		}
		else if (streq("--nogc", options[i])) {
			set_settings_flag(SETTINGS_NOGC);
		}
		else if (streq("--trace-vm", options[i])) {
			set_settings_flag(SETTINGS_TRACE_VM);
		}
		else if (streq("--trace-refcnt", options[i])) {
			set_settings_flag(SETTINGS_TRACE_REFCNT);
		}
		else if (streq("--sandbox", options[i])) {
			set_settings_flag(SETTINGS_SANDBOXED);
		}
		else if (streq("-h", options[i]) ||
				 streq("--help", options[i])) {
			return true;
		}
		else if (!has_encountered_invalid) {
			*source = options[i];
			has_encountered_invalid = true;
		}
		else {
			// Once we hit a not-valid option, the rest are all arguments to
			//   program.
			break;
		}
	}
	program_arguments_count = len - i;
	program_arguments = &options[i];
	return false;
}

int main(int argc, char** argv) {
	determine_endianness();
	char *option_result;
	if (process_options(&argv[1], argc - 1, &option_result)) {
		// User asked for -h / --help
		invalid_usage();
	}
	if (!option_result) {
		invalid_usage();
	}
	set_settings_flag(SETTINGS_STRICT_ERROR);
	// FILE READ MODE
	long length = 0;
	FILE* file = fopen(option_result, "r");
	// Compute File Size
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek (file, 0, SEEK_SET);

	int header_length = strlen(WENDY_VM_HEADER);
	if (length < header_length) {
		printf("File length is less than header size!\n");
		exit(1);
	}
	char header[header_length + 1];
	fread(header, sizeof(char), header_length + 1, file);
	fseek(file, 0, SEEK_SET);
	if (!streq(WENDY_VM_HEADER, header)) {
		printf("File does not start with valid header!\n");
		exit(1);
	}
	// File Pointer should be reset
	uint8_t* bytecode_stream;
	size_t size;

	bytecode_stream = safe_malloc(sizeof(uint8_t) * length);
	size = length;
	fread(bytecode_stream, sizeof(uint8_t), length, file);
	fclose(file);

	struct vm* vm = vm_init();
	push_frame(vm->memory, "main", 0, 0);

	vm_run(vm, vm_load_code(vm, bytecode_stream, size, get_settings_flag(SETTINGS_REPL)));

	vm_destroy(vm);
	if (!last_printed_newline) {
		printf("\n");
	}

	safe_free(bytecode_stream);
	free_imported_libraries_ll();
	check_leak();
	return 0;
}
