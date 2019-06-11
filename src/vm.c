#include "vm.h"
#include "codegen.h"
#include "memory.h"
#include "error.h"
#include "stdint.h"
#include "math.h"
#include "global.h"
#include "native.h"
#include "imports.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>

static int line;
static address i = 0;
static uint8_t* bytecode = 0;
static size_t bytecode_size = 0;
static char* last_pushed_identifier;

// Forward Declarations
static struct data eval_binop(enum operator op, struct data a, struct data b);
static struct data eval_uniop(enum operator op, struct data a);
static struct data type_of(struct data a);
static struct data size_of(struct data a);
static struct data value_of(struct data a);
static struct data char_of(struct data a);

address get_instruction_pointer() {
	return i;
}

void vm_cleanup_if_repl() {
	// bytecode could be null if codegen didn't generate anything
	if (bytecode) safe_free(bytecode);
}

#define num_args(...) (sizeof((char*[]){__VA_ARGS__})/sizeof(char*))
#define first_that(condition, ...) first_that_impl(condition, num_args(__VA_ARGS__), __VA_ARGS__)
static char* first_that_impl(bool (*condition)(char*), int count, ...) {
	va_list ap;
	va_start(ap, count);
	for (int i = 0; i < count; i++) {
		char* s = va_arg(ap, char*);
		if (condition(s)) {
			return s;
		}
	}
	va_end(ap);
	return 0;
}

static bool _id_exist(char* fn) {
	return id_exist(fn, true);
}

static char* get_binary_overload_name(enum operator op, struct data a, struct data b) {
	struct data type_a = type_of(a);
	struct data type_b = type_of(b);
	char* fn_name = safe_concat(OPERATOR_OVERLOAD_PREFIX, type_a.value.string,
		operator_string[op], type_b.value.string);

	destroy_data(&type_a);
	destroy_data(&type_b);
	return fn_name;
}

static char* get_unary_overload_name(enum operator op, struct data a) {
	struct data type_a = type_of(a);
	char* fn_name = safe_concat(OPERATOR_OVERLOAD_PREFIX,
		operator_string[op], type_a.value.string);

	destroy_data(&type_a);
	return fn_name;
}

static char* get_print_overload_name(struct data a) {
	struct data type_a = type_of(a);
	char* fn_name = safe_concat(OPERATOR_OVERLOAD_PREFIX "@", type_a.value.string);
	destroy_data(&type_a);
	return fn_name;
}

void vm_run(uint8_t* new_bytecode, size_t size) {
	if (get_settings_flag(SETTINGS_DRY_RUN)) {
		return;
	}
	// Verify Header
	address start_at;
	size_t saved_size = bytecode_size;
	if (!get_settings_flag(SETTINGS_REPL)) {
		bytecode = new_bytecode;
		start_at = verify_header(bytecode);
	}
	else {
		// REPL Bytecode has no headers!
		// Resize Bytecode Block, Offset New Addresses, Push to End
		bytecode_size += size;
		if (bytecode) {
			// This gets rid of the OP_HALT from the previous chain of BC
			bytecode_size--;
			bytecode = safe_realloc(bytecode, bytecode_size * sizeof(uint8_t));
		}
		else {
			bytecode = safe_malloc(bytecode_size * sizeof(uint8_t));
		}
		if (saved_size != 0) {
			start_at = saved_size - 1;
		}
		else {
			start_at = 0;
		}
		offset_addresses(new_bytecode, size, start_at);
		for (size_t i = 0; i < size; i++) {
			bytecode[start_at + i] = new_bytecode[i];
		}
	}
	for (i = start_at;;) {
		reset_error_flag();
		enum opcode op = bytecode[i];
		if (get_settings_flag(SETTINGS_TRACE_VM)) {
			// This branch could slow down the VM but the CPU should branch
			// predict after one or two iterations.
			printf(BLU "<+%04X>: " RESET "%s\n", i, opcode_string[op]);
		}
		i += 1;
		switch (op) {
			case OP_PUSH: {
				struct data t = get_data(bytecode + i, &i);
				// t will never be a reference type
				struct data d;
				if (t.type == D_IDENTIFIER) {
					if (streq(t.value.string, "time")) {
						d = time_data();
					}
					else {
						struct data* value = get_value_of_id(t.value.string, line);
						if (!value) {
							break;
						}
						d = copy_data(*value);
					}
				}
				else {
					d = copy_data(t);
				}
				last_pushed_identifier = t.value.string;
				push_arg(d);
				break;
			}
			case OP_BIN: {
				enum operator op = bytecode[i++];
				struct data a = pop_arg(line);
				struct data b = pop_arg(line);
				struct data any_d = any_data();
				char* a_and_b = get_binary_overload_name(op, a, b);
				char* any_a = get_binary_overload_name(op, any_d, b);
				char* any_b = get_binary_overload_name(op, a, any_d);
				destroy_data(&any_d);
				char* fn_name = first_that(_id_exist, a_and_b, any_a, any_b);
				if (fn_name) {
					push_arg(make_data(D_END_OF_ARGUMENTS, data_value_num(0)));
					push_arg(b);
					push_arg(a);
					push_arg(copy_data(*get_value_of_id(fn_name, line)));
					safe_free(a_and_b);
					safe_free(any_a);
					safe_free(any_b);
					goto wendy_vm_call;
				}
				else {
					push_arg(eval_binop(op, a, b));
					destroy_data(&a);
					destroy_data(&b);
				}
				safe_free(a_and_b);
				safe_free(any_a);
				safe_free(any_b);
				break;
			}
			case OP_UNA: {
				enum operator op = bytecode[i++];
				struct data a = pop_arg(line);
				char* fn_name = get_unary_overload_name(op, a);
				if (id_exist(fn_name, true)) {
					push_arg(make_data(D_END_OF_ARGUMENTS, data_value_num(0)));
					push_arg(a);
					push_arg(copy_data(*get_value_of_id(fn_name, line)));
					safe_free(fn_name);
					goto wendy_vm_call;
				}
				else {
					// Uni-op has a special spread operator that returns noneret
					struct data result = eval_uniop(op, a);
					push_arg(result);
					destroy_data(&a);
				}
				safe_free(fn_name);
				break;
			}
			case OP_SRC: {
				void* ad = &bytecode[i];
				line = get_address(ad, &i);
				break;
			}
			case OP_NATIVE: {
				void* ag = &bytecode[i];
				address args = get_address(ag, &i);
				char* name = get_string(bytecode + i, &i);
				native_call(name, args, line);
				break;
			}
			case OP_DECL: {
				char *id = get_string(bytecode + i, &i);
				if (id_exist(id, false)) {
					address a = get_stack_pos_of_id(id, line);
					if (!call_stack[a].is_closure) {
						error_runtime(line, VM_VAR_DECLARED_ALREADY, id);
						continue;
					}
				}
				struct stack_entry* result = push_stack_entry(id, line);
				push_arg(make_data(D_INTERNAL_POINTER, data_value_ptr(&result->val)));
				last_pushed_identifier = id;
				break;
			}
			case OP_WHERE: {
				char* id = get_string(bytecode + i, &i);
				last_pushed_identifier = id;
				push_arg(make_data(D_INTERNAL_POINTER,
					data_value_ptr(get_address_of_id(id, line))
				));
				break;
			}
			case OP_IMPORT: {
				char* name = get_string(bytecode + i, &i);
				address a = get_address(bytecode + i, &i);
				if (has_already_imported_library(name)) {
					i = a;
				}
				else {
					add_imported_library(name);
				}
				break;
			}
			case OP_ARGCLN: {
				// TODO: 128 limit here seems a bit arbitrary and we
				struct data* extra_args = wendy_list_malloc(128);
				size_t count = 0;
				while (top_arg(line)->type != D_END_OF_ARGUMENTS) {
					if (top_arg(line)->type == D_NAMED_ARGUMENT_NAME) {
						struct data identifier = pop_arg(line);
						struct data* loc = get_address_of_id(identifier.value.string, line);
						destroy_data(loc);
						*loc = pop_arg(line);
						destroy_data(&identifier);
					}
					else {
						extra_args[count + 1] = pop_arg(line);
						count += 1;
					}
				}
				// We need to re-write the list counter
				extra_args[0].value.number = count;
				// Assign "arguments" variable with rest of the arguments.
				push_stack_entry("arguments", line)->val =
					make_data(D_LIST, data_value_ptr(extra_args));
				// Pop End of Arguments
				struct data eoargs = pop_arg(line);
				destroy_data(&eoargs);
				break;
			}
			case OP_RET: {
				if (frame_pointer == 0) {
					// We tried to return from the main function!
					error_runtime(line, VM_RET_FROM_MAIN);
					continue;
				}
				pop_frame(true, &i);
				break;
			}
			case OP_INC: {
				struct data ptr = pop_arg(line);
				if (ptr.type != D_INTERNAL_POINTER) {
					error_runtime(line, VM_INTERNAL_ERROR, "INC on non-pointer");
					continue;
				}
				struct data* arg = ptr.value.reference;
				if (arg->type != D_NUMBER) {
					error_runtime(line, VM_TYPE_ERROR, "INC");
					continue;
				}
				arg->value.number += 1;
				break;
			}
			case OP_DEC: {
				struct data ptr = pop_arg(line);
				if (ptr.type != D_INTERNAL_POINTER) {
					error_runtime(line, VM_INTERNAL_ERROR, "DEC on non-pointer");
					continue;
				}
				struct data* arg = ptr.value.reference;
				if (arg->type != D_NUMBER) {
					error_runtime(line, VM_TYPE_ERROR, "DEC");
					continue;
				}
				arg->value.number -= 1;
				break;
			}
			case OP_FRM: {
				push_auto_frame(i, "automatic", line);
				break;
			}
			case OP_END: {
				pop_frame(false, &i);
				break;
			}
			case OP_MKREF: {
				enum data_type type = bytecode[i++];
				size_t size = get_address(&bytecode[i], &i);
				struct data reference = make_data(type, data_value_ptr(NULL));

				if (!is_reference(reference)) {
					error_runtime(line, VM_INTERNAL_ERROR,
						"MKREF called on non-reference type");
					continue;
				}

				struct data* storage = refcnt_malloc(size);

				// can't simply check additional_space because spread could
				//   expand to 1 element too
				bool has_spread = false;
				size_t additional_space = 0;

				size_t i = size;
				while (i > 0) {
					struct data next = pop_arg(line);
					storage[i - 1] = next;
					i -= 1;
					if (next.type == D_SPREAD) {
						additional_space += size_of(*next.value.reference).value.number - 1;
						has_spread = true;
					}
				}

				if (has_spread) {
					struct data* new_storage =
						refcnt_malloc(size + additional_space);
					size_t j = 0;
					for (size_t i = 0; i < size; i++) {
						if (storage[i].type == D_SPREAD) {
							struct data spread = storage[i].value.reference[0];
							if (spread.type == D_LIST) {
								for (size_t k = 0; k < spread.value.reference[0].value.number; k++) {
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
					refcnt_free(storage);
					storage = new_storage;
					if (storage[0].type == D_LIST_HEADER) {
						// - 1 for header
						storage[0].value.number = size + additional_space - 1;
					}
				}

				reference.value.reference = storage;
				push_arg(reference);
				break;
			}
			case OP_NTHPTR: {
				struct data number = pop_arg(line);
				if (number.type != D_NUMBER) {
					error_runtime(line, VM_INVALID_LVALUE_LIST_SUBSCRIPT);
					continue;
				}
				struct data list = pop_arg(line);
				if (list.type != D_LIST) {
					error_runtime(line, VM_NOT_A_LIST);
					goto nthptr_cleanup;
				}
				struct data* list_data = list.value.reference;
				if (list_data->type != D_LIST_HEADER) {
					error_runtime(line, VM_INTERNAL_ERROR, "List doesn't point to list header!");
					goto nthptr_cleanup;
				}
				size_t list_size = list_data->value.number;
				if (number.value.number >= list_size) {
					error_runtime(line, VM_LIST_REF_OUT_RANGE);
					goto nthptr_cleanup;
				}
				push_arg(make_data(D_INTERNAL_POINTER,
					data_value_ptr(&list_data[(int)number.value.number + 1])));

				nthptr_cleanup:
					destroy_data(&list);
				break;
			}
			case OP_CLOSURE: {
				// create_closure() could return NULL when there's no closure, so the
				//   refcnt code is structured to ignore null pointer references
				push_arg(make_data(D_CLOSURE, data_value_ptr(create_closure())));
				break;
			}
			case OP_MEMPTR: {
				// Member Pointer
				// Structs can only modify Static members, instances modify instance
				//   members.
				// Either will be allowed to look through static parameters.
				struct data instance = pop_arg(line);
				char* member = get_string(bytecode + i, &i);
				if (instance.type != D_STRUCT && instance.type != D_STRUCT_INSTANCE) {
					if (instance.type == D_NONERET) {
						error_runtime(line, VM_NOT_A_STRUCT_MAYBE_FORGOT_RET_THIS);
					} else {
						error_runtime(line, VM_NOT_A_STRUCT);
					}
					destroy_data(&instance);
					break;
				}
				struct data* metadata = instance.value.reference;
				if (instance.type == D_STRUCT_INSTANCE) {
					// metadata actually points to the STRUCT_INSTANCE_HEAD
					//   right now.
					metadata = metadata[1].value.reference;
					// TODO: assert metadata is a metadata
				}
				enum data_type struct_type = instance.type;
				bool found = false;
				int params_passed = 0;
				int size = metadata[0].value.number;
				for (int i = 0; i < size; i++) {
					struct data mdata = metadata[i + 1];
					if (mdata.type == D_STRUCT_SHARED &&
						streq(mdata.value.string, member)) {
						// Found the static member we were looking for.
						push_arg(make_data(D_INTERNAL_POINTER, data_value_ptr(&metadata[i + 2])));
						found = true;
						break;
					}
					else if (mdata.type == D_STRUCT_PARAM) {
						if (struct_type == D_STRUCT_INSTANCE &&
							streq(mdata.value.string, member)) {
							// Found the instance member we were looking for.
							// Address of the STRUCT_INSTANCE_HEADER offset by
							//   params_passed + 1;
							push_arg(make_data(D_INTERNAL_POINTER, data_value_ptr(&instance.value.reference[params_passed + 2])));
							found = true;
							break;
						}
						params_passed++;
					}
				}
				if (!found) {
					error_runtime(line, VM_MEMBER_NOT_EXIST, member);
				}
				destroy_data(&instance);
				break;
			}
			case OP_JMP: {
				address addr = get_address(&bytecode[i], &i);
				i = addr;
				break;
			}
			case OP_JIF: {
				// Jump IF False Instruction
				struct data top = pop_arg(line);
				address addr = get_address(&bytecode[i], &i);
				if (top.type != D_TRUE && top.type != D_FALSE) {
					error_runtime(line, VM_COND_EVAL_NOT_BOOL);
				}
				if (top.type == D_FALSE) {
					i = addr;
				}
				destroy_data(&top);
				break;
			}
			case OP_CALL:
			wendy_vm_call: {
				struct data top = pop_arg(line);
				if (top.type != D_FUNCTION && top.type != D_STRUCT && top.type != D_STRUCT_FUNCTION) {
					error_runtime(line, VM_FN_CALL_NOT_FN);
					destroy_data(&top);
					break;
				}
				struct data boundName = top.value.reference[2];
				char* function_disp = safe_malloc((128 + strlen(boundName.value.string)) * sizeof(char));
				function_disp[0] = 0;
				if (boundName.value.string && streq(boundName.value.string, "self")) {
					sprintf(function_disp, "annonymous:0x%X", i);
				}
				else {
					sprintf(function_disp, "%s:0x%X", boundName.value.string, i);
				}
				push_frame(function_disp, i, line);
				safe_free(function_disp);

				if (top.type == D_STRUCT) {
					// Calling Struct Constructor
					struct data* metadata = top.value.reference;

					struct data old_top = top;

					// Select `init` function.
					top = copy_data(metadata[3]);

					if (top.type != D_FUNCTION) {
						error_runtime(line, VM_STRUCT_CONSTRUCTOR_NOT_A_FUNCTION);
						destroy_data(&top);
						break;
					}
					top.type = D_STRUCT_FUNCTION;

					// Find how many STRUCT_PARAMs there are
					size_t params = 0;
					size_t metadata_size = metadata[0].value.number;
					for (size_t i = 0; i < metadata_size; i++) {
						if (metadata[i + 1].type == D_STRUCT_PARAM) {
							params++;
						}
					}

					// +1 for the header, +1 for the metadata pointer
					struct data* struct_instance = refcnt_malloc(params + 2);
					struct_instance[0] = make_data(D_STRUCT_INSTANCE_HEADER, data_value_num(params + 1));
					struct_instance[1] = make_data(D_STRUCT_METADATA, data_value_ptr(refcnt_copy(metadata)));

					for (size_t i = 0; i < params; i++) {
						struct_instance[i + 2] = none_data();
					}

					push_arg(make_data(D_STRUCT_INSTANCE, data_value_ptr(struct_instance)));
					destroy_data(&old_top);
				}

				struct data addr = top.value.reference[0];
				if (addr.type != D_INSTRUCTION_ADDRESS) {
					error_runtime(line, VM_INTERNAL_ERROR, "Address of function is not D_INSTRUCTION_ADDRESS");
					destroy_data(&top);
					break;
				}
				i = (address) addr.value.number;

				// Resolve Spread Objects in the Call List
				size_t ptr = working_stack_pointer - 1;
				bool has_spread = false;
				size_t additional_size = 0;
				size_t argc = 0;
				while (working_stack[ptr].type != D_END_OF_ARGUMENTS) {
					if (working_stack[ptr].type == D_SPREAD) {
						has_spread = true;
						additional_size +=
							size_of(*working_stack[ptr].value.reference).value.number - 1;
					}
					argc += 1;
					ptr -= 1;
				}

				if (has_spread) {
					// Expand the Stack if required
					ensure_working_stack_size(additional_size);
					// In-place move from the back
					size_t og_ptr = working_stack_pointer - 1;
					size_t new_ptr = working_stack_pointer + additional_size - 1;
					for (; working_stack[og_ptr].type != D_END_OF_ARGUMENTS; og_ptr--) {
						if (working_stack[og_ptr].type == D_SPREAD) {
							struct data og_spread = working_stack[og_ptr];
							struct data spread = og_spread.value.reference[0];
							if (spread.type == D_LIST) {
								for (size_t k = 0; k < spread.value.reference[0].value.number; k++) {
									working_stack[new_ptr--] = copy_data(spread.value.reference[k + 1]);
								}
							}
							else if (spread.type == D_RANGE) {
								int start = range_start(spread);
								int end = range_end(spread);
								for (int k = start; k != end; start < end ? k++ : k--) {
									working_stack[new_ptr--] = make_data(D_NUMBER, data_value_num(k));
								}
							}
							else if (spread.type == D_STRING) {
								size_t len = strlen(spread.value.string);
								for (size_t k = 0; k < len; k++) {
									struct data str = make_data(D_STRING, data_value_str(" "));
									str.value.string[0] = spread.value.string[k];
									working_stack[new_ptr--] = str;
								}
							}
							destroy_data(&og_spread);
						}
						else {
							working_stack[new_ptr--] = working_stack[og_ptr];
						}
					}
					working_stack_pointer += additional_size;
				}

				if (top.type == D_STRUCT_FUNCTION) {
					// Either we pushed the new instance on the stack on top, or
					//   codegen generated the instance on the top.
					struct data instance = pop_arg(line);
					if (instance.type != D_STRUCT_INSTANCE && instance.type != D_STRUCT) {
						error_runtime(line, VM_INTERNAL_ERROR, "D_STRUCT_FUNCTION encountered but top of stack is not a instance nor a struct.");
						destroy_data(&top);
						destroy_data(&instance);
						break;
					}
					push_stack_entry("this", line)->val = instance;
				}

				// Push closure variables
				struct data *list_data = top.value.reference[1].value.reference;
				size_t size = list_data[0].value.number;

				// Move the pointer to the "first" item, because the 0th item is the list-header
				list_data += 1;
				for (size_t i = 0; i < size; i += 2) {
					if (list_data[i].type != D_IDENTIFIER) {
						error_runtime(line, VM_INTERNAL_ERROR, "Did not find a D_IDENTIFIER in function closure");
						destroy_data(&top);
						break;
					}
					struct stack_entry *entry = push_stack_entry(list_data[i].value.string, line);
					entry->val = copy_data(list_data[i + 1]);
					entry->is_closure = true;
				}

				// At this point, we put `top` back into the stack, so no need to destroy it
				if (strcmp(boundName.value.string, "self") != 0) {
					push_stack_entry("self", line)->val = copy_data(top);
				}
				push_stack_entry(boundName.value.string, line)->val = top;
				break;
			}
			case OP_WRITE: {
				struct data ptr = pop_arg(line);
				if (ptr.type != D_INTERNAL_POINTER) {
					error_runtime(line, VM_INTERNAL_ERROR, "WRITE on non-pointer");
					destroy_data(&ptr);
					continue;
				}

				if (top_arg(line)->type == D_END_OF_ARGUMENTS ||
					top_arg(line)->type == D_NAMED_ARGUMENT_NAME) {
					if (ptr.value.reference->type == D_EMPTY) {
						*(ptr.value.reference) = none_data();
					}
					break;
				}

				struct data value = pop_arg(line);

				// Since value is written back, we don't need to destroy it
				if (value.type == D_NONERET) {
					error_runtime(line, VM_ASSIGNING_NONERET);
					destroy_data(&value);
					break;
				}

				destroy_data(&ptr.value.reference[0]);
				*(ptr.value.reference) = value;

				if (ptr.value.reference->type == D_FUNCTION) {
					// Write Name to Function
					char* bind_name = last_pushed_identifier;
					struct data* fn_data = ptr.value.reference->value.reference;
					fn_data[2].value.string = safe_realloc(
						fn_data[2].value.string, strlen(bind_name) + 1);
					strcpy(fn_data[2].value.string, bind_name);
				}
				break;
			}
			case OP_OUT: {
				struct data t = pop_arg(line);
				if (t.type != D_NONERET) {
					char* fn_name = get_print_overload_name(t);
					if (id_exist(fn_name, true)) {
						push_arg(make_data(D_END_OF_ARGUMENTS, data_value_num(0)));
						push_arg(t);
						push_arg(copy_data(*get_value_of_id(fn_name, line)));
						safe_free(fn_name);
						/* This i-- allows the overloaded function to return
						 * a string / object and have that be the printed
						 * output, i.e. it will call function and execute
						 * the OP_OUT again */
						i--;
						goto wendy_vm_call;
					}
					safe_free(fn_name);
					print_data(&t);
				}
				destroy_data(&t);
				break;
			}
			case OP_OUTL: {
				struct data t = pop_arg(line);
				if (t.type != D_NONERET) {
					char* fn_name = get_print_overload_name(t);
					if (id_exist(fn_name, true)) {
						push_arg(make_data(D_END_OF_ARGUMENTS, data_value_num(0)));
						push_arg(t);
						push_arg(copy_data(*get_value_of_id(fn_name, line)));
						safe_free(fn_name);
						destroy_data(&t);
						/* This i-- allows the overloaded function to return
						 * a string / object and have that be the printed
						 * output, i.e. it will call function and execute
						 * the OP_OUT again */
						i--;
						goto wendy_vm_call;
					}
					safe_free(fn_name);
					print_data_inline(&t, stdout);
				}
				destroy_data(&t);
				break;
			}
			case OP_IN: {
				// Scan one line from the input.
				struct data ptr = pop_arg(line);
				if (ptr.type != D_INTERNAL_POINTER) {
					error_runtime(line, VM_INTERNAL_ERROR, "INC on non-pointer");
					continue;
				}
				struct data* storage = ptr.value.reference;
				destroy_data(storage);

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
				return;
			// No default: here because we want compiler to catch missing cases.
		}
		if (get_error_flag()) {
			clear_working_stack();
			break;
		}
	}
}

static struct data eval_binop(enum operator op, struct data a, struct data b) {
	if (op == O_SUBSCRIPT) {
		// Array Reference, or String
		// A must be a list/string/range, b must be a number.
		if (a.type != D_LIST && a.type != D_STRING && a.type != D_RANGE) {
			error_runtime(line, VM_TYPE_ERROR, operator_string[op]);
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
			list_size = a.value.reference->value.number;
		}

		if (b.type != D_NUMBER && b.type != D_RANGE) {
			error_runtime(line, VM_INVALID_LIST_SUBSCRIPT);
			return none_data();
		}
		if ((b.type == D_NUMBER && (int)(b.value.number) >= list_size) ||
			(b.type == D_RANGE &&
			((range_start(b) > list_size || range_end(b) > list_size ||
			 range_start(b) < 0 || range_end(b) < 0)))) {
			error_runtime(line, VM_LIST_REF_OUT_RANGE);
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
				// 	error_runtime(line, VM_RANGE_REF_OUT_OF_RANGE);
				// }
				return range_data(a_start + b_start, a_start + b_end);
			}

			struct data* new_subarray = wendy_list_malloc(subarray_size);
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
			error_runtime(line, VM_MEMBER_NOT_IDEN);
			return false_data();
		}
		if (a.type == D_STRUCT || a.type == D_STRUCT_INSTANCE) {
			// Either will be allowed to look through static parameters.
			struct data* metadata = a.value.reference;
			if (a.type == D_STRUCT_INSTANCE) {
				// metadata actually points to the STRUCT_INSTANCE_HEADER
				//   right now, we need one below that for the metadata
				metadata = metadata[1].value.reference;
			}
			enum data_type struct_type = a.type;

			int params_passed = 0;
			int size = metadata[0].value.number;
			for (int i = 0; i < size; i++) {
				struct data mdata = metadata[i + 1];
				if (mdata.type == D_STRUCT_SHARED &&
					streq(mdata.value.string, b.value.string)) {
					// Found the static member we were looking for
					struct data result = copy_data(metadata[i + 2]);
					if (result.type == D_FUNCTION) {
						result.type = D_STRUCT_FUNCTION;
					}
					return result;
				}
				else if (mdata.type == D_STRUCT_PARAM) {
					if (struct_type == D_STRUCT_INSTANCE &&
						streq(mdata.value.string, b.value.string)) {
						// Found the instance member we were looking for.
						// Address of the STRUCT_INSTANCE_HEADER offset by
						//   params_passed + 1;
						// + 1 for header, + 1 to get to next entry where the value
						//   is stored
						struct data result = copy_data(a.value.reference[params_passed + 2]);
						if (result.type == D_FUNCTION) {
							result.type = D_STRUCT_FUNCTION;
						}
						return result;
					}
					params_passed++;
				}
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
			error_runtime(line, VM_NOT_A_STRUCT_MAYBE_FORGOT_RET_THIS);
			return none_data();
		}
		// else if (!(a.type == D_STRUCT || a.type == D_STRUCT_INSTANCE)) {
		//	error_runtime(line, VM_NOT_A_STRUCT);
		//	return none_data();
		// }
		else {
			struct data type = type_of(a);
			error_runtime(line, VM_MEMBER_NOT_EXIST, b.value.string, type.value.string);
			destroy_data(&type);
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
					error_runtime(line, VM_MATH_DISASTER);
				}
				else {
					if (op == O_REM) {
						// modulus enum operator
						double a_n = a.value.number;
						double b_n = b.value.number;

						// check integer
						if (a_n != floor(a_n) || b_n != floor(b_n)) {
							error_runtime(line, VM_TYPE_ERROR, "/");
							return none_data();
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
				error_runtime(line, VM_NUM_NUM_INVALID_OPERATOR,
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
	else if((a.type == D_NONE || b.type == D_NONE) &&
			(op == O_EQ || op == O_NEQ)) {
		return (op == O_EQ) ^
			(a.type == D_NONE && b.type == D_NONE) ?
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
			int size_a = a.value.reference->value.number;
			int size_b = b.value.reference->value.number;

			switch (op) {
				case O_EQ: {
					if (size_a != size_b) {
						return false_data();
					}
					for (int i = 0; i < size_a; i++) {
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
					for (int i = 0; i < size_a; i++) {
						if (data_equal(&a.value.reference[i + 1],
										&b.value.reference[i + 1])) {
							return false_data();
						}
					}
					return true_data();
				}
				case O_ADD: {
					int new_size = size_a + size_b;
					struct data* new_list = wendy_list_malloc(new_size);
					int n = 1; // first is the header
					for (int i = 0; i < size_a; i++) {
						new_list[n++] = copy_data(a.value.reference[i + 1]);
					}
					for (int i = 0; i < size_b; i++) {
						new_list[n++] = copy_data(b.value.reference[i + 1]);
					}
					return make_data(D_LIST, data_value_ptr(new_list));
				}
				default: error_runtime(line, VM_LIST_LIST_INVALID_OPERATOR,
					operator_string[op]); break;
			}
		} // End A==List && B==List
		else if (a.type == D_LIST) {
			if (op == O_ADD) {
				// list + element
				int size_a = a.value.reference->value.number;

				struct data* new_list = wendy_list_malloc(size_a + 1);
				int n = 1;
				for (int i = 0; i < size_a; i++) {
					new_list[n++] = copy_data(a.value.reference[i + 1]);
				}
				new_list[n++] = copy_data(b);
				return make_data(D_LIST, data_value_ptr(new_list));
			}
			else if (op == O_MUL && b.type == D_NUMBER) {
				// list * number
				int size_a = a.value.reference->value.number;
				// Size expansion
				int new_size = size_a * (int)b.value.number;
				struct data* new_list = wendy_list_malloc(new_size);
				// Copy all Elements n times
				int n = 1;
				for (int i = 0; i < new_size; i++) {
					new_list[n++] = copy_data(a.value.reference[(i % size_a) + 1]);
				}
				return make_data(D_LIST, data_value_ptr(new_list));
			}
			else {
				error_runtime(line, VM_INVALID_APPEND);
			}
		}
		else if (b.type == D_LIST) {
			int size_b = b.value.reference->value.number;

			if (op == O_ADD) {
				// element + list
				struct data* new_list = wendy_list_malloc(size_b + 1);
				int n = 1;
				new_list[n++] = copy_data(a);
				for (int i = 0; i < size_b; i++) {
					new_list[n++] = copy_data(b.value.reference[i + 1]);
				}
				return make_data(D_LIST, data_value_ptr(new_list));
			}
			else if (op == O_MUL && a.type == D_NUMBER) {
				// number * list
				int size_b = b.value.reference->value.number;
				// Size expansion
				int new_size = size_b * (int)a.value.number;
				struct data* new_list = wendy_list_malloc(new_size);
				// Copy all Elements n times
				int n = 1;
				for (int i = 0; i < new_size; i++) {
					new_list[n++] = copy_data(b.value.reference[(i % size_b) + 1]);
				}
				return make_data(D_LIST, data_value_ptr(new_list));
			}
			else if (op == O_IN) {
				// element in list
				for (int i = 0; i < size_b; i++) {
					if (data_equal(&a, &b.value.reference[i + 1])) {
						return true_data();
					}
				}
				return false_data();
			}
			else { error_runtime(line, VM_INVALID_APPEND); }
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
					error_runtime(line, VM_STRING_DUPLICATION_NEGATIVE);
					return copy_data(b);
				}
				size = times * strlen(b.value.string) + 1;
				string = b.value.string;
			}
			else {
				times = (int) b.value.number;
				if (times < 0) {
					error_runtime(line, VM_STRING_DUPLICATION_NEGATIVE);
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
			error_runtime(line,
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
				error_runtime(line, VM_TYPE_ERROR, operator_string[op]);
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
		error_runtime(line, VM_TYPE_ERROR, operator_string[op]);
	}
	else {
		error_runtime(line, VM_TYPE_ERROR, operator_string[op]);
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
		size = a.value.reference->value.number;
	}
	else if (a.type == D_RANGE) {
		size = abs(range_end(a) - range_start(a));
	}
	else if (a.type == D_SPREAD) {
		size = size_of(*a.value.reference).value.number;
	}
	return make_data(D_NUMBER, data_value_num(size));
}

static struct data type_of(struct data a) {
	switch (a.type) {
		case D_FUNCTION:
			return make_data(D_OBJ_TYPE, data_value_str("function"));
		case D_STRING:
			return make_data(D_OBJ_TYPE, data_value_str("string"));
		case D_NUMBER:
			return make_data(D_OBJ_TYPE, data_value_str("number"));
		case D_TRUE:
		case D_FALSE:
			return make_data(D_OBJ_TYPE, data_value_str("bool"));
		case D_NONE:
			return make_data(D_OBJ_TYPE, data_value_str("none"));
		case D_NONERET:
			return make_data(D_OBJ_TYPE, data_value_str("noneret"));
		case D_RANGE:
			return make_data(D_OBJ_TYPE, data_value_str("range"));
		case D_LIST:
			return make_data(D_OBJ_TYPE, data_value_str("list"));
		case D_CLOSURE:
			return make_data(D_OBJ_TYPE, data_value_str("closure"));
		case D_STRUCT:
			return make_data(D_OBJ_TYPE, data_value_str("struct"));
		case D_SPREAD:
			return make_data(D_OBJ_TYPE, data_value_str("spread"));
		case D_OBJ_TYPE:
			return make_data(D_OBJ_TYPE, data_value_str("type"));
		case D_ANY:
			return make_data(D_OBJ_TYPE, data_value_str("any"));
		case D_STRUCT_INSTANCE:
			return make_data(D_OBJ_TYPE, data_value_str(
				a.value.reference[1].value.reference[1].value.string));
		default:
			return make_data(D_OBJ_TYPE, data_value_str("unknown"));
	}
}

static struct data eval_uniop(enum operator op, struct data a) {
	if (op == O_COPY) {
		// Create copy of object a, only applies to lists or
		// struct or struct instances
		if (a.type == D_LIST) {
			// We make a copy of the list as pointed to A.
			int list_size = a.value.reference->value.number;
			struct data* new_a = wendy_list_malloc(list_size);
			int n = 1;
			for (int i = 0; i < list_size; i++) {
				new_a[n++] = copy_data(a.value.reference[i + 1]);
			}
			return make_data(D_LIST, data_value_ptr(new_a));
		}
		// TODO(felixguo): is copying structs even in the specs???
		// else if (a.type == D_STRUCT || a.type == D_STRUCT_INSTANCE) {
		// 	// We make a copy of the STRUCT
		// 	address copy_start = a.value.number;
		// 	address metadata = (int)(a.value.number);
		// 	if (a.type == D_STRUCT_INSTANCE) {
		// 		metadata = memory[metadata].value.number;
		// 	}
		// 	int size = memory[metadata].value.number + 1;
		// 	if (a.type == D_STRUCT_INSTANCE) {
		// 		// find size of instance by counting instance members
		// 		int actual_size = 0;
		// 		for (int i = 0; i < size; i++) {
		// 			if (memory[metadata + i + 1].type == D_STRUCT_PARAM) {
		// 				actual_size++;
		// 			}
		// 		}
		// 		size = actual_size;
		// 	}
		// 	size++; // for the header itself
		// 	data* copy = safe_malloc(size * sizeof(struct data));
		// 	for (int i = 0; i < size; i++) {
		// 		copy[i] = copy_data(memory[copy_start + i]);
		// 	}
		// 	address addr = push_memory_array(copy, size, line);
		// 	safe_free(copy);
		// 	return a.type == D_STRUCT ? make_data(D_STRUCT, data_value_num(addr))
		// 		: make_data(D_STRUCT_INSTANCE, data_value_num(addr));
		// }
		else {
			// No copy needs to be made
		return a;
		}
	}
	else if (op == O_NEG) {
		if (a.type != D_NUMBER) {
			error_runtime(line, VM_INVALID_NEGATE);
			return none_data();
		}
		struct data res = make_data(D_NUMBER, data_value_num(-1 * a.value.number));
		return res;
	}
	else if (op == O_NOT) {
		if (a.type != D_TRUE && a.type != D_FALSE) {
			error_runtime(line, VM_INVALID_NEGATE);
			return none_data();
		}
		return a.type == D_TRUE ? false_data() : true_data();
	}
	else if (op == O_SPREAD) {
		// Expandable Types
		if (a.type != D_LIST && a.type != D_RANGE && a.type != D_STRING) {
			error_runtime(line, VM_SPREAD_NOT_ITERABLE);
			return none_data();
		}
		struct data *storage = refcnt_malloc(1);
		*storage = copy_data(a);
		return make_data(D_SPREAD, data_value_ptr(storage));
	}
	else {
		// fallback case
		return none_data();
	}
}

void print_current_bytecode() {
	print_bytecode(bytecode, stdout);
}
