// (c) 2021 Christian R. Halaszovich
// (See LICENSE.txt for licensing information.)
#include<array>
#include "TTTRRecordProcessor.h"

// RecordTypes
constexpr int64_t
rtPicoHarpT3 = 0x00010303,    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $03 (T3), HW: $03 (PicoHarp)
rtPicoHarpT2 = 0x00010203,    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $03 (PicoHarp)
rtHydraHarpT3 = 0x00010304,    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $03 (T3), HW: $04 (HydraHarp)
rtHydraHarpT2 = 0x00010204,    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $04 (HydraHarp)
rtHydraHarp2T3 = 0x01010304,    // (SubID = $01 ,RecFmt: $01) (V2), T-Mode: $03 (T3), HW: $04 (HydraHarp)
rtHydraHarp2T2 = 0x01010204,    // (SubID = $01 ,RecFmt: $01) (V2), T-Mode: $02 (T2), HW: $04 (HydraHarp)
rtTimeHarp260NT3 = 0x00010305,    // (SubID = $00 ,RecFmt: $01) (V2), T-Mode: $03 (T3), HW: $05 (TimeHarp260N)
rtTimeHarp260NT2 = 0x00010205,    // (SubID = $00 ,RecFmt: $01) (V2), T-Mode: $02 (T2), HW: $05 (TimeHarp260N)
rtTimeHarp260PT3 = 0x00010306,    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T3), HW: $06 (TimeHarp260P)
rtTimeHarp260PT2 = 0x00010206,    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $06 (TimeHarp260P)
rtMultiHarpNT3 = 0x00010307,    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T3), HW: $07 (MultiHarp150N)
rtMultiHarpNT2 = 0x00010207;    // (SubID = $00 ,RecFmt: $01) (V1), T-Mode: $02 (T2), HW: $07 (MultiHarp150N)

constexpr auto THT3_record_types = std::array{ rtHydraHarpT3,
rtHydraHarp2T3, rtTimeHarp260NT3, rtTimeHarp260PT3, rtMultiHarpNT3 };
constexpr auto PHT3_record_types = std::array{ rtPicoHarpT3 };

constexpr uint32_t
THT3_SpecialBitMask = 0x80000000,
THT3_NSyncMask = 1023,
THT3_DTimeShift = 10, THT3_DTimeMask = 32767 << THT3_DTimeShift,
THT3_ChannelShift = 25, THT3_ChannelMask = 63 << THT3_ChannelShift,
THT3_OverflowPeriod = 1024,
PHT3_SpecialBitMask = 0xf0000000,
PHT3_NSyncMask = 0xffff,
PHT3_DTimeShift = 16, PHT3_DTimeMask = 0xfff << PHT3_DTimeShift,
PHT3_ChannelShift = 28, PHT3_ChannelMask = 0xf << PHT3_ChannelShift,
PHT3_OverflowPeriod = 65536;

// We should be able to work with HydraHarp, MultiHarp and TimeHarp260 T3 Format
template<std::size_t N> bool RecordTypeIsSupported(int64_t recordtype, const std::array<int64_t,N>& supported_record_types)
{
	
	return std::find(supported_record_types.begin(), supported_record_types.end(), recordtype) != supported_record_types.end();
}


// currently, we are implementing support for one hardware platform
TTTRRecordProcessor::TTTRRecordProcessor() :specialmask{ THT3_SpecialBitMask },
nsyncmask{ THT3_NSyncMask },
dtimemask{ THT3_DTimeMask }, dtimeshift{ THT3_DTimeShift },
channelmask{ THT3_ChannelMask }, channelshift{ THT3_ChannelShift },
overflowperiod{ THT3_OverflowPeriod }, oflcorrection{ 0 },
record_type{ 0 }
{}

// for now, only works for THT3 type records
bool TTTRRecordProcessor::init(const PTUFileHeader& fh)
{
	record_type = fh.record_type;
	return RecordTypeIsSupported(fh.record_type, THT3_record_types);
}

bool TTTRRecordProcessor::processOverflow(uint32_t record)
{
	if (isSpecial(record)) { // might delete this check to improve speed?
		if (record_type != rtPicoHarpT3) {
			if (channel(record) == 63) { // is overflow event
				if (record_type != rtHydraHarpT3) {
					oflcorrection += nsync(record) * overflowperiod;
				}
				else { // always 1 overflow for old HydraHarp
					oflcorrection += overflowperiod;
				}
				return true;
			}
		}
		else if (dtime(record) == 0) { // PicoHarp T3
			oflcorrection += overflowperiod;
			return true;
		}
	}
	return false;
}
