/*
 * wendy.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides internal WendyVM functions
 */

struct Wendy => [getRefs, getAt];

Wendy.getRefs => (ref) native vm_getRefs;
Wendy.getAt => (ref, index) native vm_getAt;
