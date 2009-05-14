/*
 * Gconnman - a GObject wrapper for the Connman D-Bus API
 * Copyright © 2009, Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Written by:	James Ketrenos <jketreno@linux.intel.com>
 *		Joshua Lock <josh@linux.intel.com>
 *
 */

#include "gconnman.h"
#include <stdio.h>
#include <string.h>

/**
 * SECTION:main
 * @short_description: Return sum of all characters in name string
 *
 * @see_also: #For more information, see <ulink role="online-location" 
 *             url="http://moblin.org">http://moblin.org</ulink>
 * @stability: Stable
 * @include: string gconnman.h
 *
 * Parses command-line.  Starts sample console app.
 */
int get_name_sum(const char *name)
{
   int sum = 0;
   int i=0;
   for (i=0; i<strlen(name); i++) {
      sum += (int)name[i];
   }
   printf ("Your name adds up to %d\n", sum);
   return sum;
}

