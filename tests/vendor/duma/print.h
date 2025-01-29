
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
 * internal header file
 * contains aborting, printing functions with minor system/platform dependencies
 */

#ifndef DUMA_PRINT_H
#define DUMA_PRINT_H

/*
 * These routines do their printing without using stdio. Stdio can't
 * be used because it calls malloc(). Internal routines of a malloc()
 * debugger should not re-enter malloc(), so stdio is out.
 */

#ifdef __cplusplus
extern "C" {
#endif

void DUMA_Abort(const char * pattern, ...);
void DUMA_Print(const char * pattern, ...);
void DUMA_Exit(const char * pattern, ...);
void DUMA_sprintf(char* buffer, const char * pattern, ...);
const char * DUMA_strerror(int duma_errno);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* DUMA_PRINT_H */
