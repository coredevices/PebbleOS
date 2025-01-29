/*
 * DUMA - Red-Zone memory allocator.
 * Copyright (C) 2002-2008 Hayati Ayguen <h_ayguen@web.de>, Procitec GmbH
 * Copyright (C) 1987-1999 Bruce Perens <bruce@perens.com>
 * License: GNU GPL (GNU General Public License, see COPYING-GPL)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * FILE CONTENTS:
 * header file for inclusion from YOUR application code.
 * Include this header before including foreign includes
 * from inside your own headers files.
 */


/* remove previous DUMA definitions */

#ifdef DUMA_newFrame
#undef DUMA_newFrame
#endif

#ifdef DUMA_delFrame
#undef DUMA_delFrame
#endif

#ifdef DUMA_SET_ALIGNMENT
#undef DUMA_SET_ALIGNMENT
#endif

#ifdef DUMA_SET_PROTECT_BELOW
#undef DUMA_SET_PROTECT_BELOW
#endif

#ifdef DUMA_SET_FILL
#undef DUMA_SET_FILL
#endif


/* remove previous wrappers to standard C functions */

#ifdef malloc
#undef malloc
#endif

#ifdef calloc
#undef calloc
#endif

#ifdef free
#undef free
#endif

#ifdef memalign
#undef memalign
#endif

#ifdef posix_memalign
#undef posix_memalign
#endif

#ifdef realloc
#undef realloc
#endif

#ifdef valloc
#undef valloc
#endif

#ifdef strdup
#undef strdup
#endif

#ifdef memcpy
#undef memcpy
#endif

#ifdef strcpy
#undef strcpy
#endif

#ifdef strncpy
#undef strncpy
#endif

#ifdef strcat
#undef strcat
#endif

#ifdef strncat
#undef strncat
#endif

#ifndef SKIP_DUMA_NO_CXX

/* remove previous wrappers to standard C++ functions / operators */

#ifdef new
#undef new
#endif

#ifdef delete
#undef delete
#endif

#ifdef new_NOTHROW
#undef new_NOTHROW
#endif

/* remove previous DUMA C++ definitions */

#ifdef NEW_ELEM
#undef NEW_ELEM
#endif

#ifdef NEW_ARRAY
#undef NEW_ARRAY
#endif

#ifdef NEW_ELEM_NOTHROW
#undef NEW_ELEM_NOTHROW
#endif

#ifdef NEW_ARRAY_NOTHROW
#undef NEW_ARRAY_NOTHROW
#endif


#ifdef DEL_ELEM
#undef DEL_ELEM
#endif

#ifdef DEL_ARRAY
#undef DEL_ARRAY
#endif

#ifdef DEL_ELEM_NOTHROW
#undef DEL_ELEM_NOTHROW
#endif

#ifdef DEL_ARRAY_NOTHROW
#undef DEL_ARRAY_NOTHROW
#endif

#endif /* SKIP_DUMA_NO_CXX */


/* remove definitions for protection of functions return address */

#ifdef DUMA_FN_PROT_START
#undef DUMA_FN_PROT_START
#endif

#ifdef DUMA_FN_PROT_END
#undef DUMA_FN_PROT_END
#endif

#ifdef DUMA_FN_PROT_RET
#undef DUMA_FN_PROT_RET
#endif

#ifdef DUMA_FN_PROT_RET_VOID
#undef DUMA_FN_PROT_RET_VOID
#endif

/* remove (C)hecked (A)rray definitions */

#ifdef CA_DECLARE
#undef CA_DECLARE
#endif

#ifdef CA_DEFINE
#undef CA_DEFINE
#endif

#ifdef CA_REF
#undef CA_REF
#endif


/* Following defines are kept:
 *
 * DUMA_ASSERT
 * DUMA_CHECK
 * DUMA_CHECKALL
 * DUMA_CDECL
 * DUMA_SIZE_T
 * DUMA_MAX_DEL_DEPTH
 *
 */

/***************/
/* END OF FILE */
/***************/
