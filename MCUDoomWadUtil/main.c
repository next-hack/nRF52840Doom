/*
   MCUDoomWadutil by Nicola Wrachien.

   Based on doomhack's GBAWadutil
   Original source: https://github.com/doomhack/GbaWadUtil

   This command line utility allows to convert your wad file
   to a format convenient for MCU-based Doom ports.

   Usage:
   MCUDoomWadutil <inputwad> <outputwad>
*/
#include <stdio.h>
#include <stdlib.h>
#include "wadfile.h"
#include "wadprocessor.h"
#define VERSIONSTR "0.3"    // fix for single patch textures.
int main(int argc, char *argv[])
{
    wadfile_t wadfile;
    wadfile_t gbawadfile;
    printf("MCUDoomWadutil by Nicola Wrachien V%s.\r\nOriginal source by doomhack.\r\n", VERSIONSTR);
    // process input
    if (argc != 3)
    {
        printf("Usage: %s <input wad> <output wad>\r\n", argv[0]);
        return 0;
    }
    if (!loadWad("gbadoom.wad", &gbawadfile))
    {
        printf("Error, gbadoom.wad must reside on the same directory of this program.\r\n");
        return 1;
    }
    if (!loadWad(argv[1], &wadfile ))
    {
        printf("Cannot open %s\r\n", argv[1]);
        return 1;
    }
    // we merge first because we need to process the additional gbawadfile patches.
    mergeWadFile(&wadfile, &gbawadfile);
    processWad(&wadfile, false, true); // do not remove sound, convert patches
    //
    if (saveWad(argv[2], &wadfile, 'I'))
    {
        printf("Saved %s.\r\n", argv[2]);
    }
    else
    {
        printf("Cannt save %s.\r\n", argv[2]);
    }
    return 0;
}
