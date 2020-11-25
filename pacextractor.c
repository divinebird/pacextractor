#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

typedef struct {
    uint16_t szVersion[24];       // packet struct version
    uint32_t dwSize;              // the whole packet size
    uint16_t productName[256];    // product name
    uint16_t firmwareName[256];   // product version
    uint32_t partitionCount;      // the number of files that will be downloaded, the file may be an operation
    uint32_t partitionsListStart; // the offset from the packet file header to the array of PartitionHeader struct buffer
    uint32_t dwMode;
    uint32_t dwFlashType;
    uint32_t dwNandStrategy;
    uint32_t dwIsNvBackup;
    uint32_t dwNandPageType;
    uint16_t szPrdAlias[100];     // product alias
    uint32_t dwOmaDmProductFlag;
    uint32_t dwIsOmaDM;
    uint32_t dwIsPreload;
    uint32_t dwReserved[200];
    uint32_t dwMagic;
    uint16_t wCRC1;
    uint16_t wCRC2;
} PacHeader;

typedef struct {
    uint32_t length;              // size of this struct itself
    uint16_t partitionName[256];  // file ID,such as FDL,Fdl2,NV and etc.
    uint16_t fileName[256];       // file name in the packet bin file. It only stores file name
    uint16_t szFileName[256];     // Reserved now
    uint32_t partitionSize;       // file size
    uint32_t nFileFlag;           // if "0", means that it need not a file, and 
                                  // it is only an operation or a list of operations, such as file ID is "FLASH"
                                  // if "1", means that it need a file
    uint32_t nCheckFlag;          // if "1", this file must be downloaded; 
                                  // if "0", this file can not be downloaded;										
    uint32_t partitionAddrInPac;  // the offset from the packet file header to this file data
    uint32_t dwCanOmitFlag;       // if "1", this file can not be downloaded and not check it as "All files" 
                                  // in download and spupgrade tool.
    uint32_t dwAddrNum;
    uint32_t dwAddr[5];
    uint32_t dwReserved[249];     // Reserved for future, not used now
} PartitionHeader;

void getString(int16_t* baseString, char* resString, int tlen) {
    if(*baseString == 0) {
        *resString = 0;
        return;
    }
    int length = 1;
    do {
        *resString = *baseString & 0xff;
        resString++;
        baseString++;
        if (++length > tlen)
            break;
    } while (*baseString > 0);
    *resString = 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: pacextractor <firmware name>.pac\n");
        exit(EXIT_FAILURE);
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        printf("file %s not found.\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    struct stat st;
    stat(argv[1], &st);
    int firmwareSize = st.st_size;
    if (firmwareSize < sizeof(PacHeader)) {
        printf("file %s is not a PAC firmware\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    PacHeader pacHeader;
    size_t rb = read(fd, &pacHeader, sizeof(PacHeader));
    if (rb <= 0) {
        printf("Error while parsing PAC header\n");
        exit(EXIT_FAILURE);
    } else if (pacHeader.dwSize != firmwareSize) {
        printf("Size mismatch error. PAC may be damaged.\n");
        exit(EXIT_FAILURE);
    }

    char buffer1[256];
    char buffer2[256];
    getString(pacHeader.firmwareName, buffer1, 256);
    printf("Product version: %s\n", buffer1);

    uint32_t curPos = pacHeader.partitionsListStart;
    PartitionHeader** partHeaders = malloc(sizeof(PartitionHeader**)*pacHeader.partitionCount);
    int i;
    for (i = 0; i < pacHeader.partitionCount; i++) {
        lseek(fd, curPos, SEEK_SET);
        size_t length = sizeof(PartitionHeader);
        partHeaders[i] = malloc(length);
        rb = read(fd, partHeaders[i], length);
        if (rb <= 0) {
            printf("Partition header error\n");
            exit(EXIT_FAILURE);
        }
        curPos += length;

        getString(partHeaders[i]->partitionName, buffer1, 256);
        getString(partHeaders[i]->fileName, buffer2, 256);
        if (partHeaders[i]->partitionSize != 0) {
            printf("Partition name: %s\n\twith file name: %s\n\twith size %u\n", buffer1, buffer2, partHeaders[i]->partitionSize);
        }
    }

    printf("\nExtracting...\n");
    for (i = 0; i < pacHeader.partitionCount; i++) {
        if (partHeaders[i]->partitionSize == 0) {
            free(partHeaders[i]);
            continue;
        }
        lseek(fd, partHeaders[i]->partitionAddrInPac, SEEK_SET);
        getString(partHeaders[i]->fileName, buffer1, 256);
        remove(buffer1);
        int fd_new = open(buffer1, O_WRONLY|O_CREAT, 0666);
        printf("\t%s", buffer1);
        uint32_t dataSizeLeft = partHeaders[i]->partitionSize;
        while (dataSizeLeft > 0) {
            uint32_t copyLength = (dataSizeLeft > 256) ? 256 : dataSizeLeft;
            rb = read(fd, buffer1, copyLength);
            if (rb != copyLength) {
                printf("\nPartition image extraction error\n");
                exit(EXIT_FAILURE);
            }
            dataSizeLeft -= copyLength;
            rb = write(fd_new, buffer1, copyLength);
            if (rb != copyLength) {
                printf("\nPartition image extraction error\n");
                exit(EXIT_FAILURE);
            }
            printf("\r%d%%", 100 - 100*dataSizeLeft/partHeaders[i]->partitionSize);
        }
        printf("\n");
        close(fd_new);
        free(partHeaders[i]);
    }
    free(partHeaders);
    close(fd);

    return EXIT_SUCCESS;
}
