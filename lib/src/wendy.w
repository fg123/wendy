/*
 * wendy.w: WendyScript 2.0
 * Created by Felix Guo
 * Provides internal WendyVM functions
 */

struct Wendy => [getRefs];

Wendy.getRefs => (ref) native vm_getRefs;
