#include "vm.h"
#include "codegen.h"
#include "memory.h"
#include "token.h"
#include "error.h"
#include "stdint.h"
#include "math.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "global.h"

static address memory_register = 0;
static address memory_register_A = 0;
static data data_register; 
static int line;
static int base_address = 0;
static address i = 0;

address get_instruction_pointer() {
	return i;
}

void vm_run(uint8_t* bytecode) {
	// Verify Header
	for (i = verify_header(bytecode);; i++) {
		reset_error_flag();
		opcode op = bytecode[i];
		if (op == OPUSH) {
			i++;
			token t = get_token(&bytecode[i], &i);
			if (t.t_type == IDENTIFIER) {
				if (strcmp(t.t_data.string, "time") == 0) {
					t = time_token();
				}
				else {
					t = *get_value_of_id(t.t_data.string, line);
				}
			}
			else if (t.t_type == MEMBER) {
				t.t_type = IDENTIFIER;
			}
			push_arg(t, line);
		}
		else if (op == SRC) {
			void* ad = &bytecode[i + 1];
			line = *((int*)ad);
			i += sizeof(int);
		}
		else if (op == OPOP) {
			token t = pop_arg(line);	
		}
		else if (op == BIN) {
			i++;

			token operator = get_token(&bytecode[i], &i);
			token b = pop_arg(line);
			token a = pop_arg(line);
			push_arg(eval_binop(operator, a, b), line);
		}
		else if (op == RBIN) {
			i++;
			token operator = get_token(&bytecode[i], &i);
			token a = pop_arg(line);
			token b = pop_arg(line);
			push_arg(eval_binop(operator, a, b), line);
		}
		else if (op == UNA) {
			i++;
			token operator = get_token(&bytecode[i], &i);
			token a = pop_arg(line);
			push_arg(eval_uniop(operator, a), line);
		}
		else if (op == CHTYPE) {
			i++;
			token* t = top_arg(line);
			t->t_type = bytecode[i];
		}
		else if (op == BIND) {
			i++;
			if (id_exist((char*)(bytecode + i), false)) {
				error_runtime(line, VM_VAR_DECLARED_ALREADY, 
								(char*)(bytecode + i)); 
			}
			else {
				push_stack_entry((char*)(bytecode + i), memory_register, line);	
				i += strlen((char*)(bytecode + i));
			}
		}
		else if (op == WHERE) {
			i++;
			memory_register = get_address_of_id((char*)(bytecode + i), i);
			i += strlen((char*)(bytecode + i));
			memory_register_A = memory_register;
		}
		else if (op == ORET) {
			pop_frame(true, &i);
			memory_register = pop_mem_reg();
			i--;
		}
		else if (op == DIN) {
			token* t = top_arg(line);
			data_register = t->t_data;
		}
		else if (op == DOUT) {
			token* t = top_arg(line);
			t->t_data = data_register;	
		}
		else if (op == LJMP) {
			// LJMP Address LoopIndexString
			void* ad = &bytecode[i + 1];
			address end_of_loop = base_address + *((address*)ad);
			i += sizeof(address);
			char* loop_index_string = (char*)(bytecode + (++i));
			i += strlen((char*)(bytecode + i));
			token loop_index_token = *(get_value_of_id(loop_index_string, line));
			int index = loop_index_token.t_data.number;
			token condition = *top_arg(line);
			bool jump = false;
			if (condition.t_type == TRUE) {
				// Do Nothing
			}
			else if (condition.t_type == LIST) {
				address lst = condition.t_data.number;
				int lst_size = memory[lst].t_data.number;
				if (index >= lst_size) jump = true;
			}
			else if (condition.t_type == RANGE) {
				int end = range_end(condition);
				int start = range_start(condition);
				if (start < end) {
					if (start + index >= end) jump = true;
				}
				else {
					if (start - index <= end) jump = true;
				}
			}
			else if (condition.t_type == STRING) {
				int size = strlen(condition.t_data.string);
				if (index >= size) jump = true;
			}
			else {
				jump = true;
			}
			if (jump) i = end_of_loop - 1;
		}
		else if (op == LBIND) {
			char* user_index = (char*)(bytecode + (++i));
			i += strlen((char*)(bytecode + i));
			char* loop_index_string = (char*)(bytecode + (++i));
			i += strlen((char*)(bytecode + i));
			token loop_index_token = *(get_value_of_id(loop_index_string, i));
			int index = loop_index_token.t_data.number;
			token condition = pop_arg(line);
			token res = loop_index_token;
			if (condition.t_type == LIST) {		
				address lst = condition.t_data.number;
				res = memory[lst + 1 + index];
			}
			else if (condition.t_type == RANGE) {
				int end = range_end(condition);
				int start = range_start(condition);
				if (start < end) {
					res = make_token(NUMBER, make_data_num(start + index));
				}
				else {
					res = make_token(NUMBER, make_data_num(start - index));
				}
			}
			else if (condition.t_type == STRING) {
				token r = make_token(STRING, make_data_str(""));
				r.t_data.string[0] = condition.t_data.string[index];
				r.t_data.string[1] = 0;
				res = r;
			}
			address mem_to_mod = get_address_of_id(user_index, i);
			memory[mem_to_mod] = res;
		}
		else if (op == OINC) {
			memory[memory_register].t_data.number++;
		}
		else if (op == ODEC) {
			memory[memory_register].t_data.number--;
		}
		else if (op == ASSERT) {
			i++;
			token_type matching = bytecode[i];
			i++;
			char* c = (char*) (bytecode + i);
			if (memory[memory_register].t_type != matching) {
				error_runtime(line, c);
			}
			i += strlen(c);
		}
		else if (op == FRM) {
			// r/A doesn't matter, autoframes have no RA
			push_auto_frame(i + 1, "automatic", line);
			push_mem_reg(memory_register);
		}
		else if (op == MPTR) {
			memory_register = (address)memory[memory_register].t_data.number;
		}
		else if (op == END) {
			pop_frame(false, &i);
			memory_register = pop_mem_reg();
		}  
		else if (op == OREQ) {
			// Memory Request Instruction
			memory_register = pls_give_memory(bytecode[i + 1], line);
			i++;
		}
		else if (op == RBW) {
			// REQUEST BIND AND WRITE
			memory_register = pls_give_memory(1, line);
			i++;
			if (id_exist((char*)(bytecode + i), false)) {
				address a = get_stack_pos_of_id((char*)(bytecode + i), line);
				if (!call_stack[a].is_closure) {
					error_runtime(line, VM_VAR_DECLARED_ALREADY, 
								(char*)(bytecode + i)); 
				}
			}
			push_stack_entry((char*)(bytecode + i), memory_register, line);	
			i += strlen((char*)(bytecode + i));
			memory[memory_register] = pop_arg(line);
		}
		else if (op == PLIST) {
			i++;
			int size = bytecode[i];
			for (int j = memory_register + size - 1; 
					j >= memory_register; j--) {
				memory[j] = pop_arg(line);
			}
			push_arg(make_token(LIST, make_data_num(memory_register)), line);
		}
		else if (op == NTHPTR) {
			// Should be a list at the memory register.
			token lst = memory[memory_register];
			if (lst.t_type != LIST) {
				error_runtime(line, VM_NOT_A_LIST);	
			}
			address lst_start = lst.t_data.number;
			token in = pop_arg(line);
			if (in.t_type != NUMBER) {
				error_runtime(line, VM_INVALID_LVALUE_LIST_SUBSCRIPT);
			}
			int lst_size = memory[lst_start].t_data.number;
			int index = in.t_data.number;
			if (index >= lst_size) {
				error_runtime(line, VM_LIST_REF_OUT_RANGE);
			}
			memory_register = lst_start + index + 1;
		}
		else if (op == CLOSUR) {
			push_arg(make_token(CLOSURE, make_data_num(create_closure())), line);
		}
		else if (op == MEMPTR) {
			// Member Pointer
			// Structs can only modify Static members, instances modify instance 
			//   members.
			// Either will be allowed to look through static parameters.
			token t = memory[memory_register];
			token e = pop_arg(line);
			if (t.t_type != STRUCT && t.t_type != STRUCT_INSTANCE) {
				error_runtime(line, VM_NOT_A_STRUCT);
			}
			int params_passed = 0;
			address metadata = (int)(t.t_data.number);
			if (t.t_type == STRUCT_INSTANCE) {
				// metadata actually points to the STRUCT_INSTANCE_HEAD
				//   right now.
				metadata = (address)(memory[metadata].t_data.number);
			}
			token_type struct_type = t.t_type;
			address struct_header = t.t_data.number;
			bool found = false;
			while(!found) {
				int params_passed = 0;
				address parent_meta = 0;
				int size = (int)(memory[metadata].t_data.number);
				for (int i = 0; i < size; i++) {
					token mdata = memory[metadata + i];
					if (mdata.t_type == STRUCT_STATIC &&
						strcmp(mdata.t_data.string, e.t_data.string) == 0) {
						// Found the static member we were looking for.
						memory_register = metadata + i + 1;
						found = true;
						if (memory[memory_register].t_type == FUNCTION) {
							memory[memory_register].t_type = STRUCT_FUNCTION;
						}
						break;
					}
					else if (mdata.t_type == STRUCT_PARAM) {
						if (struct_type == STRUCT_INSTANCE && 
							strcmp(mdata.t_data.string, e.t_data.string) == 0) {
							// Found the instance member we were looking for.
							// Address of the STRUCT_INSTANCE_HEADER offset by
							//   params_passed + 1;
							address loc = struct_header + params_passed + 1;
							memory_register = loc;
							found = true;
							if (memory[memory_register].t_type == FUNCTION) {
								memory[memory_register].t_type = STRUCT_FUNCTION;
							}
							break;
						}
						params_passed++;
					}
					else if (mdata.t_type == STRUCT_PARENT) {
						parent_meta = mdata.t_data.number;
					}
				}
				// Check now if there is a parent class
				if (found) break;
				if (parent_meta) {
					metadata = parent_meta;
					struct_header = memory[struct_header + 1].t_data.number;
				}
				else {
					error_runtime(line, VM_MEMBER_NOT_EXIST, e.t_data.string);
				}
			}
		}
		else if (op == JMP) {
			void* ad = &bytecode[i + 1];
			address addr = base_address + *((address*)ad);
			i = addr - 1;
		}
		else if (op == JIF) {
			// Jump IF False Instruction
			token top = pop_arg(line);

			void* ad = &bytecode[i + 1];
			address addr = base_address + *((address*)ad);
			i += sizeof(address);

			if (top.t_type != TRUE && top.t_type != FALSE) {
				error_runtime(line, VM_COND_EVAL_NOT_BOOL);			
			}
			if (top.t_type == FALSE) {
				i = addr - 1;
			}
		}
		else if (op == OCALL) {
			char* function_disp = safe_malloc(128 * sizeof(char));
			function_disp[0] = 0;
			sprintf(function_disp, "function(0x%X)", i);
			push_frame(function_disp, i + 1, line);
			safe_free(function_disp);
			push_mem_reg(memory_register);
			token top = pop_arg(line);
			if (top.t_type == STRUCT) {
				address j = top.t_data.number;
				top = memory[j + 3];
				top.t_type = STRUCT_FUNCTION;
				// grab the size of the metadata chain also check if there's an
				//   overloaded init.
				int m_size = memory[j].t_data.number;
				int params = 0;
				address base_instance_address = 0;	
				for (int i = 0; i < m_size; i++) {
					if (memory[j + i].t_type == STRUCT_PARAM) {
						params++;
					}
					else if (memory[j + i].t_type == STRUCT_PARENT) {
						// Structure has a parent. We'll navigate to the parent
						//   and build an empty struct instance of the parent.
						address p_mdata = (int)memory[j + i].t_data.number;
						int p_size = memory[p_mdata].t_data.number;
						int p_params = 0;
						for (int p = 0; p < p_size; p++) {
							if (memory[p_mdata + p].t_type == STRUCT_PARAM) {
								p_params++;
							}
						}
						int p_total_size = p_params + 1;
						token *parent_instance = safe_malloc(p_total_size * sizeof(token));
						parent_instance[0] = make_token(STRUCT_INSTANCE_HEAD,
								make_data_num(p_mdata));
						for (int p = 1; p < p_total_size; p++) {
							parent_instance[p] = none_token();
						}
						address parent_base_addr = 
							push_memory_array(parent_instance, p_total_size);
						base_instance_address = parent_base_addr;
						safe_free(parent_instance);
					}
				}		

				int si_size = 0;
				token* struct_instance = 
					safe_malloc(MAX_STRUCT_META_LEN * sizeof(token));
			
				si_size = params + 1; // + 1 for the header
				struct_instance[0] = make_token(STRUCT_INSTANCE_HEAD, 
						make_data_num(j));
				int offset = params;
				if (base_instance_address) {
					// First param is actually a hidden BASE param
					struct_instance[1] = make_token(STRUCT_INSTANCE,
							make_data_num(base_instance_address));
					offset++;
					si_size++;
				}
				for (int i = 0; i < params; i++) {	
					struct_instance[offset - i] = none_token();
				}
				// Struct instance is done.
				address a = push_memory_array(struct_instance, si_size);
				safe_free(struct_instance);
				address b = push_memory(make_token(STRUCT_INSTANCE, 
								make_data_num(a)));
				memory_register_A = b;
			}

			if (top.t_type != FUNCTION && top.t_type != STRUCT_FUNCTION) {
				error_runtime(line, VM_FN_CALL_NOT_FN); 
			}
			if (top.t_type == STRUCT_FUNCTION) {
				token_type t;
				if (memory[memory_register_A].t_type == STRUCT_INSTANCE_HEAD) { 
					t = STRUCT_INSTANCE;
				}
				else {
					t = STRUCT;
				}
				push_stack_entry("this", memory_register_A, line); 	
			}
			int loc = top.t_data.number;
			address addr = base_address + memory[loc].t_data.number;
			i = addr - 1;
			// push closure variables
			address cloc = memory[loc + 1].t_data.number;
			if (cloc != -1) {
				size_t size = closure_list_sizes[cloc];
				for (int i = 0; i < size; i++) {
					copy_stack_entry(closure_list[cloc][i], line);
					//printf("COPIED: %s\n", closure_list[cloc][i].id);
				}
			}
		}
		else if (op == READ) {
			push_arg(memory[memory_register], line);
		}
		else if (op == WRITE) {
			memory[memory_register] = pop_arg(line);
		}
		else if (op == OUT) {
			token t = pop_arg(line);
			if (t.t_type != NONERET) {
				print_token(&t);
			}
		}
		else if (op == OUTL) {
			token t = pop_arg(line);
			if (t.t_type != NONERET) {
				print_token_inline(&t, stdout);
			}
		}
		else if (op == BADR) {
			void* ad = &bytecode[i + 1];
			base_address += *((int*)ad);
			i += sizeof(int);
		}
		else if (op == IN) {
			// Scan one line from the input.
			char buffer[MAX_STRING_LEN];
			while(!fgets(buffer, MAX_STRING_LEN, stdin)) {};
					
			token* t = &memory[memory_register]; 
			char* end_ptr = buffer;
			errno = 0;
			double d = strtod(buffer, &end_ptr);
			// null terminator or newline character
			if (errno != 0 || (*end_ptr != 0 && *end_ptr != 10)) {
				t->t_type = STRING;
				size_t len = strlen(buffer);
				// remove last newline
				buffer[len - 1] = 0;
				strcpy(t->t_data.string, buffer);
			}
			else {
				// conversion successful
				t->t_type = NUMBER;
				t->t_data.number = d;
			}
		}
		else if (op == HALT) {
			break;
		}
		else {
			error_runtime(line, VM_INVALID_OPCODE, op, i);
		}
		if (get_error_flag()) {
			clear_arg_stack();
			break;
		}
	}
}

static token eval_binop(token op, token a, token b) {
	//print_token(&op);
	//print_token(&a);
	//print_token(&b);
	set_make_token_param(op.t_line, op.t_col);
	if (op.t_type == LEFT_BRACK) {
		// Array Reference, or String
		// A must be a list/string, b must be a number.
		if (a.t_type != LIST && a.t_type != STRING) {
			error_runtime(line, VM_NOT_A_LIST_OR_STRING);
			return none_token();
		}
		token list_header;
		if (a.t_type == LIST) {
			list_header = memory[(int)a.t_data.number];
		}
		int list_size = a.t_type == STRING ? strlen(a.t_data.string) :  
			                                 list_header.t_data.number;
//		printf("LIST SIZE IS: %d\n", list_size);
		// Pull list header reference from list object.
		address id_address = a.t_data.number;
		if (b.t_type != NUMBER && b.t_type != RANGE) {
			error_runtime(line, VM_INVALID_LIST_SUBSCRIPT);
			return none_token();
		}
//		printf("TDATA NUM IS %d\n", (int)(b.t_data.number));
		if ((b.t_type == NUMBER && (int)(b.t_data.number) >= list_size) ||
			(b.t_type == RANGE && 
			((range_start(b) > list_size || range_end(b) > list_size ||
			 range_start(b) < 0 || range_end(b) < 0)))) {
			error_runtime(line, VM_LIST_REF_OUT_RANGE);
			return none_token();
		}

		if (b.t_type == NUMBER) {
			if (a.t_type == STRING) {
				token c = make_token(STRING, make_data_str("0"));
				c.t_data.string[0] = a.t_data.string[(int)floor(b.t_data.number)];
				c.t_line = a.t_line;
				c.t_col = a.t_col;
				return c;
			}
			// Add 1 to offset because of the header.
			int offset = floor(b.t_data.number) + 1;
			token* c = get_value_of_address(id_address + offset, 
				a.t_line);
			return *c;
		}
		else {
			int start = range_start(b);
			int end = range_end(b);
			int subarray_size = start - end;	
			if (subarray_size < 0) subarray_size *= -1;

			if (a.t_type == STRING) {
				token c = make_token(STRING, make_data_str("0"));
				int n = 0;
				for (int i = start; i != end; start < end ? i++ : i--) {
					c.t_data.string[n++] = a.t_data.string[i];
				}
				c.t_data.string[n] = 0;
				c.t_line = a.t_line;
				c.t_col = a.t_col;
				return c;
			}

			address array_start = (int)(a.t_data.number);
			token* new_a = safe_malloc(subarray_size * sizeof(token));
			int n = 0;
			for (int i = start; i != end; 
				start < end ? i++ : i--) {
				new_a[n++] = memory[array_start + i + 1];
			}
			address new_aa = push_memory_a(new_a, subarray_size);
			safe_free(new_a);
			token c = make_token(LIST, make_data_num(new_aa));
			c.t_line = a.t_line;
			c.t_col = a.t_col;
			return c;
		}
	}
	if (op.t_type == DOT) {
		// Member Access! Supports two built in, size and type, value.
		// Left side should be a token.
		if (b.t_type != IDENTIFIER) {
			error_runtime(line, VM_MEMBER_NOT_IDEN);
			return false_token();
		}
		if (strcmp("size", b.t_data.string) == 0) {
			return size_of(a);	
		}
		else if (strcmp("type", b.t_data.string) == 0) {
			return type_of(a);
		}
		else if (strcmp("value", b.t_data.string) == 0) {
			return value_of(a);
		}
		else {
			// Regular Member, Must be either struct or a struct instance.
			if (a.t_type == STRUCT || a.t_type == STRUCT_INSTANCE) {
				// Either will be allowed to look through static parameters.
				address metadata = (int)(a.t_data.number);
				if (a.t_type == STRUCT_INSTANCE) {
					// metadata actually points to the STRUCT_INSTANE_HEADER 
					//   right now.
					metadata = (address)(memory[metadata].t_data.number);
				}
				memory_register_A = metadata;
				token_type struct_type = a.t_type;
				address struct_header = a.t_data.number;
				while(1) {
					int params_passed = 0;
					address parent_meta = 0;
					int size = (int)(memory[metadata].t_data.number);
					for (int i = 0; i < size; i++) {
						token mdata = memory[metadata + i];
						if (mdata.t_type == STRUCT_STATIC &&
							strcmp(mdata.t_data.string, b.t_data.string) == 0) {
							// Found the static member we were looking for
							token result = memory[metadata + i + 1];
							if (result.t_type == FUNCTION) {
								result.t_type = STRUCT_FUNCTION;
							}
							return result;
						}
						else if (mdata.t_type == STRUCT_PARAM) {
							if (struct_type == STRUCT_INSTANCE && 
								strcmp(mdata.t_data.string, b.t_data.string) == 0) {
								// Found the instance member we were looking for.
								// Address of the STRUCT_INSTANCE_HEADER offset by
								//   params_passed + 1;
								address loc = struct_header + params_passed + 1;
								token result = memory[loc];
								if (result.t_type == FUNCTION) {
									result.t_type = STRUCT_FUNCTION;
								}
								return result;
							}
							params_passed++;
						}
						else if (mdata.t_type == STRUCT_PARENT) {
							parent_meta = mdata.t_data.number;
						}
					}
					// Check now if there is a parent class
					if (parent_meta) {
						metadata = parent_meta;
						struct_header = memory[struct_header + 1].t_data.number;
					}
					else {
						break;
					}
				}
			}
			error_runtime(line, VM_MEMBER_NOT_EXIST);
			return false_token();
		}
	}
	bool has_address = (a.t_type == ADDRESS || b.t_type == ADDRESS);
	if ((a.t_type == NUMBER || a.t_type == ADDRESS) && 
		(b.t_type == NUMBER || b.t_type == ADDRESS)) {
		switch (op.t_type) {
			case EQUAL_EQUAL: 	
				return (a.t_data.number == b.t_data.number) ? 
					true_token() : false_token();
			case NOT_EQUAL:
				return (a.t_data.number != b.t_data.number) ? 
					true_token() : false_token();
			case GREATER_EQUAL:
				return (a.t_data.number >= b.t_data.number) ? 
					true_token() : false_token();
			case GREATER:
				return (a.t_data.number > b.t_data.number) ? 
					true_token() : false_token();
			case LESS_EQUAL:
				return (a.t_data.number <= b.t_data.number) ? 
					true_token() : false_token();
			case LESS:
				return (a.t_data.number < b.t_data.number) ? 
					true_token() : false_token();
			case RANGE_OP:
				return range_token(a.t_data.number, b.t_data.number);
			case PLUS: 	
				return make_token(has_address ? ADDRESS : NUMBER, 
						make_data_num(a.t_data.number + b.t_data.number));
			case MINUS:	
				return make_token(has_address ? ADDRESS : NUMBER, 
						make_data_num(a.t_data.number - b.t_data.number));
			case STAR: 	
				if (has_address) error_runtime(line, VM_TYPE_ERROR, "*");
				return make_token(NUMBER, make_data_num(
									a.t_data.number * b.t_data.number));
			case SLASH:
			case INTSLASH:
			case PERCENT: 
				if (has_address) error_runtime(line, VM_TYPE_ERROR, "%");
				// check for division by zero error
				if (b.t_data.number == 0) {
					error_runtime(line, VM_MATH_DISASTER); 
				} 
				else {
					if (op.t_type == PERCENT) {
						// modulus operator
						double a_n = a.t_data.number;
						double b_n = b.t_data.number;

						// check integer 
						if (a_n != floor(a_n) || b_n != floor(b_n)) {
							error_runtime(line, VM_TYPE_ERROR, "/"); 
							return none_token();
						}
						else {
							return make_token(NUMBER, make_data_num(
								(long long)a.t_data.number % 
								(long long)b.t_data.number));
						}
					}
					else {
						double result = a.t_data.number / b.t_data.number;
						if (op.t_type == SLASH) {
							return make_token(NUMBER, make_data_num(result));
						}
						else {
							return make_token(NUMBER, make_data_num((int)result));
						}
					}
				}
				break;
			default: 
				error_runtime(line, VM_NUM_NUM_INVALID_OPERATOR, 
					op.t_data.string);
				break;
		}
	}
	else if((a.t_type == STRING && b.t_type == STRING) && 
			(op.t_type == EQUAL_EQUAL || op.t_type == NOT_EQUAL)) {
		return (op.t_type == EQUAL_EQUAL) ^ 
			(strcmp(a.t_data.string, b.t_data.string) == 0) ? 
			false_token() : true_token();
	}
	else if((a.t_type == NONE || b.t_type == NONE) && 
			(op.t_type == EQUAL_EQUAL || op.t_type == NOT_EQUAL)) {
		return (op.t_type == EQUAL_EQUAL) ^ 
			(a.t_type == NONE && b.t_type == NONE) ? 
			false_token() : true_token();
	}
	else if((a.t_type == OBJ_TYPE && b.t_type == OBJ_TYPE) && 
			(op.t_type == EQUAL_EQUAL || op.t_type == NOT_EQUAL)) {
		return (op.t_type == EQUAL_EQUAL) ^ 
			(strcmp(a.t_data.string, b.t_data.string) == 0) ? 
			false_token() : true_token();
	}

	// Done number operations: past here, addresses are numbers too.
	if (a.t_type == ADDRESS) a.t_type = NUMBER;
	if (b.t_type == ADDRESS) b.t_type = NUMBER;
	if (a.t_type == LIST || b.t_type == LIST) {
		if (a.t_type == LIST && b.t_type == LIST) {
			address start_a = a.t_data.number;
			address start_b = b.t_data.number;
			int size_a = memory[start_a].t_data.number;
			int size_b = memory[start_b].t_data.number;
	
			switch (op.t_type) {
				case EQUAL_EQUAL: {
					if (size_a != size_b) {
						return false_token();
					}
					for (int i = 0; i < size_a; i++) {
						if (!token_equal(&memory[start_a + i + 1],
										&memory[start_b + i + 1])) {
							return false_token();
						}
					}
					return true_token();

				}
				break;
				case NOT_EQUAL: {
					if (size_a != size_b) {
						return true_token();
					}
					for (int i = 0; i < size_a; i++) {
						if (token_equal(&memory[start_a + i + 1],
										&memory[start_b + i + 1])) {
							return false_token();
						}
					}
					return true_token();
				}
				break;
				case PLUS: {
					int new_size = size_a + size_b;
					token* new_list = safe_malloc(new_size * sizeof(token));
					int n = 0;
					for (int i = 0; i < size_a; i++) {
						new_list[n++] = memory[start_a + i + 1];
					}
					for (int i = 0; i < size_b; i++) {
						new_list[n++] = memory[start_b + i + 1];
					}
					address new_adr = push_memory_a(new_list, new_size);
					safe_free(new_list);
					return make_token(LIST, make_data_num(new_adr));
				}	
				break;
				default: error_runtime(line, VM_LIST_LIST_INVALID_OPERATOR, 
					op.t_data.string); break;
			}
		} // End A==List && B==List
		else if (a.t_type == LIST) {
			if (op.t_type == PLUS) {
				// list + element
				address start_a = a.t_data.number;
				int size_a = memory[start_a].t_data.number;

				token* new_list = safe_malloc((size_a + 1) * sizeof(token));
				int n = 0;
				for (int i = 0; i < size_a; i++) {
					new_list[n++] = memory[start_a + i + 1];
				}
				new_list[n++] = b;
				address new_adr = push_memory_a(new_list, size_a + 1);
				safe_free(new_list);
				return make_token(LIST, make_data_num(new_adr));
			}
			else { 
				error_runtime(line, VM_INVALID_APPEND); 
			}
		}
		else if (b.t_type == LIST) {
			address start_b = b.t_data.number;
			int size_b = memory[start_b].t_data.number;

			if (op.t_type == PLUS) {
				// element + list

				token* new_list = safe_malloc((size_b + 1) * sizeof(token));
				int n = 0;
				new_list[n++] = a;
				for (int i = 0; i < size_b; i++) {
					new_list[n++] = memory[start_b + i + 1];
				}
				address new_adr = push_memory_a(new_list, size_b + 1);
				safe_free(new_list);
				return make_token(LIST, make_data_num(new_adr));
			}
			else if (op.t_type == IN) {
				// element in list
				for (int i = 0; i < size_b; i++) {
//					print_token(&a);
					//print_token(&memory[start_b + i + 1]);
					if (token_equal(&a, &memory[start_b + i + 1])) {
						return true_token();
					}
				}
				return false_token();
			}
			else { error_runtime(line, VM_INVALID_APPEND); }
		}
	}
	else if((a.t_type == STRING && b.t_type == STRING) ||
			(a.t_type == STRING && b.t_type == NUMBER) ||
			(a.t_type == NUMBER && b.t_type == STRING)) {
		if (op.t_type == PLUS) {
			// string concatenation
			if (a.t_type == NUMBER) {
				sprintf(a.t_data.string, "%g", a.t_data.number); 
			}
			if (b.t_type == NUMBER) {
				sprintf(b.t_data.string, "%g", b.t_data.number); 
			}
			size_t len1 = strlen(a.t_data.string);
			size_t len2 = strlen(b.t_data.string);
			
			//printf("A: %s SIZE: %zd\n", a.t_data.string, len1);
			//printf("B: %s SIZE: %zd\n", b.t_data.string, len2);

			char result[len1 + len2 + 1]; // add1 for the null terminator
			memcpy(result, a.t_data.string, len1); // exclude null term
			memcpy(result + len1, b.t_data.string, len2); // cat second string
			result[len1 + len2] = '\0'; // add null terminator
			return make_token(STRING, make_data_str(result));
		}
		else {
			error_runtime(line, VM_STRING_NUM_INVALID_OPERATOR, 
				op.t_data.string);
			return none_token();
		}
	}
	else if((a.t_type == TRUE || a.t_type == FALSE) &&
			(b.t_type == TRUE || b.t_type == FALSE)) {
		switch (op.t_type) {
			case AND: 	
				return (a.t_type == TRUE && b.t_type == TRUE) ? 
					true_token() : false_token();
			case OR:
				return (a.t_type == FALSE && b.t_type == FALSE) ?
					false_token() : true_token();
			default: 
				error_runtime(line, VM_TYPE_ERROR, op.t_data.string);
				break;
		}
	}
	else {
		error_runtime(line, VM_TYPE_ERROR, op.t_data.string);
		return none_token();
	}
	return none_token();
}

static token value_of(token a) {
	if (a.t_type == STRING && strlen(a.t_data.string) == 1) {
		return make_token(NUMBER, make_data_num(a.t_data.string[0]));
	}
	else {
		return a;	
	}
}

static token size_of(token a) {
	double size = 0;
	if (a.t_type == STRING) {
		size = strlen(a.t_data.string);
	}	
	else if (a.t_type == LIST) {
		address h = a.t_data.number;
		size = memory[h].t_data.number;
	}
	return make_token(NUMBER, make_data_num(size));
}

static token type_of(token a) {
	switch (a.t_type) {
		case STRING:
			return make_token(OBJ_TYPE, make_data_str("string"));
		case NUMBER:
			return make_token(OBJ_TYPE, make_data_str("number"));
		case TRUE:
		case FALSE:
			return make_token(OBJ_TYPE, make_data_str("bool"));
		case NONE:
			return make_token(OBJ_TYPE, make_data_str("none"));
		case ADDRESS:
			return make_token(OBJ_TYPE, make_data_str("address"));
		case LIST:
			return make_token(OBJ_TYPE, make_data_str("list"));
		case STRUCT:
			return make_token(OBJ_TYPE, make_data_str("struct"));
		case STRUCT_INSTANCE: {
			token instance_loc = memory[(int)(a.t_data.number)]; 
			return make_token(OBJ_TYPE, make_data_str(
				memory[(int)instance_loc.t_data.number + 1].t_data.string));
		}
		default:
			return make_token(OBJ_TYPE, make_data_str("none"));
	}
}

static token eval_uniop(token op, token a) {
	set_make_token_param(op.t_line, op.t_col);
	if (op.t_type == TILDE) {
		// Create copy of object a, only applies to lists or 
		// struct or struct instances
		if (a.t_type == LIST) {
			// We make a copy of the list as pointed to A.
			token list_header = memory[(int)a.t_data.number];
			int list_size = list_header.t_data.number;
			token* new_a = safe_malloc((list_size) * sizeof(token));
			int n = 0;
			address array_start = a.t_data.number;
			for (int i = 0; i < list_size; i++) {
				new_a[n++] = memory[array_start + i + 1];
			}
			address new_l_loc = push_memory_a(new_a, list_size);
			safe_free(new_a);
			return make_token(LIST, make_data_num(new_l_loc));
		}
		else if (a.t_type == STRUCT || a.t_type == STRUCT_INSTANCE) {
			// We make a copy of the STRUCT
			address copy_start = a.t_data.number;
			address metadata = (int)(a.t_data.number);
			if (a.t_type == STRUCT_INSTANCE) {
				metadata = memory[metadata].t_data.number;
			}
			int size = memory[metadata].t_data.number + 1;
			if (a.t_type == STRUCT_INSTANCE) {
				// find size of instance by counting instance members
				int actual_size = 0;
				for (int i = 0; i < size; i++) {
					if (memory[metadata + i + 1].t_type == STRUCT_PARAM) {
						actual_size++;
					}
				}
				size = actual_size;
			}
			size++; // for the header itself
			token* copy = safe_malloc(size * sizeof(token));
			for (int i = 0; i < size; i++) {
				copy[i] = memory[copy_start + i];
			}
			address addr = push_memory_array(copy, size);
			safe_free(copy);
			return a.t_type == STRUCT ? make_token(STRUCT, make_data_num(addr))
				: make_token(STRUCT_INSTANCE, make_data_num(addr));
		}
		else {
			// No copy needs to be made
			return a;
		}
	}
	else if (op.t_type == MINUS) {
		if (a.t_type != NUMBER) {
			error_runtime(line, VM_INVALID_NEGATE);
			return none_token();
		}
		token res = make_token(NUMBER, make_data_num(-1 * a.t_data.number));
		return res;
	}	
	else if (op.t_type == NOT) {
		if (a.t_type != TRUE &&  a.t_type != FALSE) {
			error_runtime(line, VM_INVALID_NEGATE);
			return none_token();
		}
		return a.t_type == TRUE ? false_token() : true_token();	
	}
	else {
		// fallback case
		return none_token();
	}
}


