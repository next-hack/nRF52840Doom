/*
   MCUDoomWadutil by Nicola Wrachien.

   Based on doomhack's GBAWadutil
   Original source: https://github.com/doomhack/GbaWadUtil

   This command line utility allows to convert your wad file
   to a format convenient for MCU-based Doom ports.

   Usage:
   MCUDoomWadutil <inputwad> <outputwad>
*/
#ifndef WADFILE_H
#define WADFILE_H
#include "doomtypes.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    filelump_t lump;
    void *data;
} lump_t;
typedef struct
{
    wadinfo_t header;
    lump_t *lumps;
} wadfile_t;
//bool loadWad(const char *fileName, uint32_t *lumpFound, lump_t **lumps, wadinfo_t *header);
bool loadWad(const char *fileName, wadfile_t *wadfile);
bool saveWad(const char *fileName, wadfile_t *wadfile, char wadType);
void mergeWadFile(wadfile_t *wad1, wadfile_t *wad2);
void removeLump(wadfile_t * wadfile, int index);
void insertLump(wadfile_t *wadfile, lump_t *newLump, int index);
int getLumpNumByName(wadfile_t *wadfile, const char *name);
lump_t * getLumpByNum(wadfile_t *wadfile, int lumpNum);
void replaceLump(wadfile_t *wadfile, lump_t *newLump, int index);

#endif // WADFILE_H
