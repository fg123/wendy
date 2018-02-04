// Simple Distance Based Djikstras
// - Felix Guo
import math;

let width;
let height;
@"Width: ";
input width;
@"Height: ";
input height;

let grid = [];
for i in 0->height
		grid += [([0] * width)];

struct posn => (x, y);
struct node => (posn, distance, parent);
let <posn> == <posn> => (lhs, rhs) (lhs.x == rhs.x and lhs.y == rhs.y);
let <posn> != <posn> => (lhs, rhs) !(lhs == rhs);
let <posn> == <node> => (lhs, rhs) lhs == rhs.posn;
let <node> == <posn> => (lhs, rhs) lhs.posn == rhs;
let <node> == <node> => (lhs, rhs) lhs.posn == rhs.posn;

let contains => (list, element) {
		for v in list {
				if element == v
						ret true;
		};
		ret false;
};

let lowestNode => (list) {
		let lowestElem = list[0];
		let i = 0;
		for v in 1->list.size
				if (list[v].distance < lowestElem.distance) {
						lowestElem = list[v];
						i = v;
				};
		ret [lowestElem, i];
};

let start = posn(0, 0);
let end = posn(width - 1, height - 1);
let path = [];

let distance => (p1, p2) sqrt(pow(p1.x - p2.x, 2) + pow(p1.y - p2.y, 2));
let isClear => (p) grid[p.y][p.x] == 0;

let getNeighbours => (n, visited) {
		let neighbours = [];
		let p = n.posn;
		if (p.x > 0) {
				let d = posn(p.x - 1, p.y);
				if (isClear(d) and !contains(visited, d))
						neighbours += node(d, distance(d, end), n);
		}
		if (p.y > 0) {
				let d = posn(p.x, p.y - 1);
				if (isClear(d) and !contains(visited, d))
						neighbours += node(d, distance(d, end), n);
		}
		if (p.x < width - 1) {
				let d = posn(p.x + 1, p.y);
				if (isClear(d) and !contains(visited, d))
						neighbours += node(d, distance(d, end), n);
		}
		if (p.y < height - 1) {
				let d = posn(p.x, p.y + 1);
				if (isClear(d) and !contains(visited, d))
						neighbours += node(d, distance(d, end), n);
		}
		ret neighbours;
};

let pathfind => () {
		let visited = [];
		let search = [node(start, distance(start, end), start)];
		for (search.size > 0 and !contains(search, end)) {
				let curr = lowestNode(search);
				visited += curr[0];
				search = search[0->curr[1]] + search[(curr[1] + 1)->search.size];
				search += getNeighbours(curr[0], visited);
		};
		path = [];
		if (search.size > 0) {
				let curr = none;
				for node in search if node.posn == end curr = node

				for (curr.posn != start) {
						path += curr.posn;
						curr = curr.parent;
				};
		};
};

let outputGrid => () {
		for y in 0->height {
				for x in 0->width
						if (start.x == x and start.y == y) {
								@"S ";
						}
						else if (end.x == x and end.y == y) {
								@"E ";
						}
						else if (contains(path, posn(x, y))) {
								@"* ";
						}
						else if (grid[y][x] == 0) {
								@"- ";
						}
						else {
								@"# ";
						};
				"";
		};
};

let getPoint => () {
		let p = posn(0, 0);
		@"X: ";
		input p.x;
		@"Y: ";
		input p.y;
		ret p;
};

let continue = true;
for continue {
		outputGrid();
		let command;
		"Toggle [B]lock, Set [S]tart, Set [E]nd, [F]ind, [C]lear Path, [Q]uit";
		@"Enter Command: ";
		input command;
		if (command == "Q") {
				continue = false;
		}
		else if (command == "B") {
				let p = getPoint();
				grid[p.y][p.x] = if (grid[p.y][p.x] == 0) 1 else 0;
		}
		else if (command == "S") {
				start = getPoint();
		}
		else if (command == "E") {
				end = getPoint();
		}
		else if (command == "C") {
				path = [];
		}
		else if (command == "F") {
				pathfind();
		};
};
