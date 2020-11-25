/*
 * This program is used for unpacking .pac file of Spreadtrum Firmware used in SPD Flash Tool for flashing.
 *
 * Created : 26th January 2020
 * Author  : HemanthJabalpuri
 *
 * This file has been put into the public domain.
 * You can do whatever you want with this file.
 */

package com.sprd.pacextractor;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.RandomAccessFile;

public class PacExtractor {

    public static final short SIZE_OF_PAC_HEADER = 2124; // bytes
    public static final short SIZE_OF_PARTITION_HEADER = 2580; // bytes
    public static final long PAC_MAGIC = 0xfffafffaL;

    private static RandomAccessFile in;
    private static boolean debug;

    public static void exit(String errorMsg) {
        throw new RuntimeException(errorMsg);
    }

    public static String getString(char[] string) {
        return new String(string).trim();
    }

    public static void main(String[] args) {
        if (args.length < 2) {
            System.err.println("Usage: PacExtractor [-d] [-c] firmware.pac outdir");
            System.err.println("\t -d\t enable debug output");
            System.err.println("\t -c\t compute and verify CRC16");
            System.exit(2);
        }
        boolean checkCRC16 = false;
        if (args[0].equals("-d") || args[1].equals("-d")) debug = true;
        if (args[1].equals("-c") || args[0].equals("-c")) checkCRC16 = true;

        try {
            File inFile = new File(args[args.length - 2]);
            if (inFile.length() < SIZE_OF_PAC_HEADER)
                exit(inFile + " is not a PAC firmware.");

            File outdir = new File(args[args.length - 1]);
            if (!outdir.exists()) outdir.mkdirs();
            if (outdir.isFile())
                exit("File with name " + outdir + " exists.");
            String outdirS = outdir.getAbsolutePath() + File.separator;

            in = new RandomAccessFile(inFile, "r");
            PacHeader pacHeader = new PacHeader();
            if (unsigned(pacHeader.dwSize) != inFile.length())
                exit("Bin packet's size is not correct");
            else if (!getString(pacHeader.szVersion).equals("BP_R1.0.0"))
                exit("Unsupported PAC version");

            if (debug) pacHeader.printInfo();

            if (checkCRC16) checkCRC16(pacHeader);

            in.seek(unsigned(pacHeader.partitionsListStart));
            PartitionHeader[] partHeaders = new PartitionHeader[pacHeader.partitionCount];
            int i;
            for (i = 0; i < partHeaders.length; i++) {
                partHeaders[i] = new PartitionHeader();
                if (partHeaders[i].length != SIZE_OF_PARTITION_HEADER)
                    exit("Unknown Partition Header format found");
                if (debug) partHeaders[i].printInfo();
            }

            System.out.println("\nExtracting...\n");
            String fiveSpaces = "     ";
            byte[] buffer = new byte[4096];
            for (i = 0; i < partHeaders.length; i++) {
                if (partHeaders[i].partitionSize == 0)
                    continue;
                in.seek(unsigned(partHeaders[i].partitionAddrInPac));
                String outfile = getString(partHeaders[i].fileName);
                System.out.print(fiveSpaces + outfile);

                FileOutputStream fos = new FileOutputStream(outdirS + outfile);
                int read;
                long len = unsigned(partHeaders[i].partitionSize);
                long tsize = len;
                while ((read = in.read(buffer, 0, len < buffer.length ? (int) len : buffer.length)) > 0) {
                    fos.write(buffer, 0, read);
                    len -= read;
                    System.out.print("\r" + (100 - ((100 * len) / tsize)) + "%");
                }
                System.out.println("\r" + outfile + fiveSpaces);
                fos.close();
            }

            System.out.println("\nDone...");
        } catch (Exception e) {
            System.err.println(e.getMessage());
        } finally {
            try {
                if (in != null) in.close();
            } catch (IOException e) {
                System.err.println(e.getMessage());
                System.exit(1);
            }
        }
    }

    public static void checkCRC16(PacHeader pacHeader) throws IOException {
        CRC16 crc16 = new CRC16();
        int read;
        if (unsigned(pacHeader.dwMagic) == PAC_MAGIC) {
            System.out.println("Checking CRC Part 1");
            in.seek(0);
            byte[] buffer = new byte[SIZE_OF_PAC_HEADER - 4];
            read = in.read(buffer);
            for (int i = 0; i < read; ++i)
                crc16.update(buffer[i]);
            int crc1InPac = unsigned(pacHeader.wCRC1);
            if (debug) System.out.println("Computed CRC1 = " + crc16.getValue() + ", CRC1 in PAC = " + crc1InPac);
            if (crc16.getValue() != crc1InPac)
                exit("CRC Check failed for CRC1\n");
        }

        System.out.println("Checking CRC Part 2");
        crc16.reset();
        int crc2InPac = unsigned(pacHeader.wCRC2);
        byte[] buffer2 = new byte[64 * 1024];
        in.seek(SIZE_OF_PAC_HEADER);
        while ((read = in.read(buffer2)) > 0)
            for (int i = 0; i < read; i++)
                crc16.update(buffer2[i]);

        if (debug) System.out.println("Computed CRC2 = " + crc16.getValue() + ", CRC2 in PAC = " + crc2InPac + "\n");
        if (crc16.getValue() != crc2InPac)
            exit("CRC Check failed for CRC2");
    }

    public static int unsigned(short n) {
        return n & 0xffff;
    }

    public static long unsigned(int n) {
        return n & 0xffffffffL;
    }

    public static int readInt() throws IOException {
        int result = 0;
        for (int i = 0; i < 4; i++)
            result |= (in.readUnsignedByte() << (8 * i));
        return result;
    }

    public static void readInt(int[] var) throws IOException {
        for (int i = 0; i < var.length; i++)
            var[i] = readInt();
    }

    public static short readShort() throws IOException {
        short result = 0;
        for (int i = 0; i < 2; i++)
            result |= (in.readUnsignedByte() << (8 * i));
        return result;
    }

    public static void readShort(short[] var) throws IOException {
        for (int i = 0; i < var.length; i++)
            var[i] = readShort();
    }

    public static void readString(char[] var) throws IOException {
        for (int i = 0; i < var.length; i++)
            var[i] = (char) readShort();
    }

    public static class PacHeader {
        char[] szVersion = new char[24];     // packet struct version
        int dwSize;                          // the whole packet size
        char[] productName = new char[256];  // product name
        char[] firmwareName = new char[256]; // product version
        int partitionCount;                  // the number of files that will be downloaded, the file may be an operation
        int partitionsListStart;             // the offset from the packet file header to the array of PartitionHeaders start
        int dwMode;
        int dwFlashType;
        int dwNandStrategy;
        int dwIsNvBackup;
        int dwNandPageType;
        char[] szPrdAlias = new char[100];   // product alias
        int dwOmaDmProductFlag;
        int dwIsOmaDM;
        int dwIsPreload;
        int[] dwReserved = new int[200];
        int dwMagic;
        short wCRC1;
        short wCRC2;

        public PacHeader() throws IOException {
            readString(szVersion);
            dwSize = readInt();
            readString(productName);
            readString(firmwareName);
            partitionCount = readInt();
            partitionsListStart = readInt();
            dwMode = readInt();
            dwFlashType = readInt();
            dwNandStrategy = readInt();
            dwIsNvBackup = readInt();
            dwNandPageType = readInt();
            readString(szPrdAlias);
            dwOmaDmProductFlag = readInt();
            dwIsPreload = readInt();
            dwIsOmaDM = readInt();
            readInt(dwReserved);
            dwMagic = readInt();
            wCRC1 = readShort();
            wCRC2 = readShort();
        }

        public void printInfo() {
            System.out.println(
              "Version"      + "\t\t= "   + getString(szVersion)                + "\n" +
              "Size"         + "\t\t= "   + unsigned(dwSize)                    + "\n" +
              "PrdName"      + "\t\t= "   + getString(productName)              + "\n" +
              "FirmwareName" +   "\t= "   + getString(firmwareName)             + "\n" +
              "FileCount"    +   "\t= "   + unsigned(partitionCount)            + "\n" +
              "FileOffset"   +   "\t= "   + unsigned(partitionsListStart)       + "\n" +
              "Mode"         + "\t\t= "   + dwMode                              + "\n" +
              "FlashType"    +   "\t= "   + dwFlashType                         + "\n" +
              "NandStrategy" +   "\t= "   + dwNandStrategy                      + "\n" +
              "IsNvBackup"   +   "\t= "   + dwIsNvBackup                        + "\n" +
              "NandPageType" +   "\t= "   + dwNandPageType                      + "\n" +
              "PrdAlias"     +   "\t= "   + getString(szPrdAlias)               + "\n" +
              "OmaDmPrdFlag" +   "\t= "   + dwOmaDmProductFlag                  + "\n" +
              "IsOmaDM"      + "\t\t= "   + dwIsOmaDM                           + "\n" +
              "IsPreload"    +   "\t= "   + dwIsPreload                         + "\n" +
              "Magic"        + "\t\t= 0x" + Long.toHexString(unsigned(dwMagic)) + "\n" +
              "CRC1"         + "\t\t= "   + unsigned(wCRC1)                     + "\n" +
              "CRC2"         + "\t\t= "   + unsigned(wCRC2)                     + "\n\n"
            );
        }
    }

    public static class PartitionHeader {
        int length;                           // size of this struct itself
        char[] partitionName = new char[256]; // file ID,such as FDL,Fdl2,NV and etc.
        char[] fileName = new char[256];      // file name in the packet bin file. It only stores file name
        char[] szFileName = new char[256];    // Reserved now
        int partitionSize;                    // file size
        int nFileFlag;                        // if "0", means that it need not a file, and
                                              // it is only an operation or a list of operations, such as file ID is "FLASH"
                                              // if "1", means that it need a file
        int nCheckFlag;                       // if "1", this file must be downloaded;
                                              // if "0", this file can not be downloaded;
        int partitionAddrInPac;               // the offset from the packet file header to this file data
        int dwCanOmitFlag;                    // if "1", this file can not be downloaded and not check it as "All files"
                                              // in download and spupgrade tool.
        int dwAddrNum;
        int[] dwAddr = new int[5];
        int[] dwReserved = new int[249];      // Reserved for future, not used now

        public PartitionHeader() throws IOException {
            length = readInt();
            readString(partitionName);
            readString(fileName);
            readString(szFileName);
            partitionSize = readInt();
            nFileFlag = readInt();
            nCheckFlag = readInt();
            partitionAddrInPac = readInt();
            dwCanOmitFlag = readInt();
            dwAddrNum = readInt();
            readInt(dwAddr);
            readInt(dwReserved);
        }

        public void printInfo() {
            System.out.println(
              "Size"        + "\t\t= " + unsigned(length)             + "\n" +
              "FileID"      + "\t\t= " + getString(partitionName)     + "\n" +
              "FileName"    +   "\t= " + getString(fileName)          + "\n" +
              "FileSize"    +   "\t= " + unsigned(partitionSize)      + "\n" +
              "FileFlag"    +   "\t= " + nFileFlag                    + "\n" +
              "CheckFlag"   +   "\t= " + nCheckFlag                   + "\n" +
              "DataOffset"  +   "\t= " + unsigned(partitionAddrInPac) + "\n" +
              "CanOmitFlag" +   "\t= " + dwCanOmitFlag                + "\n"
            );
        }
    }
}
