/*
   MCUDoomWadutil by Nicola Wrachien.

   Based on doomhack's GBAWadutil
   Original source: https://github.com/doomhack/GbaWadUtil

   This command line utility allows to convert your wad file
   to a format convenient for MCU-based Doom ports.

   Usage:
   MCUDoomWadutil <inputwad> <outputwad>
*/
#include "wadfile.h"
#include "doomtypes.h"
#include <string.h>

#define WAD_ALIGN 4

bool loadWad(const char *fileName, wadfile_t *wadfile)
{
    bool res = true;
    FILE * fp;
    // open file
    fp = fopen (fileName, "rb");
    if (fp == NULL)
        return false;
    // get size
    fseek(fp, 0L, SEEK_END);
    size_t sz = ftell(fp);
    printf("file size %d\r\n",sz);
    rewind(fp);
    // read whole file in memory for convenience.
    uint8_t *tmpBuffer;
    tmpBuffer = malloc(sz);
    //

    int read = 0;
    while (read < sz)
    {
        read += fread(tmpBuffer + read , 1, sz, fp);
    }
    printf("read %d bytes \r\n", read);
    fclose(fp);
    // copy header
    memcpy(&wadfile->header, tmpBuffer, sizeof (wadinfo_t));
    if (memcmp(&wadfile->header.identification[1], "WAD", 3) == 0)
    {
        if (wadfile->header.identification[0] == 'I')
            printf("Found IWAD\r\n");
        else if (wadfile->header.identification[0] == 'P')
            printf("Found PWAD\r\n");
        else
        {
            printf("Unknown header\r\n");
            res = false;
        }
    }
    else
    {
        res = false;
        printf("Unknown header %c%c%c%c",
               wadfile->header.identification[0],
               wadfile->header.identification[1],
               wadfile->header.identification[2],
               wadfile->header.identification[3]);
    }
    if (res)
    {
        // read table offset
       // *lumps = malloc(sizeof (int32_t) * *lumpFound );
        //
       // memcpy(*lumps, sizeof (int32_t) * *lumpFound , &tmpBuffer[header.infotableofs]);
        //
        filelump_t *fileLumpTable =  (filelump_t *) &tmpBuffer[wadfile->header.infotableofs];
        wadfile->lumps = malloc(wadfile->header.numlumps * sizeof(lump_t));
        lump_t *lumpTable = wadfile->lumps;
        for (int i = 0; i < wadfile->header.numlumps; i++)
        {
            size_t lumpSize = (fileLumpTable[i].size + (WAD_ALIGN - 1)) & ~(WAD_ALIGN - 1);     //
//            printf("Lump %d, size %d, name %s, orig size %d\r\n", i, lumpSize, fileLumpTable[i].name, fileLumpTable[i].size);
            // copy data
            lumpTable[i].lump.size = fileLumpTable[i].size; // we need to put the original size
            memcpy(lumpTable[i].lump.name, fileLumpTable[i].name, sizeof(lumpTable[i].lump.name));
            lumpTable[i].data = malloc(lumpSize);
            memset(lumpTable[i].data, 0, lumpSize);
            memcpy(lumpTable[i].data, &tmpBuffer[fileLumpTable[i].filepos], lumpSize);
        }
    }
    free(tmpBuffer);
    return res;
}
bool saveWad(const char *fileName, wadfile_t *wadfile, char wadType)
{
    size_t size;
    FILE * fp;
    // open file
    fp = fopen (fileName, "wb");
    if (fp == NULL)
        return false;
    wadfile->header.identification[0] = wadType;
    wadfile->header.identification[1] = 'W';
    wadfile->header.identification[2] = 'A';
    wadfile->header.identification[3] = 'D';
    wadfile->header.infotableofs = sizeof(wadfile->header);
    fwrite(&wadfile->header, 1, sizeof(wadfile->header),fp);
    //
    int ptr = wadfile->header.infotableofs + sizeof (filelump_t) * wadfile->header.numlumps;
    for (int i = 0; i < wadfile->header.numlumps; i++)
    {
        size = (wadfile->lumps[i].lump.size + (WAD_ALIGN - 1)) & ~(WAD_ALIGN - 1);       // size is multiple of 4
        filelump_t fl;
        fl.filepos = ptr;
        fl.size = wadfile->lumps[i].lump.size;  // need to put original size
        memcpy(fl.name, wadfile->lumps[i].lump.name, sizeof (wadfile->lumps[i].lump.name));
        fwrite(&fl, 1, sizeof (fl), fp);
        ptr += size;    // need to use aligned size
    }
    // now store a the lumps
    for (int i = 0; i < wadfile->header.numlumps; i++)
    {
        size = (wadfile->lumps[i].lump.size + (WAD_ALIGN - 1)) & ~(WAD_ALIGN - 1);       // size is multiple of 4
        fwrite(wadfile->lumps[i].data, 1, size, fp);
    }
    fclose(fp);
    return true;
}
int getLumpNumByName(wadfile_t *wadfile, const char *name)
{
    for (int i = 0; i < wadfile->header.numlumps; i++)
    {
        if (0 == strncmp(wadfile->lumps[i].lump.name, name, sizeof(wadfile->lumps[i].lump.name)))
            return i;
    }
    return -1;
}
lump_t * getLumpByNum(wadfile_t *wadfile, int lumpNum)
{
    if (lumpNum >= wadfile->header.numlumps)
    {
        printf("Error, cannot find lum %d, max %d", lumpNum, wadfile->header.numlumps);
        return NULL;
    }
    else
        return &wadfile->lumps[lumpNum];
}
void replaceLump(wadfile_t *wadfile, lump_t *newLump, int index)
{
    if (index >= wadfile->header.numlumps)
    {
        //printf("Lump %d >= numLumps %d\r\n", index, wadfile->header.numlumps);
        return;
    }
    // free old lump data, not needed anymore
    free(wadfile->lumps[index].data);
    memcpy(&wadfile->lumps[index], newLump, sizeof(lump_t));
    //printf("new lump %d size %d\r\n", index, newLump->lump.size);
}
void insertLump(wadfile_t *wadfile, lump_t *newLump, int index)
{
    // create new table
    lump_t *newTable = malloc((wadfile->header.numlumps + 1) * sizeof(lump_t));
    lump_t *oldTable = wadfile->lumps;
    // copy all lumps before index
    int i = 0;
    if (index > wadfile->header.numlumps)
        index = wadfile->header.numlumps;
    for (; i < index; i++)
    {
        memcpy(&newTable[i], &oldTable[i], sizeof(lump_t) );
    }
    // insert
    memcpy(&newTable[i], newLump, sizeof(lump_t));
    // copy remaining
    for (; i < wadfile->header.numlumps; i++)
    {
        memcpy(&newTable[i + 1], &oldTable[i], sizeof(lump_t) );
    }
    wadfile->lumps = newTable;
    free(oldTable);
    // update numlumps
    wadfile->header.numlumps++;
}
void removeLump(wadfile_t * wadfile, int index)
{
    if (index >= wadfile->header.numlumps)
        return;
    // create new table
    // create new table
    lump_t *newTable = malloc((wadfile->header.numlumps - 1) * sizeof(lump_t));
    lump_t *oldTable = wadfile->lumps;
    // copy all lumps before index
    int i = 0;
    for (; i < index; i++)
    {
        memcpy(&newTable[i], &oldTable[i], sizeof(lump_t) );
    }
    // remove
    free(oldTable[i].data);
    // copy remaining
    for (; i < wadfile->header.numlumps - 1; i++)
    {
        memcpy(&newTable[i], &oldTable[i + 1], sizeof(lump_t) );
    }
    wadfile->lumps = newTable;
    free(oldTable);
    // update numlumps
    wadfile->header.numlumps--;
}
void mergeWadFile(wadfile_t *wad1, wadfile_t *wad2)
{
    for (int i = 0; i < wad2->header.numlumps; i++)
    {
        insertLump(wad1, &wad2->lumps[i], 0xFFFF);
    }
}
