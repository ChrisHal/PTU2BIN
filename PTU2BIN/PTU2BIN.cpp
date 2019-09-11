// Tool to convert 3T PTU file from PicoQuant symphoTime
// to PicoQuant BIN format (= pre-histogrammed data)
//

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstdint>

#define	DOPERFORMANCEANALYSIS
#ifdef DOPERFORMANCEANALYSIS
#include <windows.h>
#undef max
#undef min
#endif // DOPERFORMANCEANALYSIS

#pragma pack(8)

// some important Tag Idents (TTagHead.Ident) that we will need to read the most common content of a PTU file
// check the output of this program and consult the tag dictionary if you need more
const char TTTRTagTTTRRecType[] = "TTResultFormat_TTTRRecType";
const char TTTRTagNumRecords[] = "TTResult_NumberOfRecords"; // Number of TTTR Records in the File;
const char TTTRTagRes[] = "MeasDesc_Resolution";       // Resolution for the Dtime (T3 Only)
const char TTSyncRate[] = "TTResult_SyncRate";	// snyc rate, usually repetiton rate of laser
const char TTTRTagGlobRes[] = "MeasDesc_GlobalResolution"; // Global Resolution of TimeTag(T2) /NSync (T3). usually intervall between laserpulses
const char FileTagEnd[] = "Header_End";                // Always appended as last tag (BLOCKEND)
const char ImgHdrPixX[] = "ImgHdr_PixX", ImgHdrPixY[] = "ImgHdr_PixY",
ImgHdrPixResol[] = "ImgHdr_PixResol", ImgHdrLineStart[] = "ImgHdr_LineStart",
ImgHdrLineStop[] = "ImgHdr_LineStop", ImgHdrFrame[] = "ImgHdr_Frame";

// TagTypes  (TTagHead.Typ)
#define tyEmpty8      0xFFFF0008
#define tyBool8       0x00000008
#define tyInt8        0x10000008
#define tyBitSet64    0x11000008
#define tyColor8      0x12000008
#define tyFloat8      0x20000008
#define tyTDateTime   0x21000008
#define tyFloat8Array 0x2001FFFF
#define tyAnsiString  0x4001FFFF
#define tyWideString  0x4002FFFF
#define tyBinaryBlob  0xFFFFFFFF

// RecordTypes
#define rtPicoHarpT3     0x00010303    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $03 (T3), HW: $03 (PicoHarp)
#define rtPicoHarpT2     0x00010203    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $03 (PicoHarp)
#define rtHydraHarpT3    0x00010304    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $03 (T3), HW: $04 (HydraHarp)
#define rtHydraHarpT2    0x00010204    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $04 (HydraHarp)
#define rtHydraHarp2T3   0x01010304    // (SubID = $01 ,RecFmt: $01) (V2), T-Mode: $03 (T3), HW: $04 (HydraHarp)
#define rtHydraHarp2T2   0x01010204    // (SubID = $01 ,RecFmt: $01) (V2), T-Mode: $02 (T2), HW: $04 (HydraHarp)
#define rtTimeHarp260NT3 0x00010305    // (SubID = $00 ,RecFmt: $01) (V2), T-Mode: $03 (T3), HW: $05 (TimeHarp260N)
#define rtTimeHarp260NT2 0x00010205    // (SubID = $00 ,RecFmt: $01) (V2), T-Mode: $02 (T2), HW: $05 (TimeHarp260N)
#define rtTimeHarp260PT3 0x00010306    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T3), HW: $06 (TimeHarp260P)
#define rtTimeHarp260PT2 0x00010206    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $06 (TimeHarp260P)
#define rtMultiHarpNT3   0x00010307    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T3), HW: $07 (MultiHarp150N)
#define rtMultiHarpNT2   0x00010207    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $07 (MultiHarp150N)

// A Tag entry
struct TagHead {
	char Ident[32]; // Identifier of the tag
	int32_t Idx;    // Index for multiple tags or -1
	uint32_t Typ;   // Type of tag ty..... see const section
	int64_t TagValue; // Value of tag.
};

int main(int argc, char** argv)
{
	if ((argc < 3)||(argc>4)) {
		std::cerr << "Usage: " << argv[0] << " <infile> <outfile> [<channel no.>]" << std::endl;
		return 1;
	}
	std::cout << "infile: " << argv[1] << "\noutfile: " << argv[2] << std::endl;
	std::ifstream infile(argv[1], std::ios::in | std::ios::binary);
	if (!infile.good()) {
		std::cerr << "error opening infile" << std::endl;
		return 1;
	}	// first, test if it is a valid file
	char Magic[8];
	infile.read(Magic, sizeof(Magic));
	if (!infile.good()) {
		std::cerr << "error reading infile" << std::endl;
		return 1;
	}
	if (std::strncmp(Magic, "PQTTTR", 6) != 0) {
		std::cerr << "not a valid PTU file" << std::endl;
		return 1;
	}
	char Version[9];
	Version[8] = 0;
	if (!infile.read(Version, 8).good()) {
		std::cerr << "error reading infile" << std::endl;
		return 1;
	}
	std::cout << "File version: " << Version << std::endl;
	unsigned int channelofinterest = 1;
	if (argc == 4) channelofinterest = atoi(argv[3])-1;
	int64_t NumRecords = 0, RecordType = 0, PixX = 0, PixY = 0, TrgFrame = 0, TrgLineStart = 0, TrgLineStop = 0;
	const unsigned int MAX_CHANNELS = 512; // number of histogramm channels, same as max Dtime?
	double Resolution = 0.0, // resolution for Dtime
		GlobRes = 0.0, // resolution for global timer
		PixResol = 0.0;
	TagHead tghd;
	unsigned int tagcount = 0;
	while (infile.read((char*)& tghd, sizeof(tghd)).good()) {
		++tagcount;
		switch (tghd.Typ)
		{
		case tyInt8:
			if (strcmp(tghd.Ident, TTTRTagNumRecords) == 0) // Number of records
				NumRecords = tghd.TagValue;
			if (strcmp(tghd.Ident, TTTRTagTTTRRecType) == 0) // TTTR RecordType
				RecordType = tghd.TagValue;
			if (strcmp(tghd.Ident, ImgHdrPixX) == 0) {
				PixX = tghd.TagValue;
				std::cout << "pix. x " << PixX << std::endl;
			}
			if (strcmp(tghd.Ident, ImgHdrPixY) == 0) {
				PixY = tghd.TagValue;
				std::cout << "pix. y " << PixY << std::endl;
			}
			if (strcmp(tghd.Ident, ImgHdrFrame) == 0)
				TrgFrame = tghd.TagValue;
			if (strcmp(tghd.Ident, ImgHdrLineStart) == 0)
				TrgLineStart = tghd.TagValue;
			if (strcmp(tghd.Ident, ImgHdrLineStop) == 0)
				TrgLineStop = tghd.TagValue;
			if (strcmp(tghd.Ident, TTSyncRate) == 0) {
				std::cout << "Sync rate " << tghd.TagValue << " Hz" << std::endl;
			}
			break;
		case tyFloat8:
			if (strcmp(tghd.Ident, TTTRTagRes) == 0) { // Resolution for TCSPC-Decay
				Resolution = *(double*) & (tghd.TagValue);
				std::cout << "resol. for decay " << Resolution << " s" << std::endl;
			}
			if (strcmp(tghd.Ident, TTTRTagGlobRes) == 0) {// Global resolution for timetag
				GlobRes = *(double*) & (tghd.TagValue); // in ns
				std::cout << "Sync intervall " << GlobRes << " s" << std::endl;
			}
			if (strcmp(tghd.Ident, ImgHdrPixResol) == 0)
				PixResol = *(double*) & (tghd.TagValue);
			break;
		case tyFloat8Array:
		case tyAnsiString:
		case tyWideString:
		case tyBinaryBlob:
			// need to skip a few bytes, not really interested in the data right now
			infile.seekg(tghd.TagValue, std::ios::cur);
			break;
		default:
			break;
		}
		if (strncmp(tghd.Ident, FileTagEnd, sizeof(FileTagEnd)) == 0) {
			break; // all headers have been read
		}
	}
	if (!infile.good()) {
		std::cerr << "error while reading file headers\n";
		return 1;
	}
	std::cout << tagcount << " tags read" << std::endl;
	if (RecordType != rtTimeHarp260PT3) {
		std::cerr << "Unexpected record type, was expecting TimeHarp260P T3 data." << std::endl;
		return 1;
	}

	std::cout << "estimated number of useful histogram channels " << GlobRes / Resolution << std::endl;
	std::cout << "total # records in file: " << NumRecords << std::endl;
	std::cout << "Evaluating channel " << (channelofinterest+1) << " only." << std::endl;
	const int64_t T3WRAPAROUND = 1024;
	int64_t oflcorrection = 0, lastlinestart = -1, lastlinestop = -1, lineduration = -1, linecounter = -1 /*skip 1st line*/, 
		totallines = 0,
		framecounter = 0, lastframetime = -1, truensync = 0;
	unsigned int TrgLineStartMask = 1 << (TrgLineStart - 1), TrgLineStopMask = 1 << (TrgLineStop - 1),
		TrgFrameMask = 1 << (TrgFrame - 1);
	bool isrecordingline = false, framehasstarted = false;

	// place for temporary storage of line data
	struct PixelTime {
		unsigned int dtime;
		int64_t pixeltime;
	};
	std::vector<PixelTime> pixeltimes;

	// space for histogramm data
	uint32_t* histogram = new uint32_t[MAX_CHANNELS * PixX * PixY];
	std::memset(histogram, 0, sizeof(uint32_t)*MAX_CHANNELS * PixX * PixY);
	uint32_t maxDtime = 0; // max val in histogram
	uint32_t TTTRRecord = 0;

#ifdef DOPERFORMANCEANALYSIS
	__int64 pcFreq = 0, pcStart = 0, pcStop = 0;
	QueryPerformanceFrequency((LARGE_INTEGER*)& pcFreq);
	QueryPerformanceCounter((LARGE_INTEGER*)& pcStart);
#endif
	const size_t BUFFSIZE = 1024; // bigger buffer did not help on test machine
	auto buffer = new uint32_t[BUFFSIZE];
	size_t bufidx = 0, bufnumelements = 0, recordsremaining = NumRecords;
	for (int64_t recnum = 0; recnum < NumRecords; ++recnum) {
		if (bufidx == bufnumelements) {
			// buffer is empty
			size_t numtoread = std::min(BUFFSIZE, recordsremaining);
			infile.read((char*)buffer, sizeof(uint32_t)* numtoread);
			recordsremaining -= numtoread;
			bufnumelements = numtoread;
			bufidx = 0;
		}
		TTTRRecord = buffer[bufidx++];
		if (!infile.good()) {
			std::cerr << "Error while reading TTTR records from infile. Unexpected end of file." << std::endl;
			return 1;
		}

		const uint32_t SpecialBitMask = 0x80000000,
			nsyncmask = 1023,
			dtimemask = 32767 << 10,
			channelmask = 63 << 25;
		unsigned	nsync = TTTRRecord & nsyncmask,
					dtime = (TTTRRecord & dtimemask) >> 10,
					channel = (TTTRRecord & channelmask) >> 25;

		if (TTTRRecord&SpecialBitMask)
		{
			if (channel == 0x3F) //overflow
			{
				//number of overflows is stored in nsync
				if (nsync == 0) //if it is zero it is an old style single overflow
				{
					oflcorrection += T3WRAPAROUND;
				}
				else
				{
					oflcorrection += T3WRAPAROUND * nsync;
				}
			}
			else if ((channel >= 1) && (channel <= 15)) //markers
			{
				truensync = oflcorrection + nsync;
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
					if ((linecounter >= 0)&&(linecounter < PixY)) {
						uint32_t* lp = histogram + linecounter * MAX_CHANNELS * PixX;
						for (auto pt : pixeltimes) {
							int64_t x = std::max(int64_t(0), std::min(((int64_t(pt.pixeltime) * PixX) / lineduration), PixX - 1));
							unsigned int dt = pt.dtime;
							if (dt < MAX_CHANNELS) {
								++lp[x * MAX_CHANNELS + dt];
								maxDtime = std::max(dt, maxDtime);
							}
						}
					}
					pixeltimes.clear();
					++linecounter;
					if (linecounter == PixY) {
						++framecounter;
						framehasstarted = false;
						const char SPINNER[] = "-\\|/";
						std::cout << SPINNER[framecounter & 3] << "\r" << std::flush; // NOTE: this has no significant effect on performance (tested)
						linecounter = -1;  // skip 1st line
					}
				}
				if (trigger & TrgFrameMask) { // we kind of ignore it, since it seems to be unreliable
					framehasstarted = true;
//					if (isrecordingline) {
//						std::cout << "WARNING: still recording line? trigger was " << trigger << ".\n";
//					}
					lastframetime = truensync;
				}
			}
		}
		else // photon detected
		{
			if (isrecordingline && linecounter>=0) {
				truensync = oflcorrection + nsync;
				if (channel == channelofinterest) {
					int64_t pixeltime = truensync - lastlinestart;
					// store for later use:
					PixelTime pt;
					pt.dtime = dtime;
					pt.pixeltime = pixeltime;
					pixeltimes.push_back(pt);
				}
			}
		}
	}
	delete[] buffer;
	std::cout << " \ntotal frames " << framecounter << "\ntotal lines " << totallines << std::endl;
	std::cout << "max Dtime " << maxDtime << std::endl;
	infile.close();
#ifdef DOPERFORMANCEANALYSIS
	QueryPerformanceCounter((LARGE_INTEGER*)& pcStop);
	double duration = double(pcStop - pcStart) / double(pcFreq);
	std::cout << "Time for execution: " << duration << " s (" << duration / NumRecords 
		<< " s per record)" << std::endl;
#endif
	std::cout << "Writing outfile." << std::endl;
	std::ofstream outfile(argv[2], std::ios::out | std::ios::binary);
	if (!outfile.good()) {
		std::cerr << " error opening outfile\n";
		return 1;
	}
	struct BinHeader {
		uint32_t PixX, PixY;
		float PixResol;
		uint32_t TCSPCChannels;
		float TimeResol;
	} bh;
	++maxDtime; // need to store one datapoint more than max Dtime
	bh.PixX = (uint32_t)PixX;
	bh.PixY = (uint32_t)PixY;
	bh.PixResol = (float)PixResol;
	bh.TCSPCChannels = maxDtime;
	bh.TimeResol = (float)(Resolution * 1e9); // in ns
	outfile.write((char*)& bh, sizeof(bh));
	for (int64_t y = 0; y < PixY; ++y) {
		for (int64_t x = 0; x < PixX; ++x) {
			outfile.write((char*)(histogram + y * PixX * MAX_CHANNELS + x * MAX_CHANNELS), sizeof(uint32_t) * maxDtime);
		}
	}
	outfile.close();
	delete[] histogram;
	std::cout << "Done." << std::endl;
	return 0;
}

