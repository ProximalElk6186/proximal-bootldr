/*    
   NthFlClt.c - function to return the nth cluster of a file.

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

#include "fte.h"

struct Pipe
{
   unsigned n;
   unsigned count;
   CLUSTER result;
   
   BOOL error;
};

static BOOL NthFileClusterFinder(RDWRHandle handle, CLUSTER label,
                                 SECTOR datasector, void** pipe);

/*******************************************************************
**                        GetNthFileCluster
********************************************************************
** Retreives the nth cluster of a file.
********************************************************************/                                 
                                 
CLUSTER GetNthFileCluster(RDWRHandle handle, CLUSTER firstclust, unsigned n,
                          BOOL* pasttheend)
{
   struct Pipe pipe, *ppipe = &pipe;
   
   pipe.n = n;
   pipe.count = 0;
   pipe.result = 0;
   pipe.error = FALSE;
   
   if (!FileTraverseFat(handle, firstclust, NthFileClusterFinder, 
                        (void**)&ppipe))
      return FALSE;
      
   if (pipe.error)
      return FALSE;
      
   if (pipe.n > pipe.count)   
      *pasttheend = TRUE;
   else
      *pasttheend = FALSE;
      
   return pipe.result;
}
                          
/*******************************************************************
**                        NthFileClusterFinder
********************************************************************
** Returns the nth cluster of a file.
********************************************************************/

static BOOL NthFileClusterFinder(RDWRHandle handle, CLUSTER label,
                                 SECTOR datasector, void** structure)
{
   struct Pipe* pipe = *((struct Pipe**) structure);
   
   label = label;
   
   pipe->count++;   
   if (pipe->n == pipe->count)
   {
      pipe->result = DataSectorToCluster(handle, datasector);
      if (!pipe->result)
         pipe->error = TRUE;
      
      return FALSE;
   }
   return TRUE;
}
