/* 
 * hashmap.w: WendyScript 2.0
 * Created by Felix Guo
 */
import data
struct hashmap => (buckets, mod) [add, remove, exist, print];
hashmap.init => (mod) {
  this.buckets = [[]] * mod
  this.mod = mod
  ret this
}
hashmap.add => (item)
  if !this.exist(item)
    this.buckets[hash(item) % this.mod] += item

hashmap.remove => (item) {
  let b = hash(item) % this.mod
  let s = this.buckets[b].size
  for i in 0->s
    if this.buckets[b][i] == item
      this.buckets[b] = this.buckets[b][0->i] + this.buckets[b][(i+1)->s];
}
hashmap.exist => (i) i ~ this.buckets[hash(i) % this.mod]

hashmap.print => () { "Hashmap with " + this.mod + " buckets: " this.buckets }