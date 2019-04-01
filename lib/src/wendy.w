/*
 * wendy.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides internal WendyVM functions
 */

struct Wendy => [getRefs, getAt];

Wendy.getRefs => (ref) native vm_getRefs;
Wendy.getAt => (ref, index) native vm_getAt;

struct Enum => [serialize, deserialize];
Enum.serialize => (enum_val) enum_val._num

// The first enum member is at 6
Enum.deserialize => (enum_struct, serialized)
    Wendy.getAt(enum_struct, 6 + (2 * serialized));
