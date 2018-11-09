
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <time.h>

#include "spanner.h"
#include "logging.h"

#define OLD_UPDATE 2

struct shared_memory_t {
   int fd;
   long id;
   spanner_t* spanner;
};

/* find this instance's slot, the first empty slot, or -1 */
int find_shared_memory_slot( shared_memory_t* shmem )
{
   int slot = -1;
   int emptySlot = -1;

   for ( int i = 0; i < MAX_INSTANCES; i++ )
   {
      if ( shmem->id == shmem->spanner->tracks[i].id )
      {
         DEBUG_PRINT( "Found this instance %li at: %i\n", shmem->id, i );
         slot = i;
         break;
      }

      if ( 0 == shmem->spanner->tracks[i].id && -1 == emptySlot )
      {
         emptySlot = i;
      }
   }

   if ( -1 == slot && -1 != emptySlot )
   {
      DEBUG_PRINT( "Found empty slot for %li at: %i\n", shmem->id, emptySlot );
      slot = emptySlot;
   }

   return slot;
}

/* if an instance crashed or lost connection somehow, remove its data */
void clear_old_shared_memory( shared_memory_t* shmem, long now )
{
   for ( int i = 0; i < MAX_INSTANCES; i++ )
   {
      spanned_track_t* t = &shmem->spanner->tracks[i];
      if ( 0 != t->id && (now - t->lastUpdate) > OLD_UPDATE )
      {
         DEBUG_PRINT( "Clearing old slot %i; %li - %li > %i\n", i, now, t->lastUpdate, OLD_UPDATE );
         t->id = 0;
         shmem->spanner->users -= 1;
      }
   }
}

shared_memory_t* open_shared_memory( long id )
{
   shared_memory_t* shmem = malloc( sizeof( shared_memory_t ) );
   shmem->fd = -1;
   shmem->spanner = NULL;
   shmem->id = id;

   if ( -1 == shmem->id )
   {
      DEBUG_PRINT( "Unable to get a Unique ID for this thread: %s\n", strerror( errno ) );
      return shmem;
   }

   shmem->fd = shm_open( "/" SHMEMNAME,
                         O_RDWR | O_CREAT | O_EXCL,
                         S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );

   if ( -1 != shmem->fd )
   {
      DEBUG_PRINT( "Newly created Shared Memory, initializing\n" );
      if ( 0 != ftruncate( shmem->fd, sizeof( spanner_t ) ) )
      {
         DEBUG_PRINT( "Unable to resize the Shared Memory: %s\n", strerror( errno ) );
         return shmem;
      }
   }
   else if ( EEXIST == errno )
   {
      DEBUG_PRINT( "Shared Memory exists already, trying to connect\n" );

      shmem->fd = shm_open( "/" SHMEMNAME,
                            O_RDWR | O_CREAT,
                            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP );
   }

   if ( -1 == shmem->fd )
   {
      DEBUG_PRINT( "Unable to open File Descriptor for the Shared Memory: %s\n", strerror( errno ) );
      return shmem;
   }

   DEBUG_PRINT( "Mapping Shared Memory\n" );
   shmem->spanner = mmap( NULL, sizeof( spanner_t ),
                          PROT_READ | PROT_WRITE, MAP_SHARED, shmem->fd, 0 );

   if ( MAP_FAILED == shmem->spanner )
   {
      shmem->spanner = NULL;
      DEBUG_PRINT( "Unable to Map the Shared Memory: %s\n", strerror( errno ) );
   }

   DEBUG_PRINT( "Increasing Shared Memory User Count (%zu -> %zu)\n", shmem->spanner->users, shmem->spanner->users + 1 );
   shmem->spanner->users += 1;

   DEBUG_PRINT( "Closing Shared Memory File Descriptor\n" );
   close( shmem->fd );
   shmem->fd = -1;

   return shmem;
}

void close_shared_memory( shared_memory_t* shmem )
{
   if ( NULL != shmem->spanner )
   {
      DEBUG_PRINT( "Reducing Shared Memory User Count (%zu -> %zu)\n", shmem->spanner->users, shmem->spanner->users - 1 );
      shmem->spanner->users -= 1;

      leave_shared_memory( shmem );

      DEBUG_PRINT( "Unmapping Shared Memory\n" );
      if ( 0 != munmap( shmem->spanner, sizeof( spanner_t ) ) )
      {
         DEBUG_PRINT( "Unable to UnMap the Shared Memory: %s\n", strerror( errno ) );
      }
   }

   DEBUG_PRINT( "Unlinking Shared Memory\n" );
   if ( 0 != shm_unlink( "/" SHMEMNAME ) )
   {
      DEBUG_PRINT( "Unable to Unlink the Shared Memory: %s\n", strerror( errno ) );
   }

   free( shmem );
}

void leave_shared_memory( shared_memory_t* shmem )
{
   if ( NULL != shmem->spanner )
   {
      int slot = find_shared_memory_slot( shmem );

      if ( -1 != slot )
      {
         shmem->spanner->tracks[slot].id = 0;
      }
   }
}

void update_shared_memory( shared_memory_t* shmem, track_t* track )
{
   if ( NULL == shmem->spanner )
   {
      DEBUG_PRINT( "Skip updating non-existent Shared Memory!\n" );
      return;
   }

   struct timespec cl;
   clock_gettime( CLOCK_MONOTONIC_RAW, &cl );

   clear_old_shared_memory( shmem, cl.tv_sec );

   int slot = find_shared_memory_slot( shmem );

   if ( -1 == slot )
   {
      DEBUG_PRINT( "Unable to find a slot in Shared Memory, ignoring update!\n" );
      return;
   }

   spanned_track_t* t = &shmem->spanner->tracks[slot];
   t->id = shmem->id;
   t->lastUpdate = cl.tv_sec;
   t->color = track->color;
   t->frameSize = track->frameSize;
   for ( int c = 0; c < MAX_CHANNELS; c++ )
   {
      for ( int s = 0; s < (MAX_FFT / 2 + 1); s++ )
      {
         t->fft[c][s] = track->channels[c].fft[s];
      }
   }
}

spanned_track_t* get_shared_memory_tracks( shared_memory_t* shmem )
{
   if ( NULL == shmem->spanner )
   {
      return NULL;
   }

   return shmem->spanner->tracks;
}

int is_this_track( shared_memory_t* shmem, spanned_track_t* track )
{
   if ( NULL == shmem || NULL == track )
   {
      return 0;
   }

   return shmem->id == track->id;
}