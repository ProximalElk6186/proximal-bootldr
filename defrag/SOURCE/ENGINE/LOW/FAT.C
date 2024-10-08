/*
   Fat.c - FAT manipulation code.
   Copyright (C) 2000 Imre Leber

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   If you have any questions, comments, suggestions, or fixes please
   email me at:  imre.leber@worldonline.be
*/


#include <string.h>

#include "..\..\misc\bool.h"
#include "..\header\rdwrsect.h"
#include "..\header\fat.h"
#include "..\header\boot.h"
#include "..\header\direct.h"
#include "..\header\fatconst.h"
#include "..\header\FSInfo.h"
#include "..\header\FTEMem.h"
#include "..\header\Traversl.h"

/*******************************************************************
**                        GetFatStart
********************************************************************
** returns the starting sector of the first FAT.
********************************************************************/

SECTOR GetFatStart(RDWRHandle handle)
{
    return GetReservedSectors(handle);
}

/*******************************************************************
**                        GetFatLabelSize
********************************************************************
** returns the kind of FAT being used (FAT12, FAT16 or FAT32)
********************************************************************/

int GetFatLabelSize(RDWRHandle handle)
{
    struct BootSectorStruct* boot;
     
    if (handle->FATtype) 
       return handle->FATtype;
    else
    {
       boot = AllocateBootSector();
       if (!boot) return FALSE;
       
       if (!ReadBootSector(handle, boot)) 
       {
          FreeBootSector(boot);
          return FALSE;      
       }
       handle->FATtype = DetermineFATType(boot);
       
       FreeBootSector(boot);
       return handle->FATtype;
    }
}

/*******************************************************************
**                        GetDataAreaStart
********************************************************************
** Returns the start sector of the data area
********************************************************************/

SECTOR GetDataAreaStart(RDWRHandle handle)
{
    int fatlabelsize;
    unsigned short rootcount;
    SECTOR RootDirSectors;
    unsigned long fatsize;
    unsigned short reservedsectors;
    unsigned char NumberOfFats;
    
    fatlabelsize = GetFatLabelSize(handle);
    if (fatlabelsize == FAT32)
    {
       RootDirSectors = 0;
    }
    else
    {
       rootcount = GetNumberOfRootEntries(handle);
       if (!rootcount) return FALSE;
       
       RootDirSectors = (((SECTOR) rootcount * 32) +
                          (BYTESPERSECTOR - 1)) / BYTESPERSECTOR; 
    }
    
    fatsize = GetSectorsPerFat(handle); 
    if (!fatsize) return FALSE;
    
    NumberOfFats = GetNumberOfFats(handle);
    if (!NumberOfFats) return FALSE;
    
    reservedsectors = GetReservedSectors(handle);
    if (!reservedsectors) return FALSE;
    
    return (SECTOR)reservedsectors + 
                   ((SECTOR)fatsize * (SECTOR)NumberOfFats) +
                       (SECTOR) RootDirSectors;
}

/*******************************************************************
**                        DataSectorToCluster
********************************************************************
** Returns the cluster that has the given sector as first sector
** of the cluster.
**
** Notice that the result is off if sector is not the first sector
** of a cluster.
**
** Notice also that there are many cluster values in FAT32 that map on the
** same data sector. This function returns the one that has the highest 4
** bits equal to 0.
********************************************************************/

CLUSTER DataSectorToCluster(RDWRHandle handle, SECTOR sector)
{
    unsigned char sectorspercluster ;
    SECTOR datastart;  
    
    sectorspercluster = GetSectorsPerCluster(handle);
    if (!sectorspercluster) return FALSE;
    
    datastart = GetDataAreaStart(handle);
    if (!datastart) return FALSE;        
        
    return ((sector - datastart) / sectorspercluster + 2) & 0x0fffffffL;   
}

/*******************************************************************
**                        ConvertToDataSector
********************************************************************
** Returns the first sector that belongs to the given cluster.
********************************************************************/

SECTOR ConvertToDataSector(RDWRHandle handle,
			   CLUSTER fatcluster)
{
    unsigned char sectorspercluster ;
    SECTOR datastart;
    
    sectorspercluster = GetSectorsPerCluster(handle);
    if (!sectorspercluster) return FALSE;
    
    datastart = GetDataAreaStart(handle);
    if (!datastart) return FALSE;
        
    return ((FAT_CLUSTER(fatcluster)-2) * sectorspercluster) + datastart;
}

/*******************************************************************
**                        GetFatLabel
********************************************************************
** Private function that returns the label from a buffer that contains
** a part of the FAT.
**
** The FAT is divided into blocks of 3 sectors each. This way we are
** sure that each block start with a complete FAT label.
**
** Each of these blocks thus contains a fixed number of labels from the
** FAT. When we need a label, we calculate the block number and the
** offset in the block for this label. Then we load the block, i.e. 3
** sectors in memory and we take the label at the calculated offset.
********************************************************************/

static BOOL GetFatLabel(RDWRHandle handle,
	 	        char* readbuf, CLUSTER labelnr,
		        CLUSTER* label)
{
    int     labelsize;
    CLUSTER cluster;

    labelsize = GetFatLabelSize(handle);
    if (!labelsize) return FALSE;

    labelnr  = FAT_CLUSTER(labelnr);
    labelnr %= (FATREADBUFSIZE * 8 / labelsize);

    if (labelsize == FAT12)
    {
       cluster = labelnr + (labelnr >> 1);
       memcpy(label, &readbuf[(unsigned) cluster], sizeof(CLUSTER));

       if ((labelnr & 1) == 0)
	  *label &= 0xfff;
       else
	  *label = (*label >> 4) & 0xfff;
    }
    else if (labelsize == FAT16)
    {
       memcpy(label, &readbuf[(unsigned) labelnr << 1], sizeof(CLUSTER));
       *label &= 0xffff;
    }
    else if (labelsize == FAT32)
    {
       memcpy(label, &readbuf[(unsigned) labelnr << 2], sizeof(CLUSTER));
    }
    else
       return FALSE;

    return TRUE;
}

/*******************************************************************
**                        LastFatLabel
********************************************************************
** Returns wether this is the last cluster of some file.
********************************************************************/

static BOOL LastFatLabel(RDWRHandle handle, CLUSTER label)
{
    int labelsize;

    labelsize = GetFatLabelSize(handle);

    if (labelsize == FAT12)
       return (FAT12_LAST(label));
    else if (labelsize == FAT16)
       return (FAT16_LAST(label));
    else if (labelsize == FAT32)
       return (FAT32_LAST(label));

    return FALSE;
}

/*******************************************************************
**                        GeneralizeLabel
********************************************************************
** Converts a file system specific label into a generalized one.
********************************************************************/

static CLUSTER GeneralizeLabel(RDWRHandle handle, CLUSTER label)
{
   int labelsize = GetFatLabelSize(handle);

   if (labelsize == FAT12)
   {
      if (FAT12_FREE(label))     return FAT_FREE_LABEL;
      if (FAT12_BAD(label))      return FAT_BAD_LABEL;
      if (FAT12_LAST(label))     return FAT_LAST_LABEL;
      return label;
   }
   else if (labelsize == FAT16)
   {
      if (FAT16_FREE(label))     return FAT_FREE_LABEL;
      if (FAT16_BAD(label))      return FAT_BAD_LABEL;
      if (FAT16_LAST(label))     return FAT_LAST_LABEL;
      return label;
   }
   else if (labelsize == FAT32)
   {
      if (FAT32_FREE(label))     return FAT_FREE_LABEL;
      if (FAT32_BAD(label))      return FAT_BAD_LABEL;
      if (FAT32_LAST(label))     return FAT_LAST_LABEL;
      return FAT32_CLUSTER(label);
   }
   else
      return FALSE;
}

/*******************************************************************
**                        LinearTraverseFat
********************************************************************
** Calls a function for every label in the FAT.
********************************************************************/

BOOL LinearTraverseFat(RDWRHandle handle,
		      int (*func) (RDWRHandle handle,
				   CLUSTER label,
				   SECTOR datasector,
				   void** structure),
		      void** structure)
{
    int      i, j = 2, iterations, rest;
    SECTOR   fatstart;
    int      toreadsectors;
    unsigned long  toreadlabels, labelsinbuf, dataclusters;
    char     *buffer;

    CLUSTER  label;
    SECTOR   datasector;

    unsigned short sectorsperfat;
    unsigned char  sectorspercluster;

    fatstart = GetFatStart(handle);
    if (!fatstart) return FALSE;

    sectorsperfat     = GetSectorsPerFat(handle);
    if (!sectorsperfat) return FALSE;

    sectorspercluster = GetSectorsPerCluster(handle);
    if (!sectorspercluster) return FALSE;

    dataclusters = GetClustersInDataArea(handle);
    if (!dataclusters) return FALSE;

    iterations = sectorsperfat / SECTORSPERREADBUF;
    rest       = sectorsperfat % SECTORSPERREADBUF;

    //toreadsectors = SECTORSPERREADBUF;
    toreadlabels  = labelsinbuf = FATREADBUFSIZE * 8 / GetFatLabelSize(handle);
        
    buffer = (char*) AllocateSectors(handle, SECTORSPERREADBUF);
    if (!buffer) return FALSE;
    
    for (i = 0; i < iterations + (rest > 0); i++)
    {
	toreadsectors = (i == iterations) ? rest : SECTORSPERREADBUF;
    
        if (ReadSectors(handle, toreadsectors,
			fatstart + (i*SECTORSPERREADBUF), buffer) == -1)
        {
	   FreeSectors((SECTOR*)buffer);
           return FALSE;
        }
           
        for (; j < toreadlabels; j++)
	{
	    if (j >= dataclusters) break;
 
	    if (!GetFatLabel(handle, buffer, j, &label)) 
            {
               FreeSectors((SECTOR*)buffer);
               return FALSE;
            }
            
	    datasector = ConvertToDataSector(handle, j);
	    switch (func (handle, GeneralizeLabel(handle, label), datasector,
			  structure))
	    {
	       case FALSE:
		    FreeSectors((SECTOR*)buffer);
		    return TRUE;
	       case FAIL:
		    FreeSectors((SECTOR*)buffer);
		    return FALSE;
	    }
	}
	toreadlabels += labelsinbuf;
    }
    
    FreeSectors((SECTOR*)buffer);
    return TRUE;
}

/*******************************************************************
**                        FileTraverseFat
********************************************************************
** Calls a function for every cluster in a file.
********************************************************************/

BOOL FileTraverseFat(RDWRHandle handle, CLUSTER startcluster,
		     int (*func) (RDWRHandle handle,
			 	  CLUSTER label,
				  SECTOR  datasector,
				  void** structure),
		     void** structure)
{
    int fatlabelsize;
    char* buffer;
    CLUSTER cluster, seeking, prevpart = -1, gencluster;
    SECTOR  sector;

    SECTOR  fatstart;

    cluster = FAT_CLUSTER(startcluster);
    fatstart = GetFatStart(handle);
    if (!fatstart) return FALSE;

    fatlabelsize = GetFatLabelSize(handle);
    if (!fatlabelsize) return FALSE;
    
    buffer = (char*) AllocateSectors(handle, SECTORSPERREADBUF);
    if (!buffer) return FALSE;

    while (!LastFatLabel(handle, cluster))
    {
	seeking = cluster / (FATREADBUFSIZE * 8 / fatlabelsize);

	if (prevpart != seeking)
	{
	   if (ReadSectors(handle, SECTORSPERREADBUF,
			   fatstart + (unsigned) seeking * SECTORSPERREADBUF,
			   buffer) == -1)
           {
	      FreeSectors((SECTOR*)buffer);
              return FALSE;
           }

	   prevpart = seeking;
	}

	sector = ConvertToDataSector(handle, cluster);
	if (!GetFatLabel(handle, buffer, cluster, &cluster))
        {        
           FreeSectors((SECTOR*)buffer);
           return FALSE;
        }
        
        /*
            A bad cluster in a file chain is actually quite uncommon.
            
            It can only happen when:
            - When the file was written it was not marked as bad.
            - The cluster got bad afterwards
            - A disk scanning utility marked it as bad without relocating
              the cluster.
              
            Still we check for it.
        */
        
        gencluster = GeneralizeLabel(handle, cluster);
        if (FAT_BAD(gencluster))
        {
	   FreeSectors((SECTOR*)buffer);
           return FALSE;           
        }
        
	switch (func(handle, gencluster, sector, structure))
	{
	   case FALSE:
		FreeSectors((SECTOR*)buffer);
		return TRUE;
	   case FAIL:
		FreeSectors((SECTOR*)buffer);
		return FALSE;
	}
    }

    FreeSectors((SECTOR*)buffer);
    return TRUE;
}

/*******************************************************************
**                        ReadFatLabel
********************************************************************
** Reads a label from the FAT at the indicated position.
********************************************************************/

BOOL ReadFatLabel(RDWRHandle handle, CLUSTER labelnr,
		 CLUSTER* label)
{
    int     labelsize;
    char*   buffer;
    SECTOR  sectorstart, blockindex;
    int     labelsinbuf;
    BOOL    retVal;

    labelsize = GetFatLabelSize(handle);
    if (!labelsize) return FALSE;

    sectorstart = GetFatStart(handle);
    if (!sectorstart) return FALSE;

    sectorstart = ((SECTOR) labelnr * labelsize) / (BYTESPERSECTOR * 8);

    labelsinbuf  = FATREADBUFSIZE * 8 / labelsize;
    blockindex   = labelnr / labelsinbuf;
    sectorstart += SECTORSPERREADBUF * blockindex;
    
    buffer = (char*) AllocateSectors(handle, SECTORSPERREADBUF);
    if (!buffer) return FALSE;
    
    if (ReadSectors(handle, SECTORSPERREADBUF, sectorstart, buffer) == -1)
    {
	FreeSectors((SECTOR*)buffer);
        return FALSE;
    }
    
    retVal = GetFatLabel(handle, buffer, labelnr, label);
    
    FreeSectors((SECTOR*)buffer);
    return retVal;
}

/*******************************************************************
**                        FileTraverseFat
********************************************************************
** Writes a label to the FAT at the specified location.
**
** This is the only function that should ever be used to change a fat
** label. This is because it pertains the high four bits of a fat cluster
** in FAT32.
**
** Works for both generalized and none generalized labels.
********************************************************************/
   
int WriteFatLabel(RDWRHandle handle, CLUSTER labelnr,
		  CLUSTER label)
{
    int      retVal;
    int      labelsize;
    char*    buffer;
    SECTOR   sectorstart;
    int      labelsinbuf, temp;
    
    labelsize = GetFatLabelSize(handle);
    if (!labelsize) return FALSE;

    sectorstart = GetFatStart(handle);
    if (!sectorstart) return FALSE;

    labelsinbuf  = FATREADBUFSIZE * 8 / labelsize;
    sectorstart += ((SECTOR) labelnr / labelsinbuf) * SECTORSPERREADBUF;

    buffer = (char*) AllocateSectors(handle, SECTORSPERREADBUF);
    if (!buffer) return FALSE;
   
    if (ReadSectors(handle, SECTORSPERREADBUF, sectorstart, buffer) == -1)
    {
	FreeSectors((SECTOR*)buffer);
        return FALSE;
    }
 
    labelnr %= (FATREADBUFSIZE * 8 / labelsize);   
        
    if (labelsize == FAT12)
    {
       if (FAT_BAD(label)) label = FAT12_BAD_LABEL;
       if (FAT_LAST(label)) label = 0xfff;

       labelnr *= 12;                  /* The nth bit in the block */
       
       if (labelnr % 8 == 0) /* If this bit starts at a byte offset */
       {
	  memcpy(&temp, &buffer[(unsigned) labelnr / 8], 2);
	  temp &= 0xf000;
	  temp |= ((unsigned) label & 0xfff);
	  memcpy(&buffer[(unsigned) labelnr / 8], &temp, 2);
       }
       else
       {
	  memcpy(&temp, &buffer[(unsigned) labelnr / 8], 2);
	  temp &= 0x000f;
	  temp |= ((unsigned) label << 4);
	  memcpy(&buffer[(unsigned) labelnr / 8], &temp, 2);
       }
    }
    else if (labelsize == FAT16)
    {
       if (FAT_BAD(label)) label = FAT16_BAD_LABEL;
       if (FAT_LAST(label)) label = 0xffff;

       memcpy(&buffer[(unsigned) labelnr << 1], &label, 2);
    }
    else if (labelsize == FAT32)
    {
       /* Make sure the high for bits are pertained. */
       CLUSTER old;
       memcpy(&old, &buffer[(unsigned) labelnr << 2], 4);
       
       label &= 0x0fffffffL;
       old   &= 0xf0000000L;
       label += old;
       
       memcpy(&buffer[(unsigned) labelnr << 2], &label, 4); 
    }

    retVal = WriteSectors(handle, SECTORSPERREADBUF,
			  sectorstart, buffer, WR_FAT) != -1;
    FreeSectors((SECTOR*)buffer);                      
    return retVal;
}

/*******************************************************************
**                        GetBytesInFat
********************************************************************
** Returns the number of byts occupied by one FAT.
********************************************************************/

unsigned long GetBytesInFat(RDWRHandle handle)
{
    return ((unsigned long) GetFatLabelSize(handle) *
            (unsigned long) GetClustersInDataArea(handle)) / 8L;
}
