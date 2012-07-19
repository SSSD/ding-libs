/*
    INI LIBRARY

    Value interpretation functions for single values
    and corresponding memory cleanup functions.

    Copyright (C) Dmitri Pal <dpal@redhat.com> 2012

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

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include "config.h"
#include "trace.h"
#include "collection.h"
#include "collection_tools.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"


/* The section array should be freed using this function */
void ini_free_section_list(char **section_list)
{
    TRACE_FLOW_ENTRY();

    col_free_property_list(section_list);

    TRACE_FLOW_EXIT();
}

/* The section array should be freed using this function */
void ini_free_attribute_list(char **section_list)
{
    TRACE_FLOW_ENTRY();

    col_free_property_list(section_list);

    TRACE_FLOW_EXIT();
}


/* Get list of sections as an array of strings.
 * Function allocates memory for the array of the sections.
 */
char **ini_get_section_list(struct ini_cfgobj *ini_config, int *size, int *error)
{
    char **list;

    TRACE_FLOW_ENTRY();

    /* Do we have the configuration object ? */
    if (ini_config == NULL) {
        TRACE_ERROR_NUMBER("Invalid argument.", EINVAL);
        if (error) *error = EINVAL;
        return NULL;
    }

    /* Pass it to the function from collection API */
    list = col_collection_to_list(ini_config->cfg, size, error);

    TRACE_FLOW_STRING("ini_get_section_list returning",
                      ((list == NULL) ? "NULL" : list[0]));
    return list;
}

/* Get list of attributes in a section as an array of strings.
 * Function allocates memory for the array of the strings.
 */
char **ini_get_attribute_list(struct ini_cfgobj *ini_config,
                              const char *section,
                              int *size,
                              int *error)
{
    struct collection_item *subcollection = NULL;
    char **list;
    int err;
    int i = 0;

    TRACE_FLOW_ENTRY();

    /* Do we have the configuration object ? */
    if (ini_config == NULL) {
        TRACE_ERROR_NUMBER("Invalid configuration object argument.", EINVAL);
        if (error) *error = EINVAL;
        return NULL;
    }

    /* Do we have the section ? */
    if (section == NULL) {
        TRACE_ERROR_NUMBER("Invalid section argument.", EINVAL);
        if (error) *error = EINVAL;
        return NULL;
    }

    /* Fetch section */
    err = col_get_collection_reference(ini_config->cfg, &subcollection, section);
    /* Check error */
    if (err && (subcollection == NULL)) {
        TRACE_ERROR_NUMBER("Failed to get section", err);
        if (error) *error = EINVAL;
        return NULL;
    }

    /* Pass it to the function from collection API */
    list = col_collection_to_list(subcollection, size, error);

    col_destroy_collection(subcollection);

    /* Our list of attributes has a special extra attribute - remove it */
    if ((list != NULL) && (list[0] != NULL)) {
        free(list[0]);
        while(list[i + 1] != NULL) {
            list[i] = list[i + 1];
            i++;
        }
        list[i] = NULL;
    }

    if (size) (*size)--;

    TRACE_FLOW_STRING("ini_get_attribute_list returning", ((list == NULL) ? "NULL" : list[0]));
    return list;
}
