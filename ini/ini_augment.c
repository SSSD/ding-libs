/*
    INI LIBRARY

    Module represents part of the INI interface.
    The main function in this module allows to merge
    snippets of different config files.

    Copyright (C) Dmitri Pal <dpal@redhat.com> 2014

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
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <limits.h>
#include <sys/types.h>
#include <regex.h>
#include <unistd.h>
#include "trace.h"
#include "collection.h"
#include "collection_tools.h"
#include "ini_configobj.h"
#include "ini_config_priv.h"
#include "ini_defines.h"
#include "path_utils.h"

/* Constants to match */
#define INI_CURRENT_DIR "."
#define INI_PARENT_DIR ".."

/* Size of incremental growth for ref of the array of strings */
#define INI_AUG_ARR_SIZE_INC 50


/* Function to add an error to the array */
static void ini_aug_add_string(struct ref_array *ra,
                               const char *format,
                               ...)
{
    va_list args;
    char *result = NULL;

    TRACE_FLOW_ENTRY();

    va_start(args, format);

    if(vasprintf(&result, format, args )) {
        TRACE_INFO_STRING("String:", result);
        /* This is a best effort assignment. error is not checked */
        (void)ref_array_append(ra, (void *)&result);
    }

    va_end(args);

    TRACE_FLOW_EXIT();
}

/* Add error about opening directory */
static void add_dir_open_error(int error, char *dirname,
                               struct ref_array *ra_err)
{

    TRACE_FLOW_ENTRY();

    switch(error) {
    case EACCES:
        ini_aug_add_string(ra_err,
                           "Permission denied opening %s.",
                           dirname);
        break;
    case EMFILE:
    case ENFILE:
        ini_aug_add_string(ra_err,
                           "Too many file descriptors in use while opening %s.",
                           dirname);
        break;
    case ENOENT:
        ini_aug_add_string(ra_err,
                           "Directory %s does not exist.",
                           dirname);
        break;
    case ENOTDIR:
        ini_aug_add_string(ra_err,
                           "Path %s is not a directory.",
                           dirname);
        break;
    case ENOMEM:
        ini_aug_add_string(ra_err,
                           "Insufficient memory while opening %s.",
                           dirname);
        break;
    default:
        ini_aug_add_string(ra_err,
                           "Unknown error while opening %s.",
                           dirname);
        break;
    }

    TRACE_FLOW_EXIT();
}

/* Cleanup callback for regex array */
static void regex_cleanup(void *elem,
                          ref_array_del_enum type,
                          void *data)
{
    TRACE_FLOW_ENTRY();
    regfree(*((regex_t **)elem));
    free(*((regex_t **)elem));
    TRACE_FLOW_EXIT();
}


/* Prepare array of regular expressions */
static int ini_aug_regex_prepare(const char *patterns[],
                                 struct ref_array *ra_err,
                                 struct ref_array **ra_regex)
{
    int error = EOK;
    int reg_err = 0;
    char const *pat = NULL;
    struct ref_array *ra = NULL;
    regex_t *preg = NULL;
    size_t buf_size = 0;
    char *err_str = NULL;
    size_t i;

    TRACE_FLOW_ENTRY();

    if (patterns) {

        /* Create array to mark bad patterns */
        error = ref_array_create(&ra,
                                 sizeof(regex_t *),
                                 INI_AUG_ARR_SIZE_INC,
                                 regex_cleanup,
                                 NULL);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to create array.", error);
            return error;
        }

        /* Run through the list and save precompiled patterns */
        for (i = 0; patterns[i] != NULL; i++) {
            pat = patterns[i];

            TRACE_INFO_STRING("Pattern:", pat);

            preg = calloc(1, sizeof(regex_t));
            if (preg == NULL) {
                TRACE_ERROR_NUMBER("Failed to create array.", ENOMEM);
                ref_array_destroy(ra);
                return ENOMEM;
            }
            reg_err = regcomp(preg, pat, REG_NOSUB);
            if (reg_err) {
                /* Get size, allocate buffer, record error... */
                buf_size = regerror(reg_err, preg, NULL, 0);
                err_str = malloc (buf_size);
                if (err_str == NULL) {
                    TRACE_ERROR_NUMBER("Failed to create array.", ENOMEM);
                    ref_array_destroy(ra);
                    free(preg);
                    return ENOMEM;
                }
                regerror(reg_err, preg, err_str, buf_size);
                free(preg);
                ini_aug_add_string(ra_err,
                                   "Failed to process expression: %s."
                                   " Compilation returned error: %s",
                                   pat, err_str);
                free(err_str);

                /* All error processing is done - advance to next pattern */
                pat++;
                continue;
            }
            /* In case of no error add compiled expression into the buffer */
            error = ref_array_append(ra, (void *)&preg);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to add element to array.", error);
                ref_array_destroy(ra);
                free(preg);
                return error;
            }
        }
    }

    *ra_regex = ra;
    /* ref_array_debug(*ra_regex, 1); */

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Match file name */
static bool ini_aug_match_name(char *name,
                               struct ref_array *ra_regex)
{
    uint32_t len = 0;
    uint32_t i = 0;
    bool match = false;
    regex_t *preg = NULL;

    TRACE_FLOW_ENTRY();

    len = ref_array_len(ra_regex);
    if (len == 0) {
        /* List is empty - nothing to do */
        TRACE_FLOW_EXIT();
        return true;
    }

    TRACE_INFO_STRING("Name to match:", name);
    TRACE_INFO_NUMBER("Number of regexes:", len);
    /* ref_array_debug(ra_regex, 1);*/

    for (i = 0; i < len; i++) {
        preg = *((regex_t **)ref_array_get(ra_regex, i, NULL));
        if (preg == NULL) continue;
        if (regexec(preg, name, 0, NULL, 0) == 0) {
            TRACE_INFO_NUMBER("Name matched regex number:", i);
            match = true;
            break;
        }
    }

    TRACE_FLOW_EXIT();
    return match;
}

/* Check if this is a file and validate permission */
static bool ini_check_file_perm(char *name,
                                struct access_check *check_perm,
                                struct ref_array *ra_err)
{
    bool ret = false;
    int error = EOK;
    struct stat file_info;

    TRACE_FLOW_ENTRY();

    errno = 0;
    if (stat(name, &file_info) == -1) {
        error = errno;
        TRACE_ERROR_NUMBER("Failed to get file stats", error);
        ini_aug_add_string(ra_err,
                           "Failed to read metadata for file %s."
                           " Skipping.",
                           name);
        return false;
    }

    if (!S_ISREG(file_info.st_mode)) {
        ini_aug_add_string(ra_err,
                           "File %s is not a regular file. Skipping.",
                           name);
        return false;
    }

    if ((check_perm) && (check_perm->flags)) {
        error = access_check_int(name, &file_info,
                                 check_perm->flags,
                                 check_perm->uid,
                                 check_perm->gid,
                                 check_perm->mode,
                                 check_perm->mask);
        if(error) {
            TRACE_ERROR_NUMBER("Access check returned", error);
            ini_aug_add_string(ra_err,
                               "File %s did not pass access check. Skipping.",
                               name);
            return false;
        }
    }

    ret = true;

    TRACE_FLOW_EXIT();
    return ret;
}

/* Sort array */
static void ini_aug_sort_list(struct ref_array *ra_list)
{
    unsigned len = 0, j = 0, k = 0;
    char **item1 = NULL;
    char **item2 = NULL;

    TRACE_FLOW_ENTRY();

    len = ref_array_len(ra_list);
    if (len == 0) return;

    /* If have trace output array before sorting */
/*
#ifdef HAVE_TRACE
    for (i = 0; i < len; i++) {
        TRACE_INFO_STRING("Before:",
                          *((char **) ref_array_get(ra_list, i, NULL)));

    }
#endif
*/

    for (k = 0; k < len-1; k++) {
        j = k + 1;
        while (j > 0) {
            item1 = (char **) ref_array_get(ra_list, j - 1, NULL);
            item2 = (char **) ref_array_get(ra_list, j, NULL);
            /* Swap items if they are not NULL and string comparison
             * indicates that they need to be swapped or if the first
             * one is NULL but second is not. That would push
             * NULL elements of the array to the end of the array.
             */
            if (((item1 && item2) &&
                (strcoll(*item1,*item2)) > 0) ||
                (!item1 && item2)) {
                    ref_array_swap(ra_list, j - 1, j);
            }
            j--;
        }
    }

    /* And after sorting */
/*
#ifdef HAVE_TRACE
    for (i = 0; i < len; i++) {
        TRACE_INFO_STRING("After:",
                          *((char **) ref_array_get(ra_list, i, NULL)));

    }
#endif
*/

    TRACE_FLOW_EXIT();
}

/* Construct snippet lists based on the directory */
static int ini_aug_construct_list(char *dirname ,
                                  const char *patterns[],
                                  struct access_check *check_perm,
                                  struct ref_array *ra_list,
                                  struct ref_array *ra_err)
{

    int error = EOK;
    DIR *dir = NULL;
    struct dirent *entryp = NULL;
    char *snipname = NULL;
    char fullname[PATH_MAX + 1] = {0};
    struct ref_array *ra_regex = NULL;
    bool match = false;

    TRACE_FLOW_ENTRY();

    /* Prepare patterns */
    error = ini_aug_regex_prepare(patterns,
                                  ra_err,
                                  &ra_regex);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to prepare regex array.", error);
        return error;
    }

    /* Open directory */
    errno = 0;
    dir = opendir(dirname);
    if (!dir) {
        error = errno;
        if (error == ENOMEM) {
            TRACE_ERROR_NUMBER("No memory to open dir.", ENOMEM);
            ref_array_destroy(ra_regex);
            return ENOMEM;
        }
        /* Log an error, it is a recoverable error */
        add_dir_open_error(error, dirname, ra_err);
        ref_array_destroy(ra_regex);
        return EOK;
    }

    /* Loop through the directory */
    while (true)
    {
        errno = 0;
        entryp = readdir(dir);
        if (entryp == NULL && errno != 0) {
            error = errno;
            TRACE_ERROR_NUMBER("Failed to read directory.", error);
            ref_array_destroy(ra_regex);
            closedir(dir);
            return error;
        }

        /* Stop looping if we reached the end */
        if (entryp == NULL) break;

        TRACE_INFO_STRING("Processing", entryp->d_name);

        /* Always skip current and parent dirs */
        if ((strncmp(entryp->d_name,
                     INI_CURRENT_DIR,
                     sizeof(INI_CURRENT_DIR)) == 0) ||
            (strncmp(entryp->d_name,
                     INI_PARENT_DIR,
                     sizeof(INI_PARENT_DIR)) == 0)) continue;

        error = path_concat(fullname, PATH_MAX, dirname, entryp->d_name);
        if (error != EOK) {
            TRACE_ERROR_NUMBER("path_concat failed.", error);
            ref_array_destroy(ra_regex);
            closedir(dir);
            return error;
        }

        /* Match names */
        match = ini_aug_match_name(entryp->d_name, ra_regex);
        if (match) {
            if(ini_check_file_perm(fullname, check_perm, ra_err)) {

                /* Dup name and add to the array */
                snipname = NULL;
                snipname = strdup(fullname);
                if (snipname == NULL) {
                    TRACE_ERROR_NUMBER("Failed to dup string.", ENOMEM);
                    ref_array_destroy(ra_regex);
                    closedir(dir);
                    return ENOMEM;
                }

                error = ref_array_append(ra_list, (void *)&snipname);
                if (error) {
                    TRACE_ERROR_NUMBER("No memory to add file to "
                                       "the snippet list.",
                                       ENOMEM);
                    ref_array_destroy(ra_regex);
                    closedir(dir);
                    return ENOMEM;
                }
            }
        }
        else {
            TRACE_INFO_STRING("File did not match provided patterns."
                              " Skipping:",
                              fullname);
        }
    }

    closedir(dir);
    ref_array_destroy(ra_regex);

    ini_aug_sort_list(ra_list);

    TRACE_FLOW_EXIT();
    return EOK;
}

/* Construct the full dir path */
static int ini_aug_expand_path(const char *path, char **fullname)
{
    int error = EOK;
    char *dirname = NULL;

    TRACE_FLOW_ENTRY();
    TRACE_INFO_STRING("Input path", path);

    dirname = malloc(PATH_MAX + 1);
    if (!dirname) {
        TRACE_ERROR_NUMBER("Failed to allocate memory for file path.", ENOMEM);
        return ENOMEM;
    }

    /* Make the path */
    error = make_normalized_absolute_path(dirname,
                                          PATH_MAX,
                                          path);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to resolve path", error);
        free(dirname);
        /* This is a recoverable error */
        *fullname = NULL;
    }
    else *fullname = dirname;

    TRACE_INFO_STRING("Output path", *fullname);
    TRACE_FLOW_EXIT();

    return EOK;
}

/* Prepare the lists of the files that need to be merged */
static int ini_aug_preprare(const char *path,
                            const char *patterns[],
                            struct access_check *check_perm,
                            struct ref_array *ra_list,
                            struct ref_array *ra_err)
{
    int error = EOK;
    char *dirname = NULL;

    TRACE_FLOW_ENTRY();

    /* Contruct path */
    error = ini_aug_expand_path(path, &dirname);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to allocate memory for dir path.", error);
        return error;
    }

    /* Was it a good path? */
    if (!dirname) {
        TRACE_ERROR_NUMBER("Failed to resolve path", error);
        ini_aug_add_string(ra_err, "Could not resolve directory path %s.",
                           path);
        /* Path might not exist so it is a recoverable error */
        return EOK;
    }

    /* Construct snipet lists */
    error = ini_aug_construct_list(dirname,
                                   patterns,
                                   check_perm,
                                   ra_list,
                                   ra_err);
    free(dirname);

    TRACE_FLOW_EXIT();
    return error;
}

/* Cleanup callback for string arrays */
static void array_cleanup(void *elem,
                          ref_array_del_enum type,
                          void *data)
{
    TRACE_FLOW_ENTRY();
    free(*((char **)elem));
    TRACE_FLOW_EXIT();
}

/* Check that sections are in the given list */
static int ini_aug_match_sec(struct ini_cfgobj *snip_cfg,
                             struct ref_array *ra_regex,
                             struct ref_array *ra_err,
                             char *snip_name,
                             bool *skip)
{
    int error = EOK;
    char **section_list = NULL;
    char **section_iter = NULL;
    int size = 0;
    bool match = false;
    int match_count = 0;
    int section_count = 0;

    TRACE_FLOW_ENTRY();

    section_list = ini_get_section_list(snip_cfg, &size, &error);
    if (error) {
        TRACE_ERROR_NUMBER("Failed create section list", error);
        return error;
    }

    if (section_list == NULL) {
        /* No sections in the file */
        ini_aug_add_string(ra_err, "No sections found in file %s. Skipping.",
                           snip_name);
        *skip = true;
        TRACE_FLOW_EXIT();
        return EOK;
    }

    section_iter = section_list;

    while (*section_iter) {
        match = ini_aug_match_name(*section_iter, ra_regex);
        if (match) {
            match_count++;
            TRACE_INFO_STRING("Matched section", *section_iter);
        }
        else {
            TRACE_INFO_STRING("Section not matched", *section_iter);
            ini_aug_add_string(ra_err, "Section [%s] found in file %s is"
                                       " not allowed.",
                               *section_iter, snip_name);
        }
        section_count++;
        section_iter++;
    }

    ini_free_section_list(section_list);

    /* Just in case check that we processed anything */
    if (section_count == 0) {
        TRACE_INFO_STRING("No sections found in file. Skipping:",
                          snip_name);
        *skip = true;
        TRACE_FLOW_EXIT();
        return EOK;
    }

    /* Were all sections matched? */
    if (section_count != match_count) {
        /* Snippet containes sections that are not allowed */
        ini_aug_add_string(ra_err, "File %s contains sections that"
                                   " are not allowed. Skipping.",
                           snip_name);
        *skip = true;
        TRACE_FLOW_EXIT();
        return EOK;
    }

    /* Everything matched OK so we give green light to merge */
    TRACE_INFO_STRING("File will be included", snip_name);
    *skip = false;
    TRACE_FLOW_EXIT();
    return EOK;
}


/* Apply snippets */
static int ini_aug_apply(struct ini_cfgobj *cfg,
                         struct ref_array *ra_list,
                         const char *sections[],
                         int error_level,
                         uint32_t collision_flags,
                         uint32_t parse_flags,
                         uint32_t merge_flags,
                         struct ref_array *ra_err,
                         struct ref_array *ra_ok,
                         struct ini_cfgobj **out_cfg)
{
    int error = EOK;
    uint32_t len = 0;
    uint32_t i = 0;
    uint32_t j = 0;
    struct ini_cfgfile *file_ctx = NULL;
    struct ini_cfgobj *snip_cfg = NULL;
    struct ini_cfgobj *res_cfg = NULL;
    struct ini_cfgobj *tmp_cfg = NULL;
    char **error_list = NULL;
    unsigned cnt = 0;
    bool skip = false;
    struct ref_array *ra_regex = NULL;
    char *snip_name = NULL;
    char **snip_name_ptr = NULL;

    TRACE_FLOW_ENTRY();

    error = ini_config_copy(cfg, &res_cfg);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to copy config object", error);
        *out_cfg = NULL;
        return error;
    }

    len = ref_array_len(ra_list);
    if (len == 0) {
        /* List is empty - nothing to do */
        *out_cfg = res_cfg;
        TRACE_FLOW_EXIT();
        return EOK;
    }

    /* Prepare patterns */
    error = ini_aug_regex_prepare(sections,
                                  ra_err,
                                  &ra_regex);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to prepare regex array.", error);
        *out_cfg = res_cfg;
        return error;
    }

    /* Loop through the snippets */
    for (i = 0; i < len; i++) {

        /* Prepare config object */
        error = ini_config_create(&snip_cfg);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to create config object", error);
            goto err;
        }

        /* Process snippet */
        snip_name_ptr = (char **)ref_array_get (ra_list, i, NULL);
        if (snip_name_ptr == NULL) continue;
        snip_name = *snip_name_ptr;
        if (snip_name == NULL) continue;

        TRACE_INFO_STRING("Processing", snip_name);

        /* Open file */
        error = ini_config_file_open(snip_name,
                                     INI_META_NONE,
                                     &file_ctx);
        if (error) {
            TRACE_ERROR_NUMBER("Failed to open snippet.", error);
            ini_aug_add_string(ra_err, "Failed to open file %s.", snip_name);
            ini_config_destroy(snip_cfg);
            /* We can recover so go on */
            continue;
        }

        TRACE_INFO_NUMBER("Error level:", error_level);
        TRACE_INFO_NUMBER("Collision flags:", collision_flags);
        TRACE_INFO_NUMBER("Parse level:", parse_flags);

        /* Read config */
        error = ini_config_parse(file_ctx,
                                 error_level,
                                 collision_flags,
                                 parse_flags,
                                 snip_cfg);

        ini_config_file_destroy(file_ctx);
        file_ctx = NULL;

        if (error) {
            TRACE_ERROR_NUMBER("Failed to parse configuration.", error);
            cnt = ini_config_error_count(snip_cfg);
            if (cnt) {
                ini_aug_add_string(ra_err,
                                   "Errors detected while parsing: %s.",
                                   snip_name);

                /* Extract errors */
                error = ini_config_get_errors(snip_cfg, &error_list);
                if (error) {
                    TRACE_ERROR_NUMBER("Can't get errors.", error);
                    ini_config_destroy(snip_cfg);
                    goto err;
                }

                /* Copy errors into error array */
                for (j=0; j< cnt; j++) {
                    ini_aug_add_string(ra_err, error_list[j]);
                }
                ini_config_free_errors(error_list);
            }
            /* The snippet was malformed, this is OK, go on */
            if (error_level != INI_STOP_ON_NONE) {
                ini_aug_add_string(ra_err,
                                   "Due to errors file %s is not considered."
                                   " Skipping.",
                                   snip_name);
                ini_config_destroy(snip_cfg);
                continue;
            }
            /* If we are told to not stop try to process anyway */
        }

        /* Validate that file contains only allowed sections */
        if (sections) {
            /* Use a safe default, function should update it anyways
             * but it is better to not merge than to allow bad snippet */
            skip = true;
            error = ini_aug_match_sec(snip_cfg, ra_regex, ra_err,
                                      snip_name, &skip);
            if (error) {
                TRACE_ERROR_NUMBER("Failed to validate section.", error);
                ini_config_destroy(snip_cfg);
                goto err;
            }
        }

        /* Merge */
        if (!skip) {
            /* col_debug_collection(res_cfg->cfg, COL_TRAVERSE_DEFAULT); */
            error = ini_config_merge(res_cfg, snip_cfg, merge_flags, &tmp_cfg);
            if (error) {
                if (error == ENOMEM) {
                    TRACE_ERROR_NUMBER("Merge failed.", error);
                    ini_config_destroy(snip_cfg);
                    goto err;
                }
                else if
                    ((error == EEXIST) &&
                     ((ini_flags_have(INI_MS_DETECT, merge_flags) &&
                       ((merge_flags & INI_MV2S_MASK) != INI_MV2S_ERROR)) ||
                      ((!ini_flags_have(INI_MS_ERROR, merge_flags)) &&
                       ((merge_flags & INI_MV2S_MASK) == INI_MV2S_DETECT)))) {
                        TRACE_ERROR_NUMBER("Got error in detect mode", error);
                        /* Fall through! */
                    ini_aug_add_string(ra_err, "Duplicate section detected "
                                       "in snippet: %s.", snip_name);
                }
                else {
                    ini_aug_add_string(ra_err,
                                       "Errors during merge."
                                       " Snippet ignored %s.",
                                       snip_name);
                    /* The snippet failed to merge, this is OK, go on */
                    TRACE_INFO_NUMBER("Merge failure.Continue. Error", error);
                    ini_config_destroy(snip_cfg);
                    continue;
                }
            }
            TRACE_INFO_STRING("Merged file.", snip_name);
            /* col_debug_collection(tmp_cfg->cfg, COL_TRAVERSE_DEFAULT); */
            ini_config_destroy(res_cfg);
            res_cfg = tmp_cfg;

            /* Record that snippet was successfully merged */
            ini_aug_add_string(ra_ok, "%s", snip_name);
        }
        /* Cleanup */
        ini_config_destroy(snip_cfg);
    }

    ref_array_destroy(ra_regex);
    *out_cfg = res_cfg;
    TRACE_FLOW_EXIT();
    return error;

err:
    ini_config_destroy(res_cfg);
    ref_array_destroy(ra_regex);

    if (ini_config_copy(cfg, &res_cfg)) {
        TRACE_ERROR_NUMBER("Failed to copy config object", error);
        *out_cfg = NULL;
        return error;
    }

    *out_cfg = res_cfg;

    return error;
}

/* Function to merge additional snippets of the config file
 * from a provided directory.
 */
int ini_config_augment(struct ini_cfgobj *base_cfg,
                       const char *path,
                       const char *patterns[],
                       const char *sections[],
                       struct access_check *check_perm,
                       int error_level,
                       uint32_t collision_flags,
                       uint32_t parse_flags,
                       uint32_t merge_flags,
                       struct ini_cfgobj **result_cfg,
                       struct ref_array **error_list,
                       struct ref_array **success_list)
{
    int error = EOK;
    /* The internal list that will hold snippet file names */
    struct ref_array *ra_list = NULL;
    /* List of error strings that will be returned to the caller */
    struct ref_array *ra_err = NULL;
    /* List of files that were merged */
    struct ref_array *ra_ok = NULL;

    /* Check arguments */
    if (base_cfg == NULL) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }

    if (result_cfg == NULL) {
        TRACE_ERROR_NUMBER("Invalid argument", EINVAL);
        return EINVAL;
    }


    /* Create arrays for lists */
    if ((ref_array_create(&ra_list,
                          sizeof(char *),
                          INI_AUG_ARR_SIZE_INC,
                          array_cleanup,
                          NULL) != 0) ||
        (ref_array_create(&ra_err,
                          sizeof(char *),
                          INI_AUG_ARR_SIZE_INC * 5,
                          array_cleanup,
                          NULL) != 0) ||
        (ref_array_create(&ra_ok,
                          sizeof(char *),
                          INI_AUG_ARR_SIZE_INC * 5,
                          array_cleanup,
                          NULL) != 0)) {
        TRACE_ERROR_NUMBER("Failed to allocate memory for arrays.",
                           ENOMEM);
        ref_array_destroy(ra_list);
        ref_array_destroy(ra_err);
        ref_array_destroy(ra_ok);
        return ENOMEM;
    }

    /* Construct snipet lists */
    error = ini_aug_preprare(path,
                             patterns,
                             check_perm,
                             ra_list,
                             ra_err);
    if (error) {
        TRACE_ERROR_NUMBER("Failed to prepare lists of snippets.",
                           error);
        ref_array_destroy(ra_list);
        ref_array_destroy(ra_err);
        ref_array_destroy(ra_ok);
        return error;
    }

    /* Apply snippets */
    error = ini_aug_apply(base_cfg,
                          ra_list,
                          sections,
                          error_level,
                          collision_flags,
                          parse_flags,
                          merge_flags,
                          ra_err,
                          ra_ok,
                          result_cfg);

    /* Cleanup */
    ref_array_destroy(ra_list);

    if (error_list) {
        *error_list = ra_err;
    }
    else {
        ref_array_destroy(ra_err);
    }

    if (success_list) {
        *success_list = ra_ok;
    }
    else {
        ref_array_destroy(ra_ok);
    }

    TRACE_FLOW_EXIT();
    return error;
}
