#pragma once

#pragma pack(push,1)
const int SIZE_TRACK_DATA = 100*1024;
struct TRACKDATA
{
	uint8_t		Signature[6];
	uint8_t		Code[2];
	uint32_t	Count;
	uint8_t		LastIndex8;
	uint8_t		Binary[SIZE_TRACK_DATA];
};

#pragma pack(pop)

