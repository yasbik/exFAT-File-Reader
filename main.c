/*
COMP 3430 - Assignment 4
Student Name:   Abu Yasin Sabik
Student ID:     7871615
Instructor:     Franklin Bristow
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <sys/types.h>

#define INFO_COMMAND "info"
#define LIST_COMMAND "list"
#define GET_COMMAND "get"
#define BYTES_IN_KB 1024
#define PATH_MAX_LENGTH 100
#define STREAM_EXT 0xC0
#define VOL_LABEL 0x83
#define ALLOCATION_BM 0x81
#define FILE_DIR 0x85
#define FILE_NAME 0xC1
#define HEX_ONE 0x01

//------------------------------------------------------
//
// exFAT Main Boot sector structure
//
//------------------------------------------------------

#pragma pack(1)
#pragma pack(push)
typedef struct MAIN_BOOT_SECTOR
{
    uint8_t chainSizeump_boot[3];
    char fs_name[8];
    uint8_t must_be_zero[53];
    uint64_t partition_offset;
    uint64_t volume_length;
    uint32_t fat_offset;
    uint32_t fat_length;
    uint32_t cluster_heap_offset;
    uint32_t cluster_count;
    uint32_t first_cluster_of_root_directory;
    uint32_t volume_serial_number;
    uint16_t fs_revision;
    uint16_t fs_flags;
    uint8_t bytes_per_sector_shift;
    uint8_t sectors_per_cluster_shift;
    uint8_t number_of_fats;
    uint8_t drive_select;
    uint8_t percent_in_use;
    uint8_t reserved[7];
    uint8_t bootcode[390];
    uint16_t boot_signature;
} main_boot_sector;


#pragma pack(pop)



//------------------------------------------------------
//
// Function Definitions
//
//------------------------------------------------------

static char *unicode2ascii( uint16_t *unicode_string, uint8_t length );
void infoCommand(int fd, main_boot_sector *exfatBS);
int power (int base, int exponent);
void listCommand(int fd, main_boot_sector *exfatBS, uint32_t firstCluster, int depth);
void getCommand(int fd, main_boot_sector* exfatBS, char *filePath);
void createFile(int fd, char *fname, uint32_t *chain);


//------------------------------------------------------
//
// Global Variables
//
//------------------------------------------------------

main_boot_sector *exfatBS;              // main boot sector
uint32_t endOfChain = 0xffffffff;       // end of the chain
int sectorSize_bytes;                   // length of sector in bytes
int clusterSize_bytes;                  // length of cluster in bytes
int clusterSize_sectors;                // length of cluster in sectors
int maxClusters;                        // maximum number of clusters
int numDir = 0;                         // number of directories


//------------------------------------------------------
//
// main
//
// PURPOSE: implements an exFAT file reader
//
// INPUT PARAMETERS:
//     name of the exFAT file and command to run
//
//------------------------------------------------------

int main(int argc, char *argv[]) 
{
    // check the number of arguments
    if (argc < 2) 
    {
        printf("\nIncorrect number of arguments!");
        printf("\nCorrect usage: ./q1 <imagename> <command> <filepath (only for 'get')>\n");
        exit(1);
    }

    int numCommands = argc;
    char *fileName = argv[1];
    char *commandToRun = argv[2];

    // innitialize the main boot sector
    exfatBS = (main_boot_sector *)malloc(sizeof(main_boot_sector));

    // open the file and read it into the main boot sector
    int fd = open(fileName, O_RDONLY);
    read(fd, exfatBS, sizeof(main_boot_sector));


    // calculete the statistics needed for the commands
    sectorSize_bytes = power(2, exfatBS->bytes_per_sector_shift);
    clusterSize_sectors = power(2, exfatBS->sectors_per_cluster_shift);
    clusterSize_bytes = (clusterSize_sectors * sectorSize_bytes);
    maxClusters = clusterSize_bytes / 32;

    printf("\n********** Starting execution... **********\n");

    // not a "get" command
    if (numCommands == 3)
    {
        // info 
        if (strcmp(commandToRun, INFO_COMMAND) == 0)
        {
            printf("\nRunning info command.\n");

            infoCommand(fd, exfatBS);
        }
        // list
        else if (strcmp(commandToRun, LIST_COMMAND) == 0)
        {
            printf("\nRunning list command.\n");

            listCommand(fd, exfatBS, exfatBS->first_cluster_of_root_directory, 0);
        }
        else 
        {
            printf("\nInvalid command.\n\n");
            exit(1);
        }
    }
    // get command
    else if (numCommands == 4)
    {
        // get
        if (strcmp(commandToRun, GET_COMMAND) == 0) 
        {
            printf("\nEntered get command.\n");

            getCommand(fd, exfatBS, argv[3]);
        } 
        // wrong commands were intered
        else 
        {
            printf("\nIncorrect number of arguments!");
            printf("\nCorrect usage: ./q1 <imagename> <command> <filepath (only for 'get')>\n");
            exit(1);
        }
            
    }
    // invalid commands
    else 
    {
        printf("Usage: ./q1 <imagename> <commands>");
        exit(1);
    }

    close(fd);
    free(exfatBS);
    
    printf("\n\n********** End of program. **********\n\n");

    return 0;

}



/**
 * Convert a Unicode-formatted string containing only ASCII characters
 * into a regular ASCII-formatted string (16 bit chars to 8 bit 
 * chars).
 *
 * NOTE: this function does a heap allocation for the string it 
 *       returns, caller is responsible for `free`-ing the allocation
 *       when necessary.
 *
 * uint16_t *unicode_string: the Unicode-formatted string to be 
 *                           converted.
 * uint8_t   length: the length of the Unicode-formatted string (in
 *                   characters).
 *
 * returns: a heap allocated ASCII-formatted string.
 */

 
static char *unicode2ascii( uint16_t *unicode_string, uint8_t length )
{
    assert( unicode_string != NULL );
    assert( length > 0 );

    char *ascii_string = NULL;

    if ( unicode_string != NULL && length > 0 )
    {
        // +1 for a NULL terminator
        ascii_string = calloc( sizeof(char), length + 1); 

        if ( ascii_string )
        {
            // strip the top 8 bits from every character in the 
            // unicode string
            for ( uint8_t i = 0 ; i < length; i++ )
            {
                ascii_string[i] = (char) unicode_string[i];
            }
            // stick a null terminator at the end of the string.
            ascii_string[length] = '\0';
        }
    }

    return ascii_string;
}



//------------------------------------------------------
//
// infoCommand
//
// PURPOSE: gets the information of the exFAT file
//
// INPUT PARAMETERS:
//     the file descriptor and the main boot sector
//
//------------------------------------------------------

void infoCommand(int fd, main_boot_sector *exfatBS)
{
    // check preconditions
    assert(fd >= 0);
    assert(exfatBS != NULL);

    uint32_t *clusterList;          // the cluster chain
    int chainSize = 1;              // size of the chain
    int num = exfatBS->cluster_count + 1;
    uint32_t curr[num];

    uint32_t allocationBM = 0;      // allocation bitmap
    uint8_t eightBits;              // one bite kength
    int bitsUnset = 0;              // bits which are not set
    uint8_t entryType;
    uint16_t v_label[11];           // volume label
    char *volume_label_ascii;
    int numClusters = 0;            // number of clusters
    int k = 1;
    int freeSpace_KB;               // amount of free space
    

    // seek to the root directory
    lseek(fd, (exfatBS->cluster_heap_offset * sectorSize_bytes) + ((exfatBS->first_cluster_of_root_directory - 2) * clusterSize_bytes), SEEK_SET);
    // read the entry type
    read(fd, &entryType, 1);
    
    // traverse to find the allocation bitmap
    for(int i = 0; i < maxClusters && allocationBM == 0; i++)
    {
        // if it is an allocation bitmap
        if(entryType == ALLOCATION_BM)
        {
            lseek(fd, 19, SEEK_CUR);
            // read the allocation bitmap
            read(fd, &allocationBM, 4);
        }

        // keep checking the entry type
        lseek(fd, (exfatBS->cluster_heap_offset * sectorSize_bytes) + (((exfatBS->first_cluster_of_root_directory - 2) * clusterSize_bytes) + (32 * (i + 1))), SEEK_SET);
        read(fd, &entryType, 1);
    }

    curr[0] = allocationBM;    // the first cluster

    // seek to the beginning of the fat region and to the first cluster
    lseek(fd, ((exfatBS->fat_offset) * sectorSize_bytes) + (allocationBM * 4), SEEK_SET);

    // build the cluster chain
    for(int i = 0; i < num; i++)
    {
        // seek to the beginning of the fat region and to the first cluster
        lseek(fd, ((exfatBS->fat_offset) * sectorSize_bytes) + (curr[i] * 4), SEEK_SET);
        // read the cluster
        read(fd, &curr[i+1], 4);

        // end the list 
        if(curr[i+1] == endOfChain)
        {
            curr[i+1] = endOfChain;
            chainSize++;
            break;
        }
        // end the list
        else if (curr[i+1] == 0)
        {
            curr[i+1] = endOfChain;
            chainSize++;
            break;
        }
        chainSize++;
    }

    // initialize the cluster chain
    clusterList = (uint32_t*)(malloc(sizeof(uint32_t) * chainSize));

    // add the items to the array
    for(int i = 0; i < chainSize; i++)
    {
        clusterList[i] = curr[i];
    }

    // seek to the root directory and to the allocation bitmap
    lseek(fd, (exfatBS->cluster_heap_offset * sectorSize_bytes) + ((allocationBM - 2) * clusterSize_bytes), SEEK_SET);

    // iterate the clusters to find the number of unset bits
    for(int i = 0; i < (int)(exfatBS->cluster_count - 1); i += 8)
    {
        // reat one byte at a time
        read(fd, &eightBits, 1);

        // count the number of unset bits
        for(int j = 0; j < 8; j++)
        {
            if((eightBits & HEX_ONE) == 0)
            {
                bitsUnset++;
            }
            eightBits = eightBits >> 1;
        }
        numClusters++;

        // end of the cluster list
        if(numClusters == clusterSize_bytes)
        {
            // seek to the start of the cluster heap and to the next allocation bitmap
            lseek(fd, (exfatBS->cluster_heap_offset * sectorSize_bytes) + ((clusterList[k] - 2) * clusterSize_bytes), SEEK_SET);

            // break loop when we get to the end of the chain
            if(clusterList[k] == endOfChain)
            {
                break;
            }
            k++;
            numClusters = 0;
        }
    }

    // seek to the root directory and to the cluster heap
    lseek(fd, (exfatBS->cluster_heap_offset * sectorSize_bytes) + ((exfatBS->first_cluster_of_root_directory - 2) * clusterSize_bytes), SEEK_SET);
    // read the entry type
    read(fd, &entryType, 1);

    // find the volume label
    for(int i = 0; i < maxClusters; i++)
    {
        // break if found
        if(entryType == VOL_LABEL)
        {
            break;
        }

        // keep skipping and reading
        lseek(fd, 31, SEEK_CUR);
        read(fd, &entryType, 1);
    }

    // seek one byte
    lseek(fd, 1, SEEK_CUR);

    for(int i = 0; i < 11; i++)
    {
        // read the volume label
        read(fd, &v_label[i], 2);
    }

    // convert the name into ascii
    volume_label_ascii = unicode2ascii(v_label, 11);

    // calculate the free space
    freeSpace_KB = (bitsUnset * clusterSize_bytes) / BYTES_IN_KB;

    
    // print the info
    printf("\n\nPrinting the exFAT volume info: \n");
    printf("\nVolume label: %s", volume_label_ascii);
    printf("\nVolume serial Number: %u", exfatBS->volume_serial_number);
    printf("\nFree space on the volume in KB: %d", freeSpace_KB);
    printf("\nCluster size in sectors: %d\n",clusterSize_sectors);
    printf("\nCluster size in bytes: %d\n", clusterSize_bytes);

    // deallocate memory
    free(clusterList);
    free(volume_label_ascii);

}



//------------------------------------------------------
//
// power
//
// PURPOSE: calculates power
//
// INPUT PARAMETERS:
//     the base number and the exponent
//
//------------------------------------------------------

int power (int base, int exponent)
{
    int result = 1;

    while (exponent != 0) 
    {
        result = result * base;
        exponent--;
    }
    return result;
}



//------------------------------------------------------
//
// listCommand
//
// PURPOSE: prints a list of all files and directories
//
// INPUT PARAMETERS:
//     the file descriptor, the main boot sector and the first cluster and the depth of the file
//
//------------------------------------------------------

void listCommand(int fd, main_boot_sector *exfatBS, uint32_t firstCluster, int depth)
{
    // check preconditions
    assert(fd >= 0);
    assert(exfatBS != NULL);

    uint32_t *clusterList;          // the cluster chain
    int chainSize = 1;              // size of the chain
    int num = exfatBS->cluster_count + 1;
    uint32_t curr[num];
    
    curr[0] = firstCluster;    // the first cluster

    // flags for checking and calculations
    int isDirectory = 0;
    int isFile = 0;
    int isName = 0;
    int isStream = 0;

    uint8_t entryType;
    uint32_t cluster_1;             // first cluster of the file
    uint16_t nameOfFile[15];        // name of the files in unicode
    char* nameOfFile_ascii;         // name of the file in ascii
    uint16_t attributes;            // file attributes


    // seek to the beginning of the fat region and to the first cluster
    lseek(fd, ((exfatBS->fat_offset) * sectorSize_bytes) + (firstCluster * 4), SEEK_SET);

    // build the cluster chain
    for(int i = 0; i < num; i++)
    {
        // seek to the beginning of the fat region and to the first cluster
        lseek(fd, ((exfatBS->fat_offset) * sectorSize_bytes) + (curr[i] * 4), SEEK_SET);
        // read the cluster
        read(fd, &curr[i+1], 4);

        // end the list 
        if(curr[i+1] == endOfChain)
        {
            curr[i+1] = endOfChain;
            chainSize++;
            break;
        }
        // end the list
        else if (curr[i+1] == 0)
        {
            curr[i+1] = endOfChain;
            chainSize++;
            break;
        }
        chainSize++;
    }

    // initialize the cluster chain
    clusterList = (uint32_t*)(malloc(sizeof(uint32_t) * chainSize));

    // add the items to the array
    for(int i = 0; i < chainSize; i++)
    {
        clusterList[i] = curr[i];
    }

    // seek to the cluster heap and to the cluster following the cluster chain
    lseek(fd, (exfatBS->cluster_heap_offset * sectorSize_bytes) + ((clusterList[0] - 2) * clusterSize_bytes), SEEK_SET);

    for(int i = 0; clusterList[i] != endOfChain; i++)
    {
        // iterate through the clusters
        for(int j = 0; j < maxClusters; j++)
        {
            // read the entry type
            read(fd, &entryType, 1);

            // if it is a stream extension
            if(isFile == 1 && entryType == STREAM_EXT)
            {
                lseek(fd, 19, SEEK_CUR);
                read(fd, &cluster_1, 4);
                lseek(fd, 8, SEEK_CUR);

                // set the flags
                isStream = 1;
                isFile = 0;
            }
            // if it is a file directory
            else if(entryType == FILE_DIR)
            {
                
                lseek(fd, 3, SEEK_CUR);
                // read the file attributes
                read(fd, &attributes, 2);
                lseek(fd, 26, SEEK_CUR);
                
                attributes = attributes >> 4;

                // if it is a directory
                if((attributes & HEX_ONE) == 1)
                {
                    isDirectory=1;
                }
                // set the flag
                isFile = 1;
            }
            // if there is the file name
            else if(isStream == 1 && entryType == FILE_NAME)
            {
                lseek(fd, 1, SEEK_CUR);
                for(int k = 0; k < 15; k++)
                {
                    // read the name of the file
                    read(fd, &nameOfFile[k], 2);
                }
                nameOfFile_ascii = unicode2ascii(nameOfFile, 30);

                // set the flags
                isStream = 0;
                isName = 1;
            }
            else
            {
                // skip to the next directory entry
                lseek(fd, 31, SEEK_CUR);
            }
            
            // if the file name was found
            if(isName == 1)
            {
                for(int k = 0; k < depth; k++)
                {
                    // print the dashes
                    printf("-");
                }

                // if it is a file
                if(isDirectory != 1)
                {
                    // print the name of the file
                    printf(" File: %s\n", nameOfFile_ascii);
                }
                // if it is a directory
                else
                {
                    // print the name of the directory
                    printf(" Directory: %s\n", nameOfFile_ascii);

                    // recursively perform the same operations to find more files
                    listCommand(fd, exfatBS, cluster_1, (depth + 1));
                    
                    // seek to the previous position after recursion
                    lseek(fd, (exfatBS->cluster_heap_offset * sectorSize_bytes) + ((clusterList[i] - 2) * clusterSize_bytes) + ((j + 1) * 32), SEEK_SET);

                    // set the flag
                    isDirectory = 0;
                }

                // set the flag
                isName = 0;
            }
        }

        // keep seeking to the cluster heap and to the cluster following the cluster chain
        lseek(fd, (exfatBS->cluster_heap_offset * sectorSize_bytes) + ((clusterList[i + 1] - 2) * clusterSize_bytes), SEEK_SET);
    }

    // deallocate memory
    free(clusterList);
    free(nameOfFile_ascii);
}




//------------------------------------------------------
//
// createFile
//
// PURPOSE: creates a file
//
// INPUT PARAMETERS:
//     the file descriptor, the name of the file to be created, and the cluster chain
//
//------------------------------------------------------

void createFile(int fd, char *fname, uint32_t *chain)
{
    // array to hold the data 
    uint8_t data[clusterSize_bytes];
    
    // open another file descriptor
    int fd2 = open(fname, O_RDONLY|O_WRONLY|O_CREAT);
    
    // iterate through the cluster chain
    for(int i = 0; chain[i] != endOfChain; i++)
    {
        lseek(fd, (exfatBS->cluster_heap_offset * sectorSize_bytes) + ((chain[i] - 2) * clusterSize_bytes), SEEK_SET);

        if(chain[i] != 0)
        {
            read(fd, &data, clusterSize_bytes);

            // write to the file
            write(fd2, &data, clusterSize_bytes);
        }
    }
}




//------------------------------------------------------
//
// getCommand
//
// PURPOSE: checks if the volume contains a certain file and creates one if it does
//
// INPUT PARAMETERS:
//     the file descriptor, the main boot sector, and file path
//
//------------------------------------------------------

void getCommand(int fd, main_boot_sector* exfatBS, char *path)
{    

    uint32_t *clusterList;             // the cluster chain
    int chainSize = 1;                 // size of the chain
    int num = exfatBS->cluster_count + 1;
    uint32_t curr[num];
    
    uint8_t entryType;
    uint8_t data[clusterSize_bytes];   // contents of the new file
    uint16_t nameOfFile[15];           // name of the file in unicode
    uint8_t sizeOfFileName;            // size of the file name
    char* nameOfFile_ascii;            // name of the file in ascii
    uint32_t cluster_1;                // the first cluster of the file
    int fileFound = 0;
    int j, k;

    // get the file path
    char* filePath[PATH_MAX_LENGTH];

    // tokenize the file path
    char* str;
    str = strtok(path, "/");

    while(str != NULL)
    {
        filePath[numDir] = strdup(str);
        
        // count the number of directories
        numDir++;

        str = strtok(NULL, "/");
    }

    // build the cluster chain
    curr[0] = exfatBS->first_cluster_of_root_directory;            // the first cluster

    
    // seek to the beginning of the fat region and to the first cluster
    lseek(fd, ((exfatBS->fat_offset) * sectorSize_bytes) + (exfatBS->first_cluster_of_root_directory * 4), SEEK_SET);

    // build the cluster chain
    for(int i = 0; i < num; i++)
    {
        // seek to the beginning of the fat region and to the first cluster
        lseek(fd, ((exfatBS->fat_offset) * sectorSize_bytes) + (curr[i] * 4), SEEK_SET);
        // read the cluster
        read(fd, &curr[i+1], 4);

        // end the list 
        if(curr[i+1] == endOfChain)
        {
            curr[i+1] = endOfChain;
            chainSize++;
            break;
        }
        // end the list
        else if (curr[i+1] == 0)
        {
            curr[i+1] = endOfChain;
            chainSize++;
            break;
        }
        chainSize++;
    }

    // initialize the cluster chain
    clusterList = (uint32_t*)(malloc(sizeof(uint32_t) * chainSize));

    // add the items to the array
    for(int i = 0; i < chainSize; i++)
    {
        clusterList[i] = curr[i];
    }

    // iterate through according to the number of directories in the path
    for(int i = 0; i < numDir; i++)
    {
        j = 0;

        // set the flag
        fileFound = 0;

        // search for the file
        while(clusterList[j] != endOfChain && fileFound == 0)
        {
            k = 0;

            // seek to the location of the file following the cluster chain
            lseek(fd, (exfatBS->cluster_heap_offset * sectorSize_bytes) + ((clusterList[j] - 2) * clusterSize_bytes), SEEK_SET);

            // iterate through the vlusters until the file has been found
            while(k < maxClusters && fileFound == 0)
            {
                read(fd, &entryType, 1);

                // if it is not a directory entry
                if(entryType != FILE_DIR)
                {
                    // skip the next 31 bytes
                    lseek(fd, 31, SEEK_CUR);
                    k++;
                }
                // if it is a file directory entry
                else
                {
                    // skip the next 31 bytes
                    lseek(fd, 31, SEEK_CUR);
                    k++;

                    // if we reach the end of the cluster
                    if(k >= maxClusters)
                    {
                        // seek to the beginning of the next one
                        lseek(fd, (exfatBS->cluster_heap_offset * sectorSize_bytes) + ((clusterList[j+1] - 2) * clusterSize_bytes), SEEK_SET);
                    }

                    // read the entry type
                    read(fd, &entryType, 1);

                    // if we find a stream extension
                    if(entryType == STREAM_EXT)
                    {
                        // skip 2 bytes
                        lseek(fd, 2, SEEK_CUR);
                        // read the size of the file name
                        read(fd, &sizeOfFileName, 1);

                        // skip, read in the cluster and skip again to the next directory entry
                        lseek(fd, 16, SEEK_CUR);
                        read(fd, &cluster_1, 4);
                        lseek(fd, 8, SEEK_CUR);

                        k++;

                        // if we reach the end of the cluster
                        if(k >= maxClusters)
                        {
                            // seek to the beginning of the next one
                            lseek(fd, (exfatBS->cluster_heap_offset * sectorSize_bytes) + ((clusterList[j+1] - 2) * clusterSize_bytes), SEEK_SET);
                        }

                        // read the entry type
                        read(fd, &entryType, 1);

                        // if we find the file name directory
                        if(entryType == FILE_NAME)
                        {
                            // skip one byte
                            lseek(fd, 1, SEEK_CUR);

                            int l = 0;

                            while(l < 15)
                            {
                                // read the name of the file
                                read(fd, &nameOfFile[l], 2);
                                l++;
                            }

                            k++;

                            // convert the name of the file to ascii
                            nameOfFile_ascii = unicode2ascii(nameOfFile, sizeOfFileName);

                            // compate the names and see if the file was found
                            if(strcmp(nameOfFile_ascii, filePath[i]) == 0)
                            {
                                fileFound = 1;
                            }
                        }
                    }
                }
            }

            j++;
        }
        
        // if the file was found
        if(fileFound == 1)
        {
            // deallocate the old clusted chain
            free(clusterList);

            // build the cluster chain
            curr[0] = cluster_1;

            // seek to the beginning of the fat region and to the first cluster
            lseek(fd, ((exfatBS->fat_offset) * sectorSize_bytes) + (cluster_1 * 4), SEEK_SET);

            // build the cluster chain
            for(int i = 0; i < num; i++)
            {
                // seek to the beginning of the fat region and to the first cluster
                lseek(fd, ((exfatBS->fat_offset) * sectorSize_bytes) + (curr[i] * 4), SEEK_SET);
                // read the cluster
                read(fd, &curr[i+1], 4);

                // end the list 
                if(curr[i+1] == endOfChain)
                {
                    curr[i+1] = endOfChain;
                    chainSize++;
                    break;
                }
                // end the list
                else if (curr[i+1] == 0)
                {
                    curr[i+1] = endOfChain;
                    chainSize++;
                    break;
                }
                chainSize++;
            }

            // initialize the cluster chain
            clusterList = (uint32_t*)(malloc(sizeof(uint32_t) * chainSize));

            // add the items to the array
            for(int i = 0; i < chainSize; i++)
            {
                clusterList[i] = curr[i];
            }
        }
    }
    
    // create a file if the file was found
    if(fileFound == 1)
    {
        printf("\nFile with name ' %s ' has been found.", nameOfFile_ascii);

        //createFile(nameOfFile_ascii, clusterList);
    
        // open a new file descriptor
        int fd2 = open(nameOfFile_ascii, O_RDONLY|O_WRONLY|O_CREAT);
        
        // follow the cluster chain to create the new file
        for(int i = 0; clusterList[i] != endOfChain; i++)
        {
            // seek to the location of the file
            lseek(fd, (exfatBS->cluster_heap_offset * sectorSize_bytes) + ((clusterList[i] - 2) * clusterSize_bytes), SEEK_SET);

            if(clusterList[i] != 0)
            {
                // read the contents
                read(fd, &data, clusterSize_bytes);

                // write to the new file
                write(fd2, &data, clusterSize_bytes);
            }
        }

        printf("\nA new file has been created.");
    }
    else
    {
        printf("\nThe file is not in a system.\n");
    }
    //clean up
    free(clusterList);
    free(nameOfFile_ascii);
}


