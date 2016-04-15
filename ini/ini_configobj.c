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

#include "config.h"
#include <sys/types.h>
#include <regex.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
/* For error text */
#include <libintl.h>
#define _(String) gettext (String)
#include "trace.h"
#include "collection.h"
#include "collection_tools.h"
#include "ini_configobj.h"
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

        if (ini_config->error_list) {
            col_destroy_collection(ini_config->error_list);
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
    new_co->error_list = NULL;
    new_co->count = 0;

    /* Create a collection to hold configuration data */
    error = col_create_collection(&(new_co->cfg),
                                  INI_CONFIG_NAME,
                                  COL_CLASS_INI_CONFIG);
    if (error != EOK) {
        TRACE_ERROR_NUMBER("Failed to create collection.", error);
        ini_config_destroy(new_co);
        return error;
    }

    /* Create error list collection */
    error = col_create_collection(&(new_co->error_list),
                                  INI_ERROR,
                                  COL_CLASS_INI_PERROR);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to create error list", error);
        ini_config_destroy(new_co);
        return error;
    }

    *ini_config = new_co;

    TRACE_FLOW_EXIT();
    return error;
}

/* Callback to set the boundary */
static int ini_boundary_cb(const char *property,
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
    new_co->error_list = NULL;
    new_co->count = 0;

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

/* Check if collision flags are valid */
int valid_collision_flags(uint32_t collision_flags)
{
    uint32_t flag;

    TRACE_FLOW_ENTRY();

    flag = collision_flags & INI_MV1S_MASK;
    if ((flag != INI_MV1S_OVERWRITE) &&
        (flag != INI_MV1S_ERROR) &&
        (flag != INI_MV1S_PRESERVE) &&
        (flag != INI_MV1S_ALLOW) &&
        (flag != INI_MV1S_DETECT)) {
        TRACE_ERROR_STRING("Invalid value collision flag","");
        return 0;
    }

    flag = collision_flags & INI_MV2S_MASK;
    if ((flag != INI_MV2S_OVERWRITE) &&
        (flag != INI_MV2S_ERROR) &&
        (flag != INI_MV2S_PRESERVE) &&
        (flag != INI_MV2S_ALLOW) &&
        (flag != INI_MV2S_DETECT)) {
        TRACE_ERROR_STRING("Invalid value cross-section collision flag","");
        return 0;
    }

    flag = collision_flags & INI_MS_MASK;
    if ((flag != INI_MS_MERGE) &&
        (flag != INI_MS_OVERWRITE) &&
        (flag != INI_MS_ERROR) &&
        (flag != INI_MS_PRESERVE) &&
        (flag != INI_MS_DETECT)) {
        TRACE_ERROR_STRING("Invalid section collision flag","");
        return 0;
    }

    TRACE_FLOW_EXIT();
    return 1;
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

/* How many errors do we have in the list ? */
unsigned ini_config_error_count(struct ini_cfgobj *cfg_ctx)
{
    unsigned count = 0;

    TRACE_FLOW_ENTRY();

    count = cfg_ctx->count;

    TRACE_FLOW_EXIT();
    return count;

}

/* Free error strings */
void ini_config_free_errors(char **errors)
{
    TRACE_FLOW_ENTRY();

    col_free_property_list(errors);

    TRACE_FLOW_EXIT();
}

/* Get the list of error strings */
int ini_config_get_errors(struct ini_cfgobj *cfg_ctx,
                          char ***errors)
{
    char **errlist = NULL;
    struct collection_iterator *iterator = NULL;
    int error;
    struct collection_item *item = NULL;
    struct ini_parse_error *pe;
    unsigned int count = 0;
    char *line;

    TRACE_FLOW_ENTRY();

    /* If we have something to print print it */
    if ((!errors) || (!cfg_ctx)) {
        TRACE_ERROR_NUMBER("Invalid parameter.", EINVAL);
        return EINVAL;
    }

    errlist = calloc(cfg_ctx->count + 1, sizeof(char *));
    if (!errlist) {
        TRACE_ERROR_NUMBER("Failed to allocate memory for errors.", ENOMEM);
        return ENOMEM;
    }

    /* Bind iterator */
    error =  col_bind_iterator(&iterator,
                               cfg_ctx->error_list,
                               COL_TRAVERSE_DEFAULT);
    if (error) {
        TRACE_ERROR_NUMBER("Faile to bind iterator:", error);
        ini_config_free_errors(errlist);
        return error;
    }

    while(1) {
        /* Loop through a collection */
        error = col_iterate_collection(iterator, &item);
        if (error) {
            TRACE_ERROR_NUMBER("Error iterating collection", error);
            col_unbind_iterator(iterator);
            ini_config_free_errors(errlist);
            return error;
        }

        /* Are we done ? */
        if (item == NULL) break;

        /* Process collection header */
        if (col_get_item_type(item) == COL_TYPE_COLLECTION) {
            continue;
        }
        else {
            /* Put error into provided format */
            pe = (struct ini_parse_error *)(col_get_item_data(item));

            /* Would be nice to have asprintf function...
             * ...but for now we know that all the errors
             * are pretty short and will fir into the predefined
             * error length buffer.
             */
            line = malloc(MAX_ERROR_LINE + 1);
            if (!line) {
                TRACE_ERROR_NUMBER("Failed to get memory for error.", ENOMEM);
                col_unbind_iterator(iterator);
                ini_config_free_errors(errlist);
                return ENOMEM;
            }

            snprintf(line, MAX_ERROR_LINE, LINE_FORMAT,
                     col_get_item_property(item, NULL),
                     pe->error,
                     pe->line,
                     ini_get_error_str(pe->error,
                                       INI_FAMILY_PARSING));

            errlist[count] = line;
            count++;
        }

    }

    /* Do not forget to unbind iterator - otherwise there will be a leak */
    col_unbind_iterator(iterator);

    *errors = errlist;

    TRACE_FLOW_EXIT();
    return error;
}

int ini_rules_read_from_file(const char *filename,
                             struct ini_cfgobj **_rules_obj)
{
    int ret;
    struct ini_cfgfile *cfgfile = NULL;

    if (_rules_obj == NULL) {
        return EINVAL;
    }

    ret = ini_config_create(_rules_obj);
    if (ret != EOK) {
        return ret;
    }

    ret = ini_config_file_open(filename, 0, &cfgfile);
    if (ret != EOK) {
        goto done;
    }

    ret = ini_config_parse(cfgfile, 0, INI_MV1S_ALLOW, 0, *_rules_obj);
    if (ret != EOK) {
        goto done;
    }

done:
    if (ret != EOK) {
        ini_config_destroy(*_rules_obj);
        *_rules_obj = NULL;
    }

    ini_config_file_destroy(cfgfile);
    return ret;
}

/* This is used for testing only */
static int ini_dummy_noerror(const char *rule_name,
                             struct ini_cfgobj *rules_obj,
                             struct ini_cfgobj *config_obj,
                             struct ini_errobj *errobj)
{
    return 0;
}

/* This is used for testing only */
static int ini_dummy_error(const char *rule_name,
                           struct ini_cfgobj *rules_obj,
                           struct ini_cfgobj *config_obj,
                           struct ini_errobj *errobj)
{
    return ini_errobj_add_msg(errobj, "Error");
}

static int is_allowed_section(const char *tested_section,
                              char **allowed_sections,
                              size_t num_sec,
                              regex_t *allowed_sections_re,
                              size_t num_sec_re,
                              int case_insensitive)
{
    int ret;
    int i;

    if (case_insensitive) {
        for (i = 0; i < num_sec; i++) {
            if (strcasecmp(tested_section, allowed_sections[i]) == 0) {
                return 1;
            }
        }
    } else { /* case sensitive */
        for (i = 0; i < num_sec; i++) {
            if (strcmp(tested_section, allowed_sections[i]) == 0) {
                return 1;
            }
        }
    }

    for (i = 0; i < num_sec_re; i++) {
        ret = regexec(&allowed_sections_re[i], tested_section, 0, NULL, 0);
        if (ret == 0) {
            return 1;
        }
    }

    return 0;
}

static int ini_allowed_sections(const char *rule_name,
                                struct ini_cfgobj *rules_obj,
                                struct ini_cfgobj *config_obj,
                                struct ini_errobj *errobj)
{
    struct value_obj *vo = NULL;
    int ret;
    char *regex_str = NULL;
    char **allowed_sections = NULL;
    char *insensitive_str;
    char **cfg_sections = NULL;
    int num_cfg_sections;
    char **attributes = NULL;
    int num_attributes;
    size_t num_sec = 0;
    size_t num_sec_re = 0;
    regex_t *allowed_sections_re = NULL;
    size_t buf_size;
    int reg_err;
    int is_allowed;
    int case_insensitive = 0;
    int regcomp_flags = REG_NOSUB;
    int i;

    /* Get number of 'section' and 'section_re' attributes
     * in this rule */
    attributes = ini_get_attribute_list(rules_obj,
                                        rule_name,
                                        &num_attributes,
                                        NULL);
    if (attributes == NULL) {
        ret = ENOMEM;
        goto done;
    }

    for (i = 0; i < num_attributes; i++) {
        if (strcmp("section", attributes[i]) == 0) {
            num_sec++;
        }

        if (strcmp("section_re", attributes[i]) == 0) {
            num_sec_re++;
        }
    }

    ini_free_attribute_list(attributes);

    if (num_sec == 0 && num_sec_re == 0) {
        /* This rule is empty. */
        ret = ini_errobj_add_msg(errobj,
                                 "No allowed sections specified. "
                                 "Use 'section = default' to allow only "
                                 "default section");
        goto done;
    }

    ret = ini_get_config_valueobj(rule_name,
                                  "case_insensitive",
                                  rules_obj,
                                  INI_GET_NEXT_VALUE,
                                  &vo);
    if (ret) {
        goto done;
    }

    if (vo) {
        insensitive_str = ini_get_string_config_value(vo, &ret);
        if (ret) {
            goto done;
        }

        if (strcasecmp(insensitive_str, "yes") == 0
            || strcasecmp(insensitive_str, "true") == 0
            || strcmp(insensitive_str, "1") == 0) {
            case_insensitive = 1;
            regcomp_flags |= REG_ICASE;
        }

        free(insensitive_str);
    }

    /* Create arrays for section_re regexes and section name
     * strings. */
    allowed_sections = calloc(num_sec + 1, sizeof(char *));
    if (allowed_sections == NULL) {
        ret = ENOMEM;
        goto done;
    }

    allowed_sections_re = calloc(num_sec_re + 1, sizeof(regex_t));
    if (allowed_sections_re == NULL) {
        ret = ENOMEM;
        goto done;
    }

    /* Get all allowed section names and store them to
     * allowed_sections array */
    for (i = 0; i < num_sec; i++) {
        ret = ini_get_config_valueobj(rule_name,
                                      "section",
                                      rules_obj,
                                      INI_GET_NEXT_VALUE,
                                      &vo);
        if (ret) {
            goto done;
        }

        allowed_sections[i] = ini_get_string_config_value(vo, &ret);
        if (ret) {
            goto done;
        }
    }

    /* Get all regular section_re regular expresions and
     * store them to allowed_sections_re array */
    for (i = 0; i < num_sec_re; i++) {
        ret = ini_get_config_valueobj(rule_name,
                                      "section_re",
                                      rules_obj,
                                      INI_GET_NEXT_VALUE,
                                      &vo);
        if (ret) {
            goto done;
        }

        regex_str = ini_get_string_config_value(vo, &ret);
        if (ret) {
            goto done;
        }

        reg_err = regcomp(&allowed_sections_re[i], regex_str, regcomp_flags);
        if (reg_err) {
            char *err_str;

            buf_size = regerror(reg_err, &allowed_sections_re[i], NULL, 0);
            err_str = malloc(buf_size);
            if (err_str == NULL) {
                ret = ENOMEM;
                goto done;
            }

            regerror(reg_err, &allowed_sections_re[i], err_str, buf_size);
            ret = ini_errobj_add_msg(errobj,
                                     "Validator failed to use regex [%s]:[%s]",
                                     regex_str, err_str);
            free(err_str);
            ret = ret ? ret : EINVAL;
            goto done;
        }
        free(regex_str);
        regex_str = NULL;
    }

    /* Finally get list of all sections in configuration and
     * check if they are matched by some string in allowed_sections
     * or regex in allowed_sections_re */
    cfg_sections = ini_get_section_list(config_obj, &num_cfg_sections, &ret);
    if (ret != EOK) {
        goto done;
    }

    for (i = 0; i < num_cfg_sections; i++) {
        is_allowed = is_allowed_section(cfg_sections[i],
                                        allowed_sections,
                                        num_sec,
                                        allowed_sections_re,
                                        num_sec_re,
                                        case_insensitive);
        if (!is_allowed) {
            ret = ini_errobj_add_msg(errobj,
                                     "Section [%s] is not allowed. "
                                     "Check for typos.",
                                     cfg_sections[i]);
            if (ret) {
                goto done;
            }
        }
    }

    ret = EOK;
done:
    if (allowed_sections != NULL) {
        for (i = 0; allowed_sections[i] != NULL; i++) {
            free(allowed_sections[i]);
        }
        free(allowed_sections);
    }
    if (allowed_sections_re != NULL) {
        for (i = 0; i < num_sec_re; i++) {
            regfree(&allowed_sections_re[i]);
        }
        free(allowed_sections_re);
    }
    ini_free_section_list(cfg_sections);
    free(regex_str);

    return ret;
}

static int check_if_allowed(char *section, char *attr, char **allowed,
                            int num_allowed, struct ini_errobj *errobj)
{
    int is_allowed = 0;
    int ret;
    int i;

    for (i = 0; i < num_allowed; i++) {
        if (strcmp(attr, allowed[i]) == 0) {
            is_allowed = 1;
            break;
        }
    }

    if (!is_allowed) {
        ret = ini_errobj_add_msg(errobj,
                                 "Attribute '%s' is not allowed in "
                                 "section '%s'. Check for typos.",
                                 attr, section);
        return ret;
    }

    return 0;
}

static int ini_allowed_options(const char *rule_name,
                               struct ini_cfgobj *rules_obj,
                               struct ini_cfgobj *config_obj,
                               struct ini_errobj *errobj)
{
    struct value_obj *vo = NULL;
    int ret;
    char *section_regex;
    int num_sections;
    char **sections = NULL;
    char **attributes = NULL;
    int num_attributes;
    int num_opts = 0;
    int i;
    int a;
    regex_t preg;
    size_t buf_size;
    char *err_str = NULL;
    int reg_err;
    char **allowed = NULL;

    /* Get section regex */
    ret = ini_get_config_valueobj(rule_name,
                                  "section_re",
                                  rules_obj,
                                  INI_GET_FIRST_VALUE,
                                  &vo);
    if (ret != 0) {
        return ret;
    }

    if (vo == NULL) {
        ret = ini_errobj_add_msg(errobj,
                                 "Validator misses 'section_re' parameter");
        if (ret) {
            return ret;
        }
        return EINVAL;
    }

    section_regex = ini_get_string_config_value(vo, NULL);
    if (section_regex == NULL || section_regex[0] == '\0') {
        ret = ini_errobj_add_msg(errobj,
                                 "Validator misses 'section_re' parameter");
        if (ret) {
            return ret;
        }

        free(section_regex);
        return EINVAL;
    }

    /* compile the regular expression */
    reg_err = regcomp(&preg, section_regex, REG_NOSUB);
    if (reg_err) {
        buf_size = regerror(reg_err, &preg, NULL, 0);
        err_str = malloc(buf_size);
        if (err_str == NULL) {
            ret = ENOMEM;
            goto done;
        }

        regerror(reg_err, &preg, err_str, buf_size);
        ret = ini_errobj_add_msg(errobj,
                                 "Cannot compile regular expression from "
                                 "option 'section_re'. Error: '%s'", err_str);
        ret = ret ? ret : EINVAL;
        goto done;
    }

    /* Get all sections from config_obj */
    sections = ini_get_section_list(config_obj, &num_sections, &ret);
    if (ret != EOK) {
        goto done;
    }

    /* Get number of 'option' attributes in this rule
     * and create an array long enough to store them all */
    attributes = ini_get_attribute_list(rules_obj,
                                        rule_name,
                                        &num_attributes,
                                        NULL);
    if (attributes == NULL) {
        ret = ENOMEM;
        goto done;
    }

    for (i = 0; i < num_attributes; i++) {
        if (strcmp("option", attributes[i]) == 0) {
            num_opts++;
        }
    }

    ini_free_attribute_list(attributes);
    attributes = NULL;

    allowed = calloc(num_opts + 1, sizeof(char *));
    if (allowed == NULL) {
        ret = ENOMEM;
        goto done;
    }

    for (i = 0; i < num_opts; i++) {
        ret = ini_get_config_valueobj(rule_name,
                                      "option",
                                      rules_obj,
                                      INI_GET_NEXT_VALUE,
                                      &vo);
        if (ret) {
            goto done;
        }

        allowed[i] = ini_get_string_config_value(vo, &ret);
        if (ret) {
            goto done;
        }
    }

    for (i = 0; i < num_sections; i++) {
        if (regexec(&preg, sections[i], 0, NULL, 0) == 0) {
            /* Regex matched section */
            /* Get options from this section */
            attributes = ini_get_attribute_list(config_obj,
                                                sections[i],
                                                &num_attributes,
                                                NULL);
            if (attributes == NULL) {
                ret = ENOMEM;
                goto done;
            }

            for (a = 0; a < num_attributes; a++) {
                ret = check_if_allowed(sections[i], attributes[a], allowed,
                                       num_opts, errobj);
                if (ret != 0) {
                    goto done;
                }
            }
            ini_free_attribute_list(attributes);
            attributes = NULL;
        }
    }

    ret = 0;
done:
    if (allowed != NULL) {
        for (i = 0; allowed[i] != NULL; i++) {
            free(allowed[i]);
        }
        free(allowed);
    }
    ini_free_section_list(sections);
    free(section_regex);
    ini_free_attribute_list(attributes);
    regfree(&preg);
    free(err_str);
    return ret;
}

static ini_validator_func *
get_validator(char *validator_name,
              struct ini_validator *validators,
              int num_validators)
{
    int i;

    /* First we check all internal validators */
    if (strcmp(validator_name, "ini_dummy_noerror") == 0) {
        return ini_dummy_noerror;
    } else if (strcmp(validator_name, "ini_dummy_error") == 0) {
        return ini_dummy_error;
    } else if (strcmp(validator_name, "ini_allowed_options") == 0) {
        return ini_allowed_options;
    } else if (strcmp(validator_name, "ini_allowed_sections") == 0) {
        return ini_allowed_sections;
    }

    /* Now check the custom validators */
    if (validators == NULL) {
        return NULL;
    }

    for (i = 0; i < num_validators; i++) {
        /* Skip invalid external validator. Name is required */
        if (validators[i].name == NULL) {
            continue;
        }
        if (strcmp(validator_name, validators[i].name) == 0) {
            return validators[i].func;
        }
    }

    return NULL;
}

int ini_rules_check(struct ini_cfgobj *rules_obj,
                    struct ini_cfgobj *config_obj,
                    struct ini_validator *extra_validators,
                    int num_extra_validators,
                    struct ini_errobj *errobj)
{
    char **sections;
    int ret;
    int num_sections;
    char *vname;
    ini_validator_func *vfunc;
    struct value_obj *vo = NULL;
    struct ini_errobj *localerr = NULL;
    int i;

    /* Get all sections from the rules object */
    sections = ini_get_section_list(rules_obj, &num_sections, &ret);
    if (ret != EOK) {
        return ret;
    }

    /* Now iterate through all the sections. If the section
     * name begins with a prefix "rule/", then it is a rule
     * name. */
    for (i = 0; i < num_sections; i++) {
        if (!strncmp(sections[i], "rule/", sizeof("rule/") - 1)) {
            ret = ini_get_config_valueobj(sections[i],
                                          "validator",
                                          rules_obj,
                                          INI_GET_FIRST_VALUE,
                                          &vo);
            if (ret != 0) {
                /* Failed to get value object. This should not
                 * happen. */
                continue;
            }

            if (vo == NULL) {
                ret = ini_errobj_add_msg(errobj,
                                         "Rule '%s' has no validator.",
                                         sections[i]);
                if (ret != EOK) {
                    return ret;
                }
                /* Skip problematic rule */
                continue;
            }

            vname = ini_get_string_config_value(vo, NULL);
            vfunc = get_validator(vname, extra_validators,
                                  num_extra_validators);
            if (vfunc == NULL) {
                ret = ini_errobj_add_msg(errobj,
                                         "Rule '%s' uses unknown "
                                         "validator '%s'.",
                                         sections[i], vname);
                if (ret != EOK) {
                    goto done;
                }
                /* Skip problematic rule */
                free(vname);
                continue;
            }
            free(vname);

            /* Do not pass global errobj to validators, they
             * could corrupt it. Create local one for each
             * validator. */
            ret = ini_errobj_create(&localerr);
            if (ret != EOK) {
                goto done;
            }

            ret = vfunc(sections[i], rules_obj, config_obj, localerr);
            if (ret != 0) {
                /* Just report the error and continue normally,
                 * maybe there are some errors in localerr */
                ret = ini_errobj_add_msg(errobj,
                                         "Rule '%s' returned error code '%d'",
                                         sections[i], ret);
                if (ret != EOK) {
                    goto done;
                }
            }

            /* Bad validator could destroy the localerr, check
             * for NULL */
            if (localerr == NULL) {
                continue;
            }

            ini_errobj_reset(localerr);
            while (!ini_errobj_no_more_msgs(localerr)) {
                ret = ini_errobj_add_msg(errobj,
                                         "[%s]: %s",
                                         sections[i],
                                         ini_errobj_get_msg(localerr));
                if (ret != EOK) {
                    goto done;
                }
                ini_errobj_next(localerr);
            }

            ini_errobj_destroy(&localerr);
        }
    }

    ret = EOK;
done:
    ini_free_section_list(sections);
    ini_errobj_destroy(&localerr);
    return ret;
}

/* This is just convenience function, so that
 * we manipulate with ini_rules_* functions. */
void ini_rules_destroy(struct ini_cfgobj *rules)
{
    ini_config_destroy(rules);
}

int ini_errobj_create(struct ini_errobj **_errobj)
{
    struct ini_errobj *new_errobj = NULL;

    if (_errobj == NULL) {
        return EINVAL;
    }

    new_errobj = calloc(1, sizeof(struct ini_errobj));
    if (new_errobj == NULL) {
        return ENOMEM;
    }

    *_errobj = new_errobj;
    return EOK;
}

void ini_errobj_destroy(struct ini_errobj **errobj)
{
    struct ini_errmsg *to_remove;

    if (errobj == NULL || *errobj == NULL) {
        return;
    }

    while ((*errobj)->first_msg) {
        to_remove = (*errobj)->first_msg;
        (*errobj)->first_msg = (*errobj)->first_msg->next;
        free(to_remove->str);
        free(to_remove);
    }

    free(*errobj);
    *errobj = NULL;
}

int ini_errobj_add_msg(struct ini_errobj *errobj, const char *format, ...)
{
    int ret;
    va_list args;
    struct ini_errmsg *new;

    new = calloc(1, sizeof(struct ini_errmsg));
    if (new == NULL) {
        return ENOMEM;
    }

    va_start(args, format);
    ret = vasprintf(&new->str, format, args);
    va_end(args);
    if (ret == -1) {
        free(new);
        return ENOMEM;
    }

    if (errobj->count == 0) {
        /* First addition to the list, all pointers are NULL */
        errobj->first_msg = new;
        errobj->last_msg = new;
        errobj->cur_msg = new;
        errobj->count++;
    } else {
        errobj->last_msg->next = new;
        errobj->last_msg = errobj->last_msg->next;
        errobj->count++;
    }

    return EOK;
}

void ini_errobj_reset(struct ini_errobj *errobj)
{
    errobj->cur_msg = errobj->first_msg;
}

const char *ini_errobj_get_msg(struct ini_errobj *errobj)
{
    if (errobj->cur_msg != NULL) {
        return errobj->cur_msg->str;
    }

    /* Should this be allowed? */
    return NULL;
}

void ini_errobj_next(struct ini_errobj *errobj)
{
    if (errobj->cur_msg != NULL) {
        errobj->cur_msg = errobj->cur_msg->next;
    }

    /* If we can not move next, just return */
    return;
}

int ini_errobj_no_more_msgs(struct ini_errobj *errobj)
{
    return errobj->cur_msg == NULL;
}

size_t ini_errobj_count(struct ini_errobj *errobj)
{
    return errobj->count;
}
