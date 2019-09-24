#include <ostream>
#include <string>
#include <cstring>
#include "export_igor_ibw.h"

/*	Checksum(data,oldcksum,numbytes)

	Returns shortwise simpleminded checksum over the data.
	ASSUMES data starts on an even boundary.
*/
int Checksum(short* data, int oldcksum, int numbytes)
{
	numbytes >>= 1;				// 2 bytes to a short -- ignore trailing odd byte.
	while (numbytes-- > 0)
		oldcksum += *data++;
	return oldcksum & 0xffff;
}

int ExportIBWFile(std::ostream& os, uint32_t* histogram, int64_t pix_x,
	int64_t pix_y, double res_space, double res_time, int64_t num_hist_channels,
	int64_t max_export_channel, const std::string& wavename)
{
	BinHeader5 bh;
	// make sure the packing of the structs is as expected:
	static_assert(sizeof(bh) == 64, "wrong size of bh");
	memset((void*)& bh, 0, sizeof(bh));
	WaveHeader5 wh;
	constexpr size_t numbytes_wh = offsetof(WaveHeader5, wData);
	static_assert(numbytes_wh == 320, "wrong size of wh");
	memset((void*)& wh, 0, sizeof(wh));

	auto npnts = pix_x * pix_y * max_export_channel;

	bh.version = 5;
	bh.wfmSize = int32_t(numbytes_wh + sizeof(uint32_t) * npnts);
	// we will calculate checksum later, all other entries in bh remain 0

	wh.type = NT_UNSIGNED | NT_I32;
	wavename.copy(wh.bname, MAX_WAVE_NAME5);
	wh.whVersion = 1; // yes, for version 5 files files this smust be 1...
	wh.dimUnits[0][0] = 'm';
	wh.dimUnits[1][0] = 'm';
	wh.dimUnits[2][0] = 's';

	wh.npnts = int32_t(npnts);
	wh.nDim[0] = int32_t(pix_x);
	wh.nDim[1] = int32_t(pix_y);
	wh.nDim[2] = int32_t(max_export_channel);
	wh.sfA[0] = wh.sfA[1] = res_space*1e-6; // res_space is in micrometer
	wh.sfA[2] = res_time;

	short cksum = Checksum((short*)& bh, 0, sizeof(bh));
	cksum = Checksum((short*)& wh, cksum, numbytes_wh);
	bh.checksum = -cksum;

	auto buffer = new uint32_t[npnts];
	auto p = buffer;
	// re-order data, to have time as the 3rd dimension
	for (int64_t t = 0; t < max_export_channel; ++t) {
		for (int64_t y = 0; y < pix_y; ++y) {
			for (int64_t x = 0; x < pix_x; ++x) {
				*p++ = histogram[t + x * num_hist_channels + y * num_hist_channels * pix_x];
			}
		}
	}
	os.write((char*)& bh, sizeof(bh));
	os.write((char*)& wh, numbytes_wh);
	os.write((char*)buffer, sizeof(uint32_t) * npnts);
	delete[] buffer; buffer = 0;
	if(!os.good()) {
		return 1;
	}
	return 0;
}

