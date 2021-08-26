// (c) 2021 Christian R. Halaszovich
// (See LICENSE.txt for licensing information.)
// 

#pragma once
#include <iostream>
#include <cstdint>
#include <istream>

constexpr size_t BUFFSIZE = 1024;
class RecordBuffer
{
	std::istream& infile;
	uint32_t* buffer;
	size_t bufidx, bufnumelements, recordsremaining;

	void fillbuffer() {
		size_t numtoread = std::min(BUFFSIZE, recordsremaining);
		infile.read((char*)buffer, sizeof(uint32_t) * numtoread);
		recordsremaining -= numtoread;
		bufnumelements = numtoread;
		bufidx = 0;
		if (!infile.good()) {
			throw("Error while reading TTTR records from infile. Unexpected end of file.");
		}
	};
public:
	RecordBuffer(std::istream& InFile, size_t numrecords) : infile{ InFile }, buffer { new uint32_t[BUFFSIZE] },
		bufidx { 0 }, bufnumelements{ 0 }, recordsremaining{ numrecords } {};
	bool empty() { return bufidx == bufnumelements; };
	// return and remove top element:
	uint32_t pop() {
		if (empty()) {
			fillbuffer();
		}
		return buffer[bufidx++];
	}
	// return but not remove top element:
	uint32_t peek() {
		if (empty()) {
			fillbuffer();
		}
		return buffer[bufidx];
	}
};

