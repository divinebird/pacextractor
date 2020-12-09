/*
 * PacExtractor is used to extract Spreadtrum .pac file which inturn used for flashing firmware of android.
 *
 * https://github.com/divinebird/pacextractor -> pacextractor.c
 * https://spdflashtool.com/source/spd-tool-source-code -> Source/BinPack
 */

#include <stdint.h> // uint16_t, uint32_t

#ifndef _PAC_EXTRACTOR_H_

#define _PAC_EXTRACTOR_H_

#define SIZE_OF_PAC_HEADER 2124
#define PAC_MAGIC 0xfffafffaUL
#define SIZE_OF_PARTITION_HEADER 2580
#define NAME_LENGTH 256

typedef struct {
    uint16_t szVersion[24]; // packet struct version
    uint32_t dwSize; // the whole packet size
    uint16_t productName[NAME_LENGTH]; // product name
    uint16_t firmwareName[NAME_LENGTH]; // product version
    uint32_t partitionCount; // the number of files that will be downloaded, the file may be an operation
    uint32_t partitionsListStart; // the offset from the packet file header to the array of PartitionHeader struct buffer
    uint32_t dwMode;
    uint32_t dwFlashType;
    uint32_t dwNandStrategy;
    uint32_t dwIsNvBackup;
    uint32_t dwNandPageType;
    uint16_t szPrdAlias[100]; // product alias
    uint32_t dwOmaDmProductFlag;
    uint32_t dwIsOmaDM;
    uint32_t dwIsPreload;
    uint32_t dwReserved[200];
    uint32_t dwMagic;
    uint16_t wCRC1;
    uint16_t wCRC2;
} PacHeader;

typedef struct {
    uint32_t length; // size of this struct itself
    uint16_t partitionName[NAME_LENGTH]; // file ID,such as FDL,Fdl2,NV and etc.
    uint16_t fileName[NAME_LENGTH]; // file name in the packet bin file. It only stores file name
    uint16_t szFileName[NAME_LENGTH]; // Reserved now
    uint32_t partitionSize; // file size

    /* FileFlag: if "0", means that it need not a file, and
       it is only an operation or a list of operations, such as file ID is "FLASH"
       if "1", means that it need a file */
    uint32_t nFileFlag;

    /* CheckFlag: if "1", this file must be downloaded
       if "0", this file can not be downloaded */
    uint32_t nCheckFlag;

    uint32_t partitionAddrInPac; // the offset from the packet file header to this file data

    /* CanOmitFlag: if "1", this file can not be downloaded and not check it as "All files"
       in download and spupgrade tool. */
    uint32_t dwCanOmitFlag;

    uint32_t dwAddrNum;
    uint32_t dwAddr[5];
    uint32_t dwReserved[249]; // Reserved for future, not used now
} PartitionHeader;

#endif
