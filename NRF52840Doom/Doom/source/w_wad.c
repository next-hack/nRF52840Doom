/**
 *  Doom Port to the nRF52840 by Nicola Wrachien (next-hack in the comments)
 *
 *  This port is based on the excellent doomhack's GBA Doom Port,
 *  with Kippykip additions.
 *  
 *  Several data structures and functions have been optimized 
 *  to fit in only 256kB RAM of nRF52840 (GBA has 384 kB RAM). 
 *  Z-Depth Light has been restored with almost no RAM consumption!
 *  Tons of speed optimizations have been done, and the game now
 *  runs extremely fast, despite the much higher 3D resolution with
 *  respect to GBA.
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *  Copyright (C) 2021 Nicola Wrachien (next-hack in the comments)
 *  on nRF52840 port.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *      Handles WAD file header, directory, lump I/O.
 *
 * next-hack:
 * Added code to load data from external flash.
 * Added code to store/cache some wad data (And more) to internal flash.
 * This latter might be moved sooner or later to a better place.
 *-----------------------------------------------------------------------------
 */

// use config.h if autoconf made one -- josh
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "doomstat.h"
#include "d_net.h"
#include "doomtype.h"
#include "i_system.h"

#include "doom_iwad.h"

#include "w_wad.h"
#include "lprintf.h"

#include "global_data.h"
#include <stdint.h>
#include "i_spi_support.h"
#include "nrf.h"
#include "main.h"
#define FLASH_ALIGNMENT 4
/* Flash organization
 * After 256k, 8k Aligned
 * (4-byte word alignment)
 * Wad CRC      -  used to determine, together with length, if the Wad has been changed.
 * Wad Length   -
 * Wad immutableData CRC  - To determine if the cache is ok, to speed up boot time.
 * wad immutableDataLength
 * Wad Number Of Lumps
 * Address to Lump Offset Table
 * Address to Lump Lengths Table
 * Number of Sprite
 * Address to Sprite Def table
 * Address to level table
 *
 * Lump Offset Table
 * Lump Offset Length
 *
 *
 *
 * Level Table Address: (origin 8kB alignd)
 * Cached addresses
 *
 */
#define getNumberOfLumps() (_g->numlumps)
uint32_t currentImmutableFlashOffset = 0;
uint32_t currentLevelFlashOffset = 0;
wad_immutable_flash_data_t *p_wad_immutable_flash_data;
wad_level_flash_data_t *p_wad_level_flash_data;
void **lumpPtrArray;
boolean lumpPtrArrayStoredInFlash = false;
void setCurrentImmutableFlashOffset(uint32_t offset)
{
    currentImmutableFlashOffset = offset;
}

void** storeLumpArrayToFlash(boolean levelChanged)
{
    lumpPtrArrayStoredInFlash = true;
    void **oldLumpPtrArrayAddress = lumpPtrArray;
    if (levelChanged)
    {
        lumpPtrArray = writeBufferToFlashRegion(lumpPtrArray, getNumberOfLumps() * sizeof(void*), FLASH_LEVEL_REGION, true);
        printf("changed, updated lumpAddressTable 0x%x\r\n", lumpPtrArray);
    }
    else
    {  // do not reload
        lumpPtrArray = p_wad_immutable_flash_data->levelData->lumpAddressTable;
        printf("level not changed, retrieving old lumpAddressTable 0x%x\r\n", lumpPtrArray);
    }
    Z_Free(oldLumpPtrArrayAddress);
    return lumpPtrArray;
}
#if CACHE_ALL_COLORMAP_TO_RAM
uint8_t ramColorMap[256 * 34]; // this gives only marginal gain: about 500 us per frame on average.
#endif
void initImmutableFlashRegion()
{
    // use a temporary region in RAM
    p_wad_immutable_flash_data = Z_Malloc(sizeof(*p_wad_immutable_flash_data), PU_STATIC, NULL);
    spiFlashSetAddress(EXT_FLASH_BASE);         // just to init SPI address
    // create temporary lumpptr
    lumpPtrArrayStoredInFlash = false;
    lumpPtrArray = NULL; // this will force all cache functions to look everytime from the external flash
    // get wad size
    currentImmutableFlashOffset = 0;
    spiFlashSetAddress((uint32_t) p_doom_iwad_len);
    spiFlashGetData(&p_wad_immutable_flash_data->wadSize, 4);
    spiFlashSetAddress((uint32_t) doom_iwad);
    // get wad header (includes also number of lumps and lump table offset)
    spiFlashGetData(&p_wad_immutable_flash_data->wadHeader, sizeof(p_wad_immutable_flash_data->wadHeader));
    // set size of this structure
    p_wad_immutable_flash_data->immutableDataLength = sizeof(wad_immutable_flash_data_t);
    currentImmutableFlashOffset = sizeof(wad_immutable_flash_data_t);
    // get the palette
    p_wad_immutable_flash_data->palette_lump = writeLumpToFlashRegion(W_GetNumForName("PLAYPAL"), FLASH_IMMUTABLE_REGION, true);
    // get the colormap
    p_wad_immutable_flash_data->colormaps = writeLumpToFlashRegion(W_GetNumForName("COLORMAP"), FLASH_IMMUTABLE_REGION, true);
    //
    #if CACHE_ALL_COLORMAP_TO_RAM
        memcpy (ramColorMap, p_wad_immutable_flash_data->colormaps, sizeof (ramColorMap));
        p_wad_immutable_flash_data->colormaps = ramColorMap;
    #endif
}
wad_level_flash_data_t* initLevelFlashRegion()
{
    p_wad_level_flash_data = Z_Malloc(sizeof(*p_wad_level_flash_data), PU_STATIC, NULL);
    memset(p_wad_level_flash_data, 0, sizeof(*p_wad_level_flash_data));
    currentLevelFlashOffset = sizeof(*p_wad_level_flash_data);
    return p_wad_level_flash_data;
}
void initLumpPtrTable(void)
{
    printf("Init ram lump table\r\n");
    lumpPtrArray = Z_Malloc(getNumberOfLumps() * sizeof(void*), PU_STATIC, NULL);
    // init to 0xFF
    memset(lumpPtrArray, 0xFF, getNumberOfLumps() * sizeof(void*));
    //
    lumpPtrArrayStoredInFlash = false;
    // update - just for sake of completeness, but we actually won't use it - colormaps and playpal lumps
    lumpPtrArray[W_GetNumForName("COLORMAP")] = p_wad_immutable_flash_data->colormaps;
    lumpPtrArray[W_GetNumForName("PLAYPAL")] = p_wad_immutable_flash_data->palette_lump;
    //
}
void getFileLumpByNum(int n, filelump_t *fl)
{
    spiFlashSetAddress(WAD_ADDRESS + p_wad_immutable_flash_data->wadHeader.infotableofs + n * sizeof(filelump_t));
    spiFlashGetData(fl, sizeof(filelump_t));
}
// get how much space we have in flash, taking into account if we already stored the lump pointer table or not.
int getUserFlashRegionRemainingSpace()
{
    int size = FLASH_CACHE_REGION_SIZE - (currentLevelFlashOffset + ((currentImmutableFlashOffset + FLASH_BLOCK_SIZE - 1) & ~(FLASH_BLOCK_SIZE - 1)));
    // we need to take into account that we will need to store also the lumpPtrArray.
    if (lumpPtrArrayStoredInFlash == 0)
        size -= getNumberOfLumps() * sizeof(void*);
    if (size < 0)
        return 0;
    return size;
}

void flashErasePage(uint32_t *pageAddress)
{
    nh_disable_irq();
    // enable write
    // Write enable
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}
    NRF_NVMC->ERASEPAGE = (uint32_t) pageAddress;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}
    NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}

    nh_enable_irq();
}
void programFlashWord(uint32_t *address, uint32_t word)
{
    if (*address == 0xFFFFFFFF)
    {
        nh_disable_irq();
        // enable write
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}
        // set address
        *address = word;
        __DMB();
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}
        nh_enable_irq();
    }
}
void storeWordToFlash(uint32_t *dest, uint32_t word, uint8_t flashRegion, boolean isHeader)
{
    // first check if dest is compatible, i.e. is the same as sourceo r it is erased
    if (!(*dest == word || *dest == 0xFFFFFFFF))
    {
        // we need to erase before writing. However we need to find the limits...
        uint32_t pageAddress = ((uint32_t) dest) & ~(FLASH_BLOCK_SIZE - 1); // page that will be erased
        uint32_t startCopyAddress = pageAddress;
        uint32_t stopCopyAddress = (uint32_t) dest; // copy up to the previous word.
        // if we are writing in the same page as header, and it is not a header, then we need to skip the first bytes that will be used by the header
        if (pageAddress == FLASH_IMMUTABLE_REGION_ADDRESS && !isHeader)
        {
            startCopyAddress = pageAddress + sizeof(wad_immutable_flash_data_t);
        }
        else if (flashRegion == FLASH_LEVEL_REGION && pageAddress == (uint32_t) p_wad_immutable_flash_data->levelData && !isHeader)
        {
            startCopyAddress = pageAddress + sizeof(wad_level_flash_data_t);
        }
        // now that we know from where to where (stop non inclusive) we need to copy, we can allocate enough buffer
        // note! addresses are 4-byte aligned!
        uint32_t *tmp = Z_Malloc((stopCopyAddress - startCopyAddress), PU_STATIC, NULL);
        for (int i = 0;
                i < (stopCopyAddress - startCopyAddress) / sizeof(uint32_t);
                i++)
        {
            tmp[i] = *((uint32_t*) startCopyAddress + i);
        }
        // erase
        printf("Erasing page 0x%08X Start Copy Addr 0x%08X Stop Copy Addr 0x%08X\r\n. ", pageAddress, startCopyAddress, stopCopyAddress);
        flashErasePage((uint32_t*) pageAddress);
        // write back everything
        for (int i = 0;
                i < (stopCopyAddress - startCopyAddress) / sizeof(uint32_t);
                i++)
        {
            programFlashWord(((uint32_t*) startCopyAddress + i), tmp[i]);
        }
        // free buffer
        Z_Free(tmp);
    }
    // write word
    if (!(*dest == word))
    {
        if (*dest == 0xFFFFFFFF)
            programFlashWord(dest, word);
        else
        {
            printf("trying to overwrite a non blank word: addr: 0x%08X old: 0x%08X new 0x%08X. Is Header: %d flash Region %d Blocking", dest, *dest, word, isHeader, flashRegion);
            while (1);
        }
    }
    //
}
/**
 * @brief write the buffer to the flash region
 */

void* writeBufferToFlashRegion(void *buffer, int size, uint8_t flashRegion, boolean updateSize)
{
    // todo check and eventually erase
    uint32_t *address;
    size = (size + FLASH_ALIGNMENT - 1) & ~(FLASH_ALIGNMENT - 1);
    if (flashRegion == FLASH_IMMUTABLE_REGION)
    {
        address = (uint32_t*) (FLASH_ADDRESS + currentImmutableFlashOffset);
    }
    else
    {
        address = (uint32_t*) (FLASH_ADDRESS + currentLevelFlashOffset + ((currentImmutableFlashOffset + FLASH_BLOCK_SIZE - 1) & ~(FLASH_BLOCK_SIZE - 1)));
    }
    uint32_t *s = buffer;
    uint32_t *d = address;
    for (int i = 0; i < size / 4; i++)
    {
        storeWordToFlash(d++, *s++, flashRegion, false);
    }
    if (flashRegion == FLASH_IMMUTABLE_REGION)
    {
        currentImmutableFlashOffset += size;
    }
    else
    {
        currentLevelFlashOffset += size;
    }

    if (updateSize)
    {
        if (flashRegion == FLASH_IMMUTABLE_REGION)
        {
            p_wad_immutable_flash_data->immutableDataLength += size;
        }
        else
        {
            p_wad_level_flash_data->dataLength += size;
        }

    }
    return address;
}

void updateLumpAddresses(int lump, void *address)
{
    if (lumpPtrArray)
    {
        if (lumpPtrArrayStoredInFlash)
        {
            printf("Updating lump %d, address 0x%08X\r\n", lump, address);
            if ((uint32_t) lumpPtrArray[lump] != 0xFFFFFFFF)
            {
                if (lumpPtrArray[lump] == address)
                    printf("Same lump address stored\r\n");
                else
                {
                    printf("Different lump address! old: 0x%08X new: 0x%08X. Blocking \r\n", lumpPtrArray[lump], address);
                    while (1);
                }
            }
            else
            {
                printf("Programming\r\n");
                programFlashWord((uint32_t*) &lumpPtrArray[lump], (uint32_t) address);
            }
            printf("Value now in Flash: 0x%08X\r\n", lumpPtrArray[lump]);
        }
        else
            lumpPtrArray[lump] = address;
    }
}
void* writeLumpToFlashRegion(int lumpnum, uint8_t flashRegion, boolean updateSize)
{
    uint32_t *address = 0;
    filelump_t fl;
    getFileLumpByNum(lumpnum, &fl);
    // adjust to boundary
    fl.size = (fl.size + FLASH_ALIGNMENT - 1) & ~(FLASH_ALIGNMENT - 1);

    // will it fit in flash?
    if (fl.size > getUserFlashRegionRemainingSpace() && flashRegion != FLASH_IMMUTABLE_REGION)
    {
        address = (uint32_t*) (WAD_ADDRESS + fl.filepos);
        updateLumpAddresses(lumpnum, address);
        return address;
    }
    spiFlashSetAddress(WAD_ADDRESS + fl.filepos);
    for (int i = 0; i < fl.size / 4; i++)
    {
        uint32_t data;
        spiFlashGetData(&data, 4);
        uint32_t *a = writeBufferToFlashRegion(&data, 4, flashRegion, updateSize);
        if (i == 0)
        {
            address = a;
        }
    }
    updateLumpAddresses(lumpnum, address);
    return address;
}
/*
 * @brief stores to flash the wad_immutable_flash_data_t.
 * This function shall be called after all immutable data has been stored
 * This also updates the position of where level data will start
 * @param
 * @return the address where it has been stored.
 */
void* storeFlashImmutableValues()
{
    // store header
    uint32_t *src = (uint32_t*) p_wad_immutable_flash_data;
    // update pointer to where the level data are going to be stored

    p_wad_immutable_flash_data->levelData = (wad_level_flash_data_t*) ((uint8_t*) FLASH_ADDRESS + ((p_wad_immutable_flash_data->immutableDataLength + FLASH_BLOCK_SIZE - 1) & ~(FLASH_BLOCK_SIZE - 1)));
    for (int i = 0; i < sizeof(*p_wad_immutable_flash_data) / sizeof(uint32_t);
            i++)
    {
        storeWordToFlash((uint32_t*) FLASH_ADDRESS + i, src[i], FLASH_IMMUTABLE_REGION, true);
    }
    // free block
    Z_Free(p_wad_immutable_flash_data);
    p_wad_immutable_flash_data = (void*) FLASH_ADDRESS;
    return (uint32_t*) FLASH_ADDRESS;
}
void* storeLevelValues(boolean levelChanged)
{
    if (levelChanged)
    {
        // store header
        uint32_t *src = (uint32_t*) p_wad_level_flash_data;
        for (int i = 0; i < sizeof(*p_wad_level_flash_data) / sizeof(uint32_t);
                i++)
        {
            storeWordToFlash((uint32_t*) p_wad_immutable_flash_data->levelData + i, src[i], FLASH_LEVEL_REGION, true);
        }
    }
    printf("Freeing p_wad_level_flash_data %0x\r\n", (uint32_t) p_wad_level_flash_data);
    Z_Free(p_wad_level_flash_data);
    return (uint32_t*) p_wad_immutable_flash_data->levelData;
}
//
// GLOBALS
//
void ExtractFileBase(const char *path, char *dest)
{
    const char *src = path + strlen(path) - 1;
    int length;

    // back up until a \ or the start
    while (src != path && src[-1] != ':' // killough 3/22/98: allow c:filename
    && *(src - 1) != '\\' && *(src - 1) != '/')
    {
        src--;
    }

    // copy up to eight characters
    memset(dest, 0, 8);
    length = 0;

    while ((*src) && (*src != '.') && (++length < 9))
    {
        *dest++ = toupper(*src);
        src++;  //nh fixed, it was *src++.
    }
    /* cph - length check removed, just truncate at 8 chars.
     * If there are 8 or more chars, we'll copy 8, and no zero termination
     */
}

//
// LUMP BASED ROUTINES.
//

//
// W_AddFile
// All files are optional, but at least one file must be
//  found (PWAD, if all required lumps are present).
// Files with a .wad extension are wadlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
//
// Reload hack removed by Lee Killough
// CPhipps - source is an enum
//
// proff - changed using pointer to wadfile_info_t
static void W_AddFile()
{

    wadinfo_t header;
    spiFlashSetAddress((uint32_t) p_doom_iwad_len);
    uint32_t length;
    spiFlashGetData(&length, sizeof(length));
    spiFlashSetAddress((uint32_t) doom_iwad);
    if (length)
    {
        spiFlashGetData(&header, sizeof(header));
        if (strncmp(header.identification, "IWAD", 4))
            I_Error("W_AddFile: Wad file doesn't have IWAD id");

        _g->numlumps = header.numlumps;
    }
}

//Return -1 if not found.
//Set lump ptr if found.
int lumpByNameRequest;
static int PUREFUNC FindLumpByName(const char *name, const filelump_t **lump)
{
    lumpByNameRequest++;
    const wadinfo_t *header = (const wadinfo_t*) WAD_ADDRESS;

    spiFlashSetAddress((uint32_t) p_doom_iwad_len);

    uint32_t wadsize;
    spiFlashGetData(&wadsize, sizeof(wadsize));

    if (wadsize)
    {
        spiFlashSetAddress((uint32_t) &header->infotableofs);

        int infotableofs;
        spiFlashGetData(&infotableofs, sizeof(infotableofs));

        filelump_t fileinfo;
        int_64_t nameint = 0;
        strncpy((char*) &nameint, name, 8);
#if 0
        // next-hack: why is the "for" reversed? Oh, now I read the
        // old doom code: to give priority to patches. However, reading
        // in reverse order will kill speed drastically (some menu were drawn
        // with single digit, generating also audio issues. Now we got 20fps.
        for(int i = _g->numlumps - 1; i >= 0; i--)
        {
            //This is a bit naughty with alignment.
            //For x86 doesn't matter because unaligned loads
            //are fine.
            //On ARM, unaligned loads are not fine but since it
            //doesn't have a 64bit load, the compiler will generate
            //32 bit loads. These vars are 32 aligned.
            spiFlashSetAddress(infotableofs + WAD_ADDRESS + i * sizeof(filelump_t));
            spiFlashGetData(&fileinfo, sizeof (filelump_t));
            int_64_t nameint2 = *(int_64_t*)fileinfo.name;

            if(nameint == nameint2)
            {
                *lump = (filelump_t*) (infotableofs + WAD_ADDRESS) + i;
                return i;
            }
        }
#else
        spiFlashSetAddress(infotableofs + WAD_ADDRESS);
        for (int i = 0; i < _g->numlumps; i++)
        {
            //This is a bit naughty with alignment.
            //For x86 doesn't matter because unaligned loads
            //are fine.
            //On ARM, unaligned loads are not fine but since it
            //doesn't have a 64bit load, the compiler will generate
            //32 bit loads. These vars are 32 aligned.
            spiFlashGetData(&fileinfo, sizeof(filelump_t));
            int_64_t nameint2 = *(int_64_t*) fileinfo.name;

            if (nameint == nameint2)
            {
                *lump = (filelump_t*) (infotableofs + WAD_ADDRESS) + i;
                return i;
            }
        }
#endif
    }
    else
    {
        printf("Zero Wad Size!\r\n");
    }
    *lump = NULL;
    return -1;
}

static const filelump_t* PUREFUNC FindLumpByNum(int num)
{
    wadinfo_t header;
    if (num < 0)
        return NULL;
    spiFlashSetAddress((uint32_t) p_doom_iwad_len);
    uint32_t wadLength;
    spiFlashGetData(&wadLength, sizeof(uint32_t));
    if (wadLength)
    {
        spiFlashSetAddress((uint32_t) doom_iwad);
        spiFlashGetData(&header, sizeof(header));

        if (num >= header.numlumps)
        {
            return NULL;
        }
        return (filelump_t*) (WAD_ADDRESS + header.infotableofs + num * sizeof(filelump_t));
    }
    return NULL;
}

//
// W_CheckNumForName
// Returns -1 if name not found.
//
// Rewritten by Lee Killough to use hash table for performance. Significantly
// cuts down on time -- increases Doom performance over 300%. This is the
// single most important optimization of the original Doom sources, because
// lump name lookup is used so often, and the original Doom used a sequential
// search. For large wads with > 1000 lumps this meant an average of over
// 500 were probed during every search. Now the average is under 2 probes per
// search. There is no significant benefit to packing the names into longwords
// with this new hashing algorithm, because the work to do the packing is
// just as much work as simply doing the string comparisons with the new
// algorithm, which minimizes the expected number of comparisons to under 2.
//
// killough 4/17/98: add namespace parameter to prevent collisions
// between different resources such as flats, sprites, colormaps
//

int PUREFUNC W_CheckNumForName(const char *name)
{
    const filelump_t *lump = NULL;

    return FindLumpByName(name, &lump);
}

// W_GetNumForName
// Calls W_CheckNumForName, but bombs out if not found.
//
int PUREFUNC W_GetNumForName(const char *name)     // killough -- const added
{
    int i = W_CheckNumForName(name);

    if (i == -1)
        I_Error("W_GetNumForName: %.8s not found", name);

    return i;
}

const char* PUREFUNC W_GetNameForNum(int lump)
{
    const filelump_t *l = FindLumpByNum(lump);
    if (l)
    {
        return l->name;
    }

    return NULL;
}

// W_Init
// Loads each of the files in the wadfiles array.
// All files are optional, but at least one file
//  must be found.
// Files with a .wad extension are idlink files
//  with multiple lumps.
// Other files are single lumps with the base filename
//  for the lump name.
// Lump names can appear multiple times.
// The name searcher looks backwards, so a later file
//  does override all earlier ones.
//
// CPhipps - modified to use the new wadfiles array
//

void W_Init(void)
{
    // CPhipps - start with nothing

    _g->numlumps = 0;

    W_AddFile();

    if (!_g->numlumps)
        I_Error("W_Init: No files found");
}

//
// W_LumpLength
// Returns the buffer size needed to load the given lump.
//

int PUREFUNC W_LumpLength(int lump)
{
    const filelump_t *l = FindLumpByNum(lump);

    if (l)
    {
        spiFlashSetAddress((uint32_t) &l->size);
        int size;
        spiFlashGetData(&size, sizeof(size));
        return size;
    }

    I_Error("W_LumpLength: %i >= numlumps", lump);

    return 0;
}

static const void* PUREFUNC W_GetLumpPtr(int lump)
{
    const filelump_t *l = FindLumpByNum(lump);

    if (l)
    {
        spiFlashSetAddress((uint32_t) &l->filepos);
        int filepos;
        spiFlashGetData(&filepos, sizeof(filepos));
        return (const uint8_t*) WAD_ADDRESS + filepos;
    }
    return NULL;
}
void* getAddressOrCacheLumpNum(int lump, boolean storeInFlash, uint8_t flashRegion)
{
//  printf("Get addr: %d 0x%08x 0x%08x Lump Array in Flash %s\r\n", lump, lumpPtrArray[lump], lumpPtrArray, lumpPtrArrayStoredInFlash ? "yes" : "no");
    if (lump > _g->numlumps)
    {
        printf("Attempt to load a non existent lump. %d Blocking\r\n", lump);
        while (1);
    }
    if (lumpPtrArray == NULL && !storeInFlash)
    {
        return (void*) W_GetLumpPtr(lump);
    }
    //
    if (lumpPtrArray[lump] != (void*) 0xFFFFFFFF && lumpPtrArray[lump])
        return lumpPtrArray[lump];

    void *address;
    if (storeInFlash)
    {
        printf("Storing in flash\r\n");
        address = writeLumpToFlashRegion(lump, flashRegion, true);
    }
    else
    {
        address = (void*) W_GetLumpPtr(lump);
        updateLumpAddresses(lump, address);
    }
    return address;

}

const void* /*PUREFUNC*/W_CacheLumpNum(int lump)
{

    //return   W_GetLumpPtr(lump);
    // printf("Caching lump %d ... ", lump);
    void *addr = getAddressOrCacheLumpNum(lump, false, 0);
    // printf("cached to --> 0x%08x\r\n",  addr);
    return addr;
}
