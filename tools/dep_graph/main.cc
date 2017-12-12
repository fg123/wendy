/* 
 * Dependency List Generator
 * By: Felix Guo
 * Given a list of file stems to be compiled, we grab dependencies from each, and
 *   generate a list order to build.
 *
 * eg: ./prog file1 file2 file3
 *   This will look for file1.d, file2.d, file3.d, etc
 */

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include <unordered_set>

using namespace std;

int main(int argc, char* argv[]) {
	map<string, unordered_set<string>> files;
	vector<string> nodepFiles;
	vector<string> result;
	for (int i = 1; i < argc; i++) {
		string filename(argv[i]);
		ifstream file(filename + ".d");
		string line;
		int dependencyCount = 0;
		while (getline(file, line)) {
			// Each dependency is also just the stem
			files[filename].insert(line);
			dependencyCount++;
		}
		if (dependencyCount == 0) {
			nodepFiles.push_back(filename);
		}
		file.close();
	}
	while (!nodepFiles.empty()) {
		string stem = nodepFiles.back();
		nodepFiles.pop_back();
		result.push_back(stem);
		auto file = files.begin();
		while (file != files.end()) {
			file->second.erase(stem);
			if (file->second.empty()) {
				nodepFiles.push_back(file->first);
				file = files.erase(file);
			}
			else {
				file++;
			}
		}
	}
	for (auto &r : result) {
		cout << r << endl;
	}
	return 0;
}

