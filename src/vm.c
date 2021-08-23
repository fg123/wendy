#include "vm.h"
#include "codegen.h"
#include "memory.h"
#include "error.h"
#include "stdint.h"
#include "math.h"
#include "global.h"
#include "native.h"
#include "imports.h"
#include "table.h"
#include "struct.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>

// Forward Declarations
static struct data eval_binop(struct vm * vm, enum vm_operator op, struct data a, struct data b);
static struct data eval_uniop(struct vm * vm, enum vm_operator op, struct data a);
static struct data type_of(struct data a);
static char* type_of_str(struct data a);
static struct data size_of(struct data a);
static struct data value_of(struct data a);
static struct data char_of(struct data a);

struct vm *vm_init() {
	struct vm* vm = safe_malloc(sizeof(*vm));
	vm->bytecode = 0;
	vm->instruction_ptr = 0;
	vm->bytecode_size = 0;
	vm->last_pushed_identifier = 0;
	vm->line = 0;
	vm->memory = memory_init();
	return vm;
}

void vm_destroy(struct vm * vm) {
	memory_destroy(vm->memory);
	safe_free(vm);
}

void vm_cleanup_if_repl(struct vm * vm) {
	// bytecode could be null if codegen didn't generate anything
	if (vm->bytecode) safe_free(vm->bytecode);
}

#define num_args(...) (sizeof((char*[]){__VA_ARGS__})/sizeof(char*))
#define first_that(memory, condition, ...) first_that_impl(memory, condition, num_args(__VA_ARGS__), __VA_ARGS__)
static char* first_that_impl(struct memory* memory, bool (*condition)(struct memory *, char*), int count, ...) {
	va_list ap;
	va_start(ap, count);
	for (int i = 0; i < count; i++) {
		char* s = va_arg(ap, char*);
		if (condition(memory, s)) {
			return s;
		}
	}
	va_end(ap);
	return 0;
}

static bool _id_exist(struct memory* memory, char* fn) {
	return id_exist(memory, fn, true);
}

static char* get_binary_overload_name(enum vm_operator op, struct data a, struct data b) {
	char* type_a = type_of_str(a);
	char* type_b = type_of_str(b);
	char* fn_name = safe_concat(OPERATOR_OVERLOAD_PREFIX, type_a,
		operator_string[op], type_b);
	return fn_name;
}

static char* get_unary_overload_name(enum vm_operator op, struct data a) {
	char* type_a = type_of_str(a);
	char* fn_name = safe_concat(OPERATOR_OVERLOAD_PREFIX,
		operator_string[op], type_a);
	return fn_name;
}

static char* get_print_overload_name(struct data a) {
	char* type_a = type_of_str(a);
	char* fn_name = safe_concat(OPERATOR_OVERLOAD_PREFIX "@", type_a);
	return fn_name;
}

address vm_load_code(struct vm* vm, uint8_t* new_bytecode, size_t size, bool append) {
	if (get_settings_flag(SETTINGS_DRY_RUN)) {
		return 0;
	}
	// Verify Header
	address start_at;
	size_t saved_size = vm->bytecode_size;
	if (!append) {
		if (vm->bytecode) {
			safe_free(vm->bytecode);
		}
		vm->bytecode = new_bytecode;
		start_at = verify_header(vm->bytecode, size);
	}
	else {
		// REPL Bytecode has no headers!
		// Resize Bytecode Block, Offset New Addresses, Push to End
		vm->bytecode_size += size;
		if (vm->bytecode) {
			// This gets rid of the OP_HALT from the previous chain of BC
			vm->bytecode_size--;
			vm->bytecode = safe_realloc(vm->bytecode, vm->bytecode_size * sizeof(uint8_t));
		}
		else {
			vm->bytecode = safe_malloc(vm->bytecode_size * sizeof(uint8_t));
		}
		if (saved_size != 0) {
			start_at = saved_size - 1;
		}
		else {
			start_at = 0;
		}
		offset_addresses(new_bytecode, size, start_at);
		for (size_t i = 0; i < size; i++) {
			vm->bytecode[start_at + i] = new_bytecode[i];
		}
	}
	return start_at;
}

void vm_run_instruction(struct vm* vm, enum opcode op) {
	switch (op) {
		case OP_PUSH: {
			struct data t = get_data(vm->bytecode + vm->instruction_ptr, &vm->instruction_ptr);
			// t will never be a reference type
			struct data d;
			if (t.type == D_IDENTIFIER) {
				if (streq(t.value.string, "time")) {
					d = time_data();
				}
				else {
					struct data* value = get_address_of_id(vm->memory, t.value.string, true, NULL);
					if (!value) {
						error_runtime(vm->memory, vm->line, MEMORY_ID_NOT_FOUND, t.value.string);
						break;
					}
					d = copy_data(*value);
				}
			}
			else {
				d = copy_data(t);
			}
			vm->last_pushed_identifier = t.value.string;
			push_arg(vm->memory, d);
			break;
		}
		case OP_BIN: {
			enum vm_operator op = vm->bytecode[vm->instruction_ptr++];
			struct data a = pop_arg(vm->memory, vm->line);
			struct data b = pop_arg(vm->memory, vm->line);
			struct data any_d = any_data();
			char* a_and_b = get_binary_overload_name(op, a, b);
			char* any_a = get_binary_overload_name(op, any_d, b);
			char* any_b = get_binary_overload_name(op, a, any_d);
			destroy_data_runtime(vm->memory, &any_d);
			char* fn_name = first_that(vm->memory, _id_exist, a_and_b, any_a, any_b);
			if (fn_name) {
				push_arg(vm->memory, make_data(D_END_OF_ARGUMENTS, data_value_num(0)));
				push_arg(vm->memory, b);
				push_arg(vm->memory, a);
				push_arg(vm->memory, copy_data(*get_address_of_id(vm->memory, fn_name, true, NULL)));
				safe_free(a_and_b);
				safe_free(any_a);
				safe_free(any_b);
				goto wendy_vm_call;
			}
			else {
				push_arg(vm->memory, eval_binop(vm, op, a, b));
				destroy_data_runtime(vm->memory, &a);
				destroy_data_runtime(vm->memory, &b);
			}
			safe_free(a_and_b);
			safe_free(any_a);
			safe_free(any_b);
			break;
		}
		case OP_UNA: {
			enum vm_operator op = vm->bytecode[vm->instruction_ptr++];
			struct data a = pop_arg(vm->memory, vm->line);
			char* fn_name = get_unary_overload_name(op, a);
			if (id_exist(vm->memory, fn_name, true)) {
				push_arg(vm->memory, make_data(D_END_OF_ARGUMENTS, data_value_num(0)));
				push_arg(vm->memory, a);
				push_arg(vm->memory, copy_data(*get_address_of_id(vm->memory, fn_name, true, NULL)));
				safe_free(fn_name);
				goto wendy_vm_call;
			}
			else {
				// Uni-op has a special spread vm_operator that returns noneret
				struct data result = eval_uniop(vm, op, a);
				push_arg(vm->memory, result);
				destroy_data_runtime(vm->memory, &a);
			}
			safe_free(fn_name);
			break;
		}
		case OP_SRC: {
			void* ad = &vm->bytecode[vm->instruction_ptr];
			vm->line = get_address(ad, &vm->instruction_ptr);
			break;
		}
		case OP_NATIVE: {
			void* ag = &vm->bytecode[vm->instruction_ptr];
			address args = get_address(ag, &vm->instruction_ptr);
			char* name = get_string(vm->bytecode + vm->instruction_ptr, &vm->instruction_ptr);
			native_call(vm, name, args);
			break;
		}
		case OP_DECL: {
			char *id = get_string(vm->bytecode + vm->instruction_ptr, &vm->instruction_ptr);
			if (id_exist_local_frame_ignore_closure(vm->memory, id)) {
				error_runtime(vm->memory, vm->line, VM_VAR_DECLARED_ALREADY, id);
				return;
			}
			struct data* result = push_stack_entry(vm->memory, id, vm->line);
			push_arg(vm->memory, make_data(D_INTERNAL_POINTER, data_value_ptr(result)));
			vm->last_pushed_identifier = id;
			break;
		}
		case OP_WHERE: {
			char* id = get_string(vm->bytecode + vm->instruction_ptr, &vm->instruction_ptr);
			vm->last_pushed_identifier = id;
			struct data* result = get_address_of_id(vm->memory, id, true, NULL);
			if (!result) {
				error_runtime(vm->memory, vm->line, MEMORY_ID_NOT_FOUND, id);
				break;
			}
			push_arg(vm->memory, make_data(D_INTERNAL_POINTER,
				data_value_ptr(result)
			));
			break;
		}
		case OP_IMPORT: {
			char* name = get_string(vm->bytecode + vm->instruction_ptr, &vm->instruction_ptr);
			address a = get_address(vm->bytecode + vm->instruction_ptr, &vm->instruction_ptr);
			if (has_already_imported_library(name)) {
				vm->instruction_ptr = a;
			}
			else {
				add_imported_library(name);
			}
			break;
		}
		case OP_ARGCLN: {
			// TODO: 128 limit here seems a bit arbitrary and we
			struct data* extra_args = wendy_list_malloc(vm->memory, 128);
			size_t count = 0;
			while (top_arg(vm->memory, vm->line)->type != D_END_OF_ARGUMENTS) {
				if (top_arg(vm->memory, vm->line)->type == D_NAMED_ARGUMENT_NAME) {
					struct data identifier = pop_arg(vm->memory, vm->line);
					struct data* loc = get_address_of_id(vm->memory, identifier.value.string, false, NULL);
					if (!loc) {
						error_runtime(vm->memory, vm->line, MEMORY_ID_NOT_FOUND, identifier.value.string);
						break;
					}
					destroy_data_runtime(vm->memory, loc);
					*loc = pop_arg(vm->memory, vm->line);
					destroy_data_runtime(vm->memory, &identifier);
				}
				else {
					extra_args[count + 1] = pop_arg(vm->memory, vm->line);
					count += 1;
				}
			}
			// We need to re-write the list counter
			((struct list_header*) extra_args[0].value.reference)->size = count;
			// Assign "arguments" variable with rest of the arguments.
			*push_stack_entry(vm->memory, "arguments", vm->line) =
				make_data(D_LIST, data_value_ptr(extra_args));
			// Pop End of Arguments
			struct data eoargs = pop_arg(vm->memory, vm->line);
			destroy_data_runtime(vm->memory, &eoargs);
			break;
		}
		case OP_RET: {
			size_t trace = vm->memory->call_stack_pointer - 1;
			while (vm->memory->call_stack[trace].is_automatic) {
				trace -= 1;
			}
			if (trace == 0) {
				goto VM_OP_HALT;
			}
			pop_frame(vm->memory, true, &vm->instruction_ptr);
			break;
		}
		case OP_INC: {
			struct data ptr = pop_arg(vm->memory, vm->line);
			if (ptr.type != D_INTERNAL_POINTER) {
				error_runtime(vm->memory, vm->line, VM_INTERNAL_ERROR, "INC on non-pointer");
				return;
			}
			struct data* arg = ptr.value.reference;
			if (arg->type != D_NUMBER) {
				error_runtime(vm->memory, vm->line, VM_TYPE_ERROR, "INC");
				return;
			}
			arg->value.number += 1;
			break;
		}
		case OP_DUPTOP: {
			struct data* top = top_arg(vm->memory, vm->line);
			push_arg(vm->memory, copy_data(*top));
			break;
		}
		case OP_POP: {
			pop_arg(vm->memory, vm->line);				
			break;
		}
		case OP_ROTTWO: {
			struct data first = pop_arg(vm->memory, vm->line);
			struct data second = pop_arg(vm->memory, vm->line);
			push_arg(vm->memory, first);
			push_arg(vm->memory, second);
			break;
		}
		case OP_MKTBL: {
			size_t size = get_address(&vm->bytecode[vm->instruction_ptr], &vm->instruction_ptr);
			struct table* table = table_create();
			struct data* table_storage = refcnt_malloc(vm->memory, 1);
			table_storage[0] = make_data(D_TABLE_INTERNAL_POINTER, data_value_ptr((struct data*) table));
			for (size_t i = 0; i < size; i++) {
				struct data key = pop_arg(vm->memory, vm->line);
				struct data data;
				if (key.type != D_TABLE_KEY) {
					// next is the value at the key
					data = key;
					key = pop_arg(vm->memory, vm->line);
					wendy_assert(key.type == D_TABLE_KEY, "MKTBL entry is not an Table Key type, but is %s", data_string[key.type]);
				}
				else {
					data = none_data();
				}

				struct data* _data = table_insert(table, key.value.string, vm->memory);
				*_data = data;
				destroy_data_runtime(vm->memory, &key);
			}
			struct data reference = make_data(D_TABLE, data_value_ptr(table_storage));
			push_arg(vm->memory, reference);
			break;
		}
		case OP_DEC: {
			struct data ptr = pop_arg(vm->memory, vm->line);
			if (ptr.type != D_INTERNAL_POINTER) {
				error_runtime(vm->memory, vm->line, VM_INTERNAL_ERROR, "DEC on non-pointer");
				return;
			}
			struct data* arg = ptr.value.reference;
			if (arg->type != D_NUMBER) {
				error_runtime(vm->memory, vm->line, VM_TYPE_ERROR, "DEC");
				return;
			}
			arg->value.number -= 1;
			break;
		}
		case OP_FRM: {
			push_auto_frame(vm->memory, vm->instruction_ptr, "automatic", vm->line);
			break;
		}
		case OP_END: {
			pop_frame(vm->memory, false, &vm->instruction_ptr);
			break;
		}
		case OP_MKREF: {
			enum data_type type = vm->bytecode[vm->instruction_ptr++];
			size_t size = get_address(&vm->bytecode[vm->instruction_ptr], &vm->instruction_ptr);
			struct data reference = make_data(type, data_value_ptr(NULL));

			if (!is_reference(reference)) {
				error_runtime(vm->memory, vm->line, VM_INTERNAL_ERROR,
					"MKREF called on non-reference type");
				return;
			}

			struct data* storage = refcnt_malloc(vm->memory, size);

			// can't simply check additional_space because spread could
			//   expand to 1 element too
			bool has_spread = false;
			size_t additional_space = 0;

			size_t i = size;
			while (i > 0) {
				struct data next = pop_arg(vm->memory, vm->line);
				storage[i - 1] = next;
				i -= 1;
				if (next.type == D_SPREAD) {
					additional_space += size_of(*next.value.reference).value.number - 1;
					has_spread = true;
				}
			}

			// Make this valid, codegen generates a number instead of a pointer
			if (type == D_LIST) {
				size_t list_size = storage[0].value.number;
				storage[0] = list_header_data(list_size, list_size);
			}

			if (has_spread) {
				struct data* new_storage =
					refcnt_malloc(vm->memory, size + additional_space);
				size_t j = 0;
				for (size_t i = 0; i < size; i++) {
					if (storage[i].type == D_SPREAD) {
						struct data spread = storage[i].value.reference[0];
						if (spread.type == D_LIST) {
							for (size_t k = 0; k < wendy_list_size(&spread); k++) {
								new_storage[j++] = copy_data(spread.value.reference[k + 1]);
							}
						}
						else if (spread.type == D_RANGE) {
							int start = range_start(spread);
							int end = range_end(spread);
							for (int k = start; k != end; start < end ? k++ : k--) {
								new_storage[j++] = make_data(D_NUMBER, data_value_num(k));
							}
						}
						else if (spread.type == D_STRING) {
							size_t len = strlen(spread.value.string);
							for (size_t k = 0; k < len; k++) {
								struct data str = make_data(D_STRING, data_value_str(" "));
								str.value.string[0] = spread.value.string[k];
								new_storage[j++] = str;
							}
						}
					}
					else {
						new_storage[j++] = storage[i];
					}
				}
				refcnt_free(vm->memory, storage);
				storage = new_storage;
				if (storage[0].type == D_LIST_HEADER) {
					// - 1 for header
					storage[0] = list_header_data(size + additional_space - 1,
						size + additional_space - 1);
				}
			}

			reference.value.reference = storage;
			push_arg(vm->memory, reference);
			break;
		}
		case OP_NTHPTR: {
			struct data number = pop_arg(vm->memory, vm->line);
			struct data list = pop_arg(vm->memory, vm->line);
			if (number.type != D_NUMBER && number.type != D_RANGE) {
				error_runtime(vm->memory, vm->line, VM_INVALID_LVALUE_LIST_SUBSCRIPT);
				goto nthptr_cleanup;
			}
			if (list.type != D_LIST) {
				error_runtime(vm->memory, vm->line, VM_NOT_A_LIST);
				goto nthptr_cleanup;
			}
			struct data* list_data = list.value.reference;
			if (list_data->type != D_LIST_HEADER) {
				error_runtime(vm->memory, vm->line, VM_INTERNAL_ERROR, "List doesn't point to list header!");
				goto nthptr_cleanup;
			}
			size_t list_size = wendy_list_size(&list);
			if (number.type == D_NUMBER) {
				if (number.value.number >= list_size) {
					error_runtime(vm->memory, vm->line, VM_LIST_REF_OUT_RANGE);
					goto nthptr_cleanup;
				}
				push_arg(vm->memory, make_data(D_INTERNAL_POINTER,
					data_value_ptr(&list_data[(int)number.value.number + 1])));
			}
			else if (number.type == D_RANGE) {
				int start = range_start(number);
				int end = range_end(number);
				// Test for end is different because end is exclusive
				if (start < 0 || end < -1 || start >= (int) list_size || end > (int) list_size) {
					error_runtime(vm->memory, vm->line, VM_LIST_REF_OUT_RANGE);
					goto nthptr_cleanup;
				}
				struct data *internal = refcnt_malloc(vm->memory, 2);
				internal[0] = copy_data(list);
				internal[1] = copy_data(number); // which is a range
				push_arg(vm->memory, make_data(D_LIST_RANGE_LVALUE,
					data_value_ptr(internal)));
			}
			nthptr_cleanup:
				destroy_data_runtime(vm->memory, &list);
				destroy_data_runtime(vm->memory, &number);
			break;
		}
		case OP_CLOSURE: {
			// create_closure() could return NULL when there's no closure, so the
			//   refcnt code is structured to ignore null pointer references
			push_arg(vm->memory, make_data(D_CLOSURE, data_value_ptr(create_closure(vm->memory))));
			break;
		}
		case OP_MEMPTR: {
			// Member Pointer
			// Structs can only modify Static members, instances modify instance
			//   members.
			// Either will be allowed to look through static parameters.
			struct data instance = pop_arg(vm->memory, vm->line);
			char* member = get_string(vm->bytecode + vm->instruction_ptr, &vm->instruction_ptr);
			if (instance.type != D_STRUCT &&
				instance.type != D_STRUCT_INSTANCE &&
				instance.type != D_TABLE) {
				if (instance.type == D_NONERET) {
					error_runtime(vm->memory, vm->line, VM_NOT_A_STRUCT_MAYBE_FORGOT_RET_THIS);
				} else {
					error_runtime(vm->memory, vm->line, VM_NOT_A_STRUCT);
				}
				destroy_data_runtime(vm->memory, &instance);
				break;
			}
			if (instance.type == D_TABLE) {
				struct table* table = (struct table*) instance.value.reference[0].value.reference;
				if (!table_exist(table, member)) {
					struct data type = type_of(instance);
					error_runtime(vm->memory, vm->line, VM_MEMBER_NOT_EXIST, member, type.value.string);
					destroy_data_runtime(vm->memory, &type);
					break;
				}
				push_arg(vm->memory,
					make_data(D_INTERNAL_POINTER, data_value_ptr(
							table_find(table, member)
						)
					)
				);
			}
			else {
				// Struct or struct instance
				struct data* ptr = struct_get_field(vm, instance, member);
				if (!ptr) {
					struct data type = type_of(instance);
					error_runtime(vm->memory, vm->line, VM_MEMBER_NOT_EXIST, member, type.value.string);
					destroy_data_runtime(vm->memory, &type);
				}
				else {
					push_arg(vm->memory, make_data(D_INTERNAL_POINTER, data_value_ptr(ptr)));
				}
			}
			destroy_data_runtime(vm->memory, &instance);
			break;
		}
		case OP_JMP: {
			address addr = get_address(&vm->bytecode[vm->instruction_ptr], &vm->instruction_ptr);
			vm->instruction_ptr = addr;
			break;
		}
		case OP_JIF: {
			// Jump IF False Instruction
			struct data top = pop_arg(vm->memory, vm->line);
			address addr = get_address(&vm->bytecode[vm->instruction_ptr], &vm->instruction_ptr);
			if (top.type != D_TRUE && top.type != D_FALSE) {
				error_runtime(vm->memory, vm->line, VM_COND_EVAL_NOT_BOOL);
			}
			if (top.type == D_FALSE) {
				vm->instruction_ptr = addr;
			}
			destroy_data_runtime(vm->memory, &top);
			break;
		}
		case OP_CALL:
		wendy_vm_call: {
			struct data top = pop_arg(vm->memory, vm->line);
			if (top.type != D_FUNCTION && top.type != D_STRUCT && top.type != D_STRUCT_FUNCTION) {
				error_runtime(vm->memory, vm->line, VM_FN_CALL_NOT_FN);
				destroy_data_runtime(vm->memory, &top);
				break;
			}
			struct data boundName = top.value.reference[2];
			char* function_disp = safe_malloc((128 + strlen(boundName.value.string)) * sizeof(char));
			function_disp[0] = 0;
			if (boundName.value.string && streq(boundName.value.string, "self")) {
				sprintf(function_disp, "annonymous:0x%X", vm->instruction_ptr);
			}
			else {
				sprintf(function_disp, "%s:0x%X", boundName.value.string, vm->instruction_ptr);
			}
			push_frame(vm->memory, function_disp, vm->instruction_ptr, vm->line);
			safe_free(function_disp);

			if (top.type == D_STRUCT) {
				// Calling Struct Constructor
				struct data* metadata = top.value.reference;

				struct data old_top = top;

				// Select `init` function.
				wendy_assert(metadata[2].type == D_TABLE, "not a table!");
				struct table* static_table = (struct table*) metadata[2].value.reference[0].value.reference;

				// Better Exist
				wendy_assert(table_exist(static_table, "init"), "struct static table has no init!");
				top = copy_data(*table_find(static_table, "init"));

				if (top.type != D_FUNCTION) {
					error_runtime(vm->memory, vm->line, VM_STRUCT_CONSTRUCTOR_NOT_A_FUNCTION);
					destroy_data_runtime(vm->memory, &top);
					break;
				}
				top.type = D_STRUCT_FUNCTION;

				push_arg(vm->memory,
					make_data(D_STRUCT_INSTANCE,
						data_value_ptr(
							struct_create_instance(vm, metadata)
						)
					)
				);
				destroy_data_runtime(vm->memory, &old_top);
			}

			struct data addr = top.value.reference[0];
			if (addr.type != D_INSTRUCTION_ADDRESS) {
				error_runtime(vm->memory, vm->line, VM_INTERNAL_ERROR, "Address of function is not D_INSTRUCTION_ADDRESS");
				destroy_data_runtime(vm->memory, &top);
				break;
			}
			vm->instruction_ptr = (address) addr.value.number;

			// Resolve Spread Objects in the Call List
			size_t ptr = vm->memory->working_stack_pointer - 1;
			bool has_spread = false;
			size_t additional_size = 0;
			size_t argc = 0;
			while (vm->memory->working_stack[ptr].type != D_END_OF_ARGUMENTS) {
				if (vm->memory->working_stack[ptr].type == D_SPREAD) {
					has_spread = true;
					additional_size +=
						size_of(*vm->memory->working_stack[ptr].value.reference).value.number - 1;
				}
				argc += 1;
				ptr -= 1;
			}

			if (has_spread) {
				// Expand the Stack if required
				ensure_working_stack_size(vm->memory, additional_size);
				// In-place move from the back
				size_t og_ptr = vm->memory->working_stack_pointer - 1;
				size_t new_ptr = vm->memory->working_stack_pointer + additional_size - 1;
				for (; vm->memory->working_stack[og_ptr].type != D_END_OF_ARGUMENTS; og_ptr--) {
					if (vm->memory->working_stack[og_ptr].type == D_SPREAD) {
						struct data og_spread = vm->memory->working_stack[og_ptr];
						struct data spread = og_spread.value.reference[0];
						if (spread.type == D_LIST) {
							for (size_t k = 0; k < wendy_list_size(&spread); k++) {
								vm->memory->working_stack[new_ptr--] = copy_data(spread.value.reference[k + 1]);
							}
						}
						else if (spread.type == D_RANGE) {
							int start = range_start(spread);
							int end = range_end(spread);
							for (int k = start; k != end; start < end ? k++ : k--) {
								vm->memory->working_stack[new_ptr--] = make_data(D_NUMBER, data_value_num(k));
							}
						}
						else if (spread.type == D_STRING) {
							size_t len = strlen(spread.value.string);
							for (size_t k = 0; k < len; k++) {
								struct data str = make_data(D_STRING, data_value_str(" "));
								str.value.string[0] = spread.value.string[k];
								vm->memory->working_stack[new_ptr--] = str;
							}
						}
						destroy_data_runtime(vm->memory, &og_spread);
					}
					else {
						vm->memory->working_stack[new_ptr--] = vm->memory->working_stack[og_ptr];
					}
				}
				vm->memory->working_stack_pointer += additional_size;
			}

			if (top.type == D_STRUCT_FUNCTION) {
				// Either we pushed the new instance on the stack on top, or
				//   codegen generated the instance on the top.
				struct data instance = pop_arg(vm->memory, vm->line);
				if (instance.type != D_STRUCT_INSTANCE &&
					instance.type != D_STRUCT &&
					instance.type != D_TABLE) {
					error_runtime(vm->memory, vm->line, VM_INTERNAL_ERROR, "D_STRUCT_FUNCTION encountered but top of stack is not a instance nor a struct.");
					destroy_data_runtime(vm->memory, &top);
					destroy_data_runtime(vm->memory, &instance);
					break;
				}
				*push_stack_entry(vm->memory, "this", vm->line) = instance;
			}

			// Push closure variables
			struct data *list_data = top.value.reference[1].value.reference;
			size_t size = wendy_list_size(&top.value.reference[1]);

			// Move the pointer to the "first" item, because the 0th item is the list-header
			list_data += 1;
			for (size_t i = 0; i < size; i += 2) {
				if (list_data[i].type != D_IDENTIFIER) {
					error_runtime(vm->memory, vm->line, VM_INTERNAL_ERROR, "Did not find a D_IDENTIFIER in function closure");
					destroy_data_runtime(vm->memory, &top);
					break;
				}
				*push_closure_entry(vm->memory, list_data[i].value.string, vm->line) = copy_data(list_data[i + 1]);
			}

			// At this point, we put `top` back into the stack, so no need to destroy it
			if (strcmp(boundName.value.string, "self") != 0) {
				*push_stack_entry(vm->memory, "self", vm->line) = copy_data(top);
			}
			*push_stack_entry(vm->memory, boundName.value.string, vm->line) = top;
			break;
		}
		case OP_WRITE: {
			struct data ptr = pop_arg(vm->memory, vm->line);
			if (ptr.type != D_INTERNAL_POINTER && ptr.type != D_LIST_RANGE_LVALUE) {
				error_runtime(vm->memory, vm->line, VM_INTERNAL_ERROR, "WRITE on non-pointer");
				destroy_data_runtime(vm->memory, &ptr);
				return;
			}

			if (top_arg(vm->memory, vm->line)->type == D_END_OF_ARGUMENTS ||
				top_arg(vm->memory, vm->line)->type == D_NAMED_ARGUMENT_NAME) {
				if (ptr.value.reference->type == D_EMPTY) {
					*(ptr.value.reference) = none_data();
				}
				break;
			}

			struct data value = pop_arg(vm->memory, vm->line);

			// Since value is written back, we don't need to destroy it
			if (value.type == D_NONERET) {
				error_runtime(vm->memory, vm->line, VM_ASSIGNING_NONERET);
				destroy_data_runtime(vm->memory, &value);
				break;
			}

			if (ptr.type == D_INTERNAL_POINTER) {
				destroy_data_runtime(vm->memory, &ptr.value.reference[0]);
				*(ptr.value.reference) = value;

				if (ptr.value.reference->type == D_FUNCTION) {
					// Write Name to Function
					char* bind_name = vm->last_pushed_identifier;
					struct data* fn_data = ptr.value.reference->value.reference;
					fn_data[2].value.string = safe_realloc(
						fn_data[2].value.string, strlen(bind_name) + 1);
					strcpy(fn_data[2].value.string, bind_name);
				}
				// We stole value, so don't need to destroy it here
			}
			else if (ptr.type == D_LIST_RANGE_LVALUE) {
				struct data list = ptr.value.reference[0];
				struct data* list_data = list.value.reference;
				struct data range = ptr.value.reference[1];
				int start = range_start(range);
				int end = range_end(range);

				// No range checking needed here, OP_NTHPTR checks the bounds, and
				//   the rvalue side get's evaluated first, so no side effects can
				//   occur between NTHPTR and WRITE (I hope)...

				if (value.type == D_LIST) {
					size_t needed_size = abs(start - end);
					size_t list_size = wendy_list_size(&value);
					struct data* value_data = value.value.reference;
					if (list_size != needed_size) {
						error_runtime(vm->memory, vm->line, VM_LIST_RANGE_ASSIGN_SIZE_MISMATCH,
							needed_size, list_size);
						goto write_list_range_lvalue_cleanup;
					}
					int i = 0;
					for (int k = start; k != end; start < end ? k++ : k--) {
						destroy_data_runtime(vm->memory, &list_data[k + 1]);
						list_data[k + 1] = copy_data(value_data[i + 1]);
						i++;
					}
				}
				else {
					for (int k = start; k != end; start < end ? k++ : k--) {
						destroy_data_runtime(vm->memory, &list_data[k + 1]);
						list_data[k + 1] = copy_data(value);
					}
				}
			write_list_range_lvalue_cleanup:
				destroy_data_runtime(vm->memory, &value);
				destroy_data_runtime(vm->memory, &ptr);
			}
			break;
		}
		case OP_OUT: {
			struct data t = pop_arg(vm->memory, vm->line);
			if (t.type != D_NONERET) {
				char* fn_name = get_print_overload_name(t);
				if (id_exist(vm->memory, fn_name, true)) {
					push_arg(vm->memory, make_data(D_END_OF_ARGUMENTS, data_value_num(0)));
					push_arg(vm->memory, t);
					push_arg(vm->memory, copy_data(*get_address_of_id(vm->memory, fn_name, true, NULL)));
					safe_free(fn_name);
					/* This i-- allows the overloaded function to return
						* a string / object and have that be the printed
						* output, i.e. it will call function and execute
						* the OP_OUT again */
					vm->instruction_ptr--;
					goto wendy_vm_call;
				}
				safe_free(fn_name);
				print_data(&t);
			}
			destroy_data_runtime(vm->memory, &t);
			break;
		}
		case OP_OUTL: {
			struct data t = pop_arg(vm->memory, vm->line);
			if (t.type != D_NONERET) {
				char* fn_name = get_print_overload_name(t);
				if (id_exist(vm->memory, fn_name, true)) {
					push_arg(vm->memory, make_data(D_END_OF_ARGUMENTS, data_value_num(0)));
					push_arg(vm->memory, t);
					push_arg(vm->memory, copy_data(*get_address_of_id(vm->memory, fn_name, true, NULL)));
					safe_free(fn_name);
					/* This i-- allows the overloaded function to return
						* a string / object and have that be the printed
						* output, i.e. it will call function and execute
						* the OP_OUT again */
					vm->instruction_ptr--;
					goto wendy_vm_call;
				}
				safe_free(fn_name);
				print_data_inline(&t, stdout);
			}
			destroy_data_runtime(vm->memory, &t);
			break;
		}
		case OP_IN: {
			// Scan one line from the input.
			struct data ptr = pop_arg(vm->memory, vm->line);
			if (ptr.type != D_INTERNAL_POINTER) {
				error_runtime(vm->memory, vm->line, VM_INTERNAL_ERROR, "INC on non-pointer");
				return;
			}
			struct data* storage = ptr.value.reference;
			destroy_data_runtime(vm->memory, storage);

			char buffer[INPUT_BUFFER_SIZE];
			while(!fgets(buffer, INPUT_BUFFER_SIZE, stdin)) {};

			char* end_ptr = buffer;
			errno = 0;
			double d = strtod(buffer, &end_ptr);
			// null terminator or newline character
			if (errno != 0 || (*end_ptr != 0 && *end_ptr != 10)) {
				size_t len = strlen(buffer);
				// remove last newline
				buffer[len - 1] = 0;
				*storage = make_data(D_STRING, data_value_str(buffer));
			}
			else {
				// conversion successful
				*storage = make_data(D_NUMBER, data_value_num(d));
			}
			break;
		}
		case OP_HALT:
		VM_OP_HALT:
			return;
		// No default: here because we want compiler to catch missing cases.
	}
}

void vm_set_instruction_pointer(struct vm* vm, address start) {
	vm->instruction_ptr = start;
}

void vm_run(struct vm *vm) {
	if (get_settings_flag(SETTINGS_DRY_RUN)) {
		return;
	}
	size_t starting_stack_pointer = vm->memory->call_stack_pointer;
	for (;;) {
		reset_error_flag();
		enum opcode op = vm->bytecode[vm->instruction_ptr];
		if (get_settings_flag(SETTINGS_TRACE_VM)) {
			// This branch could slow down the VM but the CPU should branch
			// predict after one or two iterations.
			printf(BLU "<+%04X>: " RESET "%s\n", vm->instruction_ptr, opcode_string[op]);
		}
		vm->instruction_ptr += 1;
		if (op == OP_HALT) return;
		vm_run_instruction(vm, op);

		if (vm->memory->call_stack_pointer < starting_stack_pointer) {
			// We returned out of the starting stack frame
			return;
		}

		if (get_error_flag()) {
			clear_working_stack(vm->memory);
			break;
		}
	}
}

static struct data eval_binop(struct vm * vm, enum vm_operator op, struct data a, struct data b) {
	if (op == O_SUBSCRIPT) {
		// Array Reference, or String
		// A must be a list/string/range, b must be a number.
		if (a.type != D_LIST && a.type != D_STRING && a.type != D_RANGE) {
			error_runtime(vm->memory, vm->line, VM_TYPE_ERROR, operator_string[op]);
			return none_data();
		}

		int list_size;
		if (a.type == D_STRING) {
			list_size = strlen(a.value.string);
		}
		else if (a.type == D_RANGE) {
			list_size = abs(range_end(a) - range_start(a));
		}
		else {
			list_size = wendy_list_size(&a);
		}

		if (b.type != D_NUMBER && b.type != D_RANGE) {
			error_runtime(vm->memory, vm->line, VM_INVALID_LIST_SUBSCRIPT);
			return none_data();
		}
		if ((b.type == D_NUMBER && (int)(b.value.number) >= list_size) ||
			(b.type == D_RANGE &&
			((range_start(b) > list_size || range_end(b) > list_size ||
			 range_start(b) < 0 || range_end(b) < 0)))) {
			error_runtime(vm->memory, vm->line, VM_LIST_REF_OUT_RANGE);
			return none_data();
		}

		if (b.type == D_NUMBER) {
			if (a.type == D_STRING) {
				struct data c = make_data(D_STRING, data_value_str("0"));
				c.value.string[0] = a.value.string[(int)floor(b.value.number)];
				return c;
			}
			else if (a.type == D_RANGE) {
				int end = range_end(a);
				int start = range_start(a);
				int index = (int)floor(b.value.number);
				if (start < end) {
					return make_data(D_NUMBER, data_value_num(start + index));
				}
				else {
					return make_data(D_NUMBER, data_value_num(start - index));
				}
			}
			// Add 1 to offset because of the header.
			int offset = floor(b.value.number) + 1;
			struct data* list_data = a.value.reference;
			return copy_data(list_data[offset]);
		}
		else {
			int start = range_start(b);
			int end = range_end(b);
			int subarray_size = start - end;
			if (subarray_size < 0) subarray_size *= -1;

			if (a.type == D_STRING) {
				struct data c = make_data(D_STRING, data_value_size(abs(start - end)));
				int n = 0;
				for (int i = start; i != end; start < end ? i++ : i--) {
					c.value.string[n++] = a.value.string[i];
				}
				c.value.string[n] = 0;
				return c;
			}
			else if (a.type == D_RANGE) {
				// Range of a Range...
				int a_start = range_start(a);
				int a_end = range_end(a); UNUSED(a_end);
				int b_start = range_start(b);
				int b_end = range_end(b);
				// Very relaxed about ranges, don't really care if it's out of
				//   bounds for now.
				// if (a_start + b_start > a_end || a_start + b_end < a_start) {
				// 	error_runtime(vm->memory, line, VM_RANGE_REF_OUT_OF_RANGE);
				// }
				return range_data(a_start + b_start, a_start + b_end);
			}

			struct data* new_subarray = wendy_list_malloc(vm->memory, subarray_size);
			// First belongs to the header.
			int n = 1;
			for (int i = start; i != end;
				start < end ? i++ : i--) {
				new_subarray[n++] = copy_data(a.value.reference[i + 1]);
			}
			return make_data(D_LIST, data_value_ptr(new_subarray));
		}
	}
	if (op == O_MEMBER) {
		// Member Access! Supports two built in, size and type, value.
		// Left side should be a token.
		// Regular Member, Must be either struct or a struct instance.
		//   Check for Regular Member before checking for built-in ones
		if (b.type != D_MEMBER_IDENTIFIER) {
			error_runtime(vm->memory, vm->line, VM_MEMBER_NOT_IDEN);
			return false_data();
		}
		if (a.type == D_TABLE) {
			struct table* table = (struct table*) a.value.reference[0].value.reference;
			if (table_exist(table, b.value.string)) {
				struct data result = copy_data(*table_find(table, b.value.string));
				if (result.type == D_FUNCTION) {
					// Hack because OP_CALL will send a reference to the table as the
					//   first argument
					result.type = D_STRUCT_FUNCTION;
				}
				return result;
			}
		}
		if (a.type == D_STRUCT || a.type == D_STRUCT_INSTANCE) {
			// Either will be allowed to look through static parameters.
			struct data* ptr = struct_get_field(vm, a, b.value.string);
			if (ptr) {
				struct data result = copy_data(*ptr);
				if (result.type == D_FUNCTION) {
					// Hack because OP_CALL will send a reference to the table as the
					//   first argument
					result.type = D_STRUCT_FUNCTION;
				}
				return result;
			}
		}
		if (streq("size", b.value.string)) {
			return size_of(a);
		}
		else if (streq("type", b.value.string)) {
			return type_of(a);
		}
		else if (streq("val", b.value.string)) {
			return value_of(a);
		}
		else if (streq("char", b.value.string)) {
			return char_of(a);
		}
		else if (a.type == D_TABLE && streq("keys", b.value.string)) {
			struct table* table = (struct table*) a.value.reference[0].value.reference;
			size_t size = table_size(table);
			struct data* new_subarray = wendy_list_malloc(vm->memory, size);
			table_write_keys_wendy_array(table, new_subarray);
			return make_data(D_LIST, data_value_ptr(new_subarray));
		}
		else if (a.type == D_RANGE &&
				 streq("start", b.value.string)) {
			return make_data(D_NUMBER,
				data_value_num(range_start(a)));
		}
		else if (a.type == D_RANGE &&
				 streq("end", b.value.string)) {
			return make_data(D_NUMBER,
				data_value_num(range_end(a)));
		}
		else if ((a.type == D_FUNCTION || a.type == D_STRUCT_FUNCTION) &&
			streq("closure", b.value.string)) {
			return copy_data(a.value.reference[1]);
		}
		else if ((a.type == D_FUNCTION || a.type == D_STRUCT_FUNCTION) &&
			streq("params", b.value.string)) {
			return copy_data(a.value.reference[3]);
		}
		else if (a.type == D_NONERET) {
			error_runtime(vm->memory, vm->line, VM_NOT_A_STRUCT_MAYBE_FORGOT_RET_THIS);
			return none_data();
		}
		// else if (!(a.type == D_STRUCT || a.type == D_STRUCT_INSTANCE)) {
		//	error_runtime(vm->memory, line, VM_NOT_A_STRUCT);
		//	return none_data();
		// }
		else {
			struct data type = type_of(a);
			error_runtime(vm->memory, vm->line, VM_MEMBER_NOT_EXIST, b.value.string, type.value.string);
			destroy_data_runtime(vm->memory, &type);
			return false_data();
		}
	}
	if (a.type == D_NUMBER && b.type == D_NUMBER) {
		switch (op) {
			case O_EQ:
				return (a.value.number == b.value.number) ?
					true_data() : false_data();
			case O_NEQ:
				return (a.value.number != b.value.number) ?
					true_data() : false_data();
			case O_GTE:
				return (a.value.number >= b.value.number) ?
					true_data() : false_data();
			case O_GT:
				return (a.value.number > b.value.number) ?
					true_data() : false_data();
			case O_LTE:
				return (a.value.number <= b.value.number) ?
					true_data() : false_data();
			case O_LT:
				return (a.value.number < b.value.number) ?
					true_data() : false_data();
			case O_RANGE:
				return range_data(a.value.number, b.value.number);
			case O_ADD:
				return make_data(D_NUMBER,
					data_value_num(a.value.number + b.value.number));
			case O_POWER:
				return make_data(D_NUMBER,
					data_value_num(pow(a.value.number, b.value.number)));
			case O_SUB:
				return make_data(D_NUMBER,
					data_value_num(a.value.number - b.value.number));
			case O_MUL:
				return make_data(D_NUMBER,
					data_value_num(a.value.number * b.value.number));
			case O_DIV:
			case O_IDIV:
			case O_REM:
				// check for division by zero error
				if (b.value.number == 0) {
					error_runtime(vm->memory, vm->line, VM_MATH_DISASTER);
				}
				else {
					if (op == O_REM) {
						// modulus enum vm_operator
						double a_n = a.value.number;
						double b_n = b.value.number;

						// check integer
						if (a_n != floor(a_n) || b_n != floor(b_n)) {
							return make_data(D_NUMBER, data_value_num(
								fmod(a.value.number, b.value.number)));
						}
						else {
							return make_data(D_NUMBER, data_value_num(
								(long long)a.value.number %
								(long long)b.value.number));
						}
					}
					else {
						double result = a.value.number / b.value.number;
						if (op == O_DIV) {
							return make_data(D_NUMBER, data_value_num(result));
						}
						else {
							return make_data(D_NUMBER, data_value_num((int)result));
						}
					}
				}
				break;
			default:
				error_runtime(vm->memory, vm->line, VM_NUM_NUM_INVALID_OPERATOR,
					operator_string[op]);
				break;
		}
	}
	else if((a.type == D_STRING && b.type == D_STRING) &&
			(op == O_EQ || op == O_NEQ)) {
		return (op == O_EQ) ^
			(streq(a.value.string, b.value.string)) ?
			false_data() : true_data();
	}
	else if ((a.type == D_NONE || b.type == D_NONE ||
			  a.type == D_NONERET || b.type == D_NONERET) &&
			(op == O_EQ || op == O_NEQ)) {
		return (op == O_EQ) ^
			((a.type == D_NONE || a.type == D_NONERET) && (b.type == D_NONE || b.type == D_NONERET)) ?
			false_data() : true_data();
	}
	else if((a.type == D_OBJ_TYPE && b.type == D_OBJ_TYPE) &&
			(op == O_EQ || op == O_NEQ)) {
		return (op == O_EQ) ^
			(streq(a.value.string, b.value.string)) ?
			false_data() : true_data();
	}

	if (a.type == D_LIST || b.type == D_LIST) {
		if (a.type == D_LIST && b.type == D_LIST) {
			size_t size_a = wendy_list_size(&a);
			size_t size_b = wendy_list_size(&b);

			switch (op) {
				case O_EQ: {
					if (size_a != size_b) {
						return false_data();
					}
					for (size_t i = 0; i < size_a; i++) {
						if (!data_equal(&a.value.reference[i + 1],
										&b.value.reference[i + 1])) {
							return false_data();
						}
					}
					return true_data();
				}
				case O_NEQ: {
					if (size_a != size_b) {
						return true_data();
					}
					for (size_t i = 0; i < size_a; i++) {
						if (data_equal(&a.value.reference[i + 1],
										&b.value.reference[i + 1])) {
							return false_data();
						}
					}
					return true_data();
				}
				case O_ADD: {
					size_t new_size = size_a + size_b;
					struct data* new_list = wendy_list_malloc(vm->memory, new_size);
					size_t n = 1; // first is the header
					for (size_t i = 0; i < size_a; i++) {
						new_list[n++] = copy_data(a.value.reference[i + 1]);
					}
					for (size_t i = 0; i < size_b; i++) {
						new_list[n++] = copy_data(b.value.reference[i + 1]);
					}
					return make_data(D_LIST, data_value_ptr(new_list));
				}
				default: error_runtime(vm->memory, vm->line, VM_LIST_LIST_INVALID_OPERATOR,
					operator_string[op]); break;
			}
		} // End A==List && B==List
		else if (a.type == D_LIST) {
			if (op == O_ADD) {
				if (b.type == D_NONERET) {
					return copy_data(a);
				}
				// list + element
				size_t size_a = wendy_list_size(&a);

				struct data* new_list = wendy_list_malloc(vm->memory, size_a + 1);
				size_t n = 1;
				for (size_t i = 0; i < size_a; i++) {
					new_list[n++] = copy_data(a.value.reference[i + 1]);
				}
				new_list[n++] = copy_data(b);
				return make_data(D_LIST, data_value_ptr(new_list));
			}
			else if (op == O_MUL && b.type == D_NUMBER) {
				// list * number
				size_t size_a = wendy_list_size(&a);
				// Size expansion
				size_t new_size = size_a * (int)b.value.number;
				struct data* new_list = wendy_list_malloc(vm->memory, new_size);
				// Copy all Elements n times
				size_t n = 1;
				for (size_t i = 0; i < new_size; i++) {
					new_list[n++] = copy_data(a.value.reference[(i % size_a) + 1]);
				}
				return make_data(D_LIST, data_value_ptr(new_list));
			}
			else {
				error_runtime(vm->memory, vm->line, VM_INVALID_APPEND, operator_string[op]);
			}
		}
		else if (b.type == D_LIST) {
			size_t size_b = wendy_list_size(&b);

			if (op == O_ADD) {
				if (a.type == D_NONERET) {
					return copy_data(b);
				}
				// element + list
				struct data* new_list = wendy_list_malloc(vm->memory, size_b + 1);
				size_t n = 1;
				new_list[n++] = copy_data(a);
				for (size_t i = 0; i < size_b; i++) {
					new_list[n++] = copy_data(b.value.reference[i + 1]);
				}
				return make_data(D_LIST, data_value_ptr(new_list));
			}
			else if (op == O_MUL && a.type == D_NUMBER) {
				// number * list
				size_t size_b = wendy_list_size(&b);
				// Size expansion
				size_t new_size = size_b * (int)a.value.number;
				struct data* new_list = wendy_list_malloc(vm->memory, new_size);
				// Copy all Elements n times
				size_t n = 1;
				for (size_t i = 0; i < new_size; i++) {
					new_list[n++] = copy_data(b.value.reference[(i % size_b) + 1]);
				}
				return make_data(D_LIST, data_value_ptr(new_list));
			}
			else if (op == O_IN) {
				// element in list
				for (size_t i = 0; i < size_b; i++) {
					if (data_equal(&a, &b.value.reference[i + 1])) {
						return true_data();
					}
				}
				return false_data();
			}
			else { error_runtime(vm->memory, vm->line, VM_INVALID_APPEND, operator_string[op]); }
		}
	}
	else if((a.type == D_STRING && b.type == D_STRING) ||
			(a.type == D_STRING && b.type == D_NUMBER) ||
			(a.type == D_NUMBER && b.type == D_STRING)) {
		if (op == O_ADD) {
			// string concatenation
			size_t total_len = 0;
			if (a.type == D_NUMBER) {
				total_len += snprintf(NULL, 0, "%g", a.value.number);
			}
			else {
				total_len += strlen(a.value.string);
			}
			if (b.type == D_NUMBER) {
				total_len += snprintf(NULL, 0, "%g", a.value.number);
			}
			else {
				total_len += strlen(b.value.string);
			}

			struct data result = make_data(D_STRING, data_value_size(total_len));
			size_t length = 0;
			if (a.type == D_NUMBER) {
				length += sprintf(result.value.string + length, "%g", a.value.number);
			}
			else {
				length += sprintf(result.value.string + length, "%s", a.value.string);
			}
			if (b.type == D_NUMBER) {
				length += sprintf(result.value.string + length, "%g", b.value.number);
			}
			else {
				length += sprintf(result.value.string + length, "%s", b.value.string);
			}
			return result;
		}
		else if (op == O_MUL && (a.type == D_NUMBER || b.type == D_NUMBER)) {
			// String Duplication (String and Number)
			int times = 0;
			char* string;
			int size;
			char* result;
			if (a.type == D_NUMBER) {
				times = (int) a.value.number;
				if (times < 0) {
					error_runtime(vm->memory, vm->line, VM_STRING_DUPLICATION_NEGATIVE);
					return copy_data(b);
				}
				size = times * strlen(b.value.string) + 1;
				string = b.value.string;
			}
			else {
				times = (int) b.value.number;
				if (times < 0) {
					error_runtime(vm->memory, vm->line, VM_STRING_DUPLICATION_NEGATIVE);
					return copy_data(b);
				}
				size = times * strlen(a.value.string) + 1;
				string = a.value.string;
			}
			result = safe_malloc(size * sizeof(char));
			result[0] = 0;
			for (int i = 0; i < times; i++) {
				// Copy it over n times
				strcat(result, string);
			}
			struct data t = make_data(D_STRING, data_value_str(result));
			safe_free(result);
			return t;
		}
		else {
			error_runtime(vm->memory, vm->line,
				(a.type == D_STRING && b.type == D_STRING) ?
				VM_STRING_STRING_INVALID_OPERATOR : VM_STRING_NUM_INVALID_OPERATOR,
				operator_string[op]);
			return none_data();
		}
	}
	else if((a.type == D_TRUE || a.type == D_FALSE) &&
			(b.type == D_TRUE || b.type == D_FALSE)) {
		switch (op) {
			case O_AND:
				return (a.type == D_TRUE && b.type == D_TRUE) ?
					true_data() : false_data();
			case O_OR:
				return (a.type == D_FALSE && b.type == D_FALSE) ?
					false_data() : true_data();
			default:
				error_runtime(vm->memory, vm->line, VM_TYPE_ERROR, operator_string[op]);
				break;
		}
	}
	else if (is_reference(a) && is_reference(b)) {
		// TODO(felixguo): Maybe check for type here?
		if (op == O_EQ) {
			return (a.value.reference == b.value.reference) ?
				true_data() : false_data();
		}
		else if (op == O_NEQ) {
			return (a.value.reference != b.value.reference) ?
				true_data() : false_data();
		}
		error_runtime(vm->memory, vm->line, VM_TYPE_ERROR, operator_string[op]);
	}
	else {
		error_runtime(vm->memory, vm->line, VM_TYPE_ERROR, operator_string[op]);
	}
	return none_data();
}

static struct data value_of(struct data a) {
	if (a.type == D_STRING && strlen(a.value.string) == 1) {
		return make_data(D_NUMBER, data_value_num(a.value.string[0]));
	}
	else {
		return copy_data(a);
	}
}

static struct data char_of(struct data a) {
	if (a.type == D_NUMBER && a.value.number >= 0 && a.value.number <= 127) {
		struct data res = make_data(D_STRING, data_value_str(" "));
		res.value.string[0] = (char)a.value.number;
		return res;
	}
	else {
		return copy_data(a);
	}
}

static struct data size_of(struct data a) {
	double size = 0;
	if (a.type == D_STRING) {
		size = strlen(a.value.string);
	}
	else if (a.type == D_LIST) {
		size = wendy_list_size(&a);
	}
	else if (a.type == D_TABLE) {
		size = table_size((struct table*) a.value.reference[0].value.reference);
	}
	else if (a.type == D_RANGE) {
		size = abs(range_end(a) - range_start(a));
	}
	else if (a.type == D_SPREAD) {
		size = size_of(*a.value.reference).value.number;
	}
	return make_data(D_NUMBER, data_value_num(size));
}

static char* type_of_str(struct data a) {
	switch (a.type) {
		case D_FUNCTION:
			return "function";
		case D_STRING:
			return "string";
		case D_NUMBER:
			return "number";
		case D_TRUE:
		case D_FALSE:
			return "bool";
		case D_NONE:
			return "none";
		case D_NONERET:
			return "noneret";
		case D_RANGE:
			return "range";
		case D_LIST:
			return "list";
		case D_CLOSURE:
			return "closure";
		case D_STRUCT:
			return "struct";
		case D_SPREAD:
			return "spread";
		case D_OBJ_TYPE:
			return "type";
		case D_TABLE:
			return "table";
		case D_ANY:
			return "any";
		case D_STRUCT_INSTANCE:
			return a.value.reference[1].value.reference[1].value.string;
		default:
			return "unknown";
	}
}

static struct data type_of(struct data a) {
	return make_data(D_OBJ_TYPE, data_value_str(type_of_str(a)));
}

static struct data eval_uniop(struct vm * vm, enum vm_operator op, struct data a) {
	if (op == O_COPY) {
		// Create copy of object a, only applies to lists or
		// struct or struct instances
		if (a.type == D_LIST) {
			// We make a copy of the list as pointed to A.
			size_t list_size = wendy_list_size(&a);
			struct data* new_a = wendy_list_malloc(vm->memory, list_size);
			int n = 1;
			for (size_t i = 0; i < list_size; i++) {
				new_a[n++] = copy_data(a.value.reference[i + 1]);
			}
			return make_data(D_LIST, data_value_ptr(new_a));
		}
		else {
			// No copy needs to be made
		return a;
		}
	}
	else if (op == O_NEG) {
		if (a.type != D_NUMBER) {
			error_runtime(vm->memory, vm->line, VM_INVALID_NEGATE);
			return none_data();
		}
		struct data res = make_data(D_NUMBER, data_value_num(-1 * a.value.number));
		return res;
	}
	else if (op == O_NOT) {
		if (a.type != D_TRUE && a.type != D_FALSE) {
			error_runtime(vm->memory, vm->line, VM_INVALID_NEGATE);
			return none_data();
		}
		return a.type == D_TRUE ? false_data() : true_data();
	}
	else if (op == O_SPREAD) {
		// Expandable Types
		if (a.type != D_LIST && a.type != D_RANGE && a.type != D_STRING) {
			error_runtime(vm->memory, vm->line, VM_SPREAD_NOT_ITERABLE);
			return none_data();
		}
		struct data *storage = refcnt_malloc(vm->memory, 1);
		*storage = copy_data(a);
		return make_data(D_SPREAD, data_value_ptr(storage));
	}
	else {
		// fallback case
		return none_data();
	}
}

void print_current_bytecode(struct vm * vm) {
	print_bytecode(vm->bytecode, vm->bytecode_size, stdout);
}
