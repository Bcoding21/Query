/*
	Searches for documents that contain user submitted key words using a pre build index and dictionaries.
	Dictionaries and index are loaded upon execution of the program on separate threads.
	Two dictionaries are used for mapping terms and documents to unique ids.
	The index maps a unique term id to a list of unique document ids.
	All list of document ids is sorted.
*/

#include <algorithm>
#include <unordered_map>
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

/*
Reads from file to create a dictionary that maps terms to term ids

Paramater 1: String representing path of file to read from
Paramater 2: Promise that saves the instance of an unordered_map.
*/
void  readTermDict(const std::string&, std::promise<std::unordered_map<std::string, unsigned long>>&&);

/*
Reads from file to create a dictionary that maps documents to document ids

Paramater 1: String representing path of file to read from
Paramater 2: Promise that saves the instance an unordered_map.
*/
void  readDocDict(const std::string&, std::promise<std::unordered_map<unsigned long, std::string>>&&);

/*
Reads from file to create a dictionary that maps term ids to a list of document ids

Paramater 1: String representing path of file to read from
*/
std::unordered_map<unsigned long, std::vector<unsigned long>> readIndex(const std::string&);

/*
	Combines common ids from multiple list into one list

	Param 1 - Vector of vectors that represent multiple list of document ids
	Returns - vector of document ids
*/
std::vector<unsigned long> mergeDocIdList(const std::vector<std::vector<unsigned long>>& docIdLists);

/*
creates new list of common values from two other list.

Param 1 - vector of unsigned long integers
Param 2 - vector of unsigned long integers
Returns - vector of unsigned long integers that occured in both vectors.
*/
std::vector<unsigned long> getIntersection(const std::vector<unsigned long>&, const std::vector<unsigned long>&);

/*
	Transforms all character in a string to lowercase

	Param - String
*/
void transformLower(std::string&);

/*
Retrieves each term's coressponding list of document ids
Param 1 - Vector of strings representing each term in a query
Param 2 - Unordered Map that maps a term to a term id
Param 3 - Unordered Map that maps a term id to a list of document ids
Returns - Vector of vectors that represent each list of doucment ids
for each term.
*/
std::vector<std::vector<unsigned long>> getDocIdLists(std::vector<std::string> termList,
	const std::unordered_map<std::string, unsigned long>& termDict,
	const std::unordered_map<unsigned long, std::vector<unsigned long>>& index);

/*
returns the terms that form a query in a vector

Param - String that represents a query
Return - Vector of strings that represent each word in the query
*/
std::vector<std::string> getTermList(const std::string& query);


void printResults(const std::vector<unsigned long>& results,
	const std::unordered_map<unsigned long, std::string>& docDict);

int main(char* argc, char* argv[]) {

	// load dictionaries/index on separate threads to increase performance

	std::promise <std::unordered_map<std::string, unsigned long>> p1;
	std::promise <std::unordered_map<unsigned long, std::string>> p2;

	auto f1 = p1.get_future();
	auto f2 = p2.get_future();

	std::thread t1(&readTermDict, "termDict.bin", std::move(p1));
	std::thread t2(&readDocDict, "docDict.bin", std::move(p2)); 

	auto index = readIndex("index.bin");

	t1.join();
	t2.join();

	auto termDict = f1.get();
	auto docDict = f2.get();

	std::string query = "stanford education";
	//std::cout << "Enter query here: ";
	//std::getline(std::cin, query);
	
	transformLower(query);
	auto termList = getTermList(query);
	auto docIdLists = getDocIdLists(termList, termDict, index);
	auto results = mergeDocIdList(docIdLists);


	std::cin.ignore();
}

void readTermDict(const std::string& path,
	std::promise< std::unordered_map<std::string, unsigned long>>&& promise) {

	std::ifstream file(path, std::ifstream::binary);

	if (!file) {
		 promise.set_value(std::unordered_map<std::string, unsigned long>());
		 return;
	}

	unsigned int indexSize = 0;

	file.read(reinterpret_cast<char*>(&indexSize), sizeof(indexSize));

	std::unordered_map<std::string, unsigned long> index;

	index.reserve(indexSize);

	for (int i = 0; i < indexSize; i++) {

		unsigned long termId = 0;

		file.read(reinterpret_cast<char*>(&termId), sizeof(termId));

		short stringSize = 0;

		file.read(reinterpret_cast<char*>(&stringSize), sizeof(stringSize));

		char* cString = new char[stringSize];

		file.read(cString, stringSize);

		std::string term(cString, stringSize);

		index.emplace(term, termId);
	}
	
	promise.set_value(index);

}

void readDocDict(const std::string& path,
	std::promise<std::unordered_map<unsigned long, std::string >>&& promise) {

	std::ifstream file(path, std::ifstream::binary);

	if (!file) {
		promise.set_value(std::unordered_map<unsigned long, std::string>());
		return;
	}

	unsigned int indexSize = 0;

	file.read(reinterpret_cast<char*>(&indexSize), sizeof(indexSize));

	std::unordered_map<unsigned long, std::string> index;

	index.reserve(indexSize);

	for (int i = 0; i < indexSize; i++) {

		unsigned long docId = 0;

		file.read(reinterpret_cast<char*>(&docId), sizeof(docId));

		unsigned short stringSize = 0;

		file.read(reinterpret_cast<char*>(&stringSize), sizeof(stringSize));

		char* cString = new char[stringSize];

		file.read(cString, stringSize);

		std::string doc(cString, stringSize);

		index.emplace(docId, doc);
	}

	promise.set_value(index);
}

std::unordered_map<unsigned long, std::vector<unsigned long>> readIndex(const std::string& path) {

	std::ifstream file(path, std::ifstream::binary | std::ifstream::ate); // open file with file pointer pointing to end

	if (!file) {
		return std::unordered_map<unsigned long, std::vector<unsigned long>>();
	}

	short sizeOfInt = sizeof(int);

	file.seekg(-sizeOfInt, std::ios::end); // point to start of last 4 bytes 

	int indexSize = 0;

	file.read(reinterpret_cast<char*>(&indexSize), sizeof(indexSize));

	file.clear(); // remove signal that prevents further reads.

	file.seekg(0); // set file pointer to beginning of file.

	std::unordered_map<unsigned long, std::vector<unsigned long>> index;

	index.reserve(indexSize);

	for (int i = 0; i < indexSize; i++) {

		unsigned long termId = 0;

		file.read(reinterpret_cast<char*>(&termId), sizeof(termId));

		unsigned int docListSize = 0;

		file.read(reinterpret_cast<char*>(&docListSize), sizeof(docListSize));

		std::vector<unsigned long> docIdList(docListSize);

		file.read(reinterpret_cast<char*>(docIdList.data()), sizeof(docIdList[0]) * docListSize);

		index.emplace(termId, docIdList);
	}

	return index;
}

void transformLower(std::string& word) {
	std::transform(
		word.begin(), word.end(),
		word.begin(), [](char letter)
	{ return tolower(letter); });
}

std::vector<std::string> getTermList(const std::string& query) {
	std::istringstream ss(query);
	std::istream_iterator<std::string> start(ss), end;
	return std::vector<std::string>(start, end);
}

std::vector<unsigned long> getIntersection(const std::vector<unsigned long>& first, const std::vector<unsigned long>& second) {

	std::vector<unsigned long> res;

	res.reserve(
		std::min(first.size(), second.size())
	);

	std::set_intersection(
		first.begin(), first.end(),
		second.begin(), second.end(),
		std::back_inserter(res)
	);

	return res;
}


std::vector<std::vector<unsigned long>> getDocIdLists(std::vector<std::string> termList,
	const std::unordered_map<std::string, unsigned long>& termDict,
	const std::unordered_map<unsigned long, std::vector<unsigned long>>& index ) {

	std::vector<std::vector<unsigned long>> docIdLists;

	for (auto term : termList) {

		auto termSeach = termDict.find(term);

		if (termSeach != termDict.end()) {
			
			auto indexSearch = index.find(termSeach->second);

			docIdLists.push_back(indexSearch->second);
		}
	}

	return docIdLists;

}

std::vector<unsigned long> mergeDocIdList(const std::vector<std::vector<unsigned long>>& docIdLists) {

	std::vector<unsigned long> res(docIdLists.at(0));

	for (int i = 1; i < docIdLists.size(); i++) {
		res = getIntersection(res, docIdLists[i]);
	}

	return res;
}

void printResults(const std::vector<unsigned long>& results, 
	const std::unordered_map<unsigned long, std::string>& docDict) {

	std::transform(
		results.begin(), results.end(),
		std::ostream_iterator<std::string>(std::cout, "\n"),
		[&docDict](unsigned long docId) {
		return docDict.at(docId); }
	);
}