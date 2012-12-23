/*
    INI LIBRARY

    Module represents interface to the main INI object.

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

#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "config.h"
#include "trace.h"
#include "collection.h"
#include "ini_config_priv.h"
#include "ini_defines.h"
#include "ini_valueobj.h"
#include "ini_configobj.h"

/* Internal structure used during the merge operation */
struct merge_data {
    struct collection_item *ci;
    uint32_t flags;
    int error;
    int found;
};

/* Callback */
void ini_cleanup_cb(const char *property,
                    int property_len,
                    int type,
                    void *data,
                    int length,
                    void *custom_data)
{
    struct value_obj *vo = NULL;

    TRACE_FLOW_ENTRY();
    TRACE_INFO_STRING("Cleaning ", property);

    /* Banary items are the values */
    if(type == COL_TYPE_BINARY) {
        vo = *((struct value_obj **)(data));
        value_destroy(vo);
    }

    TRACE_FLOW_EXIT();
}

/* Clean the search state */
void ini_config_clean_state(struct ini_cfgobj *ini_config)
{
    TRACE_FLOW_ENTRY();

    if (ini_config) {
        if (ini_config->iterator) col_unbind_iterator(ini_config->iterator);
        ini_config->iterator = NULL;
        free(ini_config->section);
        ini_config->section = NULL;
        free(ini_config->name);
        ini_config->name = NULL;
        ini_config->section_len = 0;
        ini_config->name_len = 0;
    }

    TRACE_FLOW_EXIT();
}



/* Traverse the collection and clean the object */
void ini_config_destroy(struct ini_cfgobj *ini_config)
{
    TRACE_FLOW_ENTRY();

    ini_config_clean_state(ini_config);

    if (ini_config) {
        if (ini_config->cfg) {

            col_destroy_collection_with_cb(ini_config->cfg,
                                           ini_cleanup_cb,
                                           NULL);
        }
        if (ini_config->last_comment) {
            ini_comment_destroy(ini_config->last_comment);
        }
        free(ini_config);
    }

    TRACE_FLOW_EXIT();
}

/* Create a config object */
int ini_config_create(struct ini_cfgobj **ini_config)
{
    int error = EOK;
    struct ini_cfgobj *new_co = NULL;

    TRACE_FLOW_ENTRY();

    if (!ini_config) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    new_co = malloc(sizeof(struct ini_cfgobj));
    if (!new_co) {
        TRACE_ERROR_NUMBER("Failed to allocate memory", ENOMEM);
        return ENOMEM;
    }

    new_co->cfg = NULL;
    new_co->boundary = INI_WRAP_BOUNDARY;
    new_co->last_comment = NULL;
    new_co->section = NULL;
    new_co->name = NULL;
    new_co->section_len = 0;
    new_co->name_len = 0;
    new_co->iterator = NULL;

    /* Create a collection to hold configuration data */
    error = col_create_collection(&(new_co->cfg),
                                  INI_CONFIG_NAME,
                                  COL_CLASS_INI_CONFIG);
    if (error != EOK) {
        TRACE_ERROR_NUMBER("Failed to create collection.", error);
        ini_config_destroy(new_co);
        return error;
    }

    *ini_config = new_co;

    TRACE_FLOW_EXIT();
    return error;
}

/* Callback to set the boundary */
int ini_boundary_cb(const char *property,
                     int property_len,
                     int type,
                     void *data,
                     int length,
                     void *custom_data,
                     int *dummy)
{
    int error = EOK;
    struct value_obj *vo = NULL;
    uint32_t boundary;

    TRACE_FLOW_ENTRY();

    boundary = *((uint32_t *)(custom_data));
    /* Banary items are the values */
    if(type == COL_TYPE_BINARY) {
        vo = *((struct value_obj **)(data));
        error = value_set_boundary(vo, boundary);
    }

    TRACE_FLOW_EXIT();
    return error;
}

/* Set the folding boundary for multiline values.
 * Use before serializing and saving to a file if the
 * default boundary of 80 characters does not work for you.
 */
int ini_config_set_wrap(struct ini_cfgobj *ini_config,
                        uint32_t boundary)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    if (!ini_config) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    ini_config->boundary = boundary;
    error = col_traverse_collection(ini_config->cfg,
                                    COL_TRAVERSE_DEFAULT,
                                    ini_boundary_cb,
                                    (void *)(&(ini_config->boundary)));
    if (error) {
        TRACE_ERROR_NUMBER("Failed to set wrapping boundary", error);
        return error;
    }


    TRACE_FLOW_EXIT();
    return error;
}

/* Configuration copy callback */
static int ini_copy_cb(struct collection_item *item,
                       void *ext_data,
                       int *skip)
{
    int error = EOK;
    struct value_obj *vo = NULL;
    struct value_obj *new_vo = NULL;

    TRACE_FLOW_ENTRY();

    *skip = 0;

    /* Binary items are the values */
    if(col_get_item_type(item) == COL_TYPE_BINARY) {
        vo = *((struct value_obj **)(col_get_item_data(item)));

        error = value_copy(vo, &new_vo);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to copy value", error);
            return error;
        }

        error = col_modify_binary_item(item,
                                       NULL,
                                       &new_vo,
                                       sizeof(struct value_obj *));
        if (error) {
            TRACE_ERROR_NUMBER("Failed to copy value", error);
            value_destroy(new_vo);
            return error;
        }
    }

    TRACE_FLOW_EXIT();
    return error;
}

/* Copy configuration */
int ini_config_copy(struct ini_cfgobj *ini_config,
                    struct ini_cfgobj **ini_new)
{
    int error = EOK;
    struct ini_cfgobj *new_co = NULL;

    TRACE_FLOW_ENTRY();

    if ((!ini_config) ||
        (!ini_new)) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    /* Create a new configuration object */
    new_co = malloc(sizeof(struct ini_cfgobj));
    if (!new_co) {
        TRACE_ERROR_NUMBER("Failed to allocate memory", ENOMEM);
        return ENOMEM;
    }

    new_co->cfg = NULL;
    new_co->boundary = ini_config->boundary;
    new_co->last_comment = NULL;
    new_co->section = NULL;
    new_co->name = NULL;
    new_co->section_len = 0;
    new_co->name_len = 0;
    new_co->iterator = NULL;

    error = col_copy_collection_with_cb(&(new_co->cfg),
                                        ini_config->cfg,
                                        INI_CONFIG_NAME,
                                        COL_COPY_NORMAL,
                                        ini_copy_cb,
                                        NULL);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to copy collection", error);
        ini_config_destroy(new_co);
        return error;
    }

    if (ini_config->last_comment) {
        error = ini_comment_copy(ini_config->last_comment,
                                 &(new_co->last_comment));
        if (error) {
            TRACE_ERROR_NUMBER("Failed to copy comment", error);
            ini_config_destroy(new_co);
            return error;
        }
    }

    *ini_new = new_co;

    TRACE_FLOW_EXIT();
    return error;
}


/* Callback to process merging of the sections */
static int merge_section_handler(const char *property,
                                 int property_len,
                                 int type,
                                 void *data,
                                 int length,
                                 void *custom_data,
                                 int *dummy)
{
    int error = EOK;
    struct value_obj *vo = NULL;
    struct value_obj *new_vo = NULL;
    struct value_obj *vo_old = NULL;
    struct merge_data *passed_data;
    struct collection_item *acceptor = NULL;
    struct collection_item *item = NULL;
    unsigned insertmode;
    uint32_t mergemode;
    int suppress = 0;
    int doinsert = 0;

    TRACE_FLOW_ENTRY();

    if ((type != COL_TYPE_BINARY) ||
        ((type == COL_TYPE_BINARY) &&
         (strncmp(property, INI_SECTION_KEY,
                     sizeof(INI_SECTION_KEY)) == 0))) {
        /* Skip items we do not care about */
        TRACE_FLOW_EXIT();
        return EOK;
    }

    /* Get value */
    vo = *((struct value_obj **)(data));

    /* Copy it */
    error = value_copy(vo, &new_vo);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to copy value", error);
        return error;
    }

    passed_data = (struct merge_data *)(custom_data);
    acceptor = passed_data->ci;
    mergemode = passed_data->flags & INI_MV2S_MASK;

    switch (mergemode) {
    case INI_MV2S_ERROR:     insertmode = COL_INSERT_DUPERROR;
                             doinsert = 1;
                             break;
    case INI_MV2S_PRESERVE:  insertmode = COL_INSERT_DUPERROR;
                             doinsert = 1;
                             suppress = 1;
                             break;
    case INI_MV2S_ALLOW:     insertmode = COL_INSERT_NOCHECK;
                             doinsert = 1;
                             break;
    case INI_MV2S_OVERWRITE: /* Special handling */
    case INI_MV2S_DETECT:
    default:
                             break;
    }

    /* Do not insert but search for dups first */
    if (!doinsert) {
        TRACE_INFO_STRING("Overwrite mode. Looking for:",
                          property);

        error = col_get_item(acceptor,
                             property,
                             COL_TYPE_BINARY,
                             COL_TRAVERSE_DEFAULT,
                             &item);

        if (error) {
            TRACE_ERROR_NUMBER("Failed searching for dup", error);
            value_destroy(new_vo);
            return error;
        }

        /* Check if there is a dup */
        if (item) {
            /* Check if we are in the detect mode */
            if (mergemode == INI_MV2S_DETECT) {
                passed_data->error = EEXIST;
                doinsert = 1;
                insertmode = COL_INSERT_NOCHECK;
            }
            else {

                /* We are in the OVERWRITE mode.
                 * Dup exists - update it.
                 */
                vo_old = *((struct value_obj **)(col_get_item_data(item)));
                error = col_modify_binary_item(item,
                                               NULL,
                                               &new_vo,
                                               sizeof(struct value_obj *));
                if (error) {
                    TRACE_ERROR_NUMBER("Failed updating the value", error);
                    value_destroy(new_vo);
                    return error;
                }

                /* If we failed to update it is better to leak then crash,
                 * so destroy original value only on the successful update.
                 */
                value_destroy(vo_old);
            }
        }
        else {
            /* No dup found so we can insert with no check */
            doinsert = 1;
            insertmode = COL_INSERT_NOCHECK;
        }
    }

    if (doinsert) {
        /* Add value to collection */
        error = col_insert_binary_property(acceptor,
                                           NULL,
                                           COL_DSP_END,
                                           NULL,
                                           0,
                                           insertmode,
                                           property,
                                           &new_vo,
                                           sizeof(struct value_obj *));
        if (error) {
            value_destroy(new_vo);

            if ((suppress) && (error == EEXIST)) {
                /* We are here is we do not allow dups
                 * but found one and need to ignore it.
                 */
                TRACE_INFO_STRING("Preseved exisitng value",
                                  property);
                error = 0;
            }
            else {
                /* Check if this is a critical error or not */
                if ((mergemode == INI_MV2S_ERROR) && (error == EEXIST)) {
                    TRACE_ERROR_NUMBER("Failed to add value object to "
                                       "the section in error mode ", error);
                    passed_data->error = EEXIST;
                    *dummy = 1;
                }
                else {
                    TRACE_ERROR_NUMBER("Failed to add value object"
                                       " to the section", error);
                    return error;
                }
            }
        }
    }

    TRACE_FLOW_EXIT();
    return error;
}


/* Internal function to merge two configs */
static int merge_two_sections(struct collection_item *donor,
                              struct collection_item *acceptor,
                              uint32_t flags)
{
    int error = EOK;
    struct merge_data data;

    TRACE_FLOW_ENTRY();

    data.ci = acceptor;
    data.flags = flags;
    data.error = 0;
    data.found = 0;

    error = col_traverse_collection(donor,
                                    COL_TRAVERSE_ONELEVEL,
                                    merge_section_handler,
                                    (void *)(&data));
    if (error) {
        TRACE_ERROR_NUMBER("Merge values failed", error);
        return error;
    }

    TRACE_FLOW_EXIT();
    return data.error;
}



/* Callback to process the accepting config */
static int acceptor_handler(const char *property,
                            int property_len,
                            int type,
                            void *data,
                            int length,
                            void *custom_data,
                            int *dummy)
{
    int error = EOK;
    struct merge_data *passed_data;
    struct collection_item *acceptor = NULL;
    struct collection_item *donor = NULL;
    uint32_t mergemode;

    TRACE_FLOW_ENTRY();

    /* This callback is called when the dup section is found */
    passed_data = (struct merge_data *)(custom_data);
    passed_data->found = 1;

    donor = passed_data->ci;
    acceptor = *((struct collection_item **)(data));

    mergemode = passed_data->flags & INI_MS_MASK;

    switch (mergemode) {
    case INI_MS_ERROR:      /* Report error and return */
                            TRACE_INFO_STRING("Error ",
                                              "duplicate section");
                            passed_data->error = EEXIST;
                            break;

    case INI_MS_PRESERVE:   /* Preserve what we have */
                            TRACE_INFO_STRING("Preserve mode", "");
                            break;

    case INI_MS_OVERWRITE:  /* Empty existing section */
                            TRACE_INFO_STRING("Ovewrite mode", "");
                            error = empty_section(acceptor);
                            if (error) {
                                TRACE_ERROR_NUMBER("Failed to "
                                                    "empty section",
                                                    error);
                                return error;
                            }
                            error = merge_two_sections(donor,
                                                       acceptor,
                                                       passed_data->flags);
                            if (error) {
                                TRACE_ERROR_NUMBER("Failed to merge "
                                                    "sections", error);
                                if (error == EEXIST) {
                                    passed_data->error = error;
                                }
                                return error;
                            }
                            break;

    case INI_MS_DETECT:     /* Detect mode */
                            TRACE_INFO_STRING("Detect mode", "");
                            passed_data->error = EEXIST;
                            error = merge_two_sections(donor,
                                                       acceptor,
                                                       passed_data->flags);
                            if (error) {
                                if (error != EEXIST) {
                                    TRACE_ERROR_NUMBER("Failed to merge "
                                                       "sections", error);
                                    return error;
                                }
                            }
                            break;

    case INI_MS_MERGE:      /* Merge */
    default:                TRACE_INFO_STRING("Merge mode", "");
                            error = merge_two_sections(donor,
                                                       acceptor,
                                                       passed_data->flags);
                            if (error) {
                                if (error != EEXIST) {
                                    TRACE_ERROR_NUMBER("Failed to merge "
                                                       "sections", error);
                                    return error;
                                }
                                passed_data->error = error;
                            }
                            break;
    }

    *dummy = 1;
    TRACE_FLOW_EXIT();
    return EOK;
}

/* Callback to process the donating config */
static int donor_handler(const char *property,
                         int property_len,
                         int type,
                         void *data,
                         int length,
                         void *custom_data,
                         int *dummy)
{
    int error = EOK;
    struct merge_data *passed_data;
    struct merge_data acceptor_data;
    struct collection_item *new_ci = NULL;

    TRACE_FLOW_ENTRY();

    *dummy = 0;

    /* Opaque data passed to callback is merge data */
    passed_data = (struct merge_data *)(custom_data);

    TRACE_INFO_STRING("Property: ", property);
    TRACE_INFO_NUMBER("Type is: ", type);
    TRACE_INFO_NUMBER("Flags: ", passed_data->flags);

    /* All sections are subcollections */
    if(type == COL_TYPE_COLLECTIONREF) {

        /* Prepare data for the next callback */
        acceptor_data.flags = passed_data->flags;
        acceptor_data.ci = *((struct collection_item **)(data));
        acceptor_data.error = 0;
        acceptor_data.found = 0;

        /* Try to find same section as the current one */
        error = col_get_item_and_do(passed_data->ci,
                                    property,
                                    COL_TYPE_COLLECTIONREF,
                                    COL_TRAVERSE_ONELEVEL,
                                    acceptor_handler,
                                    (void *)(&acceptor_data));
        if (error) {
            TRACE_ERROR_NUMBER("Critical error", error);
            return error;
        }

        /* Was duplicate found ? */
        if (acceptor_data.found) {
            /* Check for logical error. It can be only EEXIST */
            if (acceptor_data.error) {
                /* Save error anyway */
                passed_data->error = acceptor_data.error;
                /* If it is section DETECT or MERGE+DETECT */
                if (((passed_data->flags & INI_MS_MASK) == INI_MS_DETECT) ||
                    (((passed_data->flags & INI_MS_MASK) != INI_MS_ERROR) &&
                     ((passed_data->flags & INI_MV2S_MASK) ==
                       INI_MV2S_DETECT))) {
                    TRACE_INFO_NUMBER("Non-critical error",
                                      acceptor_data.error);
                }
                else {
                    /* In any other mode we need to stop */
                    TRACE_INFO_NUMBER("Merge error detected",
                                      acceptor_data.error);
                    /* Force stop */
                    *dummy = 1;
                }
            }
        }
        else {
            /* Not found? Then create a copy... */
            error = col_copy_collection_with_cb(&new_ci,
                                                acceptor_data.ci,
                                                NULL,
                                                COL_COPY_NORMAL,
                                                ini_copy_cb,
                                                NULL);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to copy collection", error);
                return error;
            }

            /* ... and embed into the existing collection */
            error = col_add_collection_to_collection(passed_data->ci,
                                                     NULL,
                                                     NULL,
                                                     new_ci,
                                                     COL_ADD_MODE_EMBED);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to copy collection", error);
                col_destroy_collection(new_ci);
                return error;
            }
        }
    }

    TRACE_FLOW_EXIT();
    return EOK;
}

static int merge_comment(struct ini_cfgobj *donor,
                         struct ini_cfgobj *acceptor)
{
    int error = EOK;

    TRACE_FLOW_ENTRY();

    if (donor->last_comment) {

        if (acceptor->last_comment) {

            error = ini_comment_add(donor->last_comment,
                                    acceptor->last_comment);
            if (error) {
                TRACE_ERROR_NUMBER("Merge comment failed", error);
                return error;
            }

        }
        else {
            error = ini_comment_copy(donor->last_comment,
                                     &(acceptor->last_comment));
            if (error) {
                TRACE_ERROR_NUMBER("Copy comment failed", error);
                return error;
            }
        }
    }

    TRACE_FLOW_EXIT();
    return EOK;
}



/* Internal function to merge two configs */
static int merge_configs(struct ini_cfgobj *donor,
                         struct ini_cfgobj *acceptor,
                         uint32_t collision_flags)
{
    int error = EOK;
    struct merge_data data;

    TRACE_FLOW_ENTRY();

    data.ci = acceptor->cfg;
    data.flags = collision_flags;
    data.error = 0;
    data.found = 0;

    /* Loop through the donor collection calling
     * donor_handler callback for every section we find.
     */
    error = col_traverse_collection(donor->cfg,
                                    COL_TRAVERSE_ONELEVEL,
                                    donor_handler,
                                    (void *)(&data));
    if (error) {
        TRACE_ERROR_NUMBER("Merge failed", error);
        return error;
    }

    /* Check if we got error */
    if ((data.error) &&
        (((collision_flags & INI_MS_MASK) == INI_MS_ERROR) ||
         ((collision_flags & INI_MV2S_MASK) == INI_MV2S_ERROR))) {
        TRACE_ERROR_NUMBER("Got error in error mode", data.error);
        return data.error;
    }

    /* If boundaries are different re-align the values */
    if (acceptor->boundary != donor->boundary) {
        error = ini_config_set_wrap(acceptor, acceptor->boundary);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to re-align", error);
            return error;
        }
    }

    /* Merge last comment */
    error = merge_comment(donor, acceptor);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to merge comment", error);
        return error;
    }

    /* Check if we got error */
    if ((data.error) &&
        (((collision_flags & INI_MS_MASK) == INI_MS_DETECT) ||
         ((collision_flags & INI_MV2S_MASK) == INI_MV2S_DETECT))) {
        TRACE_ERROR_NUMBER("Got error in error or detect mode", data.error);
        error = data.error;
    }

    TRACE_FLOW_EXIT();
    return error;
}

/* Merge two configurations together creating a new one */
int ini_config_merge(struct ini_cfgobj *first,
                     struct ini_cfgobj *second,
                     uint32_t collision_flags,
                     struct ini_cfgobj **result)
{
    int error = EOK;
    struct ini_cfgobj *new_co = NULL;

    TRACE_FLOW_ENTRY();

    /* Check input params */
    if ((!first) ||
        (!second) ||
        (!result)) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    /* Check collision flags */
    if (!valid_collision_flags(collision_flags)) {
        TRACE_ERROR_NUMBER("Invalid flags.", EINVAL);
        return EINVAL;
    }

    /* NOTE: We assume that the configuration we merge to
     * is consistent regarding duplicate values.
     * For example, if the duplicates are not allowed,
     * the parsing function should have been instructed
     * to not allow duplicates.
     * If in future we decide to be explicite we would need
     * to introduce a "compacting" function and call it here
     * after we create a copy.
     * For now it is treated as a corner case and thus not worth
     * implementing.
     */

    /* Create a new config object */
    error = ini_config_copy(first, &new_co);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to copy configuration", error);
        return error;
    }

    /* Merge configs */
    error = merge_configs(second, new_co, collision_flags);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to merge configuration", error);
        if ((error == EEXIST) &&
            ((((collision_flags & INI_MS_MASK) == INI_MS_DETECT) &&
              ((collision_flags & INI_MV2S_MASK) != INI_MV2S_ERROR)) ||
             (((collision_flags & INI_MS_MASK) != INI_MS_ERROR) &&
              ((collision_flags & INI_MV2S_MASK) == INI_MV2S_DETECT)))) {
            TRACE_ERROR_NUMBER("Got error in detect mode", error);
            /* Fall through! */
        }
        else {
            /* Got an error in any other mode */
            TRACE_ERROR_NUMBER("Got error in non detect mode", error);
            ini_config_destroy(new_co);
            return error;
        }
    }

    *result = new_co;
    TRACE_FLOW_EXIT();
    return error;

}
