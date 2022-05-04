#include <iostream>
#include <cstring>
#include "PTUFileHeader.h"

// some important Tag Idents (TTagHead.Ident)
const char Measurement_Mode[] = "Measurement_Mode";
const char Measurement_SubMode[]= "Measurement_SubMode";
const char TTTRTagTTTRRecType[] = "TTResultFormat_TTTRRecType";
const char TTTRTagNumRecords[] = "TTResult_NumberOfRecords"; // Number of TTTR Records in the File;
const char TTTRTagRes[] = "MeasDesc_Resolution";       // Resolution for the Dtime (T3 Only)
const char TTSyncRate[] = "TTResult_SyncRate";	// snyc rate, usually repetiton rate of laser
const char TTTRTagGlobRes[] = "MeasDesc_GlobalResolution"; // Global Resolution of TimeTag(T2) /NSync (T3). usually intervall between laserpulses
const char FileTagEnd[] = "Header_End";                // Always appended as last tag (BLOCKEND)
const char	ImgHdrBiDirect[]="ImgHdr_BiDirect", ImgHdrDimensions[]="ImgHdr_Dimensions",
ImgHdrSinCorrection[] ="ImgHdr_SinCorrection",
ImgHdrPixX[] = "ImgHdr_PixX", ImgHdrPixY[] = "ImgHdr_PixY",
ImgHdrPixResol[] = "ImgHdr_PixResol", ImgHdrLineStart[] = "ImgHdr_LineStart",
ImgHdrLineStop[] = "ImgHdr_LineStop", ImgHdrFrame[] = "ImgHdr_Frame",
FileCreatingTime[] = "File_CreatingTime",
HWType[] = "HW_Type";

// TagTypes  (TTagHead.Typ)
constexpr uint32_t
tyEmpty8 = 0xFFFF0008,
tyBool8 = 0x00000008,
tyInt8 = 0x10000008,
tyBitSet64 = 0x11000008,
tyColor8 = 0x12000008,
tyFloat8 = 0x20000008,
tyTDateTime = 0x21000008,
tyFloat8Array = 0x2001FFFF,
tyAnsiString = 0x4001FFFF,
tyWideString = 0x4002FFFF,
tyBinaryBlob = 0xFFFFFFFF;

// A Tag entry
struct TagHead {
	char Ident[32]; // Identifier of the tag
	int32_t Idx;    // Index for multiple tags or -1
	uint32_t Typ;   // Type of tag ty..... see const section
	int64_t TagValue; // Value of tag.
};

const double epochdiff = 25569.0; // days between 30/12/1899 (OLE epoch) and 01/01/1970 (UNIX epoch)
// convert OLE time, a.k.a. MS time to C time_t
time_t OLEtime2time_t(double oatime)
{
	time_t res((time_t)((oatime - epochdiff) * 24.0 * 60.0 * 60.0));
	return res;
}


double Int64ToDouble(int64_t tagval)
{
	static_assert(sizeof(double) == 8, "double ist not 8 bytes");
	double t;
	std::memcpy(&t, &tagval, 8);
	return t;
}


bool PTUFileHeader::ProcessFile(std::istream& infile)
{
	// first, test if it is a valid file
	char magic[8];
	infile.read(magic, sizeof(magic));
	if (!infile.good()) {
		std::cerr << "error reading infile" << std::endl;
		return false;
	}
	if (std::strncmp(magic, "PQTTTR", 6) != 0) {
		std::cerr << "not a valid PTU file" << std::endl;
		return false;
	}
	std::string Version(8, ' ');
	if (!infile.read(Version.data(), 8).good()) {
		std::cerr << "error reading infile" << std::endl;
		return false;
	}
	std::cout << "File version: " << Version.c_str() << std::endl;
	unsigned int tagcount = 0;
	////////////
	// read tags:
	TagHead tghd{};
	while (infile.read((char*)&tghd, sizeof(tghd)).good()) {
		++tagcount;
		switch (tghd.Typ)
		{
		case tyInt8:
			if (std::strcmp(tghd.Ident, ImgHdrDimensions) == 0) {
				dimensions = tghd.TagValue; // should match sub-mode?
				std::cout << "Dimensions: " << dimensions << std::endl;
				break;
			}
			if (std::strcmp(tghd.Ident, Measurement_Mode) == 0) {
				measurement_mode = tghd.TagValue;
				std::cout << "T-mode: " << measurement_mode << std::endl;
				break;
			}
			if (std::strcmp(tghd.Ident, Measurement_SubMode) == 0) {
				measurement_submode = tghd.TagValue;
				std::cout << "Measurement SubMode: " << measurement_submode << 
					" (" << Measurement_SubModes.at(measurement_submode) << ")" << std::endl;
				break;
			}
			if (strcmp(tghd.Ident, TTTRTagNumRecords) == 0) // Number of records
				num_records = tghd.TagValue;
			if (strcmp(tghd.Ident, TTTRTagTTTRRecType) == 0) // TTTR RecordType
				record_type = tghd.TagValue;
			if (std::strcmp(tghd.Ident, ImgHdrSinCorrection) == 0) {
				sin_correction = tghd.TagValue;
				break;
			}
			if (strcmp(tghd.Ident, ImgHdrPixX) == 0) {
				pix_x = tghd.TagValue;
				std::cout << "pix. x " << pix_x << std::endl;
			}
			if (strcmp(tghd.Ident, ImgHdrPixY) == 0) {
				pix_y = tghd.TagValue;
				std::cout << "pix. y " << pix_y << std::endl;
			}
			if (strcmp(tghd.Ident, ImgHdrFrame) == 0)
				trg_frame = tghd.TagValue;
			if (strcmp(tghd.Ident, ImgHdrLineStart) == 0)
				trg_linestart = tghd.TagValue;
			if (strcmp(tghd.Ident, ImgHdrLineStop) == 0)
				trg_linestop = tghd.TagValue;
			if (strcmp(tghd.Ident, TTSyncRate) == 0) {
				std::cout << "Sync rate " << tghd.TagValue << " Hz" << std::endl;
			}
			break;
		case tyFloat8:
			if (strcmp(tghd.Ident, TTTRTagRes) == 0) { // Resolution for TCSPC-Decay
				Resolution = Int64ToDouble(tghd.TagValue);
				std::cout << "resol. for decay " << Resolution << " s" << std::endl;
			}
			if (strcmp(tghd.Ident, TTTRTagGlobRes) == 0) {// Global resolution for timetag
				GlobRes = Int64ToDouble(tghd.TagValue); // in s
				std::cout << "Sync intervall " << GlobRes << " s" << std::endl;
			}
			if (strcmp(tghd.Ident, ImgHdrPixResol) == 0) {
				PixResol = Int64ToDouble(tghd.TagValue); // in micrometer
				std::cout << "Pixel resolution: " << PixResol << " um" << std::endl;
			}
			break;
		case tyBool8:
			if (strcmp(tghd.Ident, ImgHdrBiDirect) == 0) {
				is_bidirect = tghd.TagValue;
				break;
			}
			break;
		case tyTDateTime:
			if (strcmp(tghd.Ident, FileCreatingTime) == 0) {
				filedate = OLEtime2time_t(Int64ToDouble(tghd.TagValue));
				tm time{};
#ifdef _WIN32
				gmtime_s(&time, &filedate);
#else
				gmtime_r(&filedate, &time);
#endif
				char buf[26]{};
				std::strftime(buf, sizeof(buf), "%c", &time);
				std::cout << "File Creation Time: " << buf << std::endl;
			}
			break;
		case tyAnsiString:
			if (strcmp(tghd.Ident, HWType) == 0) {
				std::string hw_type(tghd.TagValue, '\0');
				infile.read(hw_type.data(), tghd.TagValue);
				std::cout << "HW type: " << hw_type.c_str() << std::endl;
			}
			else {
				infile.seekg(tghd.TagValue, std::ios::cur);
			}
			break;
		case tyWideString:
		case tyFloat8Array:
		case tyBinaryBlob:
			// need to skip a few bytes, not really interested in the data right now
			infile.seekg(tghd.TagValue, std::ios::cur);
			break;
		default:
			break;
		}
		if (tghd.Typ == tyEmpty8 && strncmp(tghd.Ident, FileTagEnd, sizeof(FileTagEnd)) == 0) {
			break; // all headers have been read
		}
	}
	/// done reading tags
	std::cout << tagcount << " tags read" << std::endl;
	return true; // success
}

bool PTUFileHeader::allNeededPresent()
{
	return measurement_mode != -1 && measurement_submode != -1 &&
		num_records !=-1 && record_type != -1 && pix_x != -1 && pix_y != -1 &&
		trg_frame != -1 && trg_linestart != -1 && trg_linestop !=-1;
}
