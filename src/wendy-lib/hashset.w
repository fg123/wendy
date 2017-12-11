/* 
 * hashset.w: WendyScript 2.0
 * Created by Felix Guo
 */
import data;
struct hashset => (buckets, mod) [add, remove, exist, print];
hashset.init => (mod) {
  this.buckets = [[]] * mod
  this.mod = mod
  ret this
}
hashset.add => (item)
  if !this.exist(item)
    this.buckets[hash(item) % this.mod] += item

hashset.remove => (item) {
  let b = hash(item) % this.mod
  let s = this.buckets[b].size
  for i in 0->s
    if this.buckets[b][i] == item
      this.buckets[b] = this.buckets[b][0->i] + this.buckets[b][(i+1)->s];
}
hashset.exist => (i) i ~ this.buckets[hash(i) % this.mod]

hashset.print => () { "Hashmap with " + this.mod + " buckets: " this.buckets }
