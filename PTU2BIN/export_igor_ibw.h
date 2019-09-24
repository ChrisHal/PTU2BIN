// modified from WaveMetrics(tm) IgorBin.h
#pragma once
#include <cstdint>

constexpr auto NT_I32 = 0x20;		// 32 bit integer numbers. Requires Igor Pro 2.0 or later.;
constexpr auto NT_UNSIGNED = 0x40;	// Makes above signed integers unsigned. Requires Igor Pro 3.0 or later.;

constexpr auto MAXDIMS = 4;
constexpr auto MAX_WAVE_NAME5 = 31;	// Maximum length of wave name in version 5 files. Does not include the trailing null.;
constexpr auto MAX_UNIT_CHARS = 3;

// Mac'c (deprecated) GetDateTime returnes seconde starting from 1.1.1904,
// Unix's epoch is from 1.1.1970. This is in seconds.
constexpr auto EPOCHDIFF_MAC_UNIX = 2082844800;

struct BinHeader5 {
	int16_t version;						// Version number for backwards compatibility.
	int16_t checksum;						// Checksum over this header and the wave header.
	int32_t wfmSize;					// The size of the WaveHeader5 data structure plus the wave data.
	int32_t formulaSize;				// The size of the dependency formula, including the null terminator, if any. Zero if no dependency formula.
	int32_t noteSize;					// The size of the note text.
	int32_t dataEUnitsSize;				// The size of optional extended data units.
	int32_t dimEUnitsSize[MAXDIMS];		// The size of optional extended dimension units.
	int32_t dimLabelsSize[MAXDIMS];		// The size of optional dimension labels.
	int32_t sIndicesSize;				// The size of string indicies if this is a text wave.
	int16_t longWaveNameSize;				// The size of the long name in version 7 files.
	int16_t optionsSize1;					// Reserved. Write zero. Ignore on read.
	int32_t optionsSize2;				// Reserved. Write zero. Ignore on read.
};

// compared to origianl, some pointer types have been converted to uint32_t for compilation for 64-bit
struct WaveHeader5 {
	uint32_t next;			// link to next wave in linked list.

	uint32_t creationDate;				// DateTime of creation.
	uint32_t modDate;					// DateTime of last modification.

	int32_t npnts;						// Total number of points (multiply dimensions up to first zero).
	short type;							// See types (e.g. NT_FP64) above. Zero for text waves.
	short dLock;						// Reserved. Write zero. Ignore on read.

	char whpad1[6];						// Reserved. Write zero. Ignore on read.
	short whVersion;					// Write 1. Ignore on read.
	char bname[MAX_WAVE_NAME5 + 1];		// Name of wave plus trailing null.
	int32_t whpad2;						// Reserved. Write zero. Ignore on read.
	uint32_t dFolder;		// Used in memory only. Write zero. Ignore on read.

	// Dimensioning info. [0] == rows, [1] == cols etc
	int32_t nDim[MAXDIMS];				// Number of of items in a dimension -- 0 means no data.
	// CAUTION: The next element is NOT 8 byte aligned! This is why we need the ugly pack(4)
	// or we work around it! I prefer to do so...
	//double sfA[MAXDIMS];				// Index value for element e of dimension d = sfA[d]*e + sfB[d].
	//double sfB[MAXDIMS];
	char sfA[sizeof(double) * MAXDIMS];
	char sfB[sizeof(double) * MAXDIMS];

	// SI units
	char dataUnits[MAX_UNIT_CHARS + 1];			// Natural data units go here - null if none.
	char dimUnits[MAXDIMS][MAX_UNIT_CHARS + 1];	// Natural dimension units go here - null if none.

	short fsValid;						// TRUE if full scale values have meaning.
	short whpad3;						// Reserved. Write zero. Ignore on read.
	// The next 2 are out of aligment,too:
	//double topFullScale, botFullScale;	// The max and max full scale value for wave.
	// tweak aligment:
	char topFullScale[8], botFullScale[8];
	uint32_t dataEUnits;					// Used in memory only. Write zero. Ignore on read.
	uint32_t dimEUnits[MAXDIMS];			// Used in memory only. Write zero. Ignore on read.
	uint32_t dimLabels[MAXDIMS];			// Used in memory only. Write zero. Ignore on read.

	uint32_t waveNoteH;					// Used in memory only. Write zero. Ignore on read.

	unsigned char platform;				// 0=unspecified, 1=Macintosh, 2=Windows; Added for Igor Pro 5.5.
	unsigned char spare[3];

	int32_t whUnused[13];				// Reserved. Write zero. Ignore on read.

	int32_t vRefNum, dirID;				// Used in memory only. Write zero. Ignore on read.

	// The following stuff is considered private to Igor.

	short aModified;					// Used in memory only. Write zero. Ignore on read.
	short wModified;					// Used in memory only. Write zero. Ignore on read.
	short swModified;					// Used in memory only. Write zero. Ignore on read.

	char useBits;						// Used in memory only. Write zero. Ignore on read.
	char kindBits;						// Reserved. Write zero. Ignore on read.
	uint32_t formula;						// Used in memory only. Write zero. Ignore on read.
	int32_t depID;						// Used in memory only. Write zero. Ignore on read.

	short whpad4;						// Reserved. Write zero. Ignore on read.
	short srcFldr;						// Used in memory only. Write zero. Ignore on read.
	uint32_t fileName;					// Used in memory only. Write zero. Ignore on read.

	uint32_t sIndices;					// Used in memory only. Write zero. Ignore on read.

	float wData[1];						// The start of the array of data. Must be 64 bit aligned.
};

