/*    
   RelocClt.c - Function to relocate a cluster in a volume.

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

BOOL RelocateCluster(RDWRHandle handle, CLUSTER source, CLUSTER destination)
{
   int labelsize;
   CLUSTER fatpos, freecluster;
   struct DirectoryPosition dirpos;
   struct DirectoryEntry entry;
   BOOL IsInFAT = FALSE;
   SECTOR srcsector, destsector;
   unsigned char sectorspercluster;
   CLUSTER clustervalue;
   BOOL found;
   struct FSInfoStruct FSInfo;
   
   /* Get the value that is stored at the source position in the FAT */
   if (!ReadFatLabel(handle, source, &clustervalue))
      return FALSE;
   
   /* See where the cluster is refered */
   if (!FindClusterInFAT(handle, source, &fatpos))
      return FALSE;
      
   if (!fatpos)
   {
      if (!FindClusterInDirectories(handle, source, &dirpos, &found))
         return FALSE;
      if (!found)
         return FALSE;                /* Non valid cluster! */
   }
   else
   {
      IsInFAT = TRUE;
   }
   
   /* Copy all sectors in this cluster to the new position */
   srcsector = ConvertToDataSector(handle, source);
   if (!srcsector)
      return FALSE;
   destsector = ConvertToDataSector(handle, destination);
   if (!destsector)
      return FALSE;   
      
   sectorspercluster = GetSectorsPerCluster(handle);
   if (!sectorspercluster)
      return FALSE;
      
   if (!CopySectors(handle, srcsector, destsector, sectorspercluster))
      return FALSE;
   
   /* Write the entry in the FAT */
   if (!WriteFatLabel(handle, destination, clustervalue))
      return FALSE;

   /* Adjust the pointer to the relocated cluster */
   if (IsInFAT)
   {
      if (!WriteFatLabel(handle, fatpos, destination))
         return FALSE;
   }
   else
   {
      if (!GetDirectory(handle, &dirpos, &entry))
         return FALSE;
         
      SetFirstCluster(destination, &entry);
      if (!WriteDirectory(handle, &dirpos, &entry))
         return FALSE;
   }
   
   if (!WriteFatLabel(handle, source, FAT_FREE_LABEL))
      return FALSE;   
      
   /* Adjust FSInfo on FAT32 */
   labelsize = GetFatLabelSize(handle);
   if (labelsize == FAT32)
   {
      if (!GetFreeClusterSearchStart(handle, &freecluster))
         return FALSE;
         
      if (source < freecluster) /* source cluster became available */
      {
         if (!ReadFSInfo(handle, &FSInfo))
            return FALSE;
    
         WriteFreeClusterStart(&FSInfo, source);
    
         if (!WriteFSInfo(handle, &FSInfo))
            return FALSE;          
      }
      
      if (freecluster == destination) /* We are relocating to the first
                                         free cluster */
      {
         CLUSTER dummy;     
 
         if (!FindFirstFreeSpace(handle, &dummy, &dummy))
            return FALSE;
      }
   }
   
   return TRUE;
}
