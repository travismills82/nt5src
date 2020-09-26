/*++

Copyright (c) Microsoft Corporation

Module Name:

    sync.c

Abstract:

    This module contains the implemenation of the synchronization library.
    This library is designed to address the special needs of a high performance
    SMP server application.  Each synchronization primitive is designed with
    the following goals in mind:

    -  Mutual Exclusion, Progress, Bounded Waiting, and First Come First Served

        All primitives are designed to respect the traditional rules of
        synchronization.  No cheating or starvation is used to enhance
        performance in any way.
    
    -  Minimum overhead

        All methods are optimized to most efficiently handle the non-contention
        case.  The algorithms employed never add serialization of their own to
        the application.  Analytical support tools (deadlock detection,
        ownership information, and statistical data) are usually sacrificed for
        increased performance.
    
    -  Minimum resource consumption [NYI]

        The primitives have a very small memory footprint and use no external
        memory blocks or kernel resources.  This allows the number of objects
        in use to be limited only by available virtual memory.  This also gives
        the application the freedom to place the primitives with the data they
        protect for maximum cache locality.

    -  No out of resource conditions [NYI]

        The library will never fail the creation or use of a primitive due to
        low resources.  No special exception handling or error handling is
        required in these cases.

Author:

    Andrew E. Goodsell (andygo) 21-Jun-2001

Revision History:

    21-Jun-2001     andygo

        Implementation based on technology from \nt\ds\ese98\export\sync.hxx
        and \nt\ds\ese98\src\sync\sync.cxx

        HACK:  Only rw lock and binary lock are implemented so far and both use
        kernel semaphores.  This is to facilitate testing the impact that these
        locks will have on our performance.  If they do well then the semaphore
        primitive and all its required infrastructure will also be implemented.
        NOTE:  this hack means that an OOM exception can be thrown on create
        just as is done for InitializeCriticalSection.

--*/

#include <NTDSpch.h>
#pragma  hdrstop

#include "sync.h"
#include "syncp.h"

#include "debug.h"                      // standard debugging header
#define DEBSUB     "SYNC:"              // define the subsystem for debugging

#include <fileno.h>
#define FILENO FILENO_SYNC


//  Utilities

__inline
VOID
SyncpPause(
    )
{
#if defined( _M_IX86 )

    __asm rep nop

#else  //  !_M_IX86
#endif  //  _M_IX86
}

#define SYNC_FOREVER for ( ; ; SyncpPause() )


//  Binary Lock

VOID SyncpBLUpdateQuiescedOwnerCountAsGroup1(
    IN OUT  PSYNC_BINARY_LOCK   pbl,
    IN      const DWORD         cOwnerQuiescedDelta
    )
{
    DWORD cOwnerQuiescedBI;
    DWORD cOwnerQuiescedAI;
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  update the quiesced owner count using the provided delta

    cOwnerQuiescedBI = InterlockedExchangeAdd( &pbl->cOwnerQuiesced, cOwnerQuiescedDelta );
    cOwnerQuiescedAI = cOwnerQuiescedBI + cOwnerQuiescedDelta;

    //  our update resulted in a zero quiesced owner count

    if ( !cOwnerQuiescedAI ) {
        
        //  we must release the waiters for Group 2 because we removed the last
        //  quiesced owner count

        //  try forever until we successfully change the lock state

        SYNC_FOREVER {
            
            //  read the current state of the control word as our expected before image

            dwControlWordBIExpected = pbl->dwControlWord;

            //  compute the after image of the control word such that we jump from state
            //  state 4 to state 1 or from state 5 to state 3, whichever is appropriate

            dwControlWordAI =   (DWORD)( dwControlWordBIExpected &
                                ( ( ( (LONG_PTR)( (long)( ( dwControlWordBIExpected + 0xFFFF7FFF ) << 16 ) ) >> 31 ) &
                                0xFFFF0000 ) ^ 0x8000FFFF ) );

            //  attempt to perform the transacted state transition on the control word

            dwControlWordBI = InterlockedCompareExchange( &pbl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

            //  the transaction failed because another context changed the control word

            if ( dwControlWordBI != dwControlWordBIExpected ) {
                
                //  try again

                continue;

            //  the transaction succeeded

            } else {
            
                //  we're done

                break;
            }
        }

        //  we just jumped from state 5 to state 3

        if ( dwControlWordBI & 0x00007FFF ) {
            
            //  update the quiesced owner count with the owner count that we displaced
            //  from the control word
            //
            //  NOTE:  we do not have to worry about releasing any more waiters because
            //  either this context owns one of the owner counts or at least one context
            //  that owns an owner count are currently blocked on the semaphore

            const DWORD cOwnerQuiescedDelta = ( dwControlWordBI & 0x7FFF0000 ) >> 16;
            InterlockedExchangeAdd( &pbl->cOwnerQuiesced, cOwnerQuiescedDelta );
        }

        //  release the waiters for Group 2 that we removed from the lock state

        ReleaseSemaphore( pbl->hsemGroup2, ( dwControlWordBI & 0x7FFF0000 ) >> 16, NULL );
    }
}

VOID SyncpBLUpdateQuiescedOwnerCountAsGroup2(
    IN OUT  PSYNC_BINARY_LOCK   pbl,
    IN      const DWORD         cOwnerQuiescedDelta
    )
{
    DWORD cOwnerQuiescedBI;
    DWORD cOwnerQuiescedAI;
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  update the quiesced owner count using the provided delta

    cOwnerQuiescedBI = InterlockedExchangeAdd( &pbl->cOwnerQuiesced, cOwnerQuiescedDelta );
    cOwnerQuiescedAI = cOwnerQuiescedBI + cOwnerQuiescedDelta;

    //  our update resulted in a zero quiesced owner count

    if ( !cOwnerQuiescedAI ) {
        
        //  we must release the waiters for Group 1 because we removed the last
        //  quiesced owner count

        //  try forever until we successfully change the lock state

        SYNC_FOREVER {
            
            //  read the current state of the control word as our expected before image

            dwControlWordBIExpected = pbl->dwControlWord;

            //  compute the after image of the control word such that we jump from state
            //  state 3 to state 2 or from state 5 to state 4, whichever is appropriate

            dwControlWordAI =   (DWORD)( dwControlWordBIExpected &
                                ( ( ( (LONG_PTR)( (long)( dwControlWordBIExpected + 0x7FFF0000 ) ) >> 31 ) &
                                0x0000FFFF ) ^ 0xFFFF8000 ) );

            //  attempt to perform the transacted state transition on the control word

            dwControlWordBI = InterlockedCompareExchange( &pbl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

            //  the transaction failed because another context changed the control word

            if ( dwControlWordBI != dwControlWordBIExpected ) {
                
                //  try again

                continue;

            //  the transaction succeeded

            } else {
            
                //  we're done

                break;
            }
        }

        //  we just jumped from state 5 to state 4

        if ( dwControlWordBI & 0x7FFF0000 ) {
            
            //  update the quiesced owner count with the owner count that we displaced
            //  from the control word
            //
            //  NOTE:  we do not have to worry about releasing any more waiters because
            //  either this context owns one of the owner counts or at least one context
            //  that owns an owner count are currently blocked on the semaphore

            const DWORD cOwnerQuiescedDelta = dwControlWordBI & 0x00007FFF;
            InterlockedExchangeAdd( &pbl->cOwnerQuiesced, cOwnerQuiescedDelta );
        }

        //  release the waiters for Group 1 that we removed from the lock state

        ReleaseSemaphore( pbl->hsemGroup1, dwControlWordBI & 0x00007FFF, NULL );
    }
}

VOID SyncpBLEnterAsGroup1(
    IN OUT  PSYNC_BINARY_LOCK   pbl,
    IN      const DWORD         dwControlWordBI
    )
{
    //  we just jumped from state 1 to state 3

    if ( ( dwControlWordBI & 0x80008000 ) == 0x00008000 ) {
        
        //  update the quiesced owner count with the owner count that we displaced from
        //  the control word, possibly releasing waiters.  we update the count as if we
        //  were a member of Group 2 as members of Group 1 can be released

        SyncpBLUpdateQuiescedOwnerCountAsGroup2( pbl, ( dwControlWordBI & 0x7FFF0000 ) >> 16 );
    }

    //  wait for ownership of the lock on our semaphore

    WaitForSingleObject( pbl->hsemGroup1, INFINITE );
}

VOID SyncpBLEnterAsGroup2(
    IN OUT  PSYNC_BINARY_LOCK   pbl,
    IN      const DWORD         dwControlWordBI
    )
{
    //  we just jumped from state 2 to state 4

    if ( ( dwControlWordBI & 0x80008000 ) == 0x80000000 ) {
        
        //  update the quiesced owner count with the owner count that we displaced from
        //  the control word, possibly releasing waiters.  we update the count as if we
        //  were a member of Group 1 as members of Group 2 can be released

        SyncpBLUpdateQuiescedOwnerCountAsGroup1( pbl, dwControlWordBI & 0x00007FFF );
    }

    //  wait for ownership of the lock on our semaphore

    WaitForSingleObject( pbl->hsemGroup2, INFINITE );
}

VOID SyncCreateBinaryLock(
    OUT     PSYNC_BINARY_LOCK   pbl
    )
{
    memset( pbl, 0, sizeof( SYNC_BINARY_LOCK ) );

    pbl->hsemGroup1 = CreateSemaphore( NULL, 0, 0x7FFFFFFF, NULL );
    pbl->hsemGroup2 = CreateSemaphore( NULL, 0, 0x7FFFFFFF, NULL );

    if ( !pbl->hsemGroup1 || !pbl->hsemGroup2 ) {
        if ( pbl->hsemGroup1 ) {
            CloseHandle( pbl->hsemGroup1 );
            pbl->hsemGroup1 = NULL;
        }
        if ( pbl->hsemGroup2 ) {
            CloseHandle( pbl->hsemGroup2 );
            pbl->hsemGroup2 = NULL;
        }
        RaiseException( STATUS_NO_MEMORY, EXCEPTION_NONCONTINUABLE, 0, NULL );
    }
}

VOID SyncDestroyBinaryLock(
    IN      PSYNC_BINARY_LOCK   pbl
    )
{
    if ( pbl->hsemGroup1 ) {
        CloseHandle( pbl->hsemGroup1 );
        pbl->hsemGroup1 = NULL;
    }
    if ( pbl->hsemGroup2 ) {
        CloseHandle( pbl->hsemGroup2 );
        pbl->hsemGroup2 = NULL;
    }
}

VOID SyncEnterBinaryLockAsGroup1(
    IN OUT  PSYNC_BINARY_LOCK   pbl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
    
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = pbl->dwControlWord;

        //  compute the after image of the control word by performing the global
        //  transform for the Enter1 state transition

        dwControlWordAI =   (DWORD)( ( ( dwControlWordBIExpected & ( ( (LONG_PTR)( (long)( dwControlWordBIExpected ) ) >> 31 ) |
                            0x0000FFFF ) ) | 0x80000000 ) + 0x00000001 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &pbl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed or Group 1 was quiesced from ownership

        if ( ( dwControlWordBI ^ dwControlWordBIExpected ) | ( dwControlWordBI & 0x00008000 ) ) {
            
            //  the transaction failed because another context changed the control word

            if ( dwControlWordBI != dwControlWordBIExpected ) {
                
                //  try again

                continue;

            //  the transaction succeeded but Group 1 was quiesced from ownership

            } else {
            
                //  wait to own the lock as a member of Group 1

                SyncpBLEnterAsGroup1( pbl, dwControlWordBI );

                //  we now own the lock, so we're done

                break;
            }

        //  the transaction succeeded and Group 1 was not quiesced from ownership

        } else {
        
            //  we now own the lock, so we're done

            break;
        }
    }
}

BOOL SyncTryEnterBinaryLockAsGroup1(
    IN OUT  PSYNC_BINARY_LOCK   pbl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = pbl->dwControlWord;

        //  change the expected before image so that the transaction will only work if
        //  Group 1 ownership is not quiesced

        dwControlWordBIExpected = dwControlWordBIExpected & 0xFFFF7FFF;

        //  compute the after image of the control word by performing the global
        //  transform for the Enter1 state transition

        dwControlWordAI =   (DWORD)( ( ( dwControlWordBIExpected & ( ( (LONG_PTR)( (long)( dwControlWordBIExpected ) ) >> 31 ) |
                            0x0000FFFF ) ) | 0x80000000 ) + 0x00000001 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &pbl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed

        if ( dwControlWordBI != dwControlWordBIExpected ) {
            
            //  the transaction failed because Group 1 ownership is quiesced

            if ( dwControlWordBI & 0x00008000 ) {
                
                //  return failure

                return FALSE;

            //  the transaction failed because another context changed the control word

            } else {
            
                //  try again

                continue;
            }

        //  the transaction succeeded

        } else {
        
            //  return success

            return TRUE;
        }
    }
}

VOID SyncLeaveBinaryLockAsGroup1(
    IN OUT  PSYNC_BINARY_LOCK   pbl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
    
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = pbl->dwControlWord;

        //  change the expected before image so that the transaction will only work if
        //  Group 1 ownership is not quiesced

        dwControlWordBIExpected = dwControlWordBIExpected & 0xFFFF7FFF;

        //  compute the after image of the control word by performing the transform that
        //  will take us either from state 2 to state 0 or state 2 to state 2

        dwControlWordAI = dwControlWordBIExpected + 0xFFFFFFFF;
        dwControlWordAI = dwControlWordAI - ( ( ( dwControlWordAI + 0xFFFFFFFF ) << 16 ) & 0x80000000 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &pbl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed

        if ( dwControlWordBI != dwControlWordBIExpected ) {
            
            //  the transaction failed because Group 1 ownership is quiesced

            if ( dwControlWordBI & 0x00008000 ) {
                
                //  leave the lock as a quiesced owner

                SyncpBLUpdateQuiescedOwnerCountAsGroup1( pbl, 0xFFFFFFFF );

                //  we're done

                break;

            //  the transaction failed because another context changed the control word

            } else {
                
                //  try again

                continue;
            }

        //  the transaction succeeded

        } else {
            
            // we're done

            break;
        }
    }
}

VOID SyncEnterBinaryLockAsGroup2(
    IN OUT  PSYNC_BINARY_LOCK   pbl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = pbl->dwControlWord;

        //  compute the after image of the control word by performing the global
        //  transform for the Enter2 state transition

        dwControlWordAI =   (DWORD)( ( ( dwControlWordBIExpected & ( ( (LONG_PTR)( (long)( dwControlWordBIExpected << 16 ) ) >> 31 ) |
                            0xFFFF0000 ) ) | 0x00008000 ) + 0x00010000 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &pbl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed or Group 2 was quiesced from ownership

        if ( ( dwControlWordBI ^ dwControlWordBIExpected ) | ( dwControlWordBI & 0x80000000 ) ) {
            
            //  the transaction failed because another context changed the control word

            if ( dwControlWordBI != dwControlWordBIExpected ) {
                
                //  try again

                continue;

            //  the transaction succeeded but Group 2 was quiesced from ownership

            } else {
                
                //  wait to own the lock as a member of Group 2

                SyncpBLEnterAsGroup2( pbl, dwControlWordBI );

                //  we now own the lock, so we're done

                break;
            }

        //  the transaction succeeded and Group 2 was not quiesced from ownership

        } else {
        
            //  we now own the lock, so we're done

            break;
        }
    }
}

BOOL SyncTryEnterBinaryLockAsGroup2(
    IN OUT  PSYNC_BINARY_LOCK   pbl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = pbl->dwControlWord;

        //  change the expected before image so that the transaction will only work if
        //  Group 2 ownership is not quiesced

        dwControlWordBIExpected = dwControlWordBIExpected & 0x7FFFFFFF;

        //  compute the after image of the control word by performing the global
        //  transform for the Enter2 state transition

        dwControlWordAI =   (DWORD)( ( ( dwControlWordBIExpected & ( ( (LONG_PTR)( (long)( dwControlWordBIExpected << 16 ) ) >> 31 ) |
                            0xFFFF0000 ) ) | 0x00008000 ) + 0x00010000 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &pbl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed

        if ( dwControlWordBI != dwControlWordBIExpected ) {
            
            //  the transaction failed because Group 2 ownership is quiesced

            if ( dwControlWordBI & 0x80000000 ) {
                
                //  return failure

                return FALSE;

            //  the transaction failed because another context changed the control word

            } else {
            
                //  try again

                continue;
            }

        //  the transaction succeeded

        } else {
        
            //  return success

            return TRUE;
        }
    }
}

VOID SyncLeaveBinaryLockAsGroup2(
    IN OUT  PSYNC_BINARY_LOCK   pbl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = pbl->dwControlWord;

        //  change the expected before image so that the transaction will only work if
        //  Group 2 ownership is not quiesced

        dwControlWordBIExpected = dwControlWordBIExpected & 0x7FFFFFFF;

        //  compute the after image of the control word by performing the transform that
        //  will take us either from state 1 to state 0 or state 1 to state 1

        dwControlWordAI = dwControlWordBIExpected + 0xFFFF0000;
        dwControlWordAI = dwControlWordAI - ( ( ( dwControlWordAI + 0xFFFF0000 ) >> 16 ) & 0x00008000 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &pbl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed

        if ( dwControlWordBI != dwControlWordBIExpected ) {
            
            //  the transaction failed because Group 2 ownership is quiesced

            if ( dwControlWordBI & 0x80000000 ) {
                
                //  leave the lock as a quiesced owner

                SyncpBLUpdateQuiescedOwnerCountAsGroup2( pbl, 0xFFFFFFFF );

                //  we're done

                break;

            //  the transaction failed because another context changed the control word

            } else {
            
                //  try again

                continue;
            }

        //  the transaction succeeded

        } else {
        
            // we're done

            break;
        }
    }
}



//  Reader / Writer Lock

VOID SyncpRWLUpdateQuiescedOwnerCountAsWriter(
    IN OUT  PSYNC_RW_LOCK       prwl,
    IN      const DWORD         cOwnerQuiescedDelta
    )
{
    DWORD cOwnerQuiescedBI;
    DWORD cOwnerQuiescedAI;
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  update the quiesced owner count using the provided delta

    cOwnerQuiescedBI = InterlockedExchangeAdd( &prwl->cOwnerQuiesced, cOwnerQuiescedDelta );
    cOwnerQuiescedAI = cOwnerQuiescedBI + cOwnerQuiescedDelta;

    //  our update resulted in a zero quiesced owner count

    if ( !cOwnerQuiescedAI ) {
        
        //  we must release the waiting readers because we removed the last
        //  quiesced owner count

        //  try forever until we successfully change the lock state

        SYNC_FOREVER {
            
            //  read the current state of the control word as our expected before image

            dwControlWordBIExpected = prwl->dwControlWord;

            //  compute the after image of the control word such that we jump from state
            //  state 4 to state 1 or from state 5 to state 3, whichever is appropriate

            dwControlWordAI =   (DWORD)( dwControlWordBIExpected &
                                ( ( ( (LONG_PTR)( (long)( ( dwControlWordBIExpected + 0xFFFF7FFF ) << 16 ) ) >> 31 ) &
                                0xFFFF0000 ) ^ 0x8000FFFF ) );

            //  attempt to perform the transacted state transition on the control word

            dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

            //  the transaction failed because another context changed the control word

            if ( dwControlWordBI != dwControlWordBIExpected ) {
                
                //  try again

                continue;

            //  the transaction succeeded

            } else {
            
                //  we're done

                break;
            }
        }

        //  we just jumped from state 5 to state 3

        if ( dwControlWordBI & 0x00007FFF ) {
            
            //  update the quiesced owner count with the owner count that we displaced
            //  from the control word
            //
            //  NOTE:  we do not have to worry about releasing any more waiters because
            //  either this context owns one of the owner counts or at least one context
            //  that owns an owner count are currently blocked on the semaphore

            const DWORD cOwnerQuiescedDelta = ( dwControlWordBI & 0x7FFF0000 ) >> 16;
            InterlockedExchangeAdd( &prwl->cOwnerQuiesced, cOwnerQuiescedDelta );
        }

        //  release the waiting readers that we removed from the lock state

        ReleaseSemaphore( prwl->hsemReader, ( dwControlWordBI & 0x7FFF0000 ) >> 16, NULL );
    }
}

VOID SyncpRWLUpdateQuiescedOwnerCountAsReader(
    IN OUT  PSYNC_RW_LOCK       prwl,
    IN      const DWORD         cOwnerQuiescedDelta
    )
{
    DWORD cOwnerQuiescedBI;
    DWORD cOwnerQuiescedAI;
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  update the quiesced owner count using the provided delta

    cOwnerQuiescedBI = InterlockedExchangeAdd( &prwl->cOwnerQuiesced, cOwnerQuiescedDelta );
    cOwnerQuiescedAI = cOwnerQuiescedBI + cOwnerQuiescedDelta;

    //  our update resulted in a zero quiesced owner count

    if ( !cOwnerQuiescedAI ) {
        
        //  we must release a waiting writer because we removed the last
        //  quiesced owner count

        //  try forever until we successfully change the lock state

        SYNC_FOREVER {
            
            //  read the current state of the control word as our expected before image

            dwControlWordBIExpected = prwl->dwControlWord;

            //  compute the after image of the control word such that we jump from state
            //  state 3 to state 2, from state 5 to state 4, or from state 5 to state 5,
            //  whichever is appropriate

            dwControlWordAI =   dwControlWordBIExpected +
                                ( ( dwControlWordBIExpected & 0x7FFF0000 ) ? 0xFFFFFFFF : 0xFFFF8000 );

            //  attempt to perform the transacted state transition on the control word

            dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

            //  the transaction failed because another context changed the control word

            if ( dwControlWordBI != dwControlWordBIExpected ) {
                
                //  try again

                continue;

            //  the transaction succeeded

            } else {
            
                //  we're done

                break;
            }
        }

        //  we just jumped from state 5 to state 4 or from state 5 to state 5

        if ( dwControlWordBI & 0x7FFF0000 ) {
            
            //  update the quiesced owner count with the owner count that we displaced
            //  from the control word
            //
            //  NOTE:  we do not have to worry about releasing any more waiters because
            //  either this context owns one of the owner counts or at least one context
            //  that owns an owner count are currently blocked on the semaphore

            const DWORD cOwnerQuiescedDelta = 1;
            InterlockedExchangeAdd( &prwl->cOwnerQuiesced, cOwnerQuiescedDelta );
        }

        //  release the waiting writer that we removed from the lock state

        ReleaseSemaphore( prwl->hsemWriter, 1, NULL );
    }
}

VOID SyncpRWLEnterAsWriter(
    IN OUT  PSYNC_RW_LOCK       prwl,
    IN      const DWORD         dwControlWordBI
    )
{
    //  we just jumped from state 1 to state 3

    if ( ( dwControlWordBI & 0x80008000 ) == 0x00008000 ) {
        
        //  update the quiesced owner count with the owner count that we displaced from
        //  the control word, possibly releasing a waiter.  we update the count as if we
        //  were a reader as a writer can be released

        SyncpRWLUpdateQuiescedOwnerCountAsReader( prwl, ( dwControlWordBI & 0x7FFF0000 ) >> 16 );
    }

    //  wait for ownership of the lock on our semaphore

    WaitForSingleObject( prwl->hsemWriter, INFINITE );
}

VOID SyncpRWLEnterAsReader(
    IN OUT  PSYNC_RW_LOCK       prwl,
    IN      const DWORD         dwControlWordBI
    )
{
    //  we just jumped from state 2 to state 4 or from state 2 to state 5

    if ( ( dwControlWordBI & 0x80008000 ) == 0x80000000 ) {
        
        //  update the quiesced owner count with the owner count that we displaced from
        //  the control word, possibly releasing waiters.  we update the count as if we
        //  were a writer as readers can be released

        SyncpRWLUpdateQuiescedOwnerCountAsWriter( prwl, 0x00000001 );
    }

    //  wait for ownership of the lock on our semaphore

    WaitForSingleObject( prwl->hsemReader, INFINITE );
}

VOID SyncCreateRWLock(
    OUT     PSYNC_RW_LOCK       prwl
    )
{
    memset( prwl, 0, sizeof( SYNC_BINARY_LOCK ) );

    prwl->hsemWriter    = CreateSemaphore( NULL, 0, 0x7FFFFFFF, NULL );
    prwl->hsemReader    = CreateSemaphore( NULL, 0, 0x7FFFFFFF, NULL );

    if ( !prwl->hsemWriter || !prwl->hsemReader ) {
        if ( prwl->hsemWriter ) {
            CloseHandle( prwl->hsemWriter );
            prwl->hsemWriter = NULL;
        }
        if ( prwl->hsemReader ) {
            CloseHandle( prwl->hsemReader );
            prwl->hsemReader = NULL;
        }
        RaiseException( STATUS_NO_MEMORY, EXCEPTION_NONCONTINUABLE, 0, NULL );
    }
}

VOID SyncDestroyRWLock(
    IN      PSYNC_RW_LOCK       prwl
    )
{
    if ( prwl->hsemWriter ) {
        CloseHandle( prwl->hsemWriter );
        prwl->hsemWriter = NULL;
    }
    if ( prwl->hsemReader ) {
        CloseHandle( prwl->hsemReader );
        prwl->hsemReader = NULL;
    }
}

VOID SyncEnterRWLockAsWriter(
    IN OUT  PSYNC_RW_LOCK       prwl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = prwl->dwControlWord;

        //  compute the after image of the control word by performing the global
        //  transform for the EnterAsWriter state transition

        dwControlWordAI =   (DWORD)( ( ( dwControlWordBIExpected & ( ( (LONG_PTR)( (long)( dwControlWordBIExpected ) ) >> 31 ) |
                            0x0000FFFF ) ) | 0x80000000 ) + 0x00000001 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed or writers are quiesced from ownership or a
        //  writer already owns the lock

        if ( ( dwControlWordBI ^ dwControlWordBIExpected ) | ( dwControlWordBI & 0x0000FFFF ) ) {
            
            //  the transaction failed because another context changed the control word

            if ( dwControlWordBI != dwControlWordBIExpected ) {
                
                //  try again

                continue;

            //  the transaction succeeded but writers are quiesced from ownership
            //  or a writer already owns the lock

            } else {
            
                //  wait to own the lock as a writer

                SyncpRWLEnterAsWriter( prwl, dwControlWordBI );

                //  we now own the lock, so we're done

                break;
            }

        //  the transaction succeeded and writers were not quiesced from ownership
        //  and a writer did not already own the lock

        } else {
        
            //  we now own the lock, so we're done

            break;
        }
    }
}

BOOL SyncTryEnterRWLockAsWriter(
    IN OUT  PSYNC_RW_LOCK       prwl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = prwl->dwControlWord;

        //  change the expected before image so that the transaction will only work if
        //  writers were not quiesced from ownership and another writer doesn't already
        //  own the lock

        dwControlWordBIExpected = dwControlWordBIExpected & 0xFFFF0000;

        //  compute the after image of the control word by performing the global
        //  transform for the EnterAsWriter state transition

        dwControlWordAI =   (DWORD)( ( ( dwControlWordBIExpected & ( ( (LONG_PTR)( (long)( dwControlWordBIExpected ) ) >> 31 ) |
                            0x0000FFFF ) ) | 0x80000000 ) + 0x00000001 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed

        if ( dwControlWordBI != dwControlWordBIExpected ) {
            
            //  the transaction failed because writers were quiesced from ownership
            //  or another writer already owns the lock

            if ( dwControlWordBI & 0x0000FFFF ) {
                
                //  return failure

                return FALSE;

            //  the transaction failed because another context changed the control word

            } else {
            
                //  try again

                continue;
            }

        //  the transaction succeeded

        } else {
        
            // return success

            return TRUE;
        }
    }
}

VOID SyncLeaveRWLockAsWriter(
    IN OUT  PSYNC_RW_LOCK       prwl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = prwl->dwControlWord;

        //  change the expected before image so that the transaction will only work if
        //  writers were not quiesced from ownership

        dwControlWordBIExpected = dwControlWordBIExpected & 0xFFFF7FFF;

        //  compute the after image of the control word by performing the transform that
        //  will take us either from state 2 to state 0 or state 2 to state 2

        dwControlWordAI = dwControlWordBIExpected + 0xFFFFFFFF;
        dwControlWordAI = dwControlWordAI - ( ( ( dwControlWordAI + 0xFFFFFFFF ) << 16 ) & 0x80000000 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed

        if ( dwControlWordBI != dwControlWordBIExpected ) {
            
            //  the transaction failed because writers were quiesced from ownership

            if ( dwControlWordBI & 0x00008000 ) {
                
                //  leave the lock as a quiesced owner

                SyncpRWLUpdateQuiescedOwnerCountAsWriter( prwl, 0xFFFFFFFF );

                //  we're done

                break;

            //  the transaction failed because another context changed the control word

            } else {
            
                //  try again

                continue;
            }

        //  the transaction succeeded

        } else {
        
            //  there were other writers waiting for ownership of the lock

            if ( dwControlWordAI & 0x00007FFF ) {
                
                //  release the next writer into ownership of the lock

                ReleaseSemaphore( prwl->hsemWriter, 1, NULL );
            }
            
            // we're done

            break;
        }
    }
}

VOID SyncEnterRWLockAsReader(
    IN OUT  PSYNC_RW_LOCK       prwl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = prwl->dwControlWord;

        //  compute the after image of the control word by performing the global
        //  transform for the EnterAsReader state transition

        dwControlWordAI =   ( dwControlWordBIExpected & 0xFFFF7FFF ) +
                            (   ( dwControlWordBIExpected & 0x80008000 ) == 0x80000000 ?
                                    0x00017FFF :
                                    0x00018000 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed or readers were quiesced from ownership

        if ( ( dwControlWordBI ^ dwControlWordBIExpected ) | ( dwControlWordBI & 0x80000000 ) ) {
            
            //  the transaction failed because another context changed the control word

            if ( dwControlWordBI != dwControlWordBIExpected ) {
                
                //  try again

                continue;

            //  the transaction succeeded but readers were quiesced from ownership

            } else {

                //  wait to own the lock as a reader

                SyncpRWLEnterAsReader( prwl, dwControlWordBI );

                //  we now own the lock, so we're done

                break;
            }

        //  the transaction succeeded and readers were not quiesced from ownership

        } else {
        
            //  we now own the lock, so we're done

            break;
        }
    }
}

BOOL SyncTryEnterRWLockAsReader(
    IN OUT  PSYNC_RW_LOCK       prwl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = prwl->dwControlWord;

        //  change the expected before image so that the transaction will only work if
        //  readers were not quiesced from ownership

        dwControlWordBIExpected = dwControlWordBIExpected & 0x7FFFFFFF;

        //  compute the after image of the control word by performing the global
        //  transform for the EnterAsReader state transition

        dwControlWordAI =   ( dwControlWordBIExpected & 0xFFFF7FFF ) +
                            (   ( dwControlWordBIExpected & 0x80008000 ) == 0x80000000 ?
                                    0x00017FFF :
                                    0x00018000 );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed

        if ( dwControlWordBI != dwControlWordBIExpected ) {
            
            //  the transaction failed because readers were quiesced from ownership

            if ( dwControlWordBI & 0x80000000 ) {
                
                //  return failure

                return FALSE;

            //  the transaction failed because another context changed the control word

            } else {
            
                //  try again

                continue;
            }

        //  the transaction succeeded

        } else {
        
            // return success

            return TRUE;
        }
    }
}

VOID SyncLeaveRWLockAsReader(
    IN OUT  PSYNC_RW_LOCK       prwl
    )
{
    DWORD dwControlWordBIExpected;
    DWORD dwControlWordAI;
    DWORD dwControlWordBI;
    
    //  try forever until we successfully change the lock state

    SYNC_FOREVER {
        
        //  read the current state of the control word as our expected before image

        dwControlWordBIExpected = prwl->dwControlWord;

        //  change the expected before image so that the transaction will only work if
        //  readers were not quiesced from ownership

        dwControlWordBIExpected = dwControlWordBIExpected & 0x7FFFFFFF;

        //  compute the after image of the control word by performing the transform that
        //  will take us either from state 1 to state 0 or state 1 to state 1

        dwControlWordAI =   (DWORD)( dwControlWordBIExpected + 0xFFFF0000 +
                            ( ( (LONG_PTR)( (long)( dwControlWordBIExpected + 0xFFFE0000 ) ) >> 31 ) & 0xFFFF8000 ) );

        //  attempt to perform the transacted state transition on the control word

        dwControlWordBI = InterlockedCompareExchange( &prwl->dwControlWord, dwControlWordAI, dwControlWordBIExpected );

        //  the transaction failed

        if ( dwControlWordBI != dwControlWordBIExpected ) {
            
            //  the transaction failed because readers were quiesced from ownership

            if ( dwControlWordBI & 0x80000000 ) {
                
                //  leave the lock as a quiesced owner

                SyncpRWLUpdateQuiescedOwnerCountAsReader( prwl, 0xFFFFFFFF );

                //  we're done

                break;

            //  the transaction failed because another context changed the control word

            } else {
            
                //  try again

                continue;
            }

        //  the transaction succeeded

        } else {
        
            // we're done

            break;
        }
    }
}


