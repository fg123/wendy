#include "struct.h"
#include "error.h"

// A struct is a list:
//   Header 
//   Name
//   D_TABLE -> maps shared param to data
//   D_TABLE -> maps instance param to offset
//   D_STRUCT -> parent struct
struct data* struct_find_static(struct data* metadata, const char* member) {
    struct table* static_table = (struct table*) metadata[2].value.reference[0].value.reference;
	// printf("Static Table\n");
    // table_print(stdout, static_table, "%s: ", "");
    if (table_exist(static_table, member)) {
        return table_find(static_table, member);
    }
    return NULL;
}

bool struct_find_instance_offset(struct data* metadata, const char* member, size_t* offset) {
    struct table* instance_table = (struct table*) metadata[3].value.reference[0].value.reference;
    // printf("Instance Table\n");
    // table_print(stdout, instance_table, "%s: ", "");
    if (table_exist(instance_table, member)) {
        struct data* location = table_find(instance_table, member);
        *offset = (size_t)location->value.number;
        return true;
    }
    return false;
}

size_t struct_get_instance_table_size(struct data* metadata) {
    struct table* instance_table = (struct table*) metadata[3].value.reference[0].value.reference;
    return table_size(instance_table);
}

struct data* struct_get_field(struct vm* vm, struct data ref, const char* member) {
    wendy_assert((ref.type == D_STRUCT || ref.type == D_STRUCT_INSTANCE), "struct_get_field but not struct or struct_instance");
    struct data* metadata = ref.value.reference;
    if (ref.type == D_STRUCT_INSTANCE) {
        // metadata actually points to the STRUCT_INSTANCE_HEADER
        //   right now, we need one below that for the metadata
        metadata = metadata[1].value.reference;
        
        if (streq(member, "__super_init__")) {
            // Find parent init
            if (metadata[4].type == D_STRUCT) {
                struct data* parent_metadata = metadata[4].value.reference;
                struct table* parent_static_table = (struct table*) parent_metadata[2].value.reference[0].value.reference;
                wendy_assert(table_exist(parent_static_table, "init"), "parent struct static table has no init!");
                return table_find(parent_static_table, "init");
            }
            else {
                error_runtime(vm->memory, vm->line, "Structure has no parent!");
                return NULL;
            }
        }
        else if (streq(member, "super")) {
            // Find parent init
            if (metadata[4].type == D_STRUCT) {
                return &metadata[4];
            }
            else {
                error_runtime(vm->memory, vm->line, "Structure has no parent!");
                return NULL;
            }
        }
    }
    enum data_type struct_type = ref.type;

    // Try find static, metadata[2] points to static D_TABLE
    wendy_assert(metadata[2].type == D_TABLE, "not a table!");
    
    struct data* possible_static = struct_find_static(metadata, member);
    struct data* curr_meta = metadata;
    while (!possible_static) {
        // Try parent if exists
        if (curr_meta[4].type == D_NONE) {
            break;
        }
        wendy_assert(curr_meta[4].type == D_STRUCT, "parent of struct is not a D_STRUCT");
        curr_meta = curr_meta[4].value.reference;
        possible_static = struct_find_static(curr_meta, member);
    }

    if (possible_static) return possible_static;

    if (struct_type == D_STRUCT_INSTANCE) {
        size_t offset = 2;

        size_t out_offset = 0;
        bool possible_offset = struct_find_instance_offset(metadata, member, &out_offset);
        struct data* curr_meta = metadata;
        while (!possible_offset) {
            offset += struct_get_instance_table_size(curr_meta);;
            
            // Try parent if exists
            if (curr_meta[4].type == D_NONE) {
                break;
            }
            wendy_assert(curr_meta[4].type == D_STRUCT, "parent of struct is not a D_STRUCT");
            curr_meta = curr_meta[4].value.reference;
            possible_offset = struct_find_instance_offset(curr_meta, member, &out_offset);
        }
        if (possible_offset) {
        	return &ref.value.reference[offset + out_offset];
        }
    }

    return NULL;
}

struct data* struct_create_instance(struct vm* vm, struct data* metadata) {
    // Find how many STRUCT_PARAMs there are by traversing up the parent tree

    struct data* curr_meta = metadata;
    size_t params = 0;

    while (curr_meta) {
        wendy_assert(curr_meta[3].type == D_TABLE, "not a table!");
        struct table* instance_table = (struct table*) curr_meta[3].value.reference[0].value.reference;
        params += table_size(instance_table);
        // Go to parent
        if (curr_meta[4].type == D_NONE) {
            break;
        }
        wendy_assert(curr_meta[4].type == D_STRUCT, "parent of struct is not a D_STRUCT");
        curr_meta = curr_meta[4].value.reference;
    }

    // +1 for the header, +1 for the metadata pointer
    struct data* struct_instance = refcnt_malloc(vm->memory, params + 2);
    struct_instance[0] = make_data(D_STRUCT_INSTANCE_HEADER, data_value_num(params + 1));
    struct_instance[1] = make_data(D_STRUCT_METADATA, data_value_ptr(refcnt_copy(metadata)));

    for (size_t i = 0; i < params; i++) {
        struct_instance[i + 2] = none_data();
    }
    return struct_instance;
}
