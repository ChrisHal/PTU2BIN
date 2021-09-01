#pragma once
#include <istream>
#include <array>
#include <cstdint>
#include <ctime>

constexpr auto Measurement_SubModes =
	std::array{ "Point(0)", "Point(1)", "Line", "Image" };

class PTUFileHeader
{
public:
	int64_t measurement_mode, measurement_submode,
		num_records, record_type, 
		dimensions, sin_correction, pix_x, pix_y,
		trg_frame, trg_linestart, trg_linestop;
	double Resolution, // resolution for Dtime
		GlobRes, // resolution for global timer
		PixResol;
	bool is_bidirect;
	time_t filedate;

	PTUFileHeader() : measurement_mode{ -1 }, measurement_submode{ -1 },
		num_records{ -1 }, record_type{ -1 }, dimensions{ -1 },
		sin_correction{ 0 }, pix_x{ -1 }, pix_y{ -1 },
		trg_frame{ -1 }, trg_linestart{ -1 }, trg_linestop{ -1 },
		Resolution{}, GlobRes{}, PixResol{}, is_bidirect{ false }, filedate{} {};
	bool ProcessFile(std::istream& infile);
	bool allNeededPresent();
};

