// Tool to convert 3T PTU file from PicoQuant SymphoTime
// to PicoQuant BIN format (= pre-histogrammed data)
//
// (c) 2021 - 2024 Christian R. Halaszovich
// (See LICENSE.txt for licensing information.)
// 
// Some parts are based on demo code from PicoQuant
// (see their GitHub repo)
//

#define _USE_MATH_DEFINES
#include <cmath>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <iterator>
#include <limits>
#include <memory>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
//#include <numbers>
#ifndef __STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__
#endif
#include <ctime>
#include "cxxopts.hpp"
#include "PTUFileHeader.h"
#include "TTTRRecordProcessor.h"
#include "RecordBuffer.h"

#ifdef _WIN32
#include <io.h>
bool my_isatty()
{
	return _isatty(_fileno(stdout));
}
#else
#include <unistd.h>
bool my_isatty()
{
	return isatty(STDOUT_FILENO);
}
#endif

//#define	DOPERFORMANCEANALYSIS
#ifdef DOPERFORMANCEANALYSIS
#include <chrono>
#endif // DOPERFORMANCEANALYSIS

#pragma pack(8)

extern int ExportIBWFile(std::ostream& os, uint32_t* histogram, int64_t pix_x,
	int64_t pix_y, double res_space, double res_time, int64_t num_hist_channels,
	int64_t max_export_channel, const std::string& wavename, time_t filedate);

constexpr auto APP_NAME = "PTU2BIN", VERSION = "2.0";



struct BinHeader {
	uint32_t PixX, PixY;
	float PixResol;
	uint32_t TCSPCChannels;
	float TimeResol;
};

// write histogram data in BIN format
int ExportBinFile(std::ostream& os, uint32_t* histogram, int64_t pix_x, int64_t pix_y, double res_space, double res_time, int64_t num_hist_channels, int64_t max_used_channel)
{
	BinHeader bh{};
	bh.PixX = (uint32_t)pix_x;
	bh.PixY = (uint32_t)pix_y;
	bh.PixResol = (float)res_space;
	bh.TCSPCChannels = (uint32_t)max_used_channel;
	bh.TimeResol = (float)(res_time * 1e9); // in ns
	os.write((char*)& bh, sizeof(bh));
	if (!os.good()) {
		return 1;
	}
	for (int64_t y = 0; y < pix_y; ++y) {
		for (int64_t x = 0; x < pix_x; ++x) {
			os.write((char*)(histogram + y * pix_x * num_hist_channels + x * num_hist_channels), sizeof(uint32_t) * max_used_channel);
			if (!os.good()) {
				return 1;
			}
		}
	}
	return 0; // success
}

enum FRAME_TRIGGER_TYPE {
	FRAMETRG_UNKNOW = 0,
	FRAMETRG_AT_START = 1,
	FRAMETRG_AT_STOP = 2
};

// It seems that a certain number of lines should be skipped when the PTU
// file is processed. Here we define how many. In our system is 1 line.
// I do not know yet if this is universally true. Might be a bug in SymphoTime
// or be specific to our system (like misconfigured trigger).
// AnalyzeTriggers can automatically detect if and how many lines
// should be skipped and if frame trigger is valid and if it's at start or stop/end of frame
void AnalyzeTriggers(RecordBuffer& buffer, const TTTRRecordProcessor& processor, const PTUFileHeader& fh, int& frame_trg_type, int64_t& lines_to_skip)
{
	int64_t total_linestarts{}, total_linestops{};
		//total_frames{};
	unsigned int TrgLineStartMask = 1 << (fh.trg_linestart - 1), TrgLineStopMask = 1 << (fh.trg_linestop - 1),
		TrgFrameMask = 1 << (fh.trg_frame - 1);
	frame_trg_type = FRAMETRG_UNKNOW, lines_to_skip = 0;

	while (!buffer.noMoreData()) {
		auto record = buffer.pop();
		if (processor.isMarker(record)) {
			auto marker = processor.markers(record);
			if (!buffer.noMoreData()) {
				// test if next record is also a marker event
				auto next_record = buffer.peek();
				if (processor.isMarker(next_record)) {
					// we merge here independent of time to next trigger! Might cause problems.
					next_record = buffer.pop();
					marker |= processor.markers(next_record);

#ifndef NDEBUG
					if (processor.nsync(next_record) != processor.nsync(record))
					{
						auto DT = processor.nsync(next_record) - processor.nsync(record);
						std::cout << "WARNING: merge with DT = " << DT << " (" <<
							DT * fh.GlobRes << " s)" << std::endl;
					}
#endif // !NDEBUG
				}
			}
			//if (marker & TrgFrameMask) {
			//	++total_frames;
			//}
			if (marker & TrgLineStartMask) {
				++total_linestarts;
			}
			if (marker & TrgLineStopMask) {
				++total_linestops;
			}
			if (marker & TrgFrameMask){
				if (total_linestarts != total_linestops) {
					std::cout << "frame trigger out of sequence" << std::endl;
				}
				if (total_linestops == 0 && frame_trg_type != FRAMETRG_AT_START) {
					frame_trg_type = FRAMETRG_AT_START;
					lines_to_skip = 0;
#ifndef NDEBUG
					std::cout << "frame trigger at start, lines to skip " << lines_to_skip << std::endl;
#endif // !NDEBUG
					break;
				}
				if (frame_trg_type != FRAMETRG_AT_START && frame_trg_type!=FRAMETRG_AT_STOP) {
					frame_trg_type = FRAMETRG_AT_STOP;
					lines_to_skip = total_linestarts - fh.pix_y;
#ifndef NDEBUG
					std::cout << "frame trigger at end, lines to skip " << lines_to_skip << std::endl;
#endif // !NDEBUG
#ifdef NDEBUG
					break;
#endif // NDEBUG
				}
			}
		}
	}
	buffer.rewind();
}

void parse(int argc, char** argv, std::string& infile, std::string& outfile, int& channelofinterest,
	int64_t& first_frame, int64_t& last_frame, bool& ignore_frame_trigger, int64_t& lines_to_skip)
{
	try {
		cxxopts::Options options(APP_NAME, " - convert PTU to BIN or IBW");
		options.positional_help("<infile> <outfile> [<channel#>]").show_positional_help();
		options.add_options()
			("i,infile", "input file", cxxopts::value<std::string>(),"<infile>")
			("o,outfile", "output file (use suffix '.ibw' for IBW format)", cxxopts::value<std::string>(),"<outfile>")
			("c,channel","detectorchannel (<=0: all, default: 2)",cxxopts::value<int>(),"<channel#>")
			("f,first", "first frame (default 0)", cxxopts::value<int64_t>(),"<# 1st frame>")
			("l,last", "last frame (default: last in file)", cxxopts::value<int64_t>(), "<# last frame>")
			("ignore-frame-trigger", "set if frame trigger is unreliable")
			("lines-to-skip", "lines to skip at start of frame", cxxopts::value<int64_t>(), "<#>")
			("v,version", "print version")
			/*("positional",
				"Positional arguments: these are the arguments that are entered "
				"without an option", cxxopts::value<std::vector<std::string>>())*/
				("h,help", "print help");

		options.parse_positional({ "infile", "outfile", "channel"});

		auto result = options.parse(argc, argv);
		if (result.count("help"))
		{
			std::cout << options.help() << std::endl;
			exit(0);
		}
		if (result.count("version"))
		{
			std::cout << APP_NAME << " Version " << VERSION << std::endl;
			exit(0);
		}
		if (!result.count("infile") || !result.count("outfile")) {
			std::cerr << "input and/or output file not specified (use option -h for help)" << std::endl;
			exit(-1);
		}
		infile = result["infile"].as<std::string>();
		outfile = result["outfile"].as<std::string>();
		if (result.count("channel")) {
			channelofinterest = result["channel"].as<int>()-1;
		}
		if (result.count("first")) {
			first_frame = result["first"].as<int64_t>();
		}
		if (result.count("last")) {
			last_frame = result["last"].as<int64_t>();
		}
		ignore_frame_trigger = result.count("ignore-frame-trigger");
		if (result.count("lines-to-skip")) {
			lines_to_skip = result["lines-to-skip"].as<int64_t>();
			if (!ignore_frame_trigger) {
				std::wcout << "NOTICE: option 'lines-to-skip' has only an effect in combination with ignore-frame-trigger"
					<< std::endl;
			}
		}
	}
	catch (const cxxopts::exceptions::exception& e) {
		std::cout << "error parsing options: " << e.what() << std::endl;
		exit(-1);
	}
}

int main(int argc, char** argv)
{
	std::string infilename, outfilename;
	int channelofinterest = 1;
	int64_t first_frame = 0, last_frame = std::numeric_limits<int64_t>::max(), lines_to_skip = 0;
	bool ignore_frame_trigger{ false };
	parse(argc, argv, infilename, outfilename, channelofinterest, first_frame, last_frame,
		ignore_frame_trigger, lines_to_skip);
	// check if we are running from a terminal
#ifdef DOPERFORMANCEANALYSIS
	bool isterminal = false;
#else // DOPERFORMANCEANALYSIS
	bool isterminal = my_isatty();
#endif
	std::cout << "infile: " << infilename << "\noutfile: " << outfilename << std::endl;
	if (last_frame < first_frame) {
		std::cout << "WARNING: last frame < first frame, no frames will be processed"
			<< std::endl;
	}
	std::ifstream infile(infilename, std::ios::in | std::ios::binary);
	PTUFileHeader fh;
	TTTRRecordProcessor processor;
	if (!infile.good()) {
		std::cerr << "error opening infile" << std::endl;
		exit(EXIT_FAILURE);
	}
	if (!fh.ProcessFile(infile)) {
		std::cerr << "error processing file headers" << std::endl;
		exit(EXIT_FAILURE);
	}
	if (!infile.good()) {
		std::cerr << "error while reading file headers\n";
		exit(EXIT_FAILURE);
	}
	if (fh.measurement_submode != 3) {
		// NOTE: "Measurement_SubMode" is mandatory, so we assume it is set in PTU file 
		std::cerr << "ERROR: Submode " << Measurement_SubModes.at(fh.measurement_submode) <<
			" not supported. Must be 'Image'." << std::endl;
		exit(EXIT_FAILURE);
	}
	if (fh.dimensions != 3 && fh.dimensions != -1) {
		std::cerr << "ERROR: " << fh.dimensions << " dimensions not supported. Must be 3." << std::endl;
		exit(EXIT_FAILURE);
	}
	if (!fh.allNeededPresent()) {
		std::cerr << "ERROR: some data missing from PTU file header" << std::endl;
		exit(EXIT_FAILURE);
	}
	if (fh.sin_correction != 0) {
		std::cout << 
			"NOTE: ImgHdr_SinCorrection is " << fh.sin_correction << '%' << std::endl;
	}
	//
	// we are done checking the file header, now let's init processing
	if (!processor.init(fh)) {
		std::cerr << "Unexpected record type." << std::endl;
		exit(EXIT_FAILURE);
	}
	if (processor.isT2mode() || fh.measurement_mode != 3) {
		std::cerr << "Sorry, T2 mode not supported (working on it)." << std::endl;
		exit(EXIT_FAILURE);
	}
	constexpr double MAX_TRIGGER_DIFF_SEC = 120e-6;
	int max_trig_diff = 0;
	if (fh.GlobRes > 1e-9) {
		max_trig_diff = int(MAX_TRIGGER_DIFF_SEC / fh.GlobRes);
	}
	int num_useful_histo_ch = int(std::ceil(fh.GlobRes / fh.Resolution)) + 1; // TODO: check if ok for T2 data
	std::cout << "estimated number of useful histogram channels: " << num_useful_histo_ch << std::endl;
	std::cout << "total # records in file: " << fh.num_records << std::endl;
	if (channelofinterest >= 0) {
		std::cout << "Evaluating channel " << (channelofinterest + 1) << " only." << std::endl;
	}
	else
	{
		std::cout << "Evaluating all channels." << std::endl;
	}


	unsigned int TrgLineStartMask = 1 << (fh.trg_linestart - 1), TrgLineStopMask = 1 << (fh.trg_linestop - 1),
		TrgFrameMask = 1 << (fh.trg_frame - 1);
	bool isrecordingline = false, framehasstarted = false;

	// place for temporary storage of line data
	struct PixelTime {
		unsigned int dtime;
		int64_t pixeltime;
	};
	std::vector<PixelTime> pixeltimes;
	pixeltimes.reserve(32768); // Perf. test shows only small effect of this

	// space for histogramm data
	size_t max_hist_channels = std::max(512, num_useful_histo_ch); // number of histogramm channels, same as max Dtime?
	auto histogram = std::make_unique<uint32_t[]>(max_hist_channels * fh.pix_x * fh.pix_y);
	uint32_t maxDtime = 0; // max val in histogram

	int64_t lastlinestart = -1, lastlinestop = -1, lineduration = -1, linecounter = 0,
		totallines = 0,
		framecounter = 0, lastframetime = -1, linesprocessed = 0;
	int frame_trg_type = FRAMETRG_UNKNOW;
	int64_t frametrgcount = 0; // as a control we count the frame triggers
	double sin_corr_scale{};
	if (fh.sin_correction != 0) {
		sin_corr_scale = std::sin(M_PI * fh.sin_correction / 200.0);
	}

#ifdef DOPERFORMANCEANALYSIS
	auto start_time = std::chrono::steady_clock::now();
#endif
	// prepare input buffer
	RecordBuffer buffer(infile, fh.num_records);

	//////////////
	// start processing of records
	try {
		if (!ignore_frame_trigger) {
			AnalyzeTriggers(buffer, processor, fh, frame_trg_type, lines_to_skip);
		}
		linecounter = -lines_to_skip;
		if (frame_trg_type != FRAMETRG_AT_START) {
			framehasstarted = true;
		}
		for (int64_t recnum = 0; recnum < fh.num_records; ++recnum) {
			auto TTTRRecord = buffer.pop();
			if (processor.isSpecial(TTTRRecord))
			{
				if (processor.processOverflow(TTTRRecord)) //overflow
				{
					continue;
				}
				auto trigger = processor.markers(TTTRRecord);
				if (!buffer.noMoreData()) {
					// test if next record is also a marker event
					auto next_record = buffer.peek();
					if (processor.isMarker(next_record) &&
						(processor.nsync(next_record) - processor.nsync(TTTRRecord) <= max_trig_diff)) {
						next_record = buffer.pop();
						++recnum;
						trigger |= processor.markers(next_record); // merge marker events
#ifndef NDEBUG
						std::cout << "marker events merged" << std::endl;
#endif // !NDEBUG
					}
				}
				// for the time being, we assume that any special record that is not an overflow
				// is a marker record.
				//if ((channel >= 1) && (channel <= 15)) //This is true only for TimeHarp260 and similar!
				//{
				auto truensync = processor.truesync(TTTRRecord);
				if ((trigger & TrgFrameMask) && frame_trg_type == FRAMETRG_AT_START) {
					framehasstarted = true;
					lastframetime = truensync;
					//++frametrgcount;
					linecounter = 0; // this also signals that line should be processed
				}
				if (framehasstarted && (trigger & TrgLineStartMask)) {
					++totallines;
					if (linecounter >= 0) {
						isrecordingline = true;
						lastlinestart = truensync;
					}
					else {
						++linecounter;
					}
				}
				else if ((trigger & TrgLineStopMask) && isrecordingline) { // line ended
					isrecordingline = false;
					lastlinestop = truensync;
					lineduration = lastlinestop - lastlinestart;
					assert(linecounter < fh.pix_y);
					// process line data:
					if ((framecounter >= first_frame) && (framecounter <= last_frame) && (linecounter < fh.pix_y)) {
						++linesprocessed;
						uint32_t* lp = histogram.get() + linecounter * max_hist_channels * fh.pix_x;
						for (const auto& pt : pixeltimes) {
							int64_t x;
							if (fh.sin_correction == 0) {
								x = int64_t(pt.pixeltime) * fh.pix_x / lineduration;
							}
							else {
								// apply sinusoidal correction
								// I hope this is correct, since info from on this is scarce
								double t_n = 2.0 * pt.pixeltime / lineduration - 1.0;
								double phi = t_n * M_PI * fh.sin_correction / 200.0;
								x = int64_t((std::sin(phi) / sin_corr_scale + 1.0) * fh.pix_x / 2.0);
							}
							x = std::max(int64_t(0), std::min(x, fh.pix_x - 1));
							if (fh.is_bidirect && bool(linecounter & 1)) {
								x = fh.pix_x - 1 - x;
							}
							auto dt = pt.dtime;
							if (dt < max_hist_channels) {
								++lp[x * max_hist_channels + dt];
								maxDtime = std::max(dt, maxDtime);
							}
						}
					}
					pixeltimes.clear();
					++linecounter;
					if (linecounter == fh.pix_y) {
						++framecounter;

						// for unknown frame trigger we assume we are always recording
						if (frame_trg_type != FRAMETRG_UNKNOW) { framehasstarted = false; }
						linecounter = -lines_to_skip;  // skip lines if necessary (in fact, this will also be set if frame trigger got caught)
					}
				}
				if ((trigger & TrgFrameMask) && frame_trg_type == FRAMETRG_AT_STOP) {
					framehasstarted = true;
					lastframetime = truensync;
					linecounter = -lines_to_skip;
				}
				if (trigger & TrgFrameMask) {
					++frametrgcount;
				}
				//				}
			}
			else // photon detected
			{
				auto channel = processor.channel(TTTRRecord);
				if (isrecordingline && (framecounter >= first_frame) && (framecounter <= last_frame) &&
					((channelofinterest < 0) || (channel == channelofinterest))) {
					assert(linecounter >= 0);
					int64_t pixeltime = processor.truesync(TTTRRecord) - lastlinestart;
					// store for later use:
					pixeltimes.push_back({ processor.dtime(TTTRRecord), pixeltime });
				}
			}
			if (isterminal && (recnum & 0x7ffff) == 0) { // show progress indicator only in terminal sessions
				std::cout << 100 * recnum / fh.num_records << "% done\r" << std::flush; // NOTE: this has no significant effect on performance (tested)
			}
		}
	}
	catch (std::exception& e) {
		std::cerr << "ERROR: " << e.what() << std::endl;
		exit(EXIT_FAILURE);
	}
#ifdef DOPERFORMANCEANALYSIS
	auto end_time = std::chrono::steady_clock::now();
	std::chrono::duration<double> diff = end_time - start_time;
	auto duration = diff.count();
	std::cout << "PERF-TEST: Time for execution: " << duration << " s (" << duration / fh.num_records
		<< " s per record)" << std::endl;
	std::cout << pixeltimes.capacity() << std::endl;
#endif
	infile.close();
	std::cout << "first processed frame " << first_frame
		<< " \ntotal frames " << framecounter << " (processed: " << linesprocessed/fh.pix_y
		<< ")\ntotal lines " << totallines << " (processed: " << linesprocessed
		<< ")" << std::endl;

	std::cout << "max Dtime " << maxDtime << std::endl;
	assert(lineduration > 0);
	double microsec_lastpixeltime = double(lineduration) * fh.GlobRes * 1.0e6 / double(fh.pix_x);
	// round dwell time to nearest 0.1 micros:
	std::cout << "pixel dwell time " << std::round(microsec_lastpixeltime * 10.0) / 10.0 << " microseconds" << std::endl;
	if (frametrgcount != framecounter) {
		std::cout << "WARNING: unexpected number of frame triggers in file (" << frametrgcount << ")" << std::endl;
	}
	if (totallines != ((fh.pix_y + lines_to_skip) * framecounter)) {
		std::cout << "WARNING: total lines in file do not match expected num. of lines" << std::endl;
#ifndef NDEBUG
		std::cout << "Lines per processed frame: " << double(totallines) / double(framecounter) <<
			"\nLines per frame trigger: " << double(totallines) / double(frametrgcount) << std::endl;
#endif // !NDEBUG

	}

	++maxDtime; // need to store one datapoint more than max Dtime

	// decide on the file format for export depending on the file extension given in the command line
	auto poslastdot = outfilename.find_last_of('.');
	std::string extension("bin"); // default to bin file
	if (poslastdot != std::string::npos) {
		extension = outfilename.substr(poslastdot + 1);
	}
	bool exporting_ibw;
	if (extension == "ibw") {
		exporting_ibw = true;
		std::cout << "\nExporting Igor binary wave." << std::endl;
	}
	else {
		std::cout << "\nExporting bin file." << std::endl;
		exporting_ibw = false;
	}

	std::cout << "Writing outfile." << std::endl;
	std::ofstream outfile(outfilename.c_str(), std::ios::out | std::ios::binary);
	if (!outfile.good()) {
		std::cerr << " error opening outfile\n";
		exit(EXIT_FAILURE);
	}
	int res = 0;
	if (!exporting_ibw) {
		res = ExportBinFile(outfile, histogram.get(), fh.pix_x, fh.pix_y, fh.PixResol, fh.Resolution, max_hist_channels, maxDtime);
	}
	else {
		auto lastslash = outfilename.find_last_of("/\\");
		std::string wavename("");
		if (lastslash != std::string::npos) {
			wavename = outfilename.substr(lastslash + 1, poslastdot - lastslash - 1);
		}
		else {
			wavename = outfilename.substr(0, poslastdot);
		}
		char firstchar = wavename.at(0);
		if (!std::isalpha(firstchar) && firstchar!='_') {
			wavename = "_" + wavename;
			std::cout << "wavename amended -> " << wavename << std::endl;
		}
		res = ExportIBWFile(outfile, histogram.get(), fh.pix_x, fh.pix_y, fh.PixResol, fh.Resolution, max_hist_channels,
			maxDtime, wavename, fh.filedate);
	}
	if (res != 0) {
		outfile.close();
		std::cerr << "Error while writing outfile.\n";
		exit(EXIT_FAILURE);
	}
	outfile.close();

	std::cout << "Done." << std::endl;
	exit(EXIT_SUCCESS);
}

