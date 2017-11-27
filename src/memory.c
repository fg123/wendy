#include "memory.h"
#include "error.h"
#include "global.h"
#include <string.h>
#include <stdio.h>

// Memory.c, provides functions for the interpreter to manipulate the local
//   WendyScript memory

#define FUNCTION_START_CHAR '>'
#define AUTOFRAME_START_CHAR '<'
#define RA_START_CHAR '#'

address frame_pointer = 0;
address stack_pointer = 0;
address arg_pointer = 0;
address closure_list_pointer = 0;
size_t closure_list_size = 0;
address mem_reg_pointer = 0;

// pointer to the end of the main frame
address main_end_pointer = 0;

void write_memory(unsigned int location, data d) {
    destroy_data(memory + location);
    memory[location] = d;
}

void mark_locations(bool* marked, address start, size_t block_size) {
    for (int j = start; j < start + block_size; j++) {
        marked[j] = true;
    }
}

bool garbage_collect(int size) {
    if (get_settings_flag(SETTINGS_NOGC)) {
        return has_memory(size);
    }
    // Garbage! We'll implement the most basic mark and sweep algo.
    bool *marked = safe_calloc(MEMORY_SIZE, sizeof(bool));
    for (int i = 0; i < RESERVED_MEMORY; i++) {
        // Don't delete the reserved ones!
        marked[i] = true;
    }
    for (int i = 0; i < stack_pointer; i++) {
        if (call_stack[i].id[0] != FUNCTION_START_CHAR &&
            call_stack[i].id[0] != AUTOFRAME_START_CHAR &&
            call_stack[i].id[0] != RA_START_CHAR) {

            address a = call_stack[i].val;
            // Mark it!
            marked[a] = true;
            // Check if it's a pointer type?
            if (memory[a].type == D_LIST || memory[a].type == D_STRUCT) {
                a = memory[a].value.number;
                mark_locations(marked, a, memory[a].value.number);
            }
            else if (memory[a].type == D_FUNCTION) {
                a = memory[a].value.number;
                mark_locations(marked, a, 3);
            }
            else if (memory[a].type == D_STRUCT_INSTANCE) {
                a = memory[a].value.number;
                // a points to D_STRUCT_INSTANCE_HEAD
                address meta_loc = memory[a].value.number;
                // meta_loc points to D_STRUCT_METADATA, mark the metadata
                size_t meta_size = memory[meta_loc].value.number;
                mark_locations(marked, meta_loc, meta_size);
                // count parameters
                size_t params = 0;
                for (address i = meta_loc; i < meta_loc + meta_size; i++) {
                    if (memory[i].type == D_STRUCT_PARAM) {
                        params++;
                    }
                }
                // Mark parameters, +1 for T_STRUCT_INSTANCE_HEAD
                mark_locations(marked, a, params + 1);
            }
        }
    }

    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (!marked[i]) {
            here_u_go(i, 1);
        }
    }
    safe_free(marked);
    return has_memory(size);
}

address create_closure() {
    address location = closure_list_pointer;
    // Things to reserve.
    size_t size = stack_pointer - main_end_pointer;

    stack_entry* closure = safe_malloc(sizeof(stack_entry) * size);
    size_t actual_size = 0;
    for (int i = main_end_pointer; i < stack_pointer; i++) {
        if (call_stack[i].id[0] == FUNCTION_START_CHAR ||
            call_stack[i].id[0] == AUTOFRAME_START_CHAR ||
            call_stack[i].id[0] == RA_START_CHAR) {
        }
        else {
            strcpy(closure[actual_size].id, call_stack[i].id);
            closure[actual_size].val = call_stack[i].val;
            actual_size++;
        }
    }
    if (actual_size <= 0) {
        safe_free(closure);
        return -1;
    }
    closure = safe_realloc(closure, actual_size * sizeof(stack_entry));
    closure_list[closure_list_pointer] = closure;
    closure_list_sizes[closure_list_pointer] = actual_size;
    closure_list_pointer++;

    if (closure_list_pointer == closure_list_size) {
        // Resize for more storage
        closure_list_size *= 2;
        closure_list = safe_realloc(closure_list,
            closure_list_size * sizeof(stack_entry*));
        // Reset 0s
        for (int i = closure_list_pointer; i < closure_list_size; i++) {
            closure_list[i] = 0;
        }
        closure_list_sizes = safe_realloc(closure_list_sizes,
            closure_list_size * sizeof(size_t));
    }
    return location;
}

bool has_memory(int size) {
    // Do we have memory??
    mem_block* c = free_memory;
    mem_block* p = 0;
    while (c) {
        if (c->size >= size) {
            // Enough Room!
            return true;
        }
        else {
            if (c->size == 0) {
                // What is this free block doing here with size 0? Atrocious!
                if (p) p->next = c->next;
                else free_memory = c->next;

                // Remove that memory block!
                safe_free(c);

                // Next one!
                if (p) c = p->next;
                else c = free_memory;
            }
            else {
                p = c;
                c = c->next;
            }
        }
    }
    return false;
}

address pls_give_memory(int size, int line) {
    // Do we have memory??
    if (has_memory(size)) {
        mem_block* c = free_memory;
        while (c) {
            if (c->size >= size) {
                // Enough Room!
                c->size -= size;
                address start = c->start;
                c->start += size;
                //print_free_memory();
                return start;
            }
            else {
                // Try next block...
                c = c->next;
            }
        }
    }
    // No free memory blocks???
//  printf("Test: %d\b", garbage_collect(size));
    if(garbage_collect(size)) {
        // try again
        return pls_give_memory(size, line);
    }
    else {
        error_runtime(line, MEMORY_OVERFLOW);
        return 0;
    }
}

void here_u_go(address a, int size) {
    // Returned memory! Yay!
    // Returned memory could also be already freed.
    // We'll look to see if the returned memory can be appended to another
    //   free block. If not, then we append to the front. Memory could be
    //   in front of an existing or behind.
    mem_block *c = free_memory;
    address end = a + size;
    while (c) {
        if (a >= c->start && a + size <= c->start + c->size) {
            // Block was already freed!
            return;
        }
        else if (c->start == end) {
            // New block slides in front of another.
            c->start = a;
            c->size += size;
            return;
        }
        else if (c->start + c->size == a) {
            // New block slides behind another.
            c->size += size;
            return;
        }
        else {
            // Try next block.
            c = c->next;
        }
    }
    // No other spot, we'll stick it to the front of the free_memory list!
    mem_block* new_m_block = safe_malloc(sizeof(mem_block));
    new_m_block->start = a;
    new_m_block->size = size;
    new_m_block->next = free_memory;

    free_memory = new_m_block;
}


void print_free_memory() {
    printf("=============\n");
    printf("Free Memory Blocks:\n");
    mem_block* c = free_memory;
    while(c) {
        printf("Block from %d(0x%X) with size %d.\n", c->start, c->start, c->size);
        c = c->next;
    }
    printf("=============\n");
}

void init_memory() {
    // Initialize Memory
    memory = safe_calloc(MEMORY_SIZE, sizeof(data));

    // Initialize MemReg
    mem_reg_stack = safe_calloc(MEMREGSTACK_SIZE, sizeof(address));

    // Initialize linked list of Free Memory
    free_memory = safe_malloc(sizeof(mem_block));
    free_memory->size = MEMORY_SIZE - ARGSTACK_SIZE;
    free_memory->start = 0;
    free_memory->next = 0;

    // Initialize Call Stack
    call_stack = safe_calloc(STACK_SIZE, sizeof(stack_entry));
    arg_pointer = MEMORY_SIZE - 1;

    closure_list = safe_calloc(CLOSURES_SIZE, sizeof(stack_entry*));
    closure_list_sizes = safe_malloc(sizeof(size_t) * CLOSURES_SIZE);
    closure_list_size = CLOSURES_SIZE;

    // ADDRESS 0 REFERS TO NONE_data
    push_memory(none_data(), 0);
    // ADDRESS 1 REFERS TO EMPTY RETURNS data
    push_memory(noneret_data(), 0);
}

void clear_arg_stack() {
    arg_pointer = MEMORY_SIZE - 1;
}

void c_free_memory() {
    for (size_t i = 0; i < MEMORY_SIZE; i++) {
        destroy_data(memory + i);
    }
    safe_free(memory);
    safe_free(call_stack);
    safe_free(mem_reg_stack);

    // Clear all the free_memory blocks.
    mem_block* c = free_memory;
    while (c) {
        mem_block* next = c->next;
        safe_free(c);
        c = next;
    }
    safe_free(closure_list_sizes);
    int i = 0;
    while (closure_list[i]) {
        safe_free(closure_list[i]);
        i++;
    }
    safe_free(closure_list);
}

void check_memory(int line) {
    // Check stack
    if (stack_pointer >= STACK_SIZE) {
        printf("Call stack at %d with limit %d!", stack_pointer, STACK_SIZE);
        error_runtime(line, MEMORY_STACK_OVERFLOW);
    }
    // Check memory, if the two ends overlap
    if (arg_pointer <= MEMORY_SIZE - ARGSTACK_SIZE) {
        printf("Internal Stack out of memory! %d with limit %d.\n",
                MEMORY_SIZE - arg_pointer, ARGSTACK_SIZE);
        error_runtime(line, MEMORY_STACK_OVERFLOW);
    }
    if (!has_memory(1)) {
        // Collect Garbage
        if (garbage_collect(1)) {
            check_memory(line);
        }
        else {
            printf("Out of memory with limit %d!\n", MEMORY_SIZE);
            error_runtime(line, MEMORY_OVERFLOW);
        }
    }
}

void push_frame(char* name, address ret, int line) {
    // store current frame pointer
    stack_entry new_entry = { "> ", frame_pointer };
    strcat(new_entry.id, name);
    strcat(new_entry.id, "()");
    // this new entry will be stored @ location stack_pointer, so we move the
    //   frame pointer to this location
    frame_pointer = stack_pointer;
    // add and increment stack_pointer
    call_stack[stack_pointer++] = new_entry;
    check_memory(line);
    stack_entry ne2 = { "0R/A", ret };
    ne2.id[0] = RA_START_CHAR;
    call_stack[stack_pointer++] = ne2;
    check_memory(line);
    // pointer to self
}

void push_auto_frame(address ret, char* type, int line) {
    // store current frame pointer
    stack_entry new_entry = { "<autoframe:", frame_pointer };
    strcat(new_entry.id, type);
    strcat(new_entry.id, ">");

    frame_pointer = stack_pointer;
    call_stack[stack_pointer++] = new_entry;

    check_memory(line);

    stack_entry ne2 = { "RET", ret };
    call_stack[stack_pointer++] = ne2;
    check_memory(line);
}

bool pop_frame(bool is_ret, address* ret) {
    //printf("POPPED RET:%d\n", is_ret);
    //print_call_stack();
    // trace back until we hit a FUNC
    address trace = frame_pointer;
    if (is_ret) {
        // character [ is a function frame pointer, we trace until we find it
        while (call_stack[trace].id[0] != FUNCTION_START_CHAR) {
            trace = call_stack[trace].val;
        }
        *ret = call_stack[trace + 1].val;
    }
    stack_pointer = trace;
    frame_pointer = call_stack[trace].val;
//  printf("NEW FP IS %d\n", frame_pointer);
    // function ret or popped a function
    return (is_ret || call_stack[trace].id[0] == FUNCTION_START_CHAR);
}

void write_state(FILE* fp) {
    fprintf(fp, "FramePointer: %d\n", frame_pointer);
    fprintf(fp, "StackTrace: %d\n", stack_pointer);
    for (int i = 0; i < stack_pointer; i++) {
        if (call_stack[i].id[0] == FUNCTION_START_CHAR) {
            fprintf(fp, ">%s %d\n", call_stack[i].id, call_stack[i].val);
        }
        else {
            fprintf(fp, "%s %d\n", call_stack[i].id, call_stack[i].val);
        }
    }
    fprintf(fp, "Memory: %d\n", MEMORY_SIZE);
    for (int i = 0; i < MEMORY_SIZE; i++) {
        if (memory[i].type != 0) {
            fprintf(fp, "%d %s ", i, data_string[memory[i].type]);
            print_data_inline(&memory[i], fp);
            fprintf(fp, "\n");
        }
    }
}

void print_call_stack(int maxlines) {
    printf("\n===============\n");
    printf("Dump: Stack Trace\n");
    int start = stack_pointer - maxlines;
    if (start < 0) start = 0;
    for (int i = start; i < stack_pointer; i++) {
        if (call_stack[i].id[0] != '$' && call_stack[i].id[0] != '~') {
            if (frame_pointer == i) {
                if (call_stack[i].id[0] == FUNCTION_START_CHAR) {
                    printf("%5d FP-> [" BLU "%s" RESET " -> 0x%04X", i,
                            call_stack[i].id, call_stack[i].val);
                }
                else {
                    printf("%5d FP-> [%s -> 0x%04X", i, call_stack[i].id,
                            call_stack[i].val);
                }
//              print_data_inline(&memory[call_stack[i].val], stdout);
                printf("]\n");
            }
            else {
                if (call_stack[i].id[0] == FUNCTION_START_CHAR) {
                    printf("%5d      [" BLU "%s" RESET " -> 0x%04X: ", i,
                            call_stack[i].id, call_stack[i].val);
                }
                else if (call_stack[i].is_closure) {
                    printf("%5d  C-> [%s -> 0x%04X: ",i,
                            call_stack[i].id, call_stack[i].val);
                }
                else {
                    printf("%5d      [%s -> 0x%04X: ",i,
                            call_stack[i].id, call_stack[i].val);
                }
                print_data_inline(&memory[call_stack[i].val], stdout);
                printf("]\n");

            }
        }
    }
    printf("===============\n");
}

void push_arg(data t, int line) {
    write_memory(arg_pointer, t);
    arg_pointer--;
    check_memory(line);
}

data* top_arg(int line) {
    if (arg_pointer != MEMORY_SIZE - 1) {
        return &memory[arg_pointer + 1];
    }
    error_runtime(line, MEMORY_STACK_UNDERFLOW);
    return 0;
}

data pop_arg(int line) {
    if (arg_pointer != MEMORY_SIZE - 1) {
        data ret = copy_data(memory[++arg_pointer]);
        return ret;
    }
    error_runtime(line, MEMORY_STACK_UNDERFLOW);
    return none_data();
}

address push_memory_array(data* a, int size, int line) {
    address loc = pls_give_memory(size, line);
    for (int i = 0; i < size; i++) {
        write_memory(loc + i, a[i]);
    }
    check_memory(line);
    return loc;
}

address push_memory_s(data t, int size, int line) {
    address loc = pls_give_memory(size + 1, line);
    write_memory(loc, list_header_data(size));
    for (int i = 0; i < size; i++) {
        write_memory(loc + i + 1, t);
    }
    check_memory(line);
    return loc;
}

address push_memory_a(data* a, int size, int line) {
    address loc = pls_give_memory(size + 1, line);
    write_memory(loc, list_header_data(size));
    for (int i = 0; i < size; i++) {
        write_memory(loc + i + 1, a[i]);
    }
    check_memory(line);
    return loc;
}
address push_memory(data t, int line) {
    address loc = pls_give_memory(1, line);
    write_memory(loc, t);
    check_memory(line);
    return loc;
}

void replace_memory(data t, address a, int line) {
    if (a < MEMORY_SIZE) {
        write_memory(a, t);
    }
    else {
        error_runtime(line, MEMORY_REF_ERROR);
    }
}

void copy_stack_entry(stack_entry se, int line) {
    call_stack[stack_pointer] = se;
    call_stack[stack_pointer].is_closure = true;
    stack_pointer++;
    if (frame_pointer == 0) { main_end_pointer = stack_pointer; }
    check_memory(line);
}

void push_stack_entry(char* id, address val, int line) {
    stack_entry new_entry;
    new_entry.val = val;
    new_entry.is_closure = false;
    strncpy(new_entry.id, id, MAX_IDENTIFIER_LEN);
    new_entry.id[MAX_IDENTIFIER_LEN] = 0; // null term
    call_stack[stack_pointer++] = new_entry;
    if (frame_pointer == 0) {
        // currently in main function
        main_end_pointer = stack_pointer;
    }
    check_memory(line);
}

address get_fn_frame_ptr() {
    address trace = frame_pointer;
    while (call_stack[trace].id[0] != FUNCTION_START_CHAR) {
        trace = call_stack[trace].val;
    }
    return trace;
}

bool id_exist(char* id, bool search_main) {
    // avoid the location at frame_pointer because that's the start and stores
    for (int i = get_fn_frame_ptr() + 1; i < stack_pointer; i++) {
        if (streq(id, call_stack[i].id)) {
            return true;
        }
    }
    if (search_main) {
        for (int i = 0; i < main_end_pointer; i++) {
            if (streq(id, call_stack[i].id)) {
                return true;
            }
        }
    }
    return false;
}

address get_address_of_id(char* id, int line) {
    if (!id_exist(id, true)) {
        error_runtime(line, MEMORY_ID_NOT_FOUND, id);
        return 0;
    }
    address frame_ptr = get_fn_frame_ptr();
    for (int i = stack_pointer - 1; i > frame_ptr; i--) {
    //for (int i = get_fn_frame_ptr() + 1; i < stack_pointer; i++) {
        if (streq(id, call_stack[i].id)) {
            return call_stack[i].val;
        }
    }
    for (int i = 0; i < main_end_pointer; i++) {
        if (streq(id, call_stack[i].id)) {
            return call_stack[i].val;
        }
    }
    return 0;
}

address get_stack_pos_of_id(char* id, int line) {
    if (!id_exist(id, true)) {
        error_runtime(line, MEMORY_ID_NOT_FOUND, id);
        return 0;
    }
    address frame_ptr = get_fn_frame_ptr();
    for (int i = stack_pointer - 1; i > frame_ptr; i--) {
    //for (int i = get_fn_frame_ptr() + 1; i < stack_pointer; i++) {
        if (streq(id, call_stack[i].id)) {
            return i;
        }
    }
    for (int i = 0; i < main_end_pointer; i++) {
        if (streq(id, call_stack[i].id)) {
            return i;
        }
    }
    return 0;
}

data* get_value_of_id(char* id, int line) {
    return get_value_of_address(get_address_of_id(id, line), line);
}

data* get_value_of_address(address a, int line) {
    if (a < MEMORY_SIZE) {
        return &memory[a];
    }
    else {
        error_runtime(line, MEMORY_REF_ERROR);
        return &memory[0];
    }
}
void push_mem_reg(address memory_register) {
    mem_reg_stack[mem_reg_pointer++] = memory_register;
}

address pop_mem_reg() {
    if (mem_reg_pointer == 0) {
        w_error(MEMORY_MEM_STACK_ERROR);
        return 0;
    }
    return mem_reg_stack[--mem_reg_pointer];
}
