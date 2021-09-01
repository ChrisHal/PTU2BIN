// Tool to convert 3T PTU file from PicoQuant SymphoTime
// to PicoQuant BIN format (= pre-histogrammed data)
//
// As of now, only one specific format is supported.
//
// (c) 2021 Christian R. Halaszovich
// (See LICENSE.txt for licensing information.)
// 
// Some parts are based on demo code from PicoQuant
// (see their GitHub repo)
//

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <algorithm>
#include <iterator>
#include <limits>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cmath>
#define __STDC_WANT_LIB_EXT1__
#include <ctime>
#include <cxxopts.hpp>
#include "PTUFileHeader.h"
#include "TTTRRecordProcessor.h"
#include "RecordBuffer.h"

#ifdef __linux__
#include <unistd.h>
bool my_isatty()
{
	return isatty(STDOUT_FILENO);
}
#elif _WIN32
#include <io.h>
bool my_isatty()
{
	return _isatty(_fileno(stdout));
}
#endif

//#define	DOPERFORMANCEANALYSIS
#ifdef DOPERFORMANCEANALYSIS
#include <windows.h>
#undef max
#undef min
#endif // DOPERFORMANCEANALYSIS

#pragma pack(8)

extern int ExportIBWFile(std::ostream& os, uint32_t* histogram, int64_t pix_x,
	int64_t pix_y, double res_space, double res_time, int64_t num_hist_channels,
	int64_t max_export_channel, const std::string& wavename, time_t filedate);

constexpr auto APP_NAME = "PTU2BIN", VERSION = "pre-2";

// It seems that a certain number of lines should be skipped when the PTU
// file is processed. Here we define how many. In our system is 1 line.
// I do not know yet if this is universally true. Might be a bug in SymphoTime
// or be specific to our system.
constexpr int	LINES_TO_SKIP = 1;

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

cxxopts::ParseResult parse(int argc, char** argv, std::string& infile, std::string& outfile, int& channelofinterest,
	int64_t& first_frame, int64_t& last_frame)
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
		if (argc > 1) {
			std::cerr << "Warning: more arguments given than expected. " << std::endl;
		}
		return result;
	}
	catch (const cxxopts::OptionException & e) {
		std::cout << "error parsing options: " << e.what() << std::endl;
		exit(-1);
	}
}

int main(int argc, char** argv)
{
	std::string infilename, outfilename;
	int channelofinterest = 1;
	int64_t first_frame = 0, last_frame = std::numeric_limits<int64_t>::max();
	auto parsedargs = parse(argc, argv, infilename, outfilename, channelofinterest, first_frame, last_frame);
	// check if we are running from a terminal
#ifdef DOPERFORMANCEANALYSIS
	bool isterminal = false;
#else // DOPERFORMANCEANALYSIS
	bool isterminal = my_isatty();
#endif
	std::cout << "infile: " << infilename << "\noutfile: " << outfilename << std::endl;
	std::ifstream infile(infilename.c_str(), std::ios::in | std::ios::binary);
	PTUFileHeader fh;
	TTTRRecordProcessor processor;

	if (!fh.ProcessFile(infile)) {
		std::cerr << "error processing file headers" << std::endl;
		exit(EXIT_FAILURE);
	}
	if (!infile.good()) {
		std::cerr << "error while reading file headers\n";
		exit(EXIT_FAILURE);
	}
	if (!fh.allNeededPresent()) {
		std::cerr << "ERROR: some data missing from PTU file header" << std::endl;
		exit(EXIT_FAILURE);
	}
	if (fh.measurement_submode != 3) {
		std::cerr << "Sorry, submode " << Measurement_SubModes.at(fh.measurement_submode) << "not supported." << std::endl;
		exit(EXIT_FAILURE);
	}
	if (!processor.init(fh)) {
		std::cerr << "Unexpected record type." << std::endl;
		exit(EXIT_FAILURE);
	}
	if (processor.isT2mode() || fh.measurement_mode != 3) {
		std::cerr << "Sorry, T2 mode not supported (working on it)." << std::endl;
		exit(EXIT_FAILURE);
	}
	int num_useful_histo_ch = int(std::ceil(fh.GlobRes / fh.Resolution)); // TODO: check if ok for T2 data
	std::cout << "estimated number of useful histogram channels: " << num_useful_histo_ch << std::endl;
	std::cout << "total # records in file: " << fh.num_records << std::endl;
	if (channelofinterest >= 0) {
		std::cout << "Evaluating channel " << (channelofinterest + 1) << " only." << std::endl;
	}
	else
	{
		std::cout << "Evaluating all channels." << std::endl;
	}

	int64_t lastlinestart = -1, lastlinestop = -1, lineduration = -1, linecounter = -LINES_TO_SKIP /*skip lines*/,
		totallines = 0,
		framecounter = 0, lastframetime = -1, linesprocessed = 0;
	int64_t frametrgcount = 0; // as a control we count the frame triggers
	unsigned int TrgLineStartMask = 1 << (fh.trg_linestart - 1), TrgLineStopMask = 1 << (fh.trg_linestop - 1),
		TrgFrameMask = 1 << (fh.trg_frame - 1);
	bool isrecordingline = false, framehasstarted = false;

	// place for temporary storage of line data
	struct PixelTime {
		unsigned int dtime;
		int64_t pixeltime;
	};
	std::vector<PixelTime> pixeltimes;
	pixeltimes.reserve(32768);

	// space for histogramm data
	constexpr auto MAX_CHANNELS = 512; // number of histogramm channels, same as max Dtime?
	uint32_t* histogram = new uint32_t[MAX_CHANNELS * fh.pix_x * fh.pix_y];
	std::memset(histogram, 0, sizeof(uint32_t) * MAX_CHANNELS * fh.pix_x * fh.pix_y);
	uint32_t maxDtime = 0; // max val in histogram

#ifdef DOPERFORMANCEANALYSIS
	__int64 pcFreq = 0, pcStart = 0, pcStop = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)& pcFreq);
	QueryPerformanceCounter((LARGE_INTEGER*)& pcStart);
#endif
	// prepare input buffer
	RecordBuffer buffer(infile, fh.num_records);

	//////////////
	// start processing of records
	for (int64_t recnum = 0; recnum < fh.num_records; ++recnum) {
		auto TTTRRecord = buffer.pop();
		if (processor.isSpecial(TTTRRecord))
		{
			if (processor.processOverflow(TTTRRecord)) //overflow
			{
				continue;
			}
			auto channel = processor.channel(TTTRRecord);
			if ((channel >= 1) && (channel <= 15)) //markers
			{
				auto truensync = processor.truesync(TTTRRecord);
				//the time unit depends on sync period which can be obtained from the file header
				unsigned int trigger = channel;
				if ((trigger & TrgLineStartMask) != 0) {
					lastlinestart = truensync;
					++totallines;
					isrecordingline = true;
				}
				else if ((trigger & TrgLineStopMask) != 0 && isrecordingline) { // line ended
					isrecordingline = false;
					lastlinestop = truensync;
					lineduration = lastlinestop - lastlinestart;
					// process line data:
					if ((framecounter>=first_frame) && (framecounter<=last_frame) && (linecounter >= 0) && (linecounter < fh.pix_y)) {
						++linesprocessed;
						uint32_t* lp = histogram + linecounter * MAX_CHANNELS * fh.pix_x;
						for (const auto& pt : pixeltimes) {
							int64_t x = std::max(int64_t(0), std::min(((int64_t(pt.pixeltime) * fh.pix_x) / lineduration), fh.pix_x - 1));
							unsigned int dt = pt.dtime;
							if (dt < MAX_CHANNELS) {
								++lp[x * MAX_CHANNELS + dt];
								maxDtime = std::max(dt, maxDtime);
							}
						}
					}
					pixeltimes.clear();
					++linecounter;
					if (linecounter == fh.pix_y) {
						++framecounter;
						framehasstarted = false;
						if (isterminal) { // show progress indicator only in terminal sessions
							const char SPINNER[] = "-\\|/";
							std::cout << SPINNER[framecounter & 3] << "\r" << std::flush; // NOTE: this has no significant effect on performance (tested)
						}
						linecounter = -LINES_TO_SKIP;  // skip lines
					}
				}
				if (trigger & TrgFrameMask) { // we kind of ignore it, since it seems to be unreliable
					framehasstarted = true;
					lastframetime = truensync;
					++frametrgcount;
				}
			}
		}
		else // photon detected
		{
			auto channel = processor.channel(TTTRRecord);
			if (isrecordingline && (framecounter >= first_frame) && (framecounter <= last_frame) && 
					(linecounter >= 0) && ((channelofinterest < 0) || (channel == channelofinterest))) {
				int64_t pixeltime = processor.truesync(TTTRRecord) - lastlinestart;
				// store for later use:
				pixeltimes.push_back({processor.dtime(TTTRRecord), pixeltime});
			}
		}
	}
#ifdef DOPERFORMANCEANALYSIS
	QueryPerformanceCounter((LARGE_INTEGER*)& pcStop);
	double duration = double(pcStop - pcStart) / double(pcFreq);
	std::cout << "Time for execution: " << duration << " s (" << duration / num_records
		<< " s per record)" << std::endl;
#endif
	infile.close();
	std::cout << "first processed frame " << first_frame
		<< " \ntotal frames " << framecounter << " (processed: " << linesprocessed/fh.pix_y
		<< ")\ntotal lines " << totallines << " (processed: " << linesprocessed
		<< ")" << std::endl;
	if (frametrgcount != framecounter) {
		std::cout << "WARNING: unexpected number of frame triggers in file (" << frametrgcount << ")" << std::endl;
	}
	if (totallines != ((fh.pix_y + LINES_TO_SKIP) * framecounter)) {
		std::cout << "WARNING: total lines in file do not match expected num. of lines" << std::endl;
	}
	std::cout << "max Dtime " << maxDtime << std::endl;
	double microsec_lastpixeltime = double(lastlinestop - lastlinestart) * fh.GlobRes * 1.0e6 / double(fh.pix_x);
	// round dwell time to nearest 0.1 micros:
	std::cout << "pixel dwell time " << std::round(microsec_lastpixeltime * 10.0) / 10.0 << " microseconds" << std::endl;
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
		res = ExportBinFile(outfile, histogram, fh.pix_x, fh.pix_y, fh.PixResol, fh.Resolution, MAX_CHANNELS, maxDtime);
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
		res = ExportIBWFile(outfile, histogram, fh.pix_x, fh.pix_y, fh.PixResol, fh.Resolution, MAX_CHANNELS,
			maxDtime, wavename, fh.filedate);
	}
	if (res != 0) {
		outfile.close();
		std::cerr << "Error while writing outfile.\n";
		exit(EXIT_FAILURE);
	}
	outfile.close();

	delete[] histogram;
	std::cout << "Done." << std::endl;
	exit(EXIT_SUCCESS);
}

