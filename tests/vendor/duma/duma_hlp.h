
/*
 * DUMA - Red-Zone memory allocator.
 * Copyright (C) 2002-2005 Hayati Ayguen <h_ayguen@web.de>, Procitec GmbH
 * License: GNU LGPL (GNU Lesser General Public License, see COPYING-GPL)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * FILE CONTENTS:
 * internal implementation file
 * contains helper functions for DUMA
 */


/* Function: reduceProtectedMemory
 *
 * delete reductionSizekB amount of memory, which has already
 * been freed but got protected
 * return != 0 when more memory reducable
 */
static int
reduceProtectedMemory( size_t reductionSizekB )
{
  struct _DUMA_Slot * slot  = _duma_g.allocList;
  size_t count              = _duma_s.slotCount;
  size_t alreadyReducekB    = 0;

#ifndef WIN32
  /* Windows VirtualFree(,,MEM_RELEASE) can only free whole allocations. not parts */

  size_t            delSize, newSize;

  /* 1- try reducing memory to just keep page(s) with userAddress */
  for ( ; count > 0  &&  alreadyReducekB < reductionSizekB; --count, ++slot )
    if ( DUMAST_ALL_PROTECTED == slot->state )
    {
      /* free memory above userAddr; keep userAddr protected */
      newSize = (char*)slot->userAddress - (char*)slot->internalAddress;
      newSize = (newSize + DUMA_PAGE_SIZE) & ~(DUMA_PAGE_SIZE -1);
      delSize = slot->internalSize - newSize;
      Page_Delete( (char*)slot->internalAddress + newSize, delSize );
      alreadyReducekB += (delSize+1023) >>10;
      slot->state           = DUMAST_BEGIN_PROTECTED;
      /* but keep the slot and userAddr */
      slot->internalSize    = newSize;

      if ( alreadyReducekB >= reductionSizekB )
      {
        _duma_s.sumProtectedMem -= alreadyReducekB;
        _duma_s.sumAllocatedMem -= alreadyReducekB;
        return 1;
      }
    }
#endif
  /* 2- deallocate all page(s) with userAddress, empty whole slot */
  slot  = _duma_g.allocList;
  count = _duma_s.slotCount;
  for ( ; count > 0  &&  alreadyReducekB < reductionSizekB; --count, ++slot )
    if ( DUMAST_BEGIN_PROTECTED == slot->state
#if defined(WIN32)
      || DUMAST_ALL_PROTECTED == slot->state
#endif
      )
    {
      /* free all the memory */
      Page_Delete(slot->internalAddress, slot->internalSize);
      alreadyReducekB += (slot->internalSize+1023) >>10;
      /* free slot and userAddr */
      slot->internalAddress = slot->userAddress = 0;
      slot->internalSize    = slot->userSize    = 0;
      slot->state           = DUMAST_EMPTY;
      slot->allocator       = EFA_INT_ALLOC;
      #ifndef DUMA_NO_LEAKDETECTION
      slot->fileSource      = DUMAFS_EMPTY;
        slot->filename      = 0;
        slot->lineno        = 0;
      #endif

      if ( alreadyReducekB >= reductionSizekB )
      {
        _duma_s.sumProtectedMem -= alreadyReducekB;
        _duma_s.sumAllocatedMem -= alreadyReducekB;
        return 1;
      }
    }

  return 0;
}


/* Function: slotForUserAddress
 *
 * Find the slot structure for a user address.
 */
static struct _DUMA_Slot *
slotForUserAddress(void * address)
{
  struct _DUMA_Slot * slot  = _duma_g.allocList;
  size_t            count = _duma_s.slotCount;

  for ( ; count > 0; --count, ++slot )
    if ( slot->userAddress == address )
      return slot;
  return 0;
}


/* Function: nearestSlotForUserAddress
 *
 * Find the nearest slot structure for a user address.
 */
static struct _DUMA_Slot *
nearestSlotForUserAddress(void * userAddress)
{
  struct _DUMA_Slot * slot  = _duma_g.allocList;
  size_t            count = _duma_s.slotCount;

  for ( ; count > 0; --count, ++slot )
    if (   (char*)slot->internalAddress <= (char*)userAddress
        && (char*)userAddress           <= (char*)slot->internalAddress + slot->internalSize
       )
      return slot;
  return 0;
}


#if 0
/* next to functions not needed so far .. */

/* Function: slotForInternalAddrNextTo
 *
 * Find the slot structure for an internal address.
 */
static struct _DUMA_Slot *
slotForInternalAddrNextTo(void * address)
{
  struct _DUMA_Slot * slot  = _duma_g.allocList;
  size_t            count = _duma_s.slotCount;

  for ( ; count > 0; --count, ++slot )
    if ( slot->internalAddress == address )
      return slot;
  return 0;
}


/* Function: slotForInternalAddrPrevTo
 *
 * Given the internal address of a buffer, find the buffer immediately
 * before that buffer in the address space. This is used by free() to
 * coalesce two free buffers into one.
 */
static struct _DUMA_Slot *
slotForInternalAddrPrevTo(void * address)
{
  struct _DUMA_Slot * slot  = _duma_g.allocList;
  size_t            count = _duma_s.slotCount;

  for ( ; count > 0; --count, ++slot )
    if ( (char*)slot->internalAddress + slot->internalSize == address )
      return slot;
  return 0;
}

#endif


/* Function: _duma_init_slack
 *
 * Initialise the no mans land, for a given slot
 */
static
void _duma_init_slack( struct _DUMA_Slot * slot )
{
  char * accBegAddr, * accEndAddr;
  char * tmpBegAddr, * tmpEndAddr;

#ifdef DUMA_EXPLICIT_INIT
  slot->slackfill = _duma_s.SLACKFILL;
#endif

  /* nothing to do for zero userSize */
  if ( !slot->userSize )
    return;

  /* calculate accessible non-protectable address area */
  if ( (char*)slot->protAddress < (char*)slot->userAddress )
  {
    /* DUMA_PROTECT_BELOW was 1 when allocating this piece of memory */
    accBegAddr = (char*)slot->userAddress;
    accEndAddr = (char*)slot->internalAddress + slot->internalSize;
  }
  else
  {
    /* DUMA_PROTECT_BELOW was 0 when allocating this piece of memory */
    accBegAddr = (char*)slot->internalAddress;
    accEndAddr = (char*)slot->protAddress;
  }

  tmpBegAddr = accBegAddr;
  tmpEndAddr = (char*)slot->userAddress;
  while (tmpBegAddr < tmpEndAddr)
    *tmpBegAddr++ = (char)_duma_s.SLACKFILL;

  tmpBegAddr = (char*)slot->userAddress + slot->userSize;
  tmpEndAddr = accEndAddr;
  while (tmpBegAddr < tmpEndAddr)
    *tmpBegAddr++ = (char)_duma_s.SLACKFILL;
}


/* Function: _duma_check_slack
 *
 * Checks the integrity of no mans land, for a given slot
 */
static
void _duma_check_slack( struct _DUMA_Slot * slot )
{
  char    * accBegAddr, * accEndAddr;
  char    * tmpBegAddr, * tmpEndAddr;
  char      slackfill;
#ifdef DUMA_EXPLICIT_INIT
  slackfill = (char)slot->slackfill;
#else
  slackfill = (char)_duma_s.SLACKFILL;
#endif

  /* nothing to do for zero userSize */
  if ( !slot->userSize )
    return;

  /* calculate accessible non-protectable address area */
  if ( (char*)slot->protAddress < (char*)slot->userAddress )
  {
    /* DUMA_PROTECT_BELOW was 1 when allocating this piece of memory */
    accBegAddr = (char*)slot->userAddress;
    accEndAddr = (char*)slot->internalAddress + slot->internalSize;
  }
  else
  {
    /* DUMA_PROTECT_BELOW was 0 when allocating this piece of memory */
    accBegAddr = (char*)slot->internalAddress;
    accEndAddr = (char*)slot->protAddress;
  }

  tmpBegAddr = accBegAddr;
  tmpEndAddr = (char*)slot->userAddress;
  while (tmpBegAddr < tmpEndAddr)
  {
    if ( (char)slackfill != *tmpBegAddr++ )
    {
      #ifndef DUMA_NO_LEAKDETECTION
        DUMA_Abort("ptr=%a: detected overwrite of ptrs no mans land below userSpace, size=%d alloced from %s(%i)",
          (DUMA_ADDR)slot->userAddress, (DUMA_SIZE)slot->userSize, slot->filename, slot->lineno);
      #else
        DUMA_Abort("ptr=%a: detected overwrite of ptrs no mans land below userSpace", (DUMA_ADDR)slot->userAddress);
      #endif
    }
  }

  tmpBegAddr = (char*)slot->userAddress + slot->userSize;
  tmpEndAddr = accEndAddr;
  while (tmpBegAddr < tmpEndAddr)
  {
    if ( (char)slackfill != *tmpBegAddr++ )
    {
      #ifndef DUMA_NO_LEAKDETECTION
        DUMA_Abort("detected overwrite of no mans land above userSpace: ptr=%a, size=%d\nalloced from %s(%i)",
          (DUMA_ADDR)slot->userAddress, (DUMA_SIZE)slot->userSize, slot->filename, slot->lineno);
      #else
        DUMA_Abort("detected overwrite of no mans land above userSpace: ptr=%a", (DUMA_ADDR)slot->userAddress);
      #endif
    }
  }
}


/* Function: _duma_check_all_slacks
 *
 * Checks the integrity of all no mans land
 */
static void
_duma_check_all_slacks( void )
{
  struct _DUMA_Slot * slot  = _duma_g.allocList;
  size_t            count = _duma_s.slotCount;

  for ( ; count > 0; --count, ++slot )
  {
    /* CHECK INTEGRITY OF NO MANS LAND */
    if ( DUMAST_IN_USE == slot->state  && slot->userSize )
      _duma_check_slack( slot );
  }
}

// end
