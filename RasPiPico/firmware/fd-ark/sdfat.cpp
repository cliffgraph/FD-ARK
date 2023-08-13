#include "sdfat.h"
#include <stdio.h>
#include <memory.h>
#include "def_gpio.h"

static inline void init_spi_speed()
{
	spi_init( SPIDEV, 20 * 1000 * 1000 ); 	// 20Mbps
	return;
}

int sd_fatMakdir(const char *pPath)
{
    FRESULT  ret;
    FATFS  fs;
	init_spi_speed();
    ret = f_mount(&fs, "", 1 );
    if( ret != FR_OK ) {
        return false;
    }
    ret = f_mkdir(pPath);
    if( ret != FR_OK ) {
        return false;
    }
	return true;
}

int sd_fatWriteFileTo(const char *pFileName, const char *pBuff, int size )
{
    FRESULT  ret;
    FATFS  fs;
	init_spi_speed();
    ret = f_mount(&fs, "", 1 );
    if( ret != FR_OK ) {
        return false;
    }
    FIL  fil;
    ret = f_open( &fil, pFileName, FA_WRITE|FA_CREATE_ALWAYS );
    if( ret != FR_OK ) {
        return false;
    }
    UINT  wsize;
    ret = f_write( &fil, pBuff, (UINT)size, &wsize );
    if( ret != FR_OK ) {
        return false;
    }
    f_close( &fil );
    return  (int)wsize;
}

bool sd_fatReadFileFrom(const char *pFileName, const int buffSize, uint8_t *pBuff, UINT *pReadSize)
{
    FRESULT  ret;
    FATFS  fs;
	init_spi_speed();
    ret = f_mount( &fs, "", 1 );
    if( ret != FR_OK ) {
        return false;
    }
    FIL  fil;
    ret = f_open( &fil, pFileName, FA_READ|FA_OPEN_EXISTING );
    if( ret != FR_OK ) {
        return false;
    }
    ret = f_read( &fil, pBuff, (UINT)buffSize, pReadSize);
    if( ret != FR_OK ) {
        return false;
    }
    f_close( &fil );
    return true;
}

bool sd_fatExistFile(const char *pFileName)
{
    FRESULT  ret;
    FATFS  fs;
	init_spi_speed();
    ret = f_mount( &fs, "", 1 );
    if( ret != FR_OK ) {
        return false;
    }
	FILINFO info;
	ret = f_stat(pFileName, &info);
    if( ret != FR_OK ) {
        return false;
    }
	if (info.fattrib&AM_DIR) {
        return false;
    }
	return true;
}

static const char *pSDWORKDIR = "\\fd-ark";
static char workPath[255+1];

void MakeDirWork(const int diskNo)
{
	sd_fatMakdir(pSDWORKDIR);
	sprintf(workPath, "%s\\%04d", pSDWORKDIR, diskNo);
	sd_fatMakdir(workPath);
	return;
}

void WriteFileTrackData(
	const int diskNo, const int trackNo, const int sideNo, const uint8_t *pDt, const int dataSize)
{
	sprintf(workPath, "%s\\%04d\\TK%04d-%d.dat", pSDWORKDIR, diskNo, trackNo, sideNo);
	sd_fatWriteFileTo(workPath, reinterpret_cast<const char*>(pDt), dataSize); 
	return;
}

bool ReadFileTrackData(
	const int diskNo, const int trackNo, const int sideNo, const int dataSize, uint8_t *pDt)
{
	sprintf(workPath, "%s\\%04d\\TK%04d-%d.dat", pSDWORKDIR, diskNo, trackNo, sideNo);
	UINT tempSz;
	bool bRes = sd_fatReadFileFrom(	workPath, dataSize, pDt, &tempSz);
	return bRes;
}

bool ReadStoredMark(const int fdNo, const int buffSize, char *pTxt)
{
	sprintf(workPath, "%s\\%04d\\comment.txt", pSDWORKDIR, fdNo);
	memset(pTxt, '\0', buffSize);
	UINT temp;
	const bool b = sd_fatReadFileFrom(workPath, buffSize-1, reinterpret_cast<uint8_t*>(pTxt), &temp);
	return b;
}

bool WriteStoredMark(const int fdNo)
{
	const char *pMess = "FD-DATA";
	sprintf(workPath, "%s\\%04d\\comment.txt", pSDWORKDIR, fdNo);
	const bool b = sd_fatWriteFileTo(workPath, pMess, 7);
	return b;
}

bool CheckExitStoredMark(const int fdNo)
{
	sprintf(workPath, "%s\\%04d\\comment.txt", pSDWORKDIR, fdNo);
	const bool b = sd_fatExistFile(workPath);
	return b;
}