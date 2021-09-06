// (c) 2021 Christian R. Halaszovich
// (See LICENSE.txt for licensing information.)
// 

#pragma once
#include <stdexcept>
#include <cstdint>
#include <istream>
#include <memory>

constexpr size_t BUFFSIZE = 1024;
class RecordBuffer
{
	std::istream& infile;
	std::unique_ptr<uint32_t[]> buffer;
	size_t bufidx, bufnumelements, recordsremaining;

	void fillbuffer() {
		if (recordsremaining == 0) {
			throw std::range_error("no more data");
		}
		size_t numtoread = std::min(BUFFSIZE, recordsremaining);
		infile.read((char*)buffer.get(), sizeof(uint32_t) * numtoread);
		if (!infile.good()) {
			throw std::runtime_error("Error while reading TTTR records from infile. Unexpected end of file.");
		}
		recordsremaining -= numtoread;
		bufnumelements = numtoread;
		bufidx = 0;
	};
public:
	RecordBuffer(std::istream& InFile, size_t numrecords) : infile{ InFile }, buffer { new uint32_t[BUFFSIZE] },
		bufidx { 0 }, bufnumelements{ 0 }, recordsremaining{ numrecords } {};
	bool empty() const { return bufidx == bufnumelements; };
	bool noMoreData() const { return empty() && recordsremaining == 0; };
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
			if (recordsremaining == 0) {
				throw std::range_error("tried to peek past last record");
			}
			fillbuffer();
		}
		return buffer[bufidx];
	}
};

