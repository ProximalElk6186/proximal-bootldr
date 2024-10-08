/*    
   NthEntry.c - functions to return the nth entry in a directory.

   Copyright (C) 2002 Imre Leber

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

#include "fte.h"

struct Pipe
{
   /* Input */
   unsigned long n;
   
   /* Processing */
   unsigned long counter;
   
   /* Output */
   struct DirectoryPosition* pos; 
};

static BOOL positiongetter (RDWRHandle handle,
		            struct DirectoryPosition* pos,
			    void** buffer);

/**************************************************************
**                    GetNthDirectoryPosition
***************************************************************
** Returns the nth entry in a sub directory.
**
** If cluster is 0, we assume that we have to return an entry
** in the root directory.
**
** If there are no such entries then it doesn't change the value
** of result. 
***************************************************************/ 

BOOL GetNthDirectoryPosition(RDWRHandle handle, CLUSTER cluster,
                             unsigned long n, struct DirectoryPosition* result)
{
    int fatlabelsize;
    struct Pipe pipe, *ppipe = &pipe;
   
    pipe.n = n;
    pipe.counter = 0;
    pipe.pos = result;
    
    if (cluster == 0)
    {
       fatlabelsize = GetFatLabelSize(handle);
       switch (fatlabelsize)
       {
	  case FAT12:
	  case FAT16:
	       {
		   BOOL retVal;
		   struct DirectoryPosition pos;
		   struct DirectoryEntry* entry;

		   retVal = GetRootDirPosition(handle, (unsigned short) n,
					       &pos);
		   if (retVal)
		   {
		      entry = AllocateDirectoryEntry();
		      if (!entry) return FALSE;

		      if (!GetDirectory(handle, &pos, entry))
		      {
			 FreeDirectoryEntry(entry);
			 return FALSE;
		      }

		      if (entry->filename[0] != LASTLABEL)
		      {
			 memcpy(result, &pos, sizeof(struct DirectoryPosition));
		      }

		      FreeDirectoryEntry(entry);
		   }

		   return retVal;
	       }
	  case FAT32:
	       cluster = GetFAT32RootCluster(handle);
	       if (cluster)
		  break;
	       else
		  return FALSE;
	  default:
	       return FALSE;
       }
    }    
   
    return TraverseSubdir(handle, cluster, positiongetter, (void**) &ppipe,
                          TRUE);
}

/**************************************************************
**                    PositionGetter
***************************************************************
** Higher order function that gets the nth position in a sub
** directory.
***************************************************************/ 

static BOOL positiongetter (RDWRHandle handle,
		           struct DirectoryPosition* pos,
			   void** buffer)
{
   struct Pipe** pipe = (struct Pipe**) buffer;
   
   handle = handle;
   
   if ((*pipe)->n == (*pipe)->counter)
   {
      memcpy((*pipe)->pos, pos, sizeof(struct DirectoryPosition));
      return FALSE;   
   }
   
   (*pipe)->counter++;
   return TRUE;
}

/**************************************************************
**                    GetNthDirectorySector
***************************************************************
** Reads the sector that contains the nth directory entry of a
** sub directory.
***************************************************************/ 

BOOL GetNthDirectorySector(RDWRHandle handle, CLUSTER cluster, 
                           unsigned long n, char* sector)
{
    unsigned long entry;
    struct DirectoryPosition temp;
    
    /* Calculate which entry this corresponds to */
    entry = n * ENTRIESPERSECTOR;
    if (!GetNthDirectoryPosition(handle, cluster, entry, &temp))
    {
       return FALSE;
    }
    
    return ReadDataSectors(handle, 1, temp.sector, sector);
}

/**************************************************************
**                    GetNthSubDirectoryPosition
***************************************************************
** Reads the sector that contains the nth sub directory entry of a
** sub directory.
**
** If cluster is 0, we assume that we have to return an entry
** in the root directory.
**
** If there are no such entries then it doesn't change the value
** of result. 
***************************************************************/

BOOL GetNthSubDirectoryPosition(RDWRHandle handle, CLUSTER cluster, 
                                unsigned long n, 
                                struct DirectoryPosition* result)        
{
   int counter = 0;
   struct DirectoryPosition pos;
   struct DirectoryEntry* entry;
   
   entry = AllocateDirectoryEntry();
   if (!entry) return FALSE;

   n++;                                       /* n is 0 based */
   while (n)
   {
       pos.sector = 0;
       pos.offset = 0;
       
       if (!GetNthDirectoryPosition(handle, cluster, counter++, &pos))
       {
          FreeDirectoryEntry(entry);
          return FALSE;
       }

       if ((pos.sector == 0) && (pos.offset == 0)) /* Not found*/
       {
	  FreeDirectoryEntry(entry);
	  return TRUE;
       }
          
       if (!GetDirectory(handle, &pos, entry))
       {
          FreeDirectoryEntry(entry);
	  return TRUE;
       }

       if (!IsLFNEntry((entry))        &&
	   (entry->attribute & FA_DIREC) &&
	   (!IsDeletedLabel(*entry))     &&
	   (!IsCurrentDir(*entry))       &&
	   (!IsPreviousDir(*entry)))  
       {
          n--;
       }
   }
 
   memcpy(result, &pos, sizeof(struct DirectoryPosition));
   FreeDirectoryEntry(entry);
   return TRUE;
}
