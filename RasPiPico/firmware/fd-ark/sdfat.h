#pragma once

#include "ff.h"
#include "diskio.h"

int sd_fatMakdir(const char *pPath);
int sd_fatWriteFileTo(const char *pFileName, const char *pBuff, int size );
bool sd_fatReadFileFrom(const char *pFileName, const int buffSize, uint8_t *pBuff, UINT *pReadSize);
bool sd_fatExistFile(const char *pFileName);

void MakeDirWork(const int diskNo);
void WriteFileTrackData(
	const int diskNo, const int trackNo, const int sideNo, const uint8_t *pDt, const int dataSize);
bool ReadFileTrackData(
	const int diskNo, const int trackNo, const int sideNo, const int dataSize, uint8_t *pDt);
bool ReadStoredMark(const int fdNo, const int buffSize, char *pTxt);
bool WriteStoredMark(const int fdNo);
bool CheckExitStoredMark(const int fdNo);
