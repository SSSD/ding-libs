/*
    INI LIBRARY

    Header file for the ini collection object.

    Copyright (C) Dmitri Pal <dpal@redhat.com> 2010

    INI Library is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    INI Library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with INI Library.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef INI_CONFIGOBJ_H
#define INI_CONFIGOBJ_H

#include <stdio.h>
#include "simplebuffer.h"

/********************************************************************/
/* THIS IS A BEGINNING OF THE THE NEW CONFIG OBJECT INTERFACE - TBD */
/********************************************************************/

struct configobj;

/* Create a configuration object */
int ini_config_create(struct configobj **ini_config);

/* Destroy a configuration object */
void ini_config_destroy(struct configobj *ini_config);

/* Serialize configuration object into provided buffer */
int ini_serialize_config(struct configobj *ini_config,
                         struct simplebuffer *sbobj);


#endif
