#include "vm.h"
#include "codegen.h"
#include "memory.h"
#include "token.h"
#include "error.h"
#include "stdint.h"
#include "math.h"
#include <string.h>
#include <stdlib.h>

static address memory_register = 0;

void vm_run(uint8_t* bytecode, size_t size) {
	for (address i = 0; i < size; i++) {
		opcode op = bytecode[i];
		if (op == OPUSH) {
			i++;
			token t = get_token(&bytecode[i], &i);
			if (t.t_type == IDENTIFIER) {
				t = *get_value_of_id(t.t_data.string, i);
			}
			push_arg(t);
		}
		else if (op == OPOP) {
			token t = pop_arg(i);	
			memory[memory_register] = t;
		}
		else if (op == BIN) {
			i++;

			token operator = get_token(&bytecode[i], &i);
			token b = pop_arg(i);
			token a = pop_arg(i);
			push_arg(eval_binop(operator, a, b));

		}
		else if (op == UNA) {
			i++;
			token operator = get_token(&bytecode[i], &i);
			token a = pop_arg(i);
			push_arg(eval_uniop(operator, a));
		}
		else if (op == BIND) {
			i++;
			push_stack_entry((char*)(bytecode + i), memory_register);
			
			i += strlen((char*)(bytecode + i));
		}
		else if (op == WHERE) {
			i++;
			memory_register = get_address_of_id((char*)(bytecode + i), i);
			i += strlen((char*)(bytecode + i));
		}
		else if (op == ORET) {
			pop_frame(true, &i);
			i--;
		}
		else if (op == OREQ) {
			// Memory Request Instruction
			memory_register = pls_give_memory(bytecode[i + 1]);
			i++;
		}
		else if (op == PLIST) {
			i++;
			int size = bytecode[i];
			for (int j = memory_register + size - 1; 
					j >= memory_register; j--) {
				memory[j] = pop_arg(i);
			}
			push_arg(make_token(LIST, make_data_num(memory_register)));
		}
		else if (op == JMP) {
			// Jump Instruction
			i = bytecode[i + 1] - 1;
		}
		else if (op == OCALL) {
			push_frame("function", i + 1);
			token top = pop_arg(i);
			if (top.t_type != ADDRESS) {
				error(i, FN_CALL_NOT_FN); 
			}
			i = (int)top.t_data.number - 1;
		}
		else if (op == PULL) {
			push_arg(memory[memory_register]);
		}
		else if (op == OUT) {
			token t = pop_arg(i);
			print_token(&t);
		}
		else {
			printf("Error at location %X", i);
			error(0, INVALID_OPCODE);
		}
	}
	free_error();
}

token eval_binop(token op, token a, token b) {
	//print_token(&op);
	//print_token(&a);
	//print_token(&b);
	if (op.t_type == LEFT_BRACK) {
		// Array Reference, or String
		// A must be a list/string, b must be a number.
		if (a.t_type != LIST && a.t_type != STRING) {
			error(a.t_line, NOT_A_LIST_OR_STRING);
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
			error(b.t_line, INVALID_LIST_SUBSCRIPT);
			return none_token();
		}
//		printf("TDATA NUM IS %d\n", (int)(b.t_data.number));
		if ((b.t_type == NUMBER && (int)(b.t_data.number) >= list_size) ||
			(b.t_type == RANGE && 
			((range_start(b) >= list_size || range_end(b) >= list_size ||
			 range_start(b) < 0 || range_end(b) < 0)))) {
			error(b.t_line, ARRAY_REF_OUT_RANGE);
		}

		if (b.t_type == NUMBER) {
			if (a.t_type == STRING) {
				token c = make_token(STRING, make_data_str("0"));
				c.t_data.string[0] = a.t_data.string[(int)floor(b.t_data.number)];
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
			subarray_size++;

			if (a.t_type == STRING) {
				token c = make_token(STRING, make_data_str("0"));
				int n = 0;
				for (int i = start; i != end; start < end ? i++ : i--) {
					c.t_data.string[n++] = a.t_data.string[i];
				}
				c.t_data.string[n++] = a.t_data.string[end];
				c.t_data.string[n] = 0;
				return c;
			}

		//	subarray_size++;	
			address array_start = (int)(a.t_data.number);
			token* new_a = malloc(subarray_size * sizeof(token));
			int n = 0;
			for (int i = start; i != end; 
				start < end ? i++ : i--) {
				new_a[n++] = memory[array_start + i + 1];
			}
			new_a[n++] = memory[array_start + end + 1];

			address new_aa = push_memory_a(new_a, subarray_size);
			free(new_a);
			return make_token(LIST, make_data_num(new_aa));
		}
	}
	if (op.t_type == DOT) {
		// Member Access! Supports two built in, size and type.
		// Left side should be a token.
		if (b.t_type != IDENTIFIER) {
			error(b.t_line, MEMBER_NOT_IDEN);
			return false_token();
		}
		if (strcmp("size", b.t_data.string) == 0) {
			return size_of(a);	
		}
		else if (strcmp("type", b.t_data.string) == 0) {
			return type_of(a);
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
							// Found the static member we were looking for.
							return memory[metadata + i + 1];
						}
						else if (mdata.t_type == STRUCT_PARAM) {
							if (struct_type == STRUCT_INSTANCE && 
								strcmp(mdata.t_data.string, b.t_data.string) == 0) {
								// Found the instance member we were looking for.
								// Address of the STRUCT_INSTANCE_HEADER offset by
								//   params_passed + 1;
								address loc = struct_header + params_passed + 1;
								return memory[loc];
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
			error(b.t_line, MEMBER_NOT_EXIST);
			return false_token();
		}
	}
/*	if (op.t_type == TYPEOF) {
		if (b.t_type != OBJ_TYPE && b.t_type != NONE) {
			error(op.t_line, TYPE_ERROR); 
		}
		if (a.t_type == NUMBER) {
			return (strcmp("Number", b.t_data.string) == 0) ?
				true_token() : false_token();
		}
		else if (a.t_type == STRING) {
			return (strcmp("String", b.t_data.string) == 0) ?
				true_token() : false_token();
		}
		else if (a.t_type == ADDRESS) {
			return (strcmp("Address", b.t_data.string) == 0) ?
				true_token() : false_token();
		}
		else if (a.t_type == TRUE || a.t_type == FALSE) {
			return (strcmp("Bool", b.t_data.string) == 0) ?
				true_token() : false_token();
		}
		else if (a.t_type == LIST) {
			return (strcmp("List", b.t_data.string) == 0) ?
				true_token() : false_token();
		}
		else if (a.t_type == NONE) {
			return b.t_type == NONE ? true_token() : false_token();
		}
		else {
			return false_token();
		}
	}*/
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
				if (has_address) error(a.t_line, TYPE_ERROR, "*");
				return make_token(NUMBER, make_data_num(
									a.t_data.number * b.t_data.number));
			case SLASH:
			case INTSLASH:
			case PERCENT: 
				if (has_address) error(a.t_line, TYPE_ERROR, "%");
				// check for division by zero error
				if (b.t_data.number == 0) {
					error(b.t_line, MATH_DISASTER); 
				} 
				else {
					if (op.t_type == PERCENT) {
						// modulus operator
						double a_n = a.t_data.number;
						double b_n = b.t_data.number;

						// check integer 
						if (a_n != floor(a_n) || b_n != floor(b_n)) {
							error(op.t_line, TYPE_ERROR, "/"); 
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
				error(a.t_line, NUM_NUM_INVALID_OPERATOR);
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
					token* new_list = malloc(new_size * sizeof(token));
					int n = 0;
					for (int i = 0; i < size_a; i++) {
						new_list[n++] = memory[start_a + i + 1];
					}
					for (int i = 0; i < size_b; i++) {
						new_list[n++] = memory[start_b + i + 1];
					}
					address new_adr = push_memory_a(new_list, new_size);
					free(new_list);
					return make_token(LIST, make_data_num(new_adr));
				}	
				break;
				default: error(a.t_line, LIST_LIST_INVALID_OPERATOR); break;
			}
		} // End A==List && B==List
		else if (a.t_type == LIST) {
			if (op.t_type == PLUS) {
				// list + element
				address start_a = a.t_data.number;
				int size_a = memory[start_a].t_data.number;

				token* new_list = malloc((size_a + 1) * sizeof(token));
				int n = 0;
				for (int i = 0; i < size_a; i++) {
					new_list[n++] = memory[start_a + i + 1];
				}
				new_list[n++] = b;
				address new_adr = push_memory_a(new_list, size_a + 1);
				free(new_list);
				return make_token(LIST, make_data_num(new_adr));
			}
			else { error(a.t_line, INVALID_APPEND); }
		}
		else if (b.t_type == LIST) {
			address start_b = b.t_data.number;
			int size_b = memory[start_b].t_data.number;

			if (op.t_type == PLUS) {
				// element + list

				token* new_list = malloc((size_b + 1) * sizeof(token));
				int n = 0;
				new_list[n++] = a;
				for (int i = 0; i < size_b; i++) {
					new_list[n++] = memory[start_b + i + 1];
				}
				address new_adr = push_memory_a(new_list, size_b + 1);
				free(new_list);
				return make_token(LIST, make_data_num(new_adr));
			}
			else if (op.t_type == TILDE) {
				// element ~ list
				for (int i = 0; i < size_b; i++) {
//					print_token(&a);
					//print_token(&memory[start_b + i + 1]);
					if (token_equal(&a, &memory[start_b + i + 1])) {
						return true_token();
					}
				}
				return false_token();
			}
			else { error(a.t_line, INVALID_APPEND); }
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
			error(a.t_line, STRING_NUM_INVALID_OPERATOR);
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
				error(a.t_line, TYPE_ERROR, op.t_data.string);
				break;
		}
	}
	else {
		error(a.t_line, TYPE_ERROR, op.t_data.string);
		return none_token();
	}
	return none_token();
}

token size_of(token a) {
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

token type_of(token a) {
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
token eval_uniop(token op, token a) {
	if (op.t_type == TILDE) {
		// Create copy of object a, only applies to lists or 
		// struct or struct instances
		if (a.t_type == LIST) {
			// We make a copy of the list as pointed to A.
			token list_header = memory[(int)a.t_data.number];
			int list_size = list_header.t_data.number;
			token* new_a = malloc((list_size) * sizeof(token));
			int n = 0;
			address array_start = a.t_data.number;
//			printf("listtsize %d", list_size);
			for (int i = 0; i < list_size; i++) {
				new_a[n++] = memory[array_start + i + 1];
			}
			address new_l_loc = push_memory_a(new_a, list_size);
			free(new_a);
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
			token* copy = malloc(size * sizeof(token));
			for (int i = 0; i < size; i++) {
				copy[i] = memory[copy_start + i];
			}
			address addr = push_memory_array(copy, size);
			free(copy);
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
			error(a.t_line, INVALID_NEGATE);
			return none_token();
		}
		token res = make_token(NUMBER, make_data_num(-1 * a.t_data.number));
		return res;
	}	
	else if (op.t_type == NOT) {
		if (a.t_type != TRUE &&  a.t_type != FALSE) {
			error(a.t_line, INVALID_NEGATE);
			return none_token();
		}
		return a.t_type == TRUE ? false_token() : true_token();	
	}
	else {
		// fallback case
		return none_token();
	}
}


