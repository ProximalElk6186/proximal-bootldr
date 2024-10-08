/*
   sortentrs.c - takes care that the given directory is sorted.

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

#include <stdlib.h>

#include "fte.h"
#include "sortfatf.h"
#include "expected.h"

int SortEntries(RDWRHandle handle, CLUSTER firstcluster)
{
    long count, count1, i;
    void* memory = NULL;

    /* Count the number of directory entries */
    count = low_dircount(handle, firstcluster, 0xffff);
    if (count == FAIL)
    {
       return FALSE;
    }

    /* If there are no directory entries we don't have to sort.
       If there is only one directory entry than it is already sorted. */
    if (count <= 1)
       return TRUE;

    /* See if we can sort in memory. */
    count1 = count * sizeof(struct DirectoryEntry) +
	     (((count / ENTRIESPERSECTOR)+ ((count % ENTRIESPERSECTOR) > 0))
                                * sizeof(CLUSTER));

    if (count1 <= 65535L)
       memory = malloc((size_t) count1);


    /* If so read all entries from disk */
    LogMessage("Sorting directory entries.");


    if (memory)
    {
       LogMessage("Using fast method.");
       if (!ReadEntriesToSort(handle, firstcluster,
			      (struct DirectoryEntry*)memory,
			      (SECTOR*)(((char*)memory)+
				(size_t)count * sizeof(struct DirectoryEntry))))
       {
	  LogMessage("Unable to read directory entries");
	  free(memory);
	  return FALSE;
       }

       /* Sort them */
       MemorySortEntries((struct DirectoryEntry*)memory, (unsigned)count);

       /* and write them back */
       if (!WriteSortedEntries(handle,
			       (struct DirectoryEntry*)memory,
			       (SECTOR*)(((char*)memory)+
				   (size_t)count * sizeof(struct DirectoryEntry)),
			       (size_t)count))
       {
	  LogMessage("Unable to write directory entries");
	  free(memory);
	  return FALSE;
       }

       free(memory);
    }
    else
    {
       int retVal;

       /* Otherwise try sorting the entries from disk */
       LogMessage("Using slow method.");

       retVal = DiskSortEntries(handle, firstcluster);
       if (!retVal)
       {
	  LogMessage("Unable to sort on disk.");
       }
    }

    return TRUE;
}

int SortSubdir(RDWRHandle handle,
		struct DirectoryPosition* pos,
		void** buffer)
{
    struct DirectoryEntry entry;
    CLUSTER firstcluster;
    BOOL* pError = (BOOL*)(*buffer);
    *pError = FALSE;
    
    /* Read the entry from the volume and look wether it is a directory
       be wary of long filename entries and don't try to sort directory
       aliases. */
    if (GetDirectory(handle, pos, &entry))
    {
       if ((entry.attribute & FA_DIREC) &&
	   (!IsLFNEntry(&entry))        &&
	   (!IsCurrentDir(entry))       &&
	   (!IsPreviousDir(entry))      &&
	   (!IsDeletedLabel(entry)))
       {
	  firstcluster = GetFirstCluster(&entry);

	  if (firstcluster)
	  {
	     if (SortEntries(handle, firstcluster))
		return TRUE;

	     *pError = TRUE;
	     return FALSE;
	  }
	  else
	  {
	     *pError = TRUE;
	     return FALSE;
	  }
       }
       else
       {
          return TRUE;
       }
    }
    else
    {
       *pError = TRUE;
       return FALSE;
    }
}
