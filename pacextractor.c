#include <stddef.h> // size_t
#include <stdio.h> // fclose, fopen, fprintf, fread, fseek, ftell, fwrite, printf, remove, SEEK_END, SEEK_SET, FILE, stderr
#include <stdlib.h> // exit, free, malloc, EXIT_FAILURE, EXIT_SUCCESS
#include <string.h> // strcmp

#include "crc16.h"
#include "pacextractor.h"

_Bool debug = 0;
char namebuf[NAME_LENGTH];

char* getString(uint16_t* baseString) {
    int length = 0;
    while (baseString > 0 && *baseString != 0 && ++length < NAME_LENGTH) {
        namebuf[length-1] = *baseString & 0xff;
        baseString++;
    }
    namebuf[length] = '\0';
    return namebuf;
}

void checkCRC16(PacHeader* pacHeader, FILE* filestream);
void printPartHeaderInfo(PartitionHeader* partitionHeader);
void printPacHeaderInfo(PacHeader* pacHeader);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: pacextractor [-d] [-c] firmware.pac\n");
        fprintf(stderr, "\t -d\t enable debug output\n");
        fprintf(stderr, "\t -c\t compute and verify CRC16\n");
        exit(EXIT_FAILURE);
    }
    char* file = argv[argc - 1];
    _Bool crc16check = 0;

    if ((argc > 2) && (strcmp(argv[1], "-d") == 0 || strcmp(argv[2], "-d") == 0))
        debug = 1;
    if ((argc > 2) && (strcmp(argv[2], "-c") == 0 || strcmp(argv[1], "-c") == 0))
        crc16check = 1;

    FILE* filestream = fopen(file, "rb");
    if (filestream == NULL) {
        fprintf(stderr, "cannot open file %s.\n", file);
        exit(EXIT_FAILURE);
    }

    fseek(filestream, 0, SEEK_END);
    int firmwareSize = ftell(filestream);
    if (firmwareSize < sizeof(PacHeader)) {
        fprintf(stderr, "file %s is not a PAC firmware\n", file);
        exit(EXIT_FAILURE);
    }

    PacHeader pacHeader;
    fseek(filestream, 0, SEEK_SET);
    size_t rb = fread(&pacHeader, 1, sizeof(PacHeader), filestream);
    if (rb <= 0) {
        fprintf(stderr, "Error while parsing PAC header\n");
        exit(EXIT_FAILURE);
    }
    if (pacHeader.dwSize != firmwareSize) {
        fprintf(stderr, "Size mismatch error. PAC may be damaged.\n");
        exit(EXIT_FAILURE);
    } else if (strcmp(getString(pacHeader.szVersion), "BP_R1.0.0") != 0) {
        fprintf(stderr, "Unsupported PAC version\n");
        exit(EXIT_FAILURE);
    }
    if (debug) printPacHeaderInfo(&pacHeader);

    if (crc16check) checkCRC16(&pacHeader, filestream);

    PartitionHeader** partHeaders = malloc(sizeof(PartitionHeader**) * pacHeader.partitionCount);
    int i;
    fseek(filestream, pacHeader.partitionsListStart, SEEK_SET);
    for (i = 0; i < pacHeader.partitionCount; i++) {
        size_t length = SIZE_OF_PARTITION_HEADER;
        partHeaders[i] = malloc(length);
        rb = fread(partHeaders[i], 1, length, filestream);
        if (rb <= 0) {
            fprintf(stderr, "Partition header error\n");
            exit(EXIT_FAILURE);
        }
        if (partHeaders[i]->length != SIZE_OF_PARTITION_HEADER) {
            fprintf(stderr, "Unknown Partition Header format found\n");
            exit(EXIT_FAILURE);
        }
        if (debug) printPartHeaderInfo(partHeaders[i]);
    }

    printf("\nExtracting...\n\n");
    char fiveSpaces[] = "    ";
    char filebuf[4096];
    for (i = 0; i < pacHeader.partitionCount; i++) {
        if (partHeaders[i]->partitionSize == 0) {
            free(partHeaders[i]);
            continue;
        }
        fseek(filestream, partHeaders[i]->partitionAddrInPac, SEEK_SET);
        char* partFile = getString(partHeaders[i]->fileName);
        remove(partFile);
        FILE* newstream = fopen(partFile, "wb");
        printf("%s%s", fiveSpaces, partFile);
        uint32_t dataSizeLeft = partHeaders[i]->partitionSize;
        while (dataSizeLeft > 0) {
            uint32_t copyLength = (dataSizeLeft > 4096) ? 4096 : dataSizeLeft;
            rb = fread(filebuf, 1, copyLength, filestream);
            if (rb != copyLength) {
                fprintf(stderr, "\nPartition image extraction error\n");
                exit(EXIT_FAILURE);
            }
            dataSizeLeft -= copyLength;
            rb = fwrite(filebuf, 1, copyLength, newstream);
            if (rb != copyLength) {
                fprintf(stderr, "\nPartition image extraction error\n");
                exit(EXIT_FAILURE);
            }
            printf("\r%d%%", 100 - ((100 * dataSizeLeft) / partHeaders[i]->partitionSize));
        }
        printf("\r%s%s\n", partFile, fiveSpaces);
        fclose(newstream);
        free(partHeaders[i]);
    }
    printf("\nDone...\n");
    free(partHeaders);
    fclose(filestream);

    return EXIT_SUCCESS;
}

void checkCRC16(PacHeader* pacHeader, FILE* filestream) {
    size_t rb;
    if (pacHeader->dwMagic == (uint32_t) PAC_MAGIC) {
        printf("Checking CRC Part 1\n");
        fseek(filestream, 0, SEEK_SET);
        unsigned char buffer[SIZE_OF_PAC_HEADER - 4];
        rb = fread(buffer, 1, SIZE_OF_PAC_HEADER - 4, filestream);
        if (rb != (SIZE_OF_PAC_HEADER - 4)) {
            fprintf(stderr, "Partition header error\n");
            exit(EXIT_FAILURE);
        }
        uint16_t crc1 = crc16(0, buffer, SIZE_OF_PAC_HEADER - 4);
        uint16_t crc1InPac = pacHeader->wCRC1;
        if (debug) printf("Computed CRC1 = %d, CRC1 in PAC = %d\n", crc1, crc1InPac);
        if (crc1 != crc1InPac) {
            fprintf(stderr, "CRC Check failed for CRC1\n");
            exit(EXIT_FAILURE);
        }
    }

    printf("Checking CRC Part 2\n");
    int BUF_SIZE = 64 * 1024;
    unsigned char buffer[BUF_SIZE];
    fseek(filestream, SIZE_OF_PAC_HEADER, SEEK_SET);
    uint16_t crc2 = 0;
    uint32_t dataSizeLeft = pacHeader->dwSize - SIZE_OF_PAC_HEADER;
    while (dataSizeLeft > 0) {
        uint32_t copyLength = (dataSizeLeft > BUF_SIZE) ? BUF_SIZE : dataSizeLeft;
        rb = fread(buffer, 1, copyLength, filestream);
        if (rb != copyLength) {
            fprintf(stderr, "\nPartition image extraction error\n");
            exit(EXIT_FAILURE);
        }
        dataSizeLeft -= copyLength;
        crc2 = crc16(crc2, buffer, copyLength);
    }
    uint16_t crc2InPac = pacHeader->wCRC2;
    if (debug) printf("Computed CRC2 = %d, CRC2 in PAC = %d\n\n", crc2, crc2InPac);
    if (crc2 != crc2InPac) {
        fprintf(stderr, "CRC Check failed for CRC2\n");
        exit(EXIT_FAILURE);
    }
}

void printPacHeaderInfo(PacHeader* pacHeader) {
    printf("Version"    "\t\t= %s"  "\n", getString(pacHeader->szVersion));
    printf("Size"       "\t\t= %u"  "\n", pacHeader->dwSize);
    printf("PrdName"    "\t\t= %s"  "\n", getString(pacHeader->productName));
    printf("FirmwareName" "\t= %s"  "\n", getString(pacHeader->firmwareName));
    printf("FileCount"    "\t= %d"  "\n", pacHeader->partitionCount);
    printf("FileOffset"   "\t= %d"  "\n", pacHeader->partitionsListStart);
    printf("Mode"       "\t\t= %d"  "\n", pacHeader->dwMode);
    printf("FlashType"    "\t= %d"  "\n", pacHeader->dwFlashType);
    printf("NandStrategy" "\t= %d"  "\n", pacHeader->dwNandStrategy);
    printf("IsNvBackup"   "\t= %d"  "\n", pacHeader->dwIsNvBackup);
    printf("NandPageType" "\t= %d"  "\n", pacHeader->dwNandPageType);
    printf("PrdAlias"     "\t= %s"  "\n", getString(pacHeader->szPrdAlias));
    printf("OmaDmPrdFlag" "\t= %d"  "\n", pacHeader->dwOmaDmProductFlag);
    printf("IsOmaDM"    "\t\t= %d"  "\n", pacHeader->dwIsOmaDM);
    printf("IsPreload"    "\t= %d"  "\n", pacHeader->dwIsPreload);
    printf("Magic"     "\t\t= %#x"  "\n", pacHeader->dwMagic);
    printf("CRC1"       "\t\t= %u"  "\n", pacHeader->wCRC1);
    printf("CRC2"       "\t\t= %u"  "\n", pacHeader->wCRC2);
    printf("\n\n");
}

void printPartHeaderInfo(PartitionHeader* partitionHeader) {
    printf("Size"      "\t\t= %d" "\n", partitionHeader->length);
    printf("FileID"    "\t\t= %s" "\n", getString(partitionHeader->partitionName));
    printf("FileName"    "\t= %s" "\n", getString(partitionHeader->fileName));
    printf("FileSize"    "\t= %u" "\n", partitionHeader->partitionSize);
    printf("FileFlag"    "\t= %d" "\n", partitionHeader->nFileFlag);
    printf("CheckFlag"   "\t= %d" "\n", partitionHeader->nCheckFlag);
    printf("DataOffset"  "\t= %u" "\n", partitionHeader->partitionAddrInPac);
    printf("CanOmitFlag" "\t= %d" "\n", partitionHeader->dwCanOmitFlag);
    printf("\n");
}
