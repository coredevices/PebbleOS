
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
 * internal header file
 */


#ifndef DUMA_SEM_INC_H
#define DUMA_SEM_INC_H

#ifndef DUMA_NO_THREAD_SAFETY

  #ifdef __cplusplus
  extern "C" {
  #endif

  void DUMA_init_sem(void);
  void DUMA_get_sem(void);
  int  DUMA_rel_sem(int retval);

  #ifdef __cplusplus
  } /* extern "C" */
  #endif

  #define DUMA_INIT_SEMAPHORE()     DUMA_init_sem()
  #define DUMA_GET_SEMAPHORE()      DUMA_get_sem()
  #define DUMA_RELEASE_SEMAPHORE(x) DUMA_rel_sem(x)

#else  /* DUMA_NO_THREAD_SAFETY */

  #define DUMA_INIT_SEMAPHORE()     do { } while (0)
  #define DUMA_GET_SEMAPHORE()      do { } while (0)
  #define DUMA_RELEASE_SEMAPHORE(x) do { } while (0)

#endif /* DUMA_NO_THREAD_SAFETY */

#endif /* DUMA_SEM_INC_H */


