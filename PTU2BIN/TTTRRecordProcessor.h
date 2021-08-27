// (c) 2021 Christian R. Halaszovich
// (See LICENSE.txt for licensing information.)

#pragma once
#include <cstdint>
#include"PTUFileHeader.h"


class TTTRRecordProcessor
{
	uint32_t specialmask,
		nsyncmask,
		dtimemask, dtimeshift,
		channelmask, channelshift;
	int64_t overflowperiod, oflcorrection, record_type;
public:
	TTTRRecordProcessor();
	bool init(const PTUFileHeader& fh); // true if successful
	bool isSpecial(uint32_t record) { return (record & specialmask) == specialmask; };
	bool processOverflow(uint32_t record); // true, if record was overflow (record must be special!))
	uint32_t nsync(uint32_t record) { return record & nsyncmask; };
	int64_t truesync(uint32_t record) { return oflcorrection + nsync(record); };
	uint32_t dtime(uint32_t record) { return (record & dtimemask) >> dtimeshift; };
	uint32_t channel(uint32_t record) { return (record & channelmask) >> channelshift; };
};

