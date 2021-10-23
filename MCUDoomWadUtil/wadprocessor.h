/*
   MCUDoomWadutil by Nicola Wrachien.

   Based on doomhack's GBAWadutil
   Original source: https://github.com/doomhack/GbaWadUtil

   This command line utility allows to convert your wad file
   to a format convenient for MCU-based Doom ports.

   Usage:
   MCUDoomWadutil <inputwad> <outputwad>
*/
#ifndef WADPROCESSOR_H
#define WADPROCESSOR_H

#include "wadfile.h"
bool processWad(wadfile_t *wadfile, bool removeSound, bool convertPatches);

#endif // WADPROCESSOR_H
