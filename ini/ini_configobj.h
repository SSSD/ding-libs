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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include "simplebuffer.h"


/**
 * @defgroup errorlevel Error tolerance constants
 *
 * Constants in this section define what to do if
 * error or warning encountered while parsing the INI file.
 *
 * @{
 */
/** @brief Fail if any problem is detected. */
#define INI_STOP_ON_ANY     0
/** @brief Best effort - do not fail. */
#define INI_STOP_ON_NONE    1
/** @brief Fail on errors only. */
#define INI_STOP_ON_ERROR   2

/**
 * @}
 */

/**
 * @defgroup parseerr Parsing errors and warnings
 *
 * @{
 */
/** @brief Line is too long (Error). */
#define ERR_LONGDATA        1
/** @brief No closing bracket in section definition (Error). */
#define ERR_NOCLOSESEC      2
/** @brief Section name is missing (Error). */
#define ERR_NOSECTION       3
/** @brief Section name too long (Error). */
#define ERR_SECTIONLONG     4
/** @brief No equal sign (Error). */
#define ERR_NOEQUAL         5
/** @brief No key before equal sign (Error). */
#define ERR_NOKEY           6
/** @brief Key is too long (Error). */
#define ERR_LONGKEY         7
/** @brief Failed to read line (Error). */
#define ERR_READ            8
/** @brief Line starts with space when it should not (Error). */
#define ERR_SPACE           9
/** @brief Duplicate key is not allowed (Error). */
#define ERR_DUPKEY          10
/** @brief Duplicate key is detected while merging sections (Error). */
#define ERR_DUPKEYSEC       11
/** @brief Duplicate section is not allowed (Error). */
#define ERR_DUPSECTION      12

/** @brief Size of the error array. */
#define ERR_MAXPARSE        ERR_DUPSECTION

/**
 * @}
 */

/**
 * @defgroup metacollect Constants that define what meta data to collect
 *
 * Constants in this section define what meta data to collect
 *
 *
 * @{
 */
/** @brief Do not collect any data. */
#define INI_META_NONE     0
/** @brief Collect file stats. */
#define INI_META_STATS    1

/**
 * @}
 */

/**
 * @defgroup accesscheck Access control check flags
 *
 * @{
 */

/**
 * @brief Validate access mode
 *
 * If this flag is specified the mode parameter
 * will be matched against the permissions set on the file
 * using the provided mask.
 */
#define INI_ACCESS_CHECK_MODE   0x00000001

/**
 * @brief Validate uid
 *
 * Provided uid will be checked against uid
 * of the file.
 */
#define INI_ACCESS_CHECK_UID   0x00000002

/**
 * @brief Validate gid
 *
 * Provided gid will be checked against gid
 * of the file.
 */
#define INI_ACCESS_CHECK_GID   0x00000004

/**
 * @}
 */

/**
 * @defgroup collisionflags Flags that define collision resolution logic.
 *
 * @{
 */

/**
 * @defgroup onesecvalue Colliding values come from one section
 *
 * Flags that define collision resolution logic for values in
 * the same section.
 * These flags should be used during parsing to handle duplicate
 * keys in the same section of the ini file.
 *
 * @{
 */

/** @brief Value with same key is ovewritten */
#define INI_MV1S_OVERWRITE 0x0000
/** @brief Collision causes error */
#define INI_MV1S_ERROR     0x0001
/** @brief Second value is discarded */
#define INI_MV1S_PRESERVE  0x0002
/** @brief Duplicates are allowed */
#define INI_MV1S_ALLOW     0x0003
/** @brief Duplicates are allowed but errors are logged */
#define INI_MV1S_DETECT    0x0004

/**
 * @}
 */

/**
 * @defgroup twosecvalue Colliding values come from two sections
 *
 * Flags that define collision resolution logic between two values
 * that come from two sections with the same name.
 * These flags should be used during parsing to handle duplicate
 * keys coming from the same section scattered across the ini file.
 * These flags also can be used to specify the rules of merging
 * values that come from two files separate configuration files.
 *
 * @{
 */
/** @brief Value with same key is ovewritten */
#define INI_MV2S_OVERWRITE 0x0000
/** @brief Collision causes error */
#define INI_MV2S_ERROR     0x0010
/** @brief Second value is discarded */
#define INI_MV2S_PRESERVE  0x0020
/** @brief Duplicates are allowed */
#define INI_MV2S_ALLOW     0x0030
/** @brief Duplicates are allowed but errors are logged */
#define INI_MV2S_DETECT    0x0040

/**
 * @}
 */

/**
 * @defgroup mergesec Collision in two sections
 *
 * Flags that define collision resolution logic between two sections.
 * These flags should be used during parsing to handle duplicate
 * sections scattered across the ini file.
 * These flags also can be used to specify the rules of merging
 * sections that come from two separate configuration files.
 *
 * @{
 */
/** @brief Sections are merged */
#define INI_MS_MERGE     0x0000
/** @brief Collision causes error */
#define INI_MS_ERROR     0x0100
/** @brief First section is discarded */
#define INI_MS_OVERWRITE 0x0200
/** @brief Second section is discarded */
#define INI_MS_PRESERVE  0x0300
/** @brief Duplicates are allowed */
#define INI_MS_ALLOW     0x0400
/** @brief Duplicates are allowed but errors are logged */
#define INI_MS_DETECT    0x0500

/**
 * @}
 */
/**
 * @}
 */


/**
 * @defgroup structures Structures
 * @{
 */


struct ini_cfgobj;
struct ini_cfgfile;

/** @brief Structure that holds error number and
 *  line number for the encountered error.
 */
struct ini_parse_error;


/**
 * @}
 */


/********************************************************************/
/* THIS IS A BEGINNING OF THE THE NEW CONFIG OBJECT INTERFACE - TBD */
/* It will be moved to the ini_config.h when it is ready            */
/********************************************************************/


/* Create a configuration object */
int ini_config_create(struct ini_cfgobj **ini_config);

/* Destroy a configuration object */
void ini_config_destroy(struct ini_cfgobj *ini_config);

/* Create a file object for parsing a config file */
int ini_config_file_open(const char *filename,
                         int error_level,
                         uint32_t collision_flags,
                         uint32_t metadata_flags,
                         struct ini_cfgfile **file_ctx);

/* Create a file object from existing one */
int ini_config_file_reopen(struct ini_cfgfile *file_ctx_in,
                           struct ini_cfgfile **file_ctx_out);

/* Close file context */
void ini_config_file_close(struct ini_cfgfile *file_ctx);

/* Close file context and destroy the object */
void ini_config_file_destroy(struct ini_cfgfile *file_ctx);

/* How many errors do we have in the list ? */
unsigned ini_config_error_count(struct ini_cfgfile *file_ctx);

/* Get the list of error strings */
int ini_config_get_errors(struct ini_cfgfile *file_ctx,
                          char ***errors);

/* Get the fully resolved file name */
const char *ini_config_get_filename(struct ini_cfgfile *file_ctx);

/* Free error strings */
void ini_config_free_errors(char **errors);

/* Parse the file and create a config object */
int ini_config_parse(struct ini_cfgfile *file_ctx,
                     struct ini_cfgobj *ini_config);

/* Copy configuration */
int ini_config_copy(struct ini_cfgobj *ini_config,
                    struct ini_cfgobj **ini_new);

/* Function to print errors from the list */
void ini_config_print_errors(FILE *file, char **error_list);

/* Merge two configurations together creating a new one */
int ini_config_merge(struct ini_cfgobj *first,
                     struct ini_cfgobj *second,
                     uint32_t collision_flags,
                     struct ini_cfgobj **result);

/* Set the folding boundary for multiline values.
 * Use before serializing and saving to a file if the
 * default boundary of 80 characters does not work for you.
 */
int ini_config_set_wrap(struct ini_cfgobj *ini_config,
                        uint32_t boundary);

/* Serialize configuration object into provided buffer */
int ini_config_serialize(struct ini_cfgobj *ini_config,
                         struct simplebuffer *sbobj);

/* Check access */
int ini_config_access_check(struct ini_cfgfile *file_ctx,
                            uint32_t flags,
                            uid_t uid,
                            gid_t gid,
                            mode_t mode,
                            mode_t mask);

/* Determins if two file context different by comparing
 * - time stamp
 * - device ID
 * - i-node
 */
int ini_config_changed(struct ini_cfgfile *file_ctx1,
                       struct ini_cfgfile *file_ctx2,
                       int *changed);


/****************************************************/
/* VALUE MANAGEMENT                                 */
/****************************************************/

/**
 * @brief Get list of sections.
 *
 * Get list of sections from the configuration object
 * as an array of strings.
 * Function allocates memory for the array of the sections.
 * Use \ref free_section_list() to free allocated memory.
 *
 * @param[in]  ini_config       Configuration object.
 * @param[out] size             If not NULL parameter will
 *                              receive number of sections
 *                              in the configuration.
 * @param[out] error            If not NULL parameter will
 *                              receive the error code.
 *                              0 - Success.
 *                              EINVAL - Invalid parameter.
 *                              ENOMEM - No memory.
 *
 * @return Array of strings.
 */
char **get_section_list(struct ini_cfgobj *ini_config,
                        int *size,
                        int *error);

/**
 * @brief Free list of sections.
 *
 * The section array created by \ref get_section_list()
 * should be freed using this function.
 *
 * @param[in] section_list       Array of strings returned by
 *                               \ref get_section_list() function.
 */
void free_section_list(char **section_list);

/**
 * @brief Get list of attributes.
 *
 * Get list of attributes in a section as an array of strings.
 * Function allocates memory for the array of attributes.
 * Use \ref free_attribute_list() to free allocated memory.
 *
 * @param[in]  ini_config       Configuration object.
 * @param[in]  section          Section name.
 * @param[out] size             If not NULL parameter will
 *                              receive number of attributes
 *                              in the section.
 * @param[out] error            If not NULL parameter will
 *                              receive the error code.
 *                              0 - Success.
 *                              EINVAL - Invalid parameter.
 *                              ENOMEM - No memory.
 *
 * @return Array of strings.
 */
char **get_attribute_list(struct ini_cfgobj *ini_config,
                          const char *section,
                          int *size,
                          int *error);

/**
 * @brief Free list of attributes.
 *
 * The attribute array created by \ref get_attribute_list()
 * should be freed using this function.
 *
 * @param[in] attr_list          Array of strings returned by
 *                               \ref get_attribute_list() function.
 */
void free_attribute_list(char **attr_list);


/**
 * @brief Get an integer value from the configuration.
 *
 * Function looks up the section and key in
 * in the configuration object and tries to convert
 * into an integer number.
 * The results can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 * The value range is from INT_MIN to INT_MAX.
 *
 * @param[in]  ini_config       Configuration object.
 * @param[in]  section          Section of the configuration file.
 * @param[in]  name             Key to look up inside the section.
 * @param[in]  strict           Fail the function if
 *                              the symbol after last digit
 *                              is not valid.
 * @param[in]  def              Default value to use if
 *                              conversion failed.
 * @param[out] value            Fetched value or default
 *
 * In case of failure the function assignes default value
 * to the resulting value and returns corresponging error code.
 *
 * @return 0 - Success.
 * @return EINVAL - Argument is invalid.
 * @return EIO - Conversion failed due invalid characters.
 * @return ERANGE - Value is out of range.
 * @return ENOKEY - Value is not found.
 * @return ENOMEM - No memory.
 */
int get_int_config_value(struct ini_cfgobj *ini_config,
                         const char *section,
                         const char *name,
                         int strict,
                         int def,
                         int *value);

/* Similar functions are below */
int get_long_config_value(struct ini_cfgobj *ini_config,
                          const char *section,
                          const char *name,
                          int strict,
                          long def,
                          long *value);

int get_unsigned_config_value(struct ini_cfgobj *ini_config,
                              const char *section,
                              const char *name,
                              int strict,
                              unsigned def,
                              unsigned *value);

int get_ulong_config_value(struct ini_cfgobj *ini_config,
                           const char *section,
                           const char *name,
                           int strict,
                           unsigned long def,
                           unsigned long *value);







#ifdef THE_PART_I_HAVE_PROCESSED






/**
 * @brief Convert item value to integer number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration item
 * into an int32_t number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the item.
 * The results can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 * The value range is from INT_MIN to INT_MAX.
 *
 * @param[in]  item             Item to interpret.
 *                              It must be retrieved using
 *                              \ref get_config_item().
 * @param[in]  strict           Fail the function if
 *                              the symbol after last digit
 *                              is not valid.
 * @param[in]  def              Default value to use if
 *                              conversion failed.
 * @param[out] error            Variable will get the value
 *                              of the error code if
 *                              error happened.
 *                              Can be NULL. In this case
 *                              function does not set
 *                              the code.
 *                              Codes:
 *                              - 0 - Success.
 *                              - EINVAL - Argument is invalid.
 *                              - EIO - Conversion failed due
 *                                invalid characters.
 *                              - ERANGE - Value is out of range.
 *
 * @return Converted value.
 * In case of failure the function returns default value and
 * sets error code into the provided variable.
 */
int32_t get_int32_config_value(struct collection_item *item,
                               int strict,
                               int32_t def,
                               int *error);

/**
 * @brief Convert item value to integer number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration item
 * into an uint32_t number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the item.
 * The results can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 * The value range is from 0 to ULONG_MAX.
 *
 * @param[in]  item             Item to interpret.
 *                              It must be retrieved using
 *                              \ref get_config_item().
 * @param[in]  strict           Fail the function if
 *                              the symbol after last digit
 *                              is not valid.
 * @param[in]  def              Default value to use if
 *                              conversion failed.
 * @param[out] error            Variable will get the value
 *                              of the error code if
 *                              error happened.
 *                              Can be NULL. In this case
 *                              function does not set
 *                              the code.
 *                              Codes:
 *                              - 0 - Success.
 *                              - EINVAL - Argument is invalid.
 *                              - EIO - Conversion failed due
 *                                invalid characters.
 *                              - ERANGE - Value is out of range.
 *
 * @return Converted value.
 * In case of failure the function returns default value and
 * sets error code into the provided variable.
 */
uint32_t get_uint32_config_value(struct collection_item *item,
                                 int strict,
                                 uint32_t def,
                                 int *error);

/**
 * @brief Convert item value to integer number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration item
 * into an int64_t number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the item.
 * The results can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 * The value range is from LLONG_MIN to LLONG_MAX.
 *
 * @param[in]  item             Item to interpret.
 *                              It must be retrieved using
 *                              \ref get_config_item().
 * @param[in]  strict           Fail the function if
 *                              the symbol after last digit
 *                              is not valid.
 * @param[in]  def              Default value to use if
 *                              conversion failed.
 * @param[out] error            Variable will get the value
 *                              of the error code if
 *                              error happened.
 *                              Can be NULL. In this case
 *                              function does not set
 *                              the code.
 *                              Codes:
 *                              - 0 - Success.
 *                              - EINVAL - Argument is invalid.
 *                              - EIO - Conversion failed due
 *                                invalid characters.
 *                              - ERANGE - Value is out of range.
 *
 * @return Converted value.
 * In case of failure the function returns default value and
 * sets error code into the provided variable.
 */
int64_t get_int64_config_value(struct collection_item *item,
                               int strict,
                               int64_t def,
                               int *error);

/**
 * @brief Convert item value to integer number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration item
 * into an uint64_t number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the item.
 * The results can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 * The value range is from 0 to ULLONG_MAX.
 *
 * @param[in]  item             Item to interpret.
 *                              It must be retrieved using
 *                              \ref get_config_item().
 * @param[in]  strict           Fail the function if
 *                              the symbol after last digit
 *                              is not valid.
 * @param[in]  def              Default value to use if
 *                              conversion failed.
 * @param[out] error            Variable will get the value
 *                              of the error code if
 *                              error happened.
 *                              Can be NULL. In this case
 *                              function does not set
 *                              the code.
 *                              Codes:
 *                              - 0 - Success.
 *                              - EINVAL - Argument is invalid.
 *                              - EIO - Conversion failed due
 *                                invalid characters.
 *                              - ERANGE - Value is out of range.
 *
 * @return Converted value.
 * In case of failure the function returns default value and
 * sets error code into the provided variable.
 */
uint64_t get_uint64_config_value(struct collection_item *item,
                                 int strict,
                                 uint64_t def,
                                 int *error);

/**
 * @brief Convert item value to floating point number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration item
 * into a floating point number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the item.
 * The results can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 *
 * @param[in]  item             Item to interpret.
 *                              It must be retrieved using
 *                              \ref get_config_item().
 * @param[in]  strict           Fail the function if
 *                              the symbol after last digit
 *                              is not valid.
 * @param[in]  def              Default value to use if
 *                              conversion failed.
 * @param[out] error            Variable will get the value
 *                              of the error code if
 *                              error happened.
 *                              Can be NULL. In this case
 *                              function does not set
 *                              the code.
 *                              Codes:
 *                              - 0 - Success.
 *                              - EINVAL - Argument is invalid.
 *                              - EIO - Conversion failed due
 *                                invalid characters.
 *
 * @return Converted value.
 * In case of failure the function returns default value and
 * sets error code into the provided variable.
 */
double get_double_config_value(struct collection_item *item,
                               int strict,
                               double def,
                               int *error);

/**
 * @brief Convert item value into a logical value.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration item
 * into a Boolean. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the item.
 * The results can be different depending upon
 * how the caller tries to interpret the value.
 *
 * @param[in]  item             Item to interpret.
 *                              It must be retrieved using
 *                              \ref get_config_item().
 * @param[in]  def              Default value to use if
 *                              conversion failed.
 * @param[out] error            Variable will get the value
 *                              of the error code if
 *                              error happened.
 *                              Can be NULL. In this case
 *                              function does not set
 *                              the code.
 *                              Codes:
 *                              - 0 - Success.
 *                              - EINVAL - Argument is invalid.
 *                              - EIO - Conversion failed due
 *                                invalid characters.
 *
 * @return Converted value.
 * In case of failure the function returns default value and
 * sets error code into the provided variable.
 */
unsigned char get_bool_config_value(struct collection_item *item,
                                    unsigned char def,
                                    int *error);

/**
 * @brief Get string configuration value
 *
 * Function creates a copy of the string value stored in the item.
 * Returned value needs to be freed after use.
 * If error occurred the returned value will be NULL.
 *
 * @param[in]  item             Item to use.
 *                              It must be retrieved using
 *                              \ref get_config_item().
 * @param[out] error            Variable will get the value
 *                              of the error code if
 *                              error happened.
 *                              Can be NULL. In this case
 *                              function does not set
 *                              the code.
 *                              Codes:
 *                              - 0 - Success.
 *                              - EINVAL - Argument is invalid.
 *                              - ENOMEM - No memory.
 *
 * @return Copy of the string or NULL.
 */
char *get_string_config_value(struct collection_item *item,
                              int *error);
/**
 * @brief Function returns the string stored in the item.
 *
 * Function returns a reference to the string value
 * stored inside the item. This string can't be altered.
 * The string will go out of scope if the item is deleted.
 *
 * @param[in]  item             Item to use.
 *                              It must be retrieved using
 *                              \ref get_config_item().
 * @param[out] error            Variable will get the value
 *                              of the error code if
 *                              error happened.
 *                              Can be NULL. In this case
 *                              function does not set
 *                              the code.
 *                              Codes:
 *                              - 0 - Success.
 *                              - EINVAL - Argument is invalid.
 *
 * @return String from the item.
 */
const char *get_const_string_config_value(struct collection_item *item,
                                          int *error);

/**
 * @brief Convert item value into a binary sequence.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration item
 * into a sequence of bytes.
 * Any of the conversion functions
 * can be used to try to convert the value
 * stored as a string inside the item.
 * The results can be different depending upon
 * how the caller tries to interpret the value.
 *
 * The function allocates memory.
 * It is the responsibility of the caller to free it after use.
 * Use \ref free_bin_config_value() for this purpose.
 * Functions will return NULL if conversion failed.
 *
 * Function assumes that the value being interpreted
 * has a special format.
 * The string should be taken in single quotes
 * and consist of hex encoded value represented by
 * two hex digits per byte.
 * Case does not matter.
 *
 * Example: '0a2BFeCc'
 *
 * @param[in]  item             Item to interpret.
 *                              It must be retrieved using
 *                              \ref get_config_item().
 * @param[out] length           Variable that optionally receives
 *                              the length of the binary
 *                              sequence.
 * @param[out] error            Variable will get the value
 *                              of the error code if
 *                              error happened.
 *                              Can be NULL. In this case
 *                              function does not set
 *                              the code.
 *                              Codes:
 *                              - 0 - Success.
 *                              - EINVAL - Argument is invalid.
 *                              - EIO - Conversion failed due
 *                                invalid characters.
 *                              - ENOMEM - No memory.
 *
 * @return Converted value.
 * In case of failure the function returns NULL.
 */
char *get_bin_config_value(struct collection_item *item,
                           int *length,
                           int *error);

/**
 * @brief Free binary buffer
 *
 * Free binary value returned by \ref get_bin_config_value().
 *
 * @param[in] bin              Binary buffer to free.
 *
 */
void free_bin_config_value(char *bin);

/**
 * @brief Convert value to an array of strings.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration item
 * into an array of strings. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the item.
 * The results can be different depending upon
 * how the caller tries to interpret the value.
 *
 * Separator string includes up to three different separators.
 * If separator NULL, comma is assumed.
 * The spaces are trimmed automatically around separators
 * in the string.
 * The function drops empty tokens from the list.
 * This means that the string like this: "apple, ,banana, ,orange ,"
 * will be translated into the list of three items:
 * "apple","banana" and "orange".
 *
 * The length of the allocated array is returned in "size".
 * Size and error parameters can be NULL.
 * Use \ref free_string_config_array() to free the array after use.
 *
 * The array is always NULL terminated so
 * it is safe not to get size and just loop until
 * array element is NULL.
 *
 * @param[in]  item             Item to interpret.
 *                              It must be retrieved using
 *                              \ref get_config_item().
 * @param[in]  sep              String cosisting of separator
 *                              symbols. For example: ",.;" would mean
 *                              that comma, dot and semicolon
 *                              should be treated as separators
 *                              in the value.
 * @param[out] size             Variable that optionally receives
 *                              the size of the array.
 * @param[out] error            Variable will get the value
 *                              of the error code if
 *                              error happened.
 *                              Can be NULL. In this case
 *                              function does not set
 *                              the code.
 *                              Codes:
 *                              - 0 - Success.
 *                              - EINVAL - Argument is invalid.
 *                              - EIO - Conversion failed.
 *                              - ENOMEM - No memory.
 *
 * @return Array of strings.
 * In case of failure the function returns NULL.
 */
char **get_string_config_array(struct collection_item *item,
                               const char *sep,
                               int *size,
                               int *error);

/**
 * @brief Convert value to an array of strings.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration item
 * into an array of strings. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the item.
 * The results can be different depending upon
 * how the caller tries to interpret the value.
 *
 * Separator string includes up to three different separators.
 * If separator NULL, comma is assumed.
 * The spaces are trimmed automatically around separators
 * in the string.
 * The function does not drop empty tokens from the list.
 * This means that the string like this: "apple, ,banana, ,orange ,"
 * will be translated into the list of five items:
 * "apple", "", "banana", "" and "orange".
 *
 * The length of the allocated array is returned in "size".
 * Size and error parameters can be NULL.
 * Use \ref free_string_config_array() to free the array after use.
 *
 * The array is always NULL terminated so
 * it is safe not to get size and just loop until
 * array element is NULL.
 *
 * @param[in]  item             Item to interpret.
 *                              It must be retrieved using
 *                              \ref get_config_item().
 * @param[in]  sep              String cosisting of separator
 *                              symbols. For example: ",.;" would mean
 *                              that comma, dot and semicolon
 *                              should be treated as separators
 *                              in the value.
 * @param[out] size             Variable that optionally receives
 *                              the size of the array.
 * @param[out] error            Variable will get the value
 *                              of the error code if
 *                              error happened.
 *                              Can be NULL. In this case
 *                              function does not set
 *                              the code.
 *                              Codes:
 *                              - 0 - Success.
 *                              - EINVAL - Argument is invalid.
 *                              - EIO - Conversion failed.
 *                              - ENOMEM - No memory.
 *
 * @return Array of strings.
 * In case of failure the function returns NULL.
 */
char **get_raw_string_config_array(struct collection_item *item,
                                   const char *sep,
                                   int *size,
                                   int *error);

/**
 * @brief Convert value to an array of long values.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration item
 * into an array of long values. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the item.
 * The results can be different depending upon
 * how the caller tries to interpret the value.
 *
 * Separators inside the string are detected automatically.
 * The spaces are trimmed automatically around separators
 * in the string.
 *
 * The length of the allocated array is returned in "size".
 * Size parameter can't be NULL.
 *
 * Use \ref free_long_config_array() to free the array after use.
 *
 * @param[in]  item             Item to interpret.
 *                              It must be retrieved using
 *                              \ref get_config_item().
 * @param[out] size             Variable that receives
 *                              the size of the array.
 * @param[out] error            Variable will get the value
 *                              of the error code if
 *                              error happened.
 *                              Can be NULL. In this case
 *                              function does not set
 *                              the code.
 *                              Codes:
 *                              - 0 - Success.
 *                              - EINVAL - Argument is invalid.
 *                              - EIO - Conversion failed.
 *                              - ERANGE - Value is out of range.
 *                              - ENOMEM - No memory.
 *
 * @return Array of long values.
 * In case of failure the function returns NULL.
 */
long *get_long_config_array(struct collection_item *item,
                            int *size,
                            int *error);

/**
 * @brief Convert value to an array of floating point values.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration item
 * into an array of floating point values. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the item.
 * The results can be different depending upon
 * how the caller tries to interpret the value.
 *
 * Separators inside the string are detected automatically.
 * The spaces are trimmed automatically around separators
 * in the string.
 *
 * The length of the allocated array is returned in "size".
 * Size parameter can't be NULL.
 *
 * Use \ref free_double_config_array() to free the array after use.
 *
 * @param[in]  item             Item to interpret.
 *                              It must be retrieved using
 *                              \ref get_config_item().
 * @param[out] size             Variable that receives
 *                              the size of the array.
 * @param[out] error            Variable will get the value
 *                              of the error code if
 *                              error happened.
 *                              Can be NULL. In this case
 *                              function does not set
 *                              the code.
 *                              Codes:
 *                              - 0 - Success.
 *                              - EINVAL - Argument is invalid.
 *                              - EIO - Conversion failed.
 *                              - ENOMEM - No memory.
 *
 * @return Array of floating point values.
 * In case of failure the function returns NULL.
 */
double *get_double_config_array(struct collection_item *item,
                                int *size,
                                int *error);

/**
 * @brief Free array of string values.
 *
 * Use this function to free the array returned by
 * \ref get_string_config_array() or by
 * \ref get_raw_string_config_array().
 *
 * @param[in] str_config        Array of string values.
 */
void free_string_config_array(char **str_config);

/**
 * @brief Free array of long values.
 *
 * Use this function to free the array returned by
 * \ref get_long_config_array().
 *
 * @param[in] array         Array of long values.
 */
void free_long_config_array(long *array);
/**
 * @brief Free array of floating pointer values.
 *
 * Use this function to free the array returned by
 * \ref get_double_config_array().
 *
 * @param[in] array         Array of floating pointer values.
 */
void free_double_config_array(double *array);

#endif

#endif
