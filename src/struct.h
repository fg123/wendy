#ifndef STRUCT_H
#define STRUCT_H

#include "data.h"
#include "memory.h"
#include "vm.h"

/**
 *  Helper functions to help manage structures in the wendy vm memory layout. 
 */

// Returns a pointer to where the field is stored
//   ref:    a STRUCT_INSTANCE or STRUCT
//   member: the member to get
struct data* struct_get_field(struct vm* vm, struct data ref, const char* member);

struct data* struct_create_instance(struct vm* vm, struct data* metadata);

#endif