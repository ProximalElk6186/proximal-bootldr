/*    
   Walktree.c - call a function for every entry on a volume.
   
   Copyright (C) 2000, 2002 Imre Leber

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
/*
   12-08-02
   
      Totaly rewritten to make use of an explicit stack instead of relying
      on tree recursion.
*/

/*
   The following value is chosen, because MAX_PATH = 256 in win95.
   
   Therefor the path with the deepest directory level is:
     
                A\B\C\D\E\ ...
                
   Each level takes 2 character so the deepest directory level is 128.             
*/

#include <string.h>

#include "FTE.h"

#define DIR_STACK_DEPTH 128

struct StackElement 
{
   CLUSTER cluster;
   unsigned long index;
};

struct PipeStruct {
       int (*func) (RDWRHandle handle,
		    struct DirectoryPosition* position,
		    void** structure);
       void** structure;

       BOOL stop;
};

static int ActionWalker(RDWRHandle handle,
			struct DirectoryPosition* pos,
			void** buffer);
                       
static void PushDirectoryEntry(struct StackElement* stack, int* top,
                               struct StackElement* element);
                               
static void PopDirectoryEntry(struct StackElement* stack, int* top, 
                              struct StackElement* element);                              

/**********************************************************************
***                       WalkDirectoryTree
***********************************************************************
*** Calls a function for every directory entry in a volume.
***
*** Pre order traversal:
***
*** eg.       c:\
***           c:\defrag
***           c:\defrag\source   
***           c:\defrag\source\msdefint
***           ... 
***           c:\defrag\bin
***           ... 
***********************************************************************/
                                 
BOOL WalkDirectoryTree(RDWRHandle handle,
		      int (*func) (RDWRHandle handle,
				   struct DirectoryPosition* position,
				   void** structure),
		      void** structure)
{
    int top = 0;
    struct StackElement* stack;
    CLUSTER cluster = 0;
    unsigned long current = 0;
    struct DirectoryPosition pos;
    struct DirectoryEntry* entry;
    struct StackElement element;
        
    struct PipeStruct pipe, *ppipe = &pipe;

    pipe.func      = func;
    pipe.structure = structure;
    pipe.stop      = FALSE;
      
    if (!TraverseRootDir(handle, ActionWalker, (void**)&ppipe, TRUE))
       return FALSE;
   
    stack = FTEAlloc(DIR_STACK_DEPTH * sizeof(struct StackElement));
    if (!stack) return FALSE;    
    
    for (;;)
    {
         /* If there still are sub directories in this directory, 
            push the cluster of that directory. */
         pos.sector = 0;
         pos.offset = 0;
         if (!GetNthSubDirectoryPosition(handle, cluster, current, &pos))
         {
            FTEFree(stack);
            return FALSE;
         }
         
         if ((pos.sector != 0) || (pos.offset != 0))
         {
            entry = AllocateDirectoryEntry();
            if (!entry)
            {  
               FTEFree(stack);     
               return FALSE;   
            } 
            
            if (top < DIR_STACK_DEPTH)
            {
               element.cluster = cluster;
               element.index   = current;
               
               PushDirectoryEntry(stack, &top, &element);
            }
            else
            {
               FreeDirectoryEntry(entry);
               FTEFree(stack);     
               return FALSE;               
            }
            
            if (!GetDirectory(handle, &pos, entry)) 
            {  
               FreeDirectoryEntry(entry);
               FTEFree(stack);     
               return FALSE;   
	    }

	    /* Descend in the directory tree and call the function
               for every directory entry in that directory. */
            cluster = GetFirstCluster(entry);
	    current = 0;
            
	    if (!TraverseSubdir(handle, cluster, ActionWalker, (void**) &ppipe, TRUE))
	    {
	       FTEFree(stack);
               return FALSE;
	    }
	    if (pipe.stop)
	    {
               FTEFree(stack);
	       return TRUE;
	    }

            FreeDirectoryEntry(entry);
         }
        
         /* If there are no more sub directories in the current directory, 
            pop the current directory from the stack.                 */
         else
         {
            if (top) /* Be carefull when there are no sub directories 
                        in the volume. */
            {       
               PopDirectoryEntry(stack, &top, &element);
               
               current = element.index+1; /* Then find the next sub directory. */
               cluster = element.cluster;
	    }
	    else
	    {
	       break;
	    }
         }
    }

    FTEFree(stack);        
    return TRUE;
}

/**********************************************************************
***                           ActionWalker      
***********************************************************************
*** Calls a function for every directory entry in the given directory.
***********************************************************************/

static int ActionWalker(RDWRHandle handle,
			struct DirectoryPosition* pos,
			void** buffer)
{
    struct PipeStruct* pipe = *((struct PipeStruct**) buffer);

    switch (pipe->func(handle, pos, pipe->structure))
    {
      case FALSE:
	   pipe->stop = TRUE;
	   return FALSE;
      case FAIL:
	   return FAIL;
      case TRUE:
	   return TRUE;
    }

    return FAIL; 	/* Should not be executed */
}


/**********************************************************************
***                           PushDirectoryEntry      
***********************************************************************
*** Pushes an entry to the stacks.
***********************************************************************/

static void PushDirectoryEntry(struct StackElement* stack, 
                               int* top,
                               struct StackElement* element)
{
    memcpy(&stack[*top], element, sizeof(struct StackElement));
    *top = (*top)+1;      
}

/**********************************************************************
***                           PopDirectoryEntry
***********************************************************************
*** Pops a directory entry from the stack.
***********************************************************************/

static void PopDirectoryEntry(struct StackElement* stack, 
                              int* top,
                              struct StackElement* element)
{
    *top = (*top)-1;
    memcpy(element, &stack[*top], sizeof(struct StackElement));
}
