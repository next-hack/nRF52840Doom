/*
   MCUDoomWadutil by Nicola Wrachien.

   Based on doomhack's GBAWadutil
   Original source: https://github.com/doomhack/GbaWadUtil

   This command line utility allows to convert your wad file
   to a format convenient for MCU-based Doom ports.

   Usage:
   MCUDoomWadutil <inputwad> <outputwad>
*/
#include "wadprocessor.h"
#include "doomtypes.h"
#include "wadfile.h"
#include <string.h>
#include <ctype.h>
static void convertCompositeTextures(wadfile_t *wadfile);
static bool processD2Levels(wadfile_t *wadfile);
static bool processD1Levels(wadfile_t *wadfile);

static bool processLevel(wadfile_t *wadfile, uint32_t lumpNum);

static bool processVertexes(wadfile_t *wadfile, uint32_t lumpNum);
static bool processLines(wadfile_t *wadfile, uint32_t lumpNum);
static bool processSegs(wadfile_t *wadfile, uint32_t lumpNum);
static bool processSides(wadfile_t *wadfile, uint32_t lumpNum);
static bool processPatches(wadfile_t *wadfile, int firstLump, int lastLump);
static void removeUnusedPatches(wadfile_t *wadfile);
static bool processPNames();
static bool removeUnusedLumps(wadfile_t *wadfile, bool removeSound);

int getTextureNumForName(wadfile_t *wadfile, const char* tex_name);

bool processWad(wadfile_t *wadfile, bool removeSound, bool convertPatches)
{
    removeUnusedLumps(wadfile, removeSound);
    convertCompositeTextures(wadfile);
    //
    removeUnusedPatches(wadfile);
    if (!convertPatches)
    {
        printf("WARNING, NOT CONVERTING PATCHES\r\n");
    }
    else
    {
       // for each patch, change it. Note, marksers are skipped.
        int firstLump = getLumpNumByName(wadfile, "HELP1");
        if (firstLump == -1)    // for DOOM2
            firstLump = getLumpNumByName(wadfile, "HELP");
        int lastLump = getLumpNumByName(wadfile, "P_END");
        processPatches(wadfile, firstLump, lastLump);
        // process gba specific patches, added at the end
        firstLump = getLumpNumByName(wadfile, "M_ARUN");
        lastLump = getLumpNumByName(wadfile, "STGANUM9") + 1;

        processPatches(wadfile, firstLump, lastLump);
    }
    int lumpNum = getLumpNumByName(wadfile, "MAP01");
    //Figure out if our IWAD is Doom or Doom2. (E1M1 or MAP01)
    if(lumpNum != -1)
    {
        printf("Doom 2 level found\r\n");
        return processD2Levels(wadfile);
    }
    else
    {
        int lumpNum = getLumpNumByName(wadfile, "E1M1");

        //Can't find any maps.
        if(lumpNum == -1)
        {
            printf("No maps found\r\n");
            return false;
        }
        printf("Doom 1 level found\r\n");
    }

    return processD1Levels(wadfile);
}
bool processD2Levels(wadfile_t *wadfile)
{
    for(int m = 1; m <= 32; m++)
    {
        char mapName[9];
        snprintf(mapName, sizeof(mapName), "MAP%02d",m);
        int lumpNum = getLumpNumByName(wadfile, mapName);

        if(lumpNum != -1)
        {
            processLevel(wadfile, lumpNum);
        }
    }
    return true;
}

bool processD1Levels(wadfile_t *wadfile)
{
    for(int e = 1; e <= 4; e++)
    {
        for(int m = 1; m <= 9; m++)
        {
            char mapName[9];
            snprintf(mapName, sizeof(mapName), "E%dM%d",e,m);

            int lumpNum = getLumpNumByName(wadfile, mapName);

            if(lumpNum != -1)
            {
                printf("processing %s\r\n", mapName);
                processLevel(wadfile, lumpNum);
            }
            else
            {
                printf("cannot find %s\r\n", mapName);

            }
        }
    }

    return true;
}

bool processLevel(wadfile_t *wadfile, uint32_t lumpNum)
{
    processVertexes(wadfile, lumpNum);
    processLines(wadfile, lumpNum);
    processSegs(wadfile, lumpNum);
    processSides(wadfile, lumpNum);
    processPNames(wadfile);
    return true;
}

bool processVertexes(wadfile_t *wadfile, uint32_t lumpNum)
{
    uint32_t vtxLumpNum = lumpNum + ML_VERTEXES;

    lump_t *vxl;

    vxl = getLumpByNum(wadfile, vtxLumpNum);

    if (NULL == vxl)
        return false;

    if(vxl->lump.size == 0)
        return false;
    // get number of vertex
    uint32_t vtxCount = vxl->lump.size / sizeof(mapvertex_t);
    // allocate new vertex
    vertex_t* newVtx = malloc( sizeof (vertex_t) * vtxCount);
    //
    mapvertex_t* oldVtx = (mapvertex_t*) vxl->data;
    // precalculate vertex
    for(uint32_t i = 0; i < vtxCount; i++)
    {
        newVtx[i].x = (oldVtx[i].x << 16);
        newVtx[i].y = (oldVtx[i].y << 16);
    }
    // allocate new lump info
    lump_t *newVxl = malloc(sizeof (lump_t));
    // copy name
    memcpy(newVxl->lump.name, vxl->lump.name, sizeof (vxl->lump.name));
    // change data and size information
    newVxl->data = newVtx;
    newVxl->lump.size = vtxCount * sizeof(vertex_t);
    // replace lum
    replaceLump(wadfile, newVxl, vtxLumpNum);

    return true;
}

bool processLines(wadfile_t *wadfile, uint32_t lumpNum)
{
    uint32_t lineLumpNum = lumpNum+ML_LINEDEFS;

    lump_t *lines;

    lines = getLumpByNum(wadfile, lineLumpNum);

    if (NULL == lines)
        return false;

    if(lines->lump.size == 0)
        return false;

    uint32_t lineCount = lines->lump.size / sizeof(maplinedef_t);

    line_t* newLines = malloc (sizeof(line_t) * lineCount);

    maplinedef_t* oldLines = (maplinedef_t*) lines->data;

    //We need vertexes for this...

    uint32_t vtxLumpNum = lumpNum+ML_VERTEXES;

    lump_t *vxl;

    vxl = getLumpByNum(wadfile, vtxLumpNum);

    if (NULL == vxl)
        return false;

    if(vxl->lump.size == 0)
        return false;

    vertex_t* vtx = (vertex_t*)vxl->data;

    for(unsigned int i = 0; i < lineCount; i++)
    {
        newLines[i].v1.x = vtx[oldLines[i].v1].x;
        newLines[i].v1.y = vtx[oldLines[i].v1].y;

        newLines[i].v2.x = vtx[oldLines[i].v2].x;
        newLines[i].v2.y = vtx[oldLines[i].v2].y;

        newLines[i].special = oldLines[i].special;
        newLines[i].flags = oldLines[i].flags;
        newLines[i].tag = oldLines[i].tag;

        newLines[i].dx = newLines[i].v2.x - newLines[i].v1.x;
        newLines[i].dy = newLines[i].v2.y - newLines[i].v1.y;

        newLines[i].slopetype =
                !newLines[i].dx ? ST_VERTICAL : !newLines[i].dy ? ST_HORIZONTAL :
                FixedDiv(newLines[i].dy, newLines[i].dx) > 0 ? ST_POSITIVE : ST_NEGATIVE;

        newLines[i].sidenum[0] = oldLines[i].sidenum[0];
        newLines[i].sidenum[1] = oldLines[i].sidenum[1];

        newLines[i].bbox[BOXLEFT] = (newLines[i].v1.x < newLines[i].v2.x ? newLines[i].v1.x : newLines[i].v2.x);
        newLines[i].bbox[BOXRIGHT] = (newLines[i].v1.x < newLines[i].v2.x ? newLines[i].v2.x : newLines[i].v1.x);

        newLines[i].bbox[BOXTOP] = (newLines[i].v1.y < newLines[i].v2.y ? newLines[i].v2.y : newLines[i].v1.y);
        newLines[i].bbox[BOXBOTTOM] = (newLines[i].v1.y < newLines[i].v2.y ? newLines[i].v1.y : newLines[i].v2.y);

        newLines[i].lineno = i;

    }


    // allocate new lump info
    lump_t *newLine = malloc(sizeof (lump_t));
    // copy name
    memcpy(newLine->lump.name, lines->lump.name, sizeof (lines->lump.name));
    // change data and size information
    newLine->data = newLines;
    newLine->lump.size = lineCount * sizeof(line_t);
    // replace lum
    replaceLump(wadfile, newLine, lineLumpNum);

    return true;
}

bool processSegs(wadfile_t *wadfile, uint32_t lumpNum)
{
    uint32_t segsLumpNum = lumpNum+ML_SEGS;

    lump_t *segs;

    segs = getLumpByNum(wadfile, segsLumpNum);

    if (NULL == segs)
        return false;

    if(segs->lump.size == 0)
        return false;

    uint32_t segCount = segs->lump.size / sizeof(mapseg_t);

    seg_t* newSegs = malloc(sizeof(seg_t) * segCount);

    mapseg_t* oldSegs = (mapseg_t*) segs->data;

    //We need vertexes for this...

    uint32_t vtxLumpNum = lumpNum+ML_VERTEXES;

    lump_t *vxl;

    vxl = getLumpByNum(wadfile, vtxLumpNum);

    if (NULL == vxl)
        return false;

    if(vxl->lump.size == 0)
        return false;

    vertex_t* vtx = (vertex_t*)vxl->data;

    //And LineDefs. Must process lines first.

    uint32_t linesLumpNum = lumpNum+ML_LINEDEFS;

    lump_t * lxl;

    lxl = getLumpByNum(wadfile, linesLumpNum);

    if (NULL == lxl)
        return false;

    if(lxl->lump.size == 0)
        return false;


    line_t* lines = (line_t *) lxl->data;

    //And sides too...

    uint32_t sidesLumpNum = lumpNum+ML_SIDEDEFS;

    lump_t * sxl;

    sxl = getLumpByNum(wadfile, sidesLumpNum);

    if (NULL == sxl)
        return false;

    if(sxl->lump.size == 0)
        return false;

    mapsidedef_t* sides = (mapsidedef_t*) sxl->data;


    //****************************

    for(unsigned int i = 0; i < segCount; i++)
    {
        newSegs[i].v1.x = vtx[oldSegs[i].v1].x;
        newSegs[i].v1.y = vtx[oldSegs[i].v1].y;

        newSegs[i].v2.x = vtx[oldSegs[i].v2].x;
        newSegs[i].v2.y = vtx[oldSegs[i].v2].y;

        newSegs[i].angle = oldSegs[i].angle << 16;
        newSegs[i].offset = oldSegs[i].offset << 16;

        newSegs[i].linenum = oldSegs[i].linedef;

        const line_t* ldef = &lines[newSegs[i].linenum];

        int side = oldSegs[i].side;

        newSegs[i].sidenum = ldef->sidenum[side];

        if(newSegs[i].sidenum != NO_INDEX)
        {
            newSegs[i].frontsectornum = sides[newSegs[i].sidenum].sector;
        }
        else
        {
            newSegs[i].frontsectornum = NO_INDEX;
        }

        newSegs[i].backsectornum = NO_INDEX;

        if(ldef->flags & ML_TWOSIDED)
        {
            if(ldef->sidenum[side^1] != NO_INDEX)
            {
                newSegs[i].backsectornum = sides[ldef->sidenum[side^1]].sector;
            }
        }
    }


     // allocate new lump info
    lump_t *newSeg = malloc(sizeof (lump_t));
    // copy name
    memcpy(newSeg->lump.name, segs->lump.name, sizeof (segs->lump.name));
    // change data and size information
    newSeg->data = newSegs;
    newSeg->lump.size = segCount * sizeof(seg_t);
    // replace lum
    replaceLump(wadfile, newSeg, segsLumpNum);

    return true;
}

bool processSides(wadfile_t *wadfile, uint32_t lumpNum)
{
    uint32_t sidesLumpNum = lumpNum+ML_SIDEDEFS;

    lump_t *sides;

    sides = getLumpByNum(wadfile, sidesLumpNum);

    if (NULL == sides)
        return false;

    if(sides->lump.size == 0)
        return false;

    uint32_t sideCount = sides->lump.size / sizeof(mapsidedef_t);

    sidedef_t* newSides = malloc(sizeof(sidedef_t) * sideCount);

    mapsidedef_t* oldSides = (mapsidedef_t*) sides->data;

    for(unsigned int i = 0; i < sideCount; i++)
    {
        newSides[i].textureoffset = oldSides[i].textureoffset;
        newSides[i].rowoffset = oldSides[i].rowoffset;

        newSides[i].toptexture = getTextureNumForName(wadfile, oldSides[i].toptexture);
        newSides[i].bottomtexture = getTextureNumForName(wadfile, oldSides[i].bottomtexture);
        newSides[i].midtexture = getTextureNumForName(wadfile, oldSides[i].midtexture);

        newSides[i].sector = oldSides[i].sector;
    }

       // allocate new lump info
    lump_t *newSide = malloc(sizeof (lump_t));
    // copy name
    memcpy(newSide->lump.name, sides->lump.name, sizeof (sides->lump.name));
    // change data and size information
    newSide->data = newSides;
    newSide->lump.size = sideCount * sizeof(sidedef_t);
    // replace lum
    replaceLump(wadfile, newSide, sidesLumpNum);

    return true;
}

int getTextureNumForName(wadfile_t *wadfile, const char* tex_name)
{

    const int  *maptex1, *maptex2;
    int  numtextures1, numtextures2 = 0;
    const int *directory1, *directory2;


    //Convert name to uppercase for comparison.
    char tex_name_upper[9];

    strncpy(tex_name_upper, tex_name, 8);
    tex_name_upper[8] = 0; //Ensure null terminated.

    for (int i = 0; i < 8; i++)
    {
        tex_name_upper[i] = toupper(tex_name_upper[i]);
    }

    lump_t * tex1lump;
    int tex1LumpNum = getLumpNumByName(wadfile, "TEXTURE1");
    tex1lump = getLumpByNum(wadfile, tex1LumpNum);

    maptex1 = (const int*)tex1lump->data;
    numtextures1 = *maptex1;
    directory1 = maptex1+1;

    lump_t * tex2lump;
    int tex2LumpNum = getLumpNumByName(wadfile, "TEXTURE2");

    if (tex2LumpNum != -1)
    {
        tex2lump = getLumpByNum(wadfile, tex2LumpNum);

        maptex2 = (const int*)tex2lump->data;
        directory2 = maptex2+1;
        numtextures2 = *maptex2;
    }
    else
    {
        maptex2 = NULL;
        directory2 = NULL;
    }

    const int *directory = directory1;
    const int *maptex = maptex1;

    int numtextures = (numtextures1 + numtextures2);

    for (int i=0 ; i<numtextures; i++, directory++)
    {
        if (i == numtextures1)
        {
            // Start looking in second texture file.
            maptex = maptex2;
            directory = directory2;
        }

        int offset = *directory;

        const maptexture_t* mtexture = (const maptexture_t *) ( (const uint8_t *)maptex + offset);

        if(!strncmp(tex_name_upper, mtexture->name, 8))
        {
            return i;
        }
    }

    return 0;
}
static uint16_t getColumnLength(column_t* column)
{
    uint8_t *columnData = (uint8_t *) column;
    uint16_t n = 1;
    while (column->topdelta != 0xff)
    {
        // get length
        unsigned int length = column->length;
        n++;
        n += length + 3;
        // get next top delta.
        column = (column_t*) &columnData[n - 1];
    }
    return n;
}
static bool processPatches(wadfile_t *wadfile, int firstLump, int lastLump)
{
    printf("First %d, last %d\r\n", firstLump, lastLump);
    for(uint32_t patchLumpNum = firstLump; patchLumpNum < lastLump; patchLumpNum++)
    {
        // get next patch lump
        lump_t * patchLump = getLumpByNum(wadfile, patchLumpNum);
        if (0 == patchLump->lump.size) // sprites and texture patches share the same format. However they are separted by 0-length markers
            continue;
        // allocate new lump info
        lump_t *newPatchLump = malloc(sizeof (lump_t));
        // create an empty patch whose size is the same as the old patch
        patch_t *newPatch = (patch_t *) malloc (patchLump->lump.size);
        // make an exact copy of the old ump
        memcpy (newPatch, patchLump->data, patchLump->lump.size);
        //
        for (int c = 0; c < newPatch->width; c++)
        {
            // set the length on the upper part of the old columnOfs word.
            int l = (getColumnLength(((uint8_t*)newPatch + newPatch->columnofs[c].offset)) + 3) & ~3;
            if (l > 255)
                printf("WARNING!!! FOUND A LENGTH > 255. (%d) on lump %d\r\n", l, patchLumpNum);
            newPatch->columnofs[c].length = l;
        }
        // copy name
        memcpy(newPatchLump->lump.name, patchLump->lump.name, sizeof (patchLump->lump.name));
        // change data and size informationColumn length
        newPatchLump->data = newPatch;
        newPatchLump->lump.size = patchLump->lump.size;
        // replace lump
        replaceLump(wadfile, newPatchLump, patchLumpNum);



    }
    return true;


}

bool processPNames(wadfile_t *wadfile)
{
    lump_t * pnamesLump;
    int32_t lumpNum = getLumpNumByName(wadfile, "PNAMES");

    if(lumpNum == -1)
        return false;

    pnamesLump = getLumpByNum(wadfile, lumpNum);

    char* pnamesData = (char*)pnamesLump->data;

    uint32_t count = *((uint32_t*)pnamesData);

    pnamesData += 4; //Fist 4 bytes are count.
    // uppercase everything
    for(uint32_t i = 0; i < count; i++)
    {
        bool nullChar = false;
        for (int j = 0; j < 8; j++)
        {
            uint8_t c;
            if (nullChar)
                c = 0;
            else
                c = toupper(pnamesData[i*8 + j]);
            pnamesData[i*8 + j] = c;
            if (0 == c)
            {
                nullChar = true;
            }

        }
    }
    return true;
}
static volatile bool startsWith(char *string, char *prefix)
{
    return strncmp(string, prefix, strlen(prefix)) == 0;
}
bool removeUnusedLumps(wadfile_t *wadfile, bool removeSound)
{
    for(uint32_t i = 0; i < wadfile->header.numlumps; i++)
    {
        lump_t *l;

        l = getLumpByNum(wadfile, i);
        char *name = l->lump.name;
        if (startsWith(name, "D_") ||
           startsWith(name, "DP") ||
           (startsWith(name, "DS") && removeSound && !startsWith(name, "DSPISTOL"))||
           startsWith(name, "GENMIDI"))
        {
            //printf("removing lump %s\r\n", l->lump.name);
            removeLump(wadfile, i);
            i--;
        }
    }
    return true;
}

static inline void R_DrawColumnInCache(const column_t *patch, uint8_t *cache, int originy, int cacheheight)
{
    while (patch->topdelta != 0xff)
    {
        const uint8_t *source = (const uint8_t*) patch + 3;
        int count = patch->length;
        int position = originy + patch->topdelta;

        if (position < 0)
        {
            count += position;
            position = 0;
        }

        if (position + count > cacheheight)
            count = cacheheight - position;

        if (count > 0)
            memcpy(cache + position, source, count);

        patch = (const column_t*) ((const uint8_t*) patch + patch->length + 4);
    }
}



const uint8_t* R_ComposeColumn(wadfile_t *wadfile, const maptexture_t *tex, int xc, uint8_t* columnCacheBuffer)
{
    unsigned int i = 0;
    unsigned int patchcount = tex->patchcount;
    do
    {
        // note: all texetures and patch addresses are cached in flash.
        const mappatch_t *patch = &tex->patches[i];

        // get pnames
        int32_t lumpNum = getLumpNumByName(wadfile, "PNAMES");

        if(lumpNum == -1)
        {
            printf("Error, cannot find pnames\r\n");
            return NULL;
        }

        lump_t *pnamesLump = getLumpByNum(wadfile, lumpNum);

        char* pnames = (char*)pnamesLump->data + 4;

        int patchLumpNum = getLumpNumByName(wadfile, &pnames[patch->patch * 8]);

        if (patchLumpNum == -1)
        {
            printf("Error, cannot find patch lump\r\n");
            return NULL;
        }
        lump_t *patchLump = getLumpByNum(wadfile, patchLumpNum);

        const patch_t *realpatch = patchLump->data;

        const int x1 = patch->originx;

        if (xc < x1)
            continue;
        short width;
        width = realpatch->width;
        const int x2 = x1 + width;

        if (xc < x2)
        {
            const column_t *patchcol;
            patchcol = (const column_t*) ((const uint8_t*) realpatch + (0xFFFFFF & realpatch->columnofs[xc - x1].offset));
            R_DrawColumnInCache(patchcol, columnCacheBuffer, patch->originy, tex->height);
        }
    } while (++i < patchcount);
    return columnCacheBuffer;
}
void convertRectangularBitmapToPatch(lump_t * lump, uint8_t *textureBuffer, int width, int height)
{
    // patch header: 8 + columnofs[width]
    int size = 8 + 4 * width;
    // add topdelta, length, pad, data[heigth], pad, topdelta
    size += (1 + 1 + 1 + height + 1 + 1) * width;
    //
    patch_t *patchData = malloc(size);
    // already set lump size and data ptr
    lump->lump.size = size;
    lump->data = patchData;
    // set up size etc
    patchData->leftoffset = 0;
    patchData->topoffset = 0;
    patchData->width = width;
    patchData->height = height;
    // set column offsets and copy data
    for (int i = 0; i < width; i++)
    {
        patchData->columnofs[i].offset = 8 + 4 * width + (1 + 1 + 1 + height + 1 + 1) * i;
        uint8_t *data = (uint8_t*)patchData + patchData->columnofs[i].offset;
        *data++ = 0; // top delta
        *data++ = height; // post length
        *data++ = 0; // pad
        memcpy(data, &textureBuffer[i * height], height);
        data += height;
        *data++ = 0;      // pad
        *data++ = 0xFF;   // last top delta
    }
}
int addPatchToPnames(wadfile_t *wadfile, char *name)
{
    lump_t * pnamesLump;
    int32_t lumpNum = getLumpNumByName(wadfile, "PNAMES");

    if(lumpNum == -1)
        return 0;

    pnamesLump = getLumpByNum(wadfile, lumpNum);

    uint8_t *newPnamesData = malloc(pnamesLump->lump.size + 8);

    memcpy (newPnamesData, pnamesLump->data, pnamesLump->lump.size);

    uint32_t count = *((uint32_t*)pnamesLump->data);

    // increase count
    *((uint32_t*)newPnamesData) = count + 1;

    // copy new name
    memcpy (newPnamesData + 4 + count * 8, name, 8);

    // allocate new lump info
    lump_t *newPnames = malloc(sizeof (lump_t));
    // copy name
    memcpy(newPnames->lump.name, pnamesLump->lump.name, sizeof (pnamesLump->lump.name));
    // change data and size information
    newPnames->data = newPnamesData;
    // increase size by 8, i.e. new name
    newPnames->lump.size = pnamesLump->lump.size + 8;
    // replace lum
    replaceLump(wadfile, newPnames, lumpNum);
    return count;
}

static void convertCompositeTextures(wadfile_t *wadfile)
{
    lump_t * pnamesLump;
    int32_t lumpNum = getLumpNumByName(wadfile, "PNAMES");

    if(lumpNum == -1)
        return;

    pnamesLump = getLumpByNum(wadfile, lumpNum);

    char* pnamesData = (char*)pnamesLump->data;

    uint32_t count = *((uint32_t*)pnamesData);

    pnamesData += 4; //Fist 4 bytes are count.

    const int *maptex1, *maptex2;
    int numtextures1, numtextures2;
    const int *directory1, *directory2;
    // get texture lump
    lumpNum = getLumpNumByName(wadfile, "TEXTURE1");
    // load it
    maptex1 = getLumpByNum(wadfile, lumpNum)->data;
    //
    numtextures1 = *maptex1;

    directory1 = maptex1 + 1;


    if ( (lumpNum = getLumpNumByName(wadfile, "TEXTURE2")) != -1)
    {
        maptex2 = getLumpByNum(wadfile, lumpNum)->data;
        numtextures2 = *maptex2;
        directory2 = maptex2 + 1;
    }
    else
    {
        maptex2 = NULL;
        numtextures2 = 0;
        directory2 = NULL;
    }

    int offset = 0;

    // now scan all the textures
    uint32_t size = 0;
    for (int texture_num = 0; texture_num < numtextures2 + numtextures1; texture_num++)
    {
        const int *maptex = maptex1;
        if (texture_num < numtextures1)
        {
            offset = directory1[texture_num];
        }
        else if (maptex2 && ((texture_num - numtextures1) < numtextures2))
        {
            maptex = maptex2;
            offset = directory2[texture_num-numtextures1];
        }
        maptexture_t *mtexture = (maptexture_t*) ((const uint8_t*) maptex + offset);

        // 2022-04-19: removed check with patch < 2, as textures with 1 patches need to be addressed as well, as they might have different offsets.
        // 2022-04-24: this screwed up  masked textures. Modified so that we only change textures with 2 patches or single patch with 0 offset
        // FIXME: this is not  perfect solution anyway, as we might have some masked textures with offset. TO be checked.
        if (mtexture->patchcount < 2 && mtexture->patches[0].originx == 0 && mtexture->patches[0].originy == 0)
            continue;       // we are interested only in composite textures.
        uint8_t columnData[256];    // this should be enough.
        size += mtexture->height * mtexture->width;
        if (mtexture->height & 3)
            printf("Warning, texture %d has height %d\r\n", texture_num, mtexture->height);
        // allocate buffer for texture. Nofte that this has x-y swapped
        uint8_t *textureBuffer = malloc(mtexture->height * mtexture->width);
        for (int x = 0; x < mtexture->width; x++)
        {
            R_ComposeColumn(wadfile,mtexture, x, &textureBuffer[mtexture->height * x]);
        }
        lump_t newPatchLump;
        convertRectangularBitmapToPatch(&newPatchLump ,textureBuffer, mtexture->width, mtexture->height);
        int endPatchLumpNum = getLumpNumByName(wadfile, "P_END") - 1;
        char newName[9];
        snprintf(newName,9, "SPTE%04X",  texture_num); // Note X must be UPPERCASE!
        memcpy(newPatchLump.lump.name, newName, 8);
        printf("New patch added: %d SPTE%04X\r\n", texture_num, texture_num);
        insertLump(wadfile, &newPatchLump, endPatchLumpNum);
        // add entry to pname
        int newPatchNum = addPatchToPnames(wadfile, newPatchLump.lump.name);

        // now change texture data. Since offsets are used, we do not need to worry about resizing.
        mtexture->patchcount = 1;
        mtexture->patches[0].originx = 0;
        mtexture->patches[0].originy = 0;
        mtexture->patches[0].patch = newPatchNum;
        // free textureBuffer
        free(textureBuffer);
        //
    }
    printf("Converting composite will increas size by %d bytes\r\n", size);
}
static void removeUnusedPatches(wadfile_t *wadfile)
{
    // scan for all patches in pnames. For each patch, see if it is listed in any texture
    lump_t * pnamesLump;
    int32_t lumpNum = getLumpNumByName(wadfile, "PNAMES");

    if(lumpNum == -1)
        return;

    pnamesLump = getLumpByNum(wadfile, lumpNum);

    char* pnamesData = (char*)pnamesLump->data;

    uint32_t count = *((uint32_t*)pnamesData);

    pnamesData += 4; //Fist 4 bytes are count.
    //
    // Load texture lumps
    //
    const int *maptex1, *maptex2;
    int numtextures1, numtextures2;
    const int *directory1, *directory2;
    // get texture lump
    lumpNum = getLumpNumByName(wadfile, "TEXTURE1");
    // load it
    maptex1 = getLumpByNum(wadfile, lumpNum)->data;
    //
    numtextures1 = *maptex1;

    directory1 = maptex1 + 1;

    if ( (lumpNum = getLumpNumByName(wadfile, "TEXTURE2")) != -1)
    {
        maptex2 = getLumpByNum(wadfile, lumpNum)->data;
        numtextures2 = *maptex2;
        directory2 = maptex2 + 1;
    }
    else
    {
        maptex2 = NULL;
        numtextures2 = 0;
        directory2 = NULL;
    }

    int offset = 0;

    uint32_t patchRemoved = 0;
    uint32_t sizeSaved = 0;
    // get texture data
    for (int i = 0; i < count; i++)
    {

        bool patchUsed = false;
        for (int texture_num = 0; texture_num < numtextures2 + numtextures1; texture_num++)
        {
            const int *maptex = maptex1;
            if (texture_num < numtextures1)
            {
                offset = directory1[texture_num];
            }
            else if (maptex2 && ((texture_num - numtextures1) < numtextures2))
            {
                maptex = maptex2;
                offset = directory2[texture_num-numtextures1];
            }
            maptexture_t *mtexture = (maptexture_t*) ((const uint8_t*) maptex + offset);
            if (mtexture->patches[0].patch == i)
            {
                patchUsed = true;
                break;
            }
        }
        if (false == patchUsed)
        {
            int patchLumpNum = getLumpNumByName(wadfile, pnamesData);
            if (-1 != patchLumpNum)
            {
                lump_t * lump = getLumpByNum(wadfile, patchLumpNum);
                sizeSaved += lump->lump.size;
                removeLump(wadfile, patchLumpNum);
                patchRemoved++;
            }
        }
        // point to next name
        pnamesData += 8;
    }
    printf("Removed %d patches. Saved %d bytes\r\n", patchRemoved, sizeSaved);
}
