#pragma once
#include <istream>
#include <cstdint>
#include <ctime>

class PTUFileHeader
{
public:
	int64_t num_records, record_type, pix_x, pix_y,
		trg_frame, trg_linestart, trg_linestop;
	double Resolution, // resolution for Dtime
		GlobRes, // resolution for global timer
		PixResol;
	time_t filedate;

	PTUFileHeader() : num_records{ -1 }, record_type{ -1 }, pix_x{ -1 }, pix_y{ -1 },
		trg_frame{ -1 }, trg_linestart{ -1 }, trg_linestop{ -1 },
		Resolution{}, GlobRes{}, PixResol{}, filedate{} {};
	bool ProcessFile(std::istream& infile);
	bool allNeededPresent();
};

