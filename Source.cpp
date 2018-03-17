#include <algorithm>
#include <unordered_map>
#include <set>
#include <fstream>
#include <vector>
#include <iostream>
#include <thread>
#include <string>
#include <sstream>
#include <iterator>
#include <string>
#include <execution>
#include <future>

void  readWordIndex(const std::string&, std::promise<std::unordered_map<std::string, unsigned long>>&&);

void  readLinkIndex(const std::string&, std::promise<std::unordered_map<unsigned long, std::string>>&&);

std::unordered_map<unsigned long, std::vector<unsigned long>> readWordLinkIndex(const std::string&);

std::vector<unsigned long> getIntersection(const std::vector<unsigned long>&, const std::vector<unsigned long>&);

std::unordered_map<unsigned long, std::string> reverseIndex(const std::unordered_map<std::string, unsigned long>&);

int main(char* argc, char* argv[]) {

	std::promise<std::unordered_map<std::string, unsigned long>> p1;

	std::promise <std::unordered_map<unsigned long, std::string>> p2;

	auto f1 = p1.get_future();

	auto f2 = p2.get_future();

	std::thread t1(&readWordIndex, "wordIndex.bin", std::move(p1));

	std::thread t2(&readLinkIndex, "docIndex.bin", std::move(p2));

	auto wordDocIndex = readWordLinkIndex("wordDocIndex.bin");

	t1.join();
	t2.join();

	auto wordIndex = f1.get();

	auto docIndex = f2.get();
	
	std::string query = "stanford education";

	//while (true) {

		/*std::cout << "Enter query here: ";
		std::getline(std::cin, query);*/

		std::transform ( // query make lower case
			query.begin(), query.end(),
			query.begin(), [](char letter)
			{ return tolower(letter); });

		std::istringstream ss(query);
		std::istream_iterator<std::string> start(ss), end;
		std::vector<std::string> words(start, end); // separate into words

		auto wordSearch = wordIndex.find(words[0]);

		if (wordSearch == wordIndex.end()) {
			std::cout << words[0] << " not in any docs\n\n";
			//continue;
		}

		auto searchResults = wordDocIndex[wordSearch->second];

		for (int i = 1; i < words.size(); ++i) {

			auto wordID = wordIndex.find(words[i])->second;

			auto linkIDs = wordDocIndex.find(wordID)->second;

			searchResults = getIntersection(searchResults, linkIDs);

		}
		if (searchResults.empty()) {
			std::cout << "No results\n\n";
			//continue;
		}

		std::transform( // PRINT
			searchResults.begin(), searchResults.end(),
			std::ostream_iterator<std::string>(std::cout, "\n"),
			[&docIndex](unsigned long docID) {
			return docIndex[docID]; }
		);

		std::cout << '\n';
	//}

		std::cin.ignore();
}

void readWordIndex(const std::string& filePath,
	std::promise<std::unordered_map<std::string, unsigned long>>&& p) {

	std::ifstream in(filePath, std::ifstream::binary);

	if (!in) {
		 p.set_value(std::unordered_map<std::string, unsigned long>());
		 return;
	}

	unsigned int indexSize = 0;

	in.read(reinterpret_cast<char*>(&indexSize), sizeof(indexSize));

	std::unordered_map<std::string, unsigned long> index;

	index.reserve(indexSize);

	for (int i = 0; i < indexSize; i++) {

		unsigned long ID = 0;

		in.read(reinterpret_cast<char*>(&ID), sizeof(ID));

		short stringSize = 0;

		in.read(reinterpret_cast<char*>(&stringSize), sizeof(stringSize));

		char* cString = new char[stringSize];

		in.read(cString, stringSize);

		std::string str(cString, stringSize);

		index.emplace(str, ID);
	}
	
	p.set_value(index);

}

void readLinkIndex(const std::string& filePath,
	std::promise<std::unordered_map<unsigned long, std::string >>&& p) {

	std::ifstream in(filePath, std::ifstream::binary);

	if (!in) {
		p.set_value(std::unordered_map<unsigned long, std::string>());
		return;
	}

	unsigned int indexSize = 0;

	in.read(reinterpret_cast<char*>(&indexSize), sizeof(indexSize));

	std::unordered_map<unsigned long, std::string> index;

	index.reserve(indexSize);

	for (int i = 0; i < indexSize; i++) {

		unsigned long ID = 0;

		in.read(reinterpret_cast<char*>(&ID), sizeof(ID));

		short stringSize = 0;

		in.read(reinterpret_cast<char*>(&stringSize), sizeof(stringSize));

		char* cString = new char[stringSize];

		in.read(cString, stringSize);

		std::string link(cString, stringSize);

		index.emplace(ID, link);
	}

	p.set_value(index);
}

std::unordered_map<unsigned long, std::vector<unsigned long>> readWordLinkIndex(const std::string& filePath) {

	std::ifstream in("wordDocIndex.bin", std::ifstream::binary | std::ifstream::ate); // open file with pointer to end

	if (!in) {
		return std::unordered_map<unsigned long, std::vector<unsigned long>>();
	}

	short sizeOfInt = sizeof(int);

	in.seekg(-sizeOfInt, std::ios::end); // point to start of last 4 bytes 

	int indexSize = 0;

	in.read(reinterpret_cast<char*>(&indexSize), sizeof(indexSize));

	in.clear();

	in.seekg(0);

	std::unordered_map<unsigned long, std::vector<unsigned long>> index;

	index.reserve(sizeOfInt);

	for (int i = 0; i < indexSize; i++) {

		unsigned long wordID = 0;

		in.read(reinterpret_cast<char*>(&wordID), sizeof(wordID));

		int listSize = 0;

		in.read(reinterpret_cast<char*>(&listSize), sizeof(listSize));

		std::vector<unsigned long> docIDs(listSize);

		in.read(reinterpret_cast<char*>(&docIDs[0]), sizeof(long) * listSize);

		index.emplace(wordID, docIDs);
	}

	return index;
}

std::vector<unsigned long> getIntersection(const std::vector<unsigned long>& first, const std::vector<unsigned long>& second) {

	std::vector<unsigned long> intersection;

	intersection.reserve(
		std::min(first.size(), second.size())
	);

	std::set_intersection(
		first.begin(), first.end(),
		second.begin(), second.end(),
		std::back_inserter(intersection)
	);

	return intersection;

}

std::unordered_map<unsigned long, std::string> reverseIndex(const std::unordered_map<std::string, unsigned long>& index) {

	std::unordered_map<unsigned long, std::string> reverse;
	reverse.reserve(index.size());

	for (const auto& pair : index) { reverse.emplace(pair.second, pair.first); }

	return reverse;

}