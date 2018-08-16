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

static address memory_register = 0;
static address memory_register_A = 0;
static int line;
static address i = 0;
static uint8_t* bytecode = 0;
static size_t bytecode_size = 0;
static char* last_pushed_identifier;

// Forward Declarations
static data eval_binop(operator op, data a, data b);
static data eval_uniop(operator op, data a);
static data type_of(data a);
static data size_of(data a);
static data value_of(data a);
static data char_of(data a);

address get_instruction_pointer() {
	return i;
}

void vm_cleanup_if_repl() {
	safe_free(bytecode);
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

static char* get_binary_overload_name(operator op, data a, data b) {
	data type_a = type_of(a);
	data type_b = type_of(b);
	int len = strlen(OPERATOR_OVERLOAD_PREFIX);
	len += strlen(type_a.value.string);
	len += strlen(operator_string[op]);
	len += strlen(type_b.value.string);
	char* fn_name = safe_malloc((len + 1) * sizeof(char));
	fn_name[0] = 0;
	strcat(fn_name, OPERATOR_OVERLOAD_PREFIX);
	strcat(fn_name, type_a.value.string);
	strcat(fn_name, operator_string[op]);
	strcat(fn_name, type_b.value.string);
	destroy_data(&type_a);
	destroy_data(&type_b);
	return fn_name;
}

static char* get_unary_overload_name(operator op, data a) {
	data type_a = type_of(a);
	int len = strlen(OPERATOR_OVERLOAD_PREFIX);
	len += strlen(operator_string[op]);
	len += strlen(type_a.value.string);
	char* fn_name = safe_malloc((len + 1) * sizeof(char));
	fn_name[0] = 0;
	strcat(fn_name, OPERATOR_OVERLOAD_PREFIX);
	strcat(fn_name, operator_string[op]);
	strcat(fn_name, type_a.value.string);
	destroy_data(&type_a);
	return fn_name;
}

void vm_run(uint8_t* new_bytecode, size_t size) {
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
		opcode op = bytecode[i];
		if (get_settings_flag(SETTINGS_TRACE_VM)) {
			// This branch could slow down the VM but the CPU should branch
			// predict after one or two iterations.
			printf(BLU "<+%04X>: " RESET "%s\n", i, opcode_string[op]);
		}
		i += 1;
		switch (op) {
			case OP_PUSH: {
				data t = get_data(bytecode + i, &i);
				data d;
				if (t.type == D_IDENTIFIER) {
					if (streq(t.value.string, "time")) {
						d = time_data();
					}
					else {
						d = copy_data(*get_value_of_id(t.value.string, line));
					}
				}
				else {
					d = copy_data(t);
				}
				last_pushed_identifier = t.value.string;
				push_arg(d, line);
				break;
			}
			case OP_SRC: {
				void* ad = &bytecode[i];
				line = get_address(ad, &i);
				break;
			}
			case OP_POP: {
				data r = pop_arg(line);
				destroy_data(&r);
				break;
			}
			case OP_BIN: {
				operator op = bytecode[i++];
				data b = pop_arg(line);
				data a = pop_arg(line);
				data any_d = any_data();
				char* a_and_b = get_binary_overload_name(op, a, b);
				char* any_a = get_binary_overload_name(op, any_d, b);
				char* any_b = get_binary_overload_name(op, a, any_d);
				destroy_data(&any_d);
				char* fn_name = first_that(_id_exist, a_and_b, any_a, any_b);
				if (fn_name) {
					push_arg(a, line);
					push_arg(b, line);
					push_arg(copy_data(*get_value_of_id(fn_name, line)), line);
					safe_free(a_and_b);
					safe_free(any_a);
					safe_free(any_b);
					goto wendy_vm_call;
				}
				else {
					push_arg(eval_binop(op, a, b), line);
					destroy_data(&a);
					destroy_data(&b);
				}
				safe_free(a_and_b);
				safe_free(any_a);
				safe_free(any_b);
				break;
			}
			case OP_RBIN: {
				operator op = bytecode[i++];
				data a = pop_arg(line);
				data b = pop_arg(line);
				data any_d = any_data();
				char* a_and_b = get_binary_overload_name(op, a, b);
				char* any_a = get_binary_overload_name(op, any_d, b);
				char* any_b = get_binary_overload_name(op, a, any_d);
				destroy_data(&any_d);
				char* fn_name = first_that(_id_exist, a_and_b, any_a, any_b);
				if (fn_name) {
					push_arg(b, line);
					push_arg(a, line);
					push_arg(copy_data(*get_value_of_id(fn_name, line)), line);
					safe_free(a_and_b);
					safe_free(any_a);
					safe_free(any_b);
					goto wendy_vm_call;
				}
				else {
					push_arg(eval_binop(op, a, b), line);
					destroy_data(&a);
					destroy_data(&b);
				}
				safe_free(a_and_b);
				safe_free(any_a);
				safe_free(any_b);
				break;
			}
			case OP_UNA: {
				operator op = bytecode[i++];
				data a = pop_arg(line);
				char* fn_name = get_unary_overload_name(op, a);
				if (id_exist(fn_name, true)) {
					push_arg(a, line);
					push_arg(copy_data(*get_value_of_id(fn_name, line)), line);
					safe_free(fn_name);
					goto wendy_vm_call;
				}
				else {
					push_arg(eval_uniop(op, a), line);
					destroy_data(&a);
				}
				safe_free(fn_name);
				break;
			}
			case OP_NATIVE: {
				void* ag = &bytecode[i];
				address args = get_address(ag, &i);
				char* name = get_string(bytecode + i, &i);
				native_call(name, args, line);
				break;
			}
			case OP_BIND: {
				char *id = get_string(bytecode + i, &i);
				if (id_exist(id, false)) {
					error_runtime(line, VM_VAR_DECLARED_ALREADY, id);
				}
				else {
					push_stack_entry(id, memory_register, line);
				}
				break;
			}
			case OP_WHERE: {
				char* id = get_string(bytecode + i, &i);
				memory_register = get_address_of_id(id, line);
				memory_register_A = memory_register;
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
			case OP_RET: {
				pop_frame(true, &i);
				memory_register = pop_mem_reg();
				break;
			}
			case OP_LJMP: {
				// L_JMP Address LoopIndexString
				address end_of_loop = get_address(bytecode + i, &i);
				char* loop_index_string = get_string(bytecode + i, &i);
				data* loop_index_token = get_value_of_id(loop_index_string, line);
				int index = loop_index_token->value.number;
				data condition = *top_arg(line);
				bool jump = false;
				if (condition.type == D_TRUE) {
					// Do Nothing
				}
				else if (condition.type == D_LIST) {
					address lst = condition.value.number;
					int lst_size = memory[lst].value.number;
					if (index >= lst_size) jump = true;
				}
				else if (condition.type == D_RANGE) {
					int end = range_end(condition);
					int start = range_start(condition);
					if (start < end) {
						if (start + index >= end) jump = true;
					}
					else {
						if (start - index <= end) jump = true;
					}
				}
				else if (condition.type == D_STRING) {
					int size = strlen(condition.value.string);
					if (index >= size) jump = true;
				}
				else {
					jump = true;
				}
				if (jump)  {
					// Pop Condition too!
					data res = pop_arg(line);
					destroy_data(&res);
					i = end_of_loop;
				}
				break;
			}
			case OP_LBIND: {
				char* user_index = get_string(bytecode + i, &i);
				char* loop_index_string = get_string(bytecode + i, &i);
				data* loop_index_token = get_value_of_id(loop_index_string, line);
				int index = loop_index_token->value.number;
				data condition = pop_arg(line);
				data res;
				if (condition.type == D_LIST) {
					address lst = condition.value.number;
					res = copy_data(memory[lst + 1 + index]);
				}
				else if (condition.type == D_RANGE) {
					int end = range_end(condition);
					int start = range_start(condition);
					if (start < end) {
						res = make_data(D_NUMBER, data_value_num(start + index));
					}
					else {
						res = make_data(D_NUMBER, data_value_num(start - index));
					}
				}
				else if (condition.type == D_STRING) {
					data r = make_data(D_STRING, data_value_str(" "));
					r.value.string[0] = condition.value.string[index];
					r.value.string[1] = 0;
					res = r;
				}
				else {
					res = copy_data(*loop_index_token);
				}
				address mem_to_mod = get_address_of_id(user_index, line);
				write_memory(mem_to_mod, res, -1);
				destroy_data(&condition);
				break;
			}
			case OP_INC: {
				memory[memory_register].value.number++;
				break;
			}
			case OP_DEC: {
				memory[memory_register].value.number--;
				break;
			}
			case OP_ASSERT: {
				data_type matching = bytecode[i++];
				char* c = get_string(bytecode + i, &i);
				if (memory[memory_register].type != matching) {
					error_runtime(line, c);
				}
				break;
			}
			case OP_FRM: {
				push_auto_frame(i, "automatic", line);
				push_mem_reg(memory_register, line);
				break;
			}
			case OP_MPTR: {
				memory_register = (address)memory[memory_register].value.number;
				break;
			}
			case OP_END: {
				pop_frame(false, &i);
				memory_register = pop_mem_reg();
				break;
			}
			case OP_REQ: {
				memory_register = pls_give_memory(bytecode[i++], line);
				break;
			}
			case OP_RBW: {
				// REQUEST BIND AND WRITE
				if (top_arg(line)->type == D_END_OF_ARGUMENTS ||
					top_arg(line)->type == D_NAMED_ARGUMENT_NAME)
					break;
				memory_register = pls_give_memory(1, line);
				char* bind_name = get_string(bytecode + i, &i);
				if (id_exist(bind_name, false)) {
					address a = get_stack_pos_of_id(bind_name, line);
					if (!call_stack[a].is_closure) {
						error_runtime(line, VM_VAR_DECLARED_ALREADY, bind_name);
					}
				}
				push_stack_entry(bind_name, memory_register, line);
				data value = pop_arg(line);
				if (value.type == D_FUNCTION) {
					// Modify Name to be the base_name
					address fn_adr = value.value.number;
					strcpy(memory[fn_adr + 2].value.string, bind_name);
				}
				write_memory(memory_register, value, line);
				break;
			}
			case OP_MKPTR: {
				push_arg(make_data((data_type) bytecode[i++],
					data_value_num(memory_register)), line);
				break;
			}
			case OP_NTHPTR: {
				// Should be a list at the memory register.
				data lst = memory[memory_register];
				if (lst.type != D_LIST) {
					error_runtime(line, VM_NOT_A_LIST);
				}
				address lst_start = lst.value.number;
				data in = pop_arg(line);
				if (in.type != D_NUMBER) {
					error_runtime(line, VM_INVALID_LVALUE_LIST_SUBSCRIPT);
				}
				int lst_size = memory[lst_start].value.number;
				int index = in.value.number;
				if (index >= lst_size) {
					error_runtime(line, VM_LIST_REF_OUT_RANGE);
				}
				memory_register = lst_start + index + 1;
				destroy_data(&in);
				break;
			}
			case OP_CLOSUR: {
				push_arg(make_data(D_CLOSURE, data_value_num(create_closure())), line);
				break;
			}
			case OP_MEMPTR: {
				// Member Pointer
				// Structs can only modify Static members, instances modify instance
				//   members.
				// Either will be allowed to look through static parameters.
				data t = memory[memory_register];
				char* member = get_string(bytecode + i, &i);
				if (t.type != D_STRUCT && t.type != D_STRUCT_INSTANCE) {
					error_runtime(line, VM_NOT_A_STRUCT);
				}
				address metadata = (int)(t.value.number);
				if (t.type == D_STRUCT_INSTANCE) {
					// metadata actually points to the STRUCT_INSTANCE_HEAD
					//   right now.
					metadata = (address)(memory[metadata].value.number);
				}
				data_type struct_type = t.type;
				address struct_header = t.value.number;
				bool found = false;
				while(!found) {
					int params_passed = 0;
					int size = (int)(memory[metadata].value.number);
					for (int i = 0; i < size; i++) {
						data mdata = memory[metadata + i];
						if (mdata.type == D_STRUCT_SHARED &&
							streq(mdata.value.string, member)) {
							// Found the static member we were looking for.
							memory_register = metadata + i + 1;
							found = true;
							if (memory[memory_register].type == D_FUNCTION) {
								memory[memory_register].type = D_STRUCT_FUNCTION;
							}
							break;
						}
						else if (mdata.type == D_STRUCT_PARAM) {
							if (struct_type == D_STRUCT_INSTANCE &&
								streq(mdata.value.string, member)) {
								// Found the instance member we were looking for.
								// Address of the STRUCT_INSTANCE_HEADER offset by
								//   params_passed + 1;
								address loc = struct_header + params_passed + 1;
								memory_register = loc;
								found = true;
								if (memory[memory_register].type == D_FUNCTION) {
									memory[memory_register].type = D_STRUCT_FUNCTION;
								}
								break;
							}
							params_passed++;
						}
					}
					if (found) break;
					error_runtime(line, VM_MEMBER_NOT_EXIST, member);
				}
				break;
			}
			case OP_JMP: {
				address addr = get_address(bytecode + i, &i);
				i = addr;
				break;
			}
			case OP_JIF: {
				// Jump IF False Instruction
				data top = pop_arg(line);
				address addr = get_address(bytecode + i, &i);
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
				data top = pop_arg(line);
				int loc = top.value.number;
				data boundName = memory[loc + 2];
				char* function_disp = safe_malloc(128 * sizeof(char));
				function_disp[0] = 0;
				if (streq(boundName.value.string, "self")) {
					sprintf(function_disp, "annonymous:0x%X", i);
				}
				else {
					sprintf(function_disp, "%s:0x%X", boundName.value.string, i);
				}
				push_frame(function_disp, i, line);
				safe_free(function_disp);
				push_mem_reg(memory_register, line);
				if (top.type == D_STRUCT) {
					address j = top.value.number;
					top = memory[j + 3];
					top.type = D_STRUCT_FUNCTION;
					// grab the size of the metadata chain also check if there's an
					//   overloaded init.
					int m_size = memory[j].value.number;
					int params = 0;
					for (int i = 0; i < m_size; i++) {
						if (memory[j + i].type == D_STRUCT_PARAM) {
							params++;
						}
					}

					int si_size = 0;
					data* struct_instance =
						safe_malloc(MAX_STRUCT_META_LEN * sizeof(data));

					si_size = params + 1; // + 1 for the header
					struct_instance[0] = make_data(D_STRUCT_INSTANCE_HEAD,
							data_value_num(j));
					int offset = params;
					for (int i = 0; i < params; i++) {
						struct_instance[offset - i] = none_data();
					}
					// Struct instance is done.
					address a = push_memory_array(struct_instance, si_size, line);
					safe_free(struct_instance);
					memory_register_A = a;
				}

				if (top.type != D_FUNCTION && top.type != D_STRUCT_FUNCTION) {
					error_runtime(line, VM_FN_CALL_NOT_FN);
				}
				if (top.type == D_STRUCT_FUNCTION) {
					data_type t;
					if (memory[memory_register_A].type == D_STRUCT_INSTANCE_HEAD) {
						t = D_STRUCT_INSTANCE;
					}
					else {
						t = D_STRUCT;
					}
					push_stack_entry("this", push_memory(make_data(
						t, data_value_num(memory_register_A)), line), line);
				}
				// Top might have changed, reload
				loc = top.value.number;
				address addr = memory[loc].value.number;
				i = addr;
				// push closure variables
				address cloc = memory[loc + 1].value.number;
				if (cloc != NO_CLOSURE) {
					size_t size = closure_list_sizes[cloc];
					for (size_t i = 0; i < size; i++) {
						copy_stack_entry(closure_list[cloc][i], line);
					}
				}
				address adr = push_memory(top, line);
				if (strcmp(boundName.value.string, "self") != 0) {
					push_stack_entry("self", adr, line);
				}
				push_stack_entry(boundName.value.string, adr, line);
				break;
			}
			case OP_READ: {
				push_arg(copy_data(memory[memory_register]), line);
				break;
			}
			case OP_WRITE: {
				size_t size = bytecode[i++];
				if (top_arg(line)->type == D_END_OF_ARGUMENTS ||
					top_arg(line)->type == D_NAMED_ARGUMENT_NAME)
					break;

				for (address j = memory_register + size - 1;
						j >= memory_register; j--) {
					write_memory(j, pop_arg(line), line);
					if (memory[j].type == D_FUNCTION) {
						// Write Name to Function
						char* bind_name = last_pushed_identifier;
						address fn_adr = memory[j].value.number;
						strcpy(memory[fn_adr + 2].value.string, bind_name);
					}
				}
				break;
			}
			case OP_OUT: {
				data t = pop_arg(line);
				if (t.type != D_NONERET) {
					print_data(&t);
				}
				destroy_data(&t);
				break;
			}
			case OP_OUTL: {
				data t = pop_arg(line);
				if (t.type != D_NONERET) {
					print_data_inline(&t, stdout);
				}
				destroy_data(&t);
				break;
			}
			// DEPRECATED
			case OP_IN: {
				// Scan one line from the input.
				char buffer[MAX_STRING_LEN];
				while(!fgets(buffer, MAX_STRING_LEN, stdin)) {};

				char* end_ptr = buffer;
				errno = 0;
				double d = strtod(buffer, &end_ptr);
				// null terminator or newline character
				if (errno != 0 || (*end_ptr != 0 && *end_ptr != 10)) {
					size_t len = strlen(buffer);
					// remove last newline
					buffer[len - 1] = 0;
					write_memory(memory_register, make_data(D_STRING, data_value_str(buffer)), line);
				}
				else {
					// conversion successful
					write_memory(memory_register, make_data(D_NUMBER, data_value_num(d)), line);
				}
				break;
			}
			case OP_HALT:
				return;
			default:
				error_runtime(line, VM_INVALID_OPCODE, bytecode[i], i);
		}
		if (get_error_flag()) {
			clear_arg_stack();
			break;
		}
	}
}

static data eval_binop(operator op, data a, data b) {
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
			list_size = memory[(int)a.value.number].value.number;
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
				data c = make_data(D_STRING, data_value_str("0"));
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
			address list_address = a.value.number;
			data* c = get_value_of_address(list_address + offset, line);
			return copy_data(*c);
		}
		else {
			int start = range_start(b);
			int end = range_end(b);
			int subarray_size = start - end;
			if (subarray_size < 0) subarray_size *= -1;

			if (a.type == D_STRING) {
				data c = make_data(D_STRING, data_value_size(abs(start - end)));
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

			address array_start = (int)(a.value.number);
			data* new_a = safe_malloc(subarray_size * sizeof(data));
			int n = 0;
			for (int i = start; i != end;
				start < end ? i++ : i--) {
				new_a[n++] = copy_data(memory[array_start + i + 1]);
			}
			address new_aa = push_memory_wendy_list(new_a, subarray_size, line);
			safe_free(new_a);
			data c = make_data(D_LIST, data_value_num(new_aa));
			return c;
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
			address metadata = (int)(a.value.number);
			memory_register_A = metadata;
			if (a.type == D_STRUCT_INSTANCE) {
				// metadata actually points to the STRUCT_INSTANE_HEADER
				//   right now.
				metadata = (address)(memory[metadata].value.number);
			}
			data_type struct_type = a.type;
			address struct_header = a.value.number;

			int params_passed = 0;
			int size = (int)(memory[metadata].value.number);
			for (int i = 0; i < size; i++) {
				data mdata = memory[metadata + i];
				if (mdata.type == D_STRUCT_SHARED &&
					streq(mdata.value.string, b.value.string)) {
					// Found the static member we were looking for
					data result = copy_data(memory[metadata + i + 1]);
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
						address loc = struct_header + params_passed + 1;
						data result = copy_data(memory[loc]);
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
		else {
			error_runtime(line, VM_MEMBER_NOT_EXIST, b.value.string);
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
						// modulus operator
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
			address start_a = a.value.number;
			address start_b = b.value.number;
			int size_a = memory[start_a].value.number;
			int size_b = memory[start_b].value.number;

			switch (op) {
				case O_EQ: {
					if (size_a != size_b) {
						return false_data();
					}
					for (int i = 0; i < size_a; i++) {
						if (!data_equal(&memory[start_a + i + 1],
										&memory[start_b + i + 1])) {
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
						if (data_equal(&memory[start_a + i + 1],
										&memory[start_b + i + 1])) {
							return false_data();
						}
					}
					return true_data();
				}
				case O_ADD: {
					int new_size = size_a + size_b;
					data* new_list = safe_malloc(new_size * sizeof(data));
					int n = 0;
					for (int i = 0; i < size_a; i++) {
						new_list[n++] = copy_data(memory[start_a + i + 1]);
					}
					for (int i = 0; i < size_b; i++) {
						new_list[n++] = copy_data(memory[start_b + i + 1]);
					}
					address new_adr = push_memory_wendy_list(new_list, new_size, line);
					safe_free(new_list);
					return make_data(D_LIST, data_value_num(new_adr));
				}
				default: error_runtime(line, VM_LIST_LIST_INVALID_OPERATOR,
					operator_string[op]); break;
			}
		} // End A==List && B==List
		else if (a.type == D_LIST) {
			if (op == O_ADD) {
				// list + element
				address start_a = a.value.number;
				int size_a = memory[start_a].value.number;

				data* new_list = safe_malloc((size_a + 1) * sizeof(data));
				int n = 0;
				for (int i = 0; i < size_a; i++) {
					new_list[n++] = copy_data(memory[start_a + i + 1]);
				}
				new_list[n++] = copy_data(b);
				address new_adr = push_memory_wendy_list(new_list, size_a + 1, line);
				safe_free(new_list);
				return make_data(D_LIST, data_value_num(new_adr));
			}
			else if (op == O_MUL && b.type == D_NUMBER) {
				// list * number
				address start_a = a.value.number;
				int size_a = memory[start_a].value.number;
				// Size expansion
				int new_size = size_a * (int)b.value.number;
				data* new_list = safe_malloc(new_size * sizeof(data));
				// Copy all Elements n times
				int n = 0;
				for (int i = 0; i < new_size; i++) {
					new_list[n++] = copy_data(memory[(start_a + (i % size_a)) + 1]);
				}
				address new_adr = push_memory_wendy_list(new_list, new_size, line);
				safe_free(new_list);
				return make_data(D_LIST, data_value_num(new_adr));
			}
			else {
				error_runtime(line, VM_INVALID_APPEND);
			}
		}
		else if (b.type == D_LIST) {
			address start_b = b.value.number;
			int size_b = memory[start_b].value.number;

			if (op == O_ADD) {
				// element + list
				data* new_list = safe_malloc((size_b + 1) * sizeof(data));
				int n = 0;
				new_list[n++] = copy_data(a);
				for (int i = 0; i < size_b; i++) {
					new_list[n++] = copy_data(memory[start_b + i + 1]);
				}
				address new_adr = push_memory_wendy_list(new_list, size_b + 1, line);
				safe_free(new_list);
				return make_data(D_LIST, data_value_num(new_adr));
			}
			else if (op == O_MUL && a.type == D_NUMBER) {
				// number * list
				address start_b = b.value.number;
				int size_b = memory[start_b].value.number;
				// Size expansion
				int new_size = size_b * (int)a.value.number;
				data* new_list = safe_malloc(new_size * sizeof(data));
				// Copy all Elements n times
				int n = 0;
				for (int i = 0; i < new_size; i++) {
					new_list[n++] = copy_data(memory[(start_b + (i % size_b)) + 1]);
				}
				address new_adr = push_memory_wendy_list(new_list, new_size, line);
				safe_free(new_list);
				return make_data(D_LIST, data_value_num(new_adr));
			}
			else if (op == O_IN) {
				// element in list
				for (int i = 0; i < size_b; i++) {
//                  print_token(&a);
					//print_token(&memory[start_b + i + 1]);
					if (data_equal(&a, &memory[start_b + i + 1])) {
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

			data result = make_data(D_STRING, data_value_size(total_len));
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
				size = times * strlen(b.value.string) + 1;
				string = b.value.string;
			}
			else {
				times = (int) b.value.number;
				size = times * strlen(a.value.string) + 1;
				string = a.value.string;
			}
			result = safe_malloc(size * sizeof(char));
			result[0] = 0;
			for (int i = 0; i < times; i++) {
				// Copy it over n times
				strcat(result, string);
			}
			data t = make_data(D_STRING, data_value_str(result));
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
	else {
		error_runtime(line, VM_TYPE_ERROR, operator_string[op]);
	}
	return none_data();
}

static data value_of(data a) {
	if (a.type == D_STRING && strlen(a.value.string) == 1) {
		return make_data(D_NUMBER, data_value_num(a.value.string[0]));
	}
	else {
		return a;
	}
}

static data char_of(data a) {
	if (a.type == D_NUMBER && a.value.number >= 0 && a.value.number <= 127) {
		data res = make_data(D_STRING, data_value_str(" "));
		res.value.string[0] = (char)a.value.number;
		return res;
	}
	else {
		return a;
	}
}

static data size_of(data a) {
	double size = 0;
	if (a.type == D_STRING) {
		size = strlen(a.value.string);
	}
	else if (a.type == D_LIST) {
		address h = a.value.number;
		size = memory[h].value.number;
	}
	return make_data(D_NUMBER, data_value_num(size));
}

static data type_of(data a) {
	switch (a.type) {
		case D_STRING:
			return make_data(D_OBJ_TYPE, data_value_str("string"));
		case D_NUMBER:
			return make_data(D_OBJ_TYPE, data_value_str("number"));
		case D_TRUE:
		case D_FALSE:
			return make_data(D_OBJ_TYPE, data_value_str("bool"));
		case D_NONE:
			return make_data(D_OBJ_TYPE, data_value_str("none"));
		case D_RANGE:
			return make_data(D_OBJ_TYPE, data_value_str("range"));
		case D_LIST:
			return make_data(D_OBJ_TYPE, data_value_str("list"));
		case D_STRUCT:
			return make_data(D_OBJ_TYPE, data_value_str("struct"));
		case D_OBJ_TYPE:
			return make_data(D_OBJ_TYPE, data_value_str("type"));
		case D_ANY:
			return make_data(D_OBJ_TYPE, data_value_str("any"));
		case D_STRUCT_INSTANCE: {
			data instance_loc = memory[(int)(a.value.number)];
			return make_data(D_OBJ_TYPE, data_value_str(
				memory[(int)instance_loc.value.number + 1].value.string));
		}
		default:
			return make_data(D_OBJ_TYPE, data_value_str("none"));
	}
}

static data eval_uniop(operator op, data a) {
	if (op == O_COPY) {
		// Create copy of object a, only applies to lists or
		// struct or struct instances
		if (a.type == D_LIST) {
			// We make a copy of the list as pointed to A.
			data list_header = memory[(int)a.value.number];
			int list_size = list_header.value.number;
			data* new_a = safe_malloc((list_size) * sizeof(data));
			int n = 0;
			address array_start = a.value.number;
			for (int i = 0; i < list_size; i++) {
				new_a[n++] = copy_data(memory[array_start + i + 1]);
			}
			address new_l_loc = push_memory_wendy_list(new_a, list_size, line);
			safe_free(new_a);
			return make_data(D_LIST, data_value_num(new_l_loc));
		}
		else if (a.type == D_STRUCT || a.type == D_STRUCT_INSTANCE) {
			// We make a copy of the STRUCT
			address copy_start = a.value.number;
			address metadata = (int)(a.value.number);
			if (a.type == D_STRUCT_INSTANCE) {
				metadata = memory[metadata].value.number;
			}
			int size = memory[metadata].value.number + 1;
			if (a.type == D_STRUCT_INSTANCE) {
				// find size of instance by counting instance members
				int actual_size = 0;
				for (int i = 0; i < size; i++) {
					if (memory[metadata + i + 1].type == D_STRUCT_PARAM) {
						actual_size++;
					}
				}
				size = actual_size;
			}
			size++; // for the header itself
			data* copy = safe_malloc(size * sizeof(data));
			for (int i = 0; i < size; i++) {
				copy[i] = copy_data(memory[copy_start + i]);
			}
			address addr = push_memory_array(copy, size, line);
			safe_free(copy);
			return a.type == D_STRUCT ? make_data(D_STRUCT, data_value_num(addr))
				: make_data(D_STRUCT_INSTANCE, data_value_num(addr));
		}
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
		data res = make_data(D_NUMBER, data_value_num(-1 * a.value.number));
		return res;
	}
	else if (op == O_NOT) {
		if (a.type != D_TRUE && a.type != D_FALSE) {
			error_runtime(line, VM_INVALID_NEGATE);
			return none_data();
		}
		return a.type == D_TRUE ? false_data() : true_data();
	}
	else {
		// fallback case
		return none_data();
	}
}

void print_current_bytecode() {
	print_bytecode(bytecode, stdout);
}
