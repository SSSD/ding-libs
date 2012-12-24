/*
    INI LIBRARY

    Header file for the ini configuration interface.
    THIS IS THE PREFERRED INTERFACE TO USE.

    Copyright (C) Dmitri Pal <dpal@redhat.com> 2010 - 2012

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
#include "ini_valueobj.h"

/** @mainpage The INI configuration interface
 *
 * The goal of the this interface is to allow applications
 * to read configuration from an INI file.
 *
 * So why yet another library to read data from an INI file?
 * As we started the SSSD project we looked around for a
 * open source library that would meet the following
 * requirements:
 * - Is written in C (not C++)
 * - Is lightweight.
 * - Has an live community.
 * - Supported on multiple platforms .
 * - Can evolve as we build SSSD solution.
 * - Can deal with different types of values including arrays.
 * - Can deal with sections that are related to each other
 *   and can form a hierarchy of sections.
 * - Has a compatible license we can use.
 *
 * We have seen several solutions but none was able to fully address
 * our requirements. As a result we started developing our own
 * INI parsing library.
 *
 * Currently INI parser allows reading and merging INI files
 * and getting a resulting configuration in one object.
 *
 * One of the main advantages of this interface is that
 * the library is created with the idea of reading the configuration
 * data, not managing it. Thus currently you will not find
 * any function that alters the configuration data read from the files.
 * There is a set of proposed enhancements to be able to manipulate
 * the configuration data and save it back but there has been no real
 * driver for it. This API is focused on letting applications read data
 * from a file (or files) and interpret it, not to generate configuration
 * files. There are all sorts of different tools that already do that.
 *
 * The INI configuration interface uses COLLECTION (see libcollection
 * interface) to store data internally.
 *
 * Concepts:
 * - INI file consists of the key value pairs.
 * - Keys and values are separated by the equal sign.
 *   Spaces around equal sign are trimmed. Everything before the equal
 *   sign is the key, everything after is the value.
 * - Comments are the lines that start with ";" or "#" in the first
 *   position of the line.
 * - Library now supports multi-line values. Values that span across multiple
 *   lines should start with a single space on every new line.
 * - After being read both keys and values are stored in the internal
 *   objects.
 * - Application configuration can consist from multiple files.
 *   For example, there can be a generic file in /etc containing
 *   configuration for all applications of a particular class running
 *   on a box and then there might be a special file
 *   with parameters specific for each application in the
 *   /etc/whatever.d directory. Interface does not allow reading
 *   multiple files in one call. Instead files need to be read separately
 *   and then merged together. A helper function to do so might be added
 *   later.
 * - If there is no section in the file or there are key value pairs
 *   declared before the first section, those pairs will be placed into
 *   the default section with the name "default".
 * - All values are treated as strings. Spaces are trimmed at the beginning
 *   and the end of the value. The value ends at the end of the line.
 *   If values is too long, an error will be returned.
 * - Parsing of a value happens when a caller tries to interpret
 *   the value. The caller can use different functions to do this.
 *   The value can be treated as numeric, logical, string, binary,
 *   array of strings or array of numbers. In case of arrays parsing functions
 *   accept separators that will be used to slice the value into the array
 *   of elements.
 * - If there is any error parsing section and key values it can be
 *   intercepted by the caller. There are different modes that the library
 *   supports regarding error handling. See details in the description
 *   of individual functions.
 * - The library allows ini files with fragmented sections. This means that
 *   a section can be scattered across the file. Flags control what to
 *   do in such situation.
 * - The library allows storing and retrieving multiple key value pairs with
 *   the same key in one section.
 *
 *   <i>There is a deprecated interface that can be found in ini_config.h.
 *   This interface is supported only for backwards compatibility and should
 *   not be used.</i>
 */

/**
 * @defgroup structures Structures
 *
 * All structures used in the interface should be treated as internal opaque objects.
 *
 * @{
 *
 * @}
 */


/**
 * @defgroup errorlevel Error tolerance constants
 *
 * Constants in this section define what to do if
 * error or warning encountered while parsing the INI file.
 *
 * @{
 */
/** Enumeration of error levels. */
enum ERR_LEVEL {
    INI_STOP_ON_ANY   = 0, /**< Fail if any problem is detected. */
    INI_STOP_ON_NONE  = 1, /**< Best effort - do not fail. */
    INI_STOP_ON_ERROR = 2  /**< Fail on errors only. */
};

/**
 * @}
 */

/**
 * @defgroup parseerr Parsing errors and warnings
 *
 * Parsing errors and warnings
 *
 * @{
 */
/** Enumeration of parsing errors. */
enum ERR_PARSE {
    ERR_LONGDATA = 1, /**< Line is too long (Error). */
    ERR_NOCLOSESEC,   /**< No closing bracket in section
                           definition (Error). */
    ERR_NOSECTION,    /**< Section name is missing (Error). */
    ERR_SECTIONLONG,  /**< Section name too long (Error). */
    ERR_NOEQUAL,      /**< No equal sign (Error). */
    ERR_NOKEY,        /**< No key before equal sign (Error). */
    ERR_LONGKEY,      /**< Key is too long (Error). */
    ERR_READ,         /**< Failed to read line (Error). */
    ERR_SPACE,        /**< Line starts with space when it
                           should not (Error). */
    ERR_DUPKEY,       /**< Duplicate key is not allowed (Error). */
    ERR_DUPKEYSEC,    /**< Duplicate key is detected while merging
                           sections (Error). */
    ERR_DUPSECTION,   /**< Duplicate section is not allowed (Error). */
    ERR_SPECIAL,      /**< Line contains invalid characters (Error). */
    ERR_TAB,          /**< Line starts with tab when it
                           should not (Error). */
    ERR_MAXPARSE = ERR_TAB /**< Special value. Size of the error array. */
};

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

/** @brief Value with same key is overwritten */
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
 * values that come from two different configuration files.
 *
 * @{
 */
/** @brief Value with same key is overwritten */
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
 * sections that come from two different configuration files.
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
/** @brief Merge but log errors if duplicate sections are detected */
#define INI_MS_DETECT    0x0400

/**
 * @}
 */

/**
 * @}
 */

/**
 * @defgroup parseflags Flags that define parsing rules
 *
 * Flags that define how the file should be parsed.
 *
 * @{
 */

/** @brief Do not wrap */
#define INI_PARSE_NOWRAP    0x0001
/** @brief No spaces are allowed to the left of the key */
#define INI_PARSE_NOSPACE   0x0002
/** @brief No tabs are allowed to the left of the key */
#define INI_PARSE_NOTAB     0x0004

/**
 * @}
 */

/**
 * @defgroup searchmode Constants that define how to look for a value
 *
 * Configuration file can allow several keys with the same name
 * in one section. Use the constants below to define which
 * value you are looking for.
 * You can search for the next value only if you are looking
 * for the same section and key as in the previous search. If you
 * specify INI_GET_NEXT_VALUE but the section or key is
 * different from the values that were used in the previous search
 * the value will be ignored and the function will act as if
 * INI_GET_FIRST_VALUE is specified.
 * This functionality allows creating an attribute list and
 * actually fetching every value including duplicate values
 * in a single loop.
 *
 * @{
 */
/** Enumeration of parsing errors. */
enum INI_GET {
    INI_GET_FIRST_VALUE, /**< Get the first value (default). */
    INI_GET_NEXT_VALUE,  /**< Look for the next value in the section */
    INI_GET_LAST_VALUE   /**< Look for the last value in the section */
};

/**
 * @}
 */

/**
 * @brief Name of the default section.
 *
 * This is the name of the implied section where orphan key-value
 * pairs will be put.
 */
#define INI_DEFAULT_SECTION "default"

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


/**
 * @defgroup ini_core Core interface functions
 *
 * Functions in this section allow manipulation with the configuration file,
 * parsing data from the configuration file and storing it in a configuration
 * object, merging configuration objects and other operations.
 *
 * This interface is currently incomplete as it does not allow:
 * - constructing configuration in memory
 * - altering existing configuration by adding, modifying or removing sections
 *   and keys.
 *
 * @{
 *
 */

/**
 * @brief Create a configuration object.
 *
 * Allocates an object that will store configuration data.
 * Configuration object is populated by parsing a file.
 *
 * @param[out] ini_config       Configuration object.
 *
 * @return 0 - Success.
 * @return EINVAL - Invalid parameter.
 * @return ENOMEM - No memory.
 */
int ini_config_create(struct ini_cfgobj **ini_config);

/**
 * @brief Destroy a configuration object.
 *
 * Frees configuration data.
 *
 * @param[in] ini_config       Configuration object.
 *
 */
void ini_config_destroy(struct ini_cfgobj *ini_config);

/**
 * @brief Flush cached search data.
 *
 * Frees cached search data. This will cause
 * any iteration over the same keys to start over.
 *
 * @param[in] ini_config       Configuration object.
 *
 */
void ini_config_clean_state(struct ini_cfgobj *ini_config);

/**
 * @brief Create a configuration file object.
 *
 * Create a file object for parsing a configuration file.
 *
 * A "configuration file object" is different from
 * a "configuration object". The former stores metadata
 * about a file the configuration data is read from,
 * while the latter holds the configuration itself.
 *
 * @param[in]  filename         Name or path to the ini file.
 *                              This argument can contain
 *                              a short or a fully qualified
 *                              file name. If a short name is
 *                              specified the full path
 *                              will be resolved internally.
 * @param[in]  metadata_flags   Flags that specify what additional
 *                              data if any needs to be collected
 *                              about the ini file.
 * @param[out] file_ctx         Configuration file object.
 *
 * @return 0 - Success.
 * @return EINVAL - Invalid parameter.
 * @return ENOMEM - No memory.
 */
int ini_config_file_open(const char *filename,
                         uint32_t metadata_flags,
                         struct ini_cfgfile **file_ctx);

/**
 * @brief Close configuration file after parsing
 *
 * Closes file but keeps the context. File can be reopened
 * and reread using \ref ini_config_file_reopen function.
 *
 * @param[in]  file_ctx         Configuration file object.
 *
 */
void ini_config_file_close(struct ini_cfgfile *file_ctx);


/**
 * @brief Reopen the configuration file
 *
 * Creates a new file object from the original one.
 * The file configuration objects then can be compared
 * to determine whether the file actually changed.
 *
 * @param[in]  file_ctx_in      Original configuration file object.
 * @param[out] file_ctx_out     A new configuration file object.
 *
 * @return 0 - Success.
 * @return EINVAL - Invalid parameter.
 * @return ENOMEM - No memory.
 */
int ini_config_file_reopen(struct ini_cfgfile *file_ctx_in,
                           struct ini_cfgfile **file_ctx_out);


/**
 * @brief Close configuration file and free all data
 *
 * Closes file and frees the context.
 *
 * @param[in]  file_ctx         Configuration file object.
 *
 */
void ini_config_file_destroy(struct ini_cfgfile *file_ctx);

/**
 * @brief Check parsing errors count
 *
 * Query the configuration file object about
 * how many parsing errors were found during last
 * parsing operation.
 *
 * @param[in]  ini_config       Configuration object.
 *
 * @return Number of errors.
 */
unsigned ini_config_error_count(struct ini_cfgobj *ini_config);

/**
 * @brief Get array of parsing errors
 *
 * Function returns a newly allocated array of strings
 * that should be later freed by the \ref ini_config_free_errors
 * function.
 * Array can be referenced as a normal array of strings.
 * The NULL entry indicates the end of the array.
 *
 * @param[in]  ini_config       Configuration object.
 * @param[out] errors           Array of error strings.
 *
 * @return 0 - Success.
 * @return EINVAL - Invalid parameter.
 * @return ENOMEM - No memory.
 */
int ini_config_get_errors(struct ini_cfgobj *ini_config,
                          char ***errors);

/**
 * @brief Free array of parsing errors
 *
 * Free array of parsing errors previously allocated
 * by using \ref ini_config_get_errors function.
 *
 * @param[in]  errors           Array of error strings.
 *
 */
void ini_config_free_errors(char **errors);

/**
 * @brief Print errors to a file
 *
 * Prints array of parsing errors previously allocated
 * by using \ref ini_config_get_errors function into
 * a provided file.
 *
 * @param[in]  file             File or stream to send errors to.
 * @param[in]  error_list       Array of error strings.
 *
 */
void ini_config_print_errors(FILE *file, char **error_list);

/**
 * @brief Get the fully resolved file name
 *
 * Returns the full name to the configuration file
 * that was resolved by the library.
 *
 * @param[in]  file_ctx         Configuration file object.
 *
 * @return Full file name.
 */
const char *ini_config_get_filename(struct ini_cfgfile *file_ctx);


/**
 * @brief Print file context
 *
 * Function is useful for debugging purposes only.
 *
 * @param[in]  file_ctx         Configuration file object.
 *
 */
void ini_config_file_print(struct ini_cfgfile *file_ctx);

/**
 * @brief Check file properties
 *
 * Before parsing it makes sense to make sure
 * that the file you are trying to read is properly
 * owned and has proper permissions.
 *
 * @param[in]  file_ctx         Configuration file object.
 * @param[in]  flags            Define what to check.
 *                              One can check file
 *                              permissions with mask,
 *                              uid, and gid of the file.
 * @param[in]  uid              Expected uid of the file.
 * @param[in]  gid              Expected gid of the file.
 * @param[in]  mode             Expected mode of the file.
 * @param[in]  mask             Mask to use in the mode check.
 *                              Mask is always adjusted to
 *                              include at least S_IRWXU,
 *                              S_IRWXG and S_IRWXO
 *
 * @return 0 - Success.
 * @return EINVAL - Invalid parameter.
 * @return EACCES - File attributes do no match expectations.
 */
int ini_config_access_check(struct ini_cfgfile *file_ctx,
                            uint32_t flags,
                            uid_t uid,
                            gid_t gid,
                            mode_t mode,
                            mode_t mask);

/**
 * @brief Check if file has changed
 *
 * Compares two configuration file objects.
 * Determines if two objects are different
 * by comparing:
 * - time stamp
 * - device ID
 * - i-node
 *
 * Function can be used to check if the file
 * has changed since last time the it was read.
 *
 * <i> Note:</i> If the file was deleted and quickly
 * re-created the kernel seems to restore the same i-node.
 * The stat structure keeps time granularity of seconds.
 * As a result if the file is quickly recreated
 * with the same contents like in the unit test the check
 * would assume that file did not change.
 * This is why the unit test has a one second delay.
 *
 * @param[in]  file_ctx1        First configuration file object.
 * @param[in]  file_ctx2        Second configuration file object.
 * @param[out] changed          A value will be set to 0 if
 *                              the objects are same and to 1
 *                              if they are different.
 *
 * @return 0 - Success.
 * @return EINVAL - Invalid parameter.
 * @return EACCES - File attributes do no match expectations.
 */
int ini_config_changed(struct ini_cfgfile *file_ctx1,
                       struct ini_cfgfile *file_ctx2,
                       int *changed);

/**
 * @brief Parse the file and populate a configuration object
 *
 * Function parses the file. It is assumed that
 * the configuration object was just created.
 * Using a non empty configuration object in
 * a parsing operation would fail with EINVAL.
 *
 * @param[in]  file_ctx         Configuration file object.
 * @param[in]  error_level      Flags that control actions
 *                              in case of parsing error.
 * @param[in]  collision_flags  Flags that control handling
 *                              of the duplicate sections or keys.
 * @param[out] ini_config       Configuration object.
 *
 * @return 0 - Success.
 * @return EINVAL - Invalid parameter.
 * @return ENOMEM - No memory.
 */
int ini_config_parse(struct ini_cfgfile *file_ctx,
                     int error_level,
                     uint32_t collision_flags,
                     uint32_t parse_flags,
                     struct ini_cfgobj *ini_config);

/**
 * @brief Create a copy of the configuration object
 *
 * Function creates a deep copy of all the configuration data.
 * Error list created during parsing is not copied over.
 *
 * @param[in]  ini_config       Original configuration object.
 * @param[out] ini_new          A new configuration object.
 *
 * @return 0 - Success.
 * @return EINVAL - Invalid parameter.
 * @return ENOMEM - No memory.
 */
int ini_config_copy(struct ini_cfgobj *ini_config,
                    struct ini_cfgobj **ini_new);

/**
 * @brief Merge two configuration objects
 *
 * Function merges configuration objects and creates
 * a new resulting object out of the two.
 * IMPORTANT: Use same collision flags for reading
 * of the files and then merging.
 * Mixing the flags would lead to strange results
 * that would be hard to debug.
 *
 *
 * @param[in]  first            A base object
 *                              the other object will
 *                              be merged with.
 * @param[in]  second           The object that will
 *                              be merged to the first one.
 * @param[in]  collision_flags  Flags that control handling
 *                              of the duplicate sections or keys.
 * @param[out] result           A new configuration object,
 *                              the result of the merge.
 *
 * @return 0 - Success.
 * @return EINVAL - Invalid parameter.
 * @return ENOMEM - No memory.
 */
int ini_config_merge(struct ini_cfgobj *first,
                     struct ini_cfgobj *second,
                     uint32_t collision_flags,
                     struct ini_cfgobj **result);

/**
 * @brief Set the folding boundary
 *
 * Set the folding boundary for multiline values.
 * Use before serializing and saving to a file if the
 * default boundary of 80 characters does not work for you.
 *
 * @param[in]  ini_config       Configuration object.
 * @param[in]  boundary         Wrapping boundary.
 *
 * @return 0 - Success.
 * @return EINVAL - Invalid parameter.
 */
int ini_config_set_wrap(struct ini_cfgobj *ini_config,
                        uint32_t boundary);

/**
 * @brief Serialize configuration object
 *
 * Serialize configuration object into provided buffer.
 * Use buffer object functions to manipulate or save
 * the buffer to a file/stream.
 *
 * @param[in]  ini_config       Configuration object.
 * @param[out] sbobj            Serialized configuration.
 *
 * @return 0 - Success.
 * @return EINVAL - Invalid parameter.
 * @return ENOMEM - No memory.
 */
int ini_config_serialize(struct ini_cfgobj *ini_config,
                         struct simplebuffer *sbobj);


/* TODO: Functions that add, modify or delete sections and values in
 * the configuration object.
 */

/**
 * @}
 */


/**
 * @defgroup ini_section_and_attr Section and attribute management
 *
 * Functions in this section allow getting the lists of
 * sections in the configuration file and keys in a section
 * as arrays of strings.
 *
 * @{
 *
 */

/**
 * @brief Get list of sections.
 *
 * Get list of sections from the configuration object
 * as an array of strings.
 * Function allocates memory for the array of the sections.
 * Use \ref ini_free_section_list() to free allocated memory.
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
char **ini_get_section_list(struct ini_cfgobj *ini_config,
                            int *size,
                            int *error);

/**
 * @brief Free list of sections.
 *
 * The section array created by \ref ini_get_section_list()
 * should be freed using this function.
 *
 * @param[in] section_list       Array of strings returned by
 *                               \ref ini_get_section_list() function.
 */
void ini_free_section_list(char **section_list);

/**
 * @brief Get list of attributes.
 *
 * Get list of attributes in a section as an array of strings.
 * Function allocates memory for the array of attributes.
 * Use \ref ini_free_attribute_list() to free allocated memory.
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
char **ini_get_attribute_list(struct ini_cfgobj *ini_config,
                              const char *section,
                              int *size,
                              int *error);

/**
 * @brief Free list of attributes.
 *
 * The attribute array created by \ref ini_get_attribute_list()
 * should be freed using this function.
 *
 * @param[in] attr_list          Array of strings returned by
 *                               \ref ini_get_attribute_list() function.
 */
void ini_free_attribute_list(char **attr_list);

/**
 * @}
 */

/**
 * @defgroup ini_value Value management
 *
 * This section contains value management functions. These functions
 * can be used to interpret values that are stored in the configuration
 * object in memory.
 *
 * @{
 *
 */


/**
 * @brief Retrieve a value object form the configuration.
 *
 * Check return error code first. If the function returns
 * an error there is a serious problem.
 * Then check if object is found. Function will set
 * vo parameter to NULL if no attribute with
 * provided name is found in the collection.
 *
 * @param[in]  section          Section name.
 *                              If NULL assumed default.
 * @param[in]  name             Attribute name to find.
 * @param[in]  ini_config       Configuration object to search.
 * @param[in]  mode             See \ref searchmode "search mode"
 *                              section for more info.
 * @param[out] vo               Value object.
 *                              Will be set to NULL if
 *                              element with the given name
 *                              is not found.
 * @return 0 - Success.
 * @return EINVAL - Invalid parameter.
 * @return ENOMEM - No memory.
 *
 */

int ini_get_config_valueobj(const char *section,
                            const char *name,
                            struct ini_cfgobj *ini_config,
                            int mode,
                            struct value_obj **vo);



/**
 * @brief Convert value to integer number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration element
 * into an int number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 * The value range is from INT_MIN to INT_MAX.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
int ini_get_int_config_value(struct value_obj *vo,
                             int strict,
                             int def,
                             int *error);

/**
 * @brief Convert value object to a unsigned number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration element
 * into an unsigned number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 * The value range is from 0 to UINT_MAX.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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

unsigned ini_get_unsigned_config_value(struct value_obj *vo,
                                       int strict,
                                       unsigned def,
                                       int *error);

/**
 * @brief Convert value to long number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration element
 * into a long number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 * The value range is from LONG_MIN to LONG_MAX.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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

long ini_get_long_config_value(struct value_obj *vo,
                               int strict,
                               long def,
                               int *error);

/**
 * @brief Convert value to unsigned long number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration element
 * into an unsigned long number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 * The value range is from 0 to ULONG_MAX.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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

unsigned long ini_get_ulong_config_value(struct value_obj *vo,
                                         int strict,
                                         unsigned long def,
                                         int *error);


/**
 * @brief Convert value to int32_t number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration element
 * into an int32_t number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 * The value range is from INT_MIN to INT_MAX.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
int32_t ini_get_int32_config_value(struct value_obj *vo,
                                   int strict,
                                   int32_t def,
                                   int *error);

/**
 * @brief Convert value to uint32_t number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration value
 * into an uint32_t number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 * The value range is from 0 to ULONG_MAX.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
uint32_t ini_get_uint32_config_value(struct value_obj *vo,
                                     int strict,
                                     uint32_t def,
                                     int *error);

/**
 * @brief Convert value to integer number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration element
 * into an int64_t number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 * The value range is from LLONG_MIN to LLONG_MAX.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
int64_t ini_get_int64_config_value(struct value_obj *vo,
                                   int strict,
                                   int64_t def,
                                   int *error);

/**
 * @brief Convert value to integer number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration element
 * into an uint64_t number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 * The value range is from 0 to ULLONG_MAX.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
uint64_t ini_get_uint64_config_value(struct value_obj *vo,
                                     int strict,
                                     uint64_t def,
                                     int *error);

/**
 * @brief Convert value to floating point number.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration element
 * into a floating point number. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
 * how the caller tries to interpret the value.
 * If "strict" parameter is non zero the function will fail
 * if there are more characters after the last digit.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
double ini_get_double_config_value(struct value_obj *vo,
                                   int strict,
                                   double def,
                                   int *error);

/**
 * @brief Convert value into a logical value.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration element
 * into a Boolean. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
 * how the caller tries to interpret the value.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
unsigned char ini_get_bool_config_value(struct value_obj *vo,
                                        unsigned char def,
                                        int *error);

/**
 * @brief Get the copy of string stored in the configuration value
 *
 * Function creates a copy of the string value stored in
 * the configuration element.
 * Returned value needs to be freed after use.
 * If error occurred the returned value will be NULL.
 *
 * @param[in]  vo               Value object to use.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
char *ini_get_string_config_value(struct value_obj *vo,
                                  int *error);
/**
 * @brief Get the string stored in the configuration value
 *
 * Function returns a reference to the string value
 * stored inside the configuration element. This string can't be altered.
 * The string will go out of scope if the value object is deleted.
 *
 * @param[in]  vo               Value object to use.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
 * @return String from the value object.
 */
const char *ini_get_const_string_config_value(struct value_obj *vo,
                                              int *error);

/**
 * @brief Convert value into a binary sequence.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration element
 * into a sequence of bytes.
 * Any of the conversion functions
 * can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
 * how the caller tries to interpret the value.
 *
 * The function allocates memory.
 * It is the responsibility of the caller to free it after use.
 * Use \ref ini_free_bin_config_value() for this purpose.
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
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
char *ini_get_bin_config_value(struct value_obj *vo,
                               int *length,
                               int *error);

/**
 * @brief Free binary buffer
 *
 * Free binary value returned by \ref ini_get_bin_config_value().
 *
 * @param[in] bin              Binary buffer to free.
 *
 */
void ini_free_bin_config_value(char *bin);

/**
 * @brief Convert value to an array of strings.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration value object
 * into an array of strings. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
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
 * Use \ref ini_free_string_config_array() to free the array after use.
 *
 * The array is always NULL terminated so
 * it is safe not to get size and just loop until
 * array element is NULL.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
char **ini_get_string_config_array(struct value_obj *vo,
                                   const char *sep,
                                   int *size,
                                   int *error);

/**
 * @brief Convert value to an array of strings.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration element
 * into an array of strings. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
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
 * Use \ref ini_free_string_config_array() to free the array after use.
 *
 * The array is always NULL terminated so
 * it is safe not to get size and just loop until
 * array element is NULL.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
char **ini_get_raw_string_config_array(struct value_obj *vo,
                                       const char *sep,
                                       int *size,
                                       int *error);

/**
 * @brief Convert value to an array of long values.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration element
 * into an array of long values. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
 * how the caller tries to interpret the value.
 *
 * Separators inside the string are detected automatically.
 * The spaces are trimmed automatically around separators
 * in the string.
 *
 * The length of the allocated array is returned in "size".
 * Size parameter can't be NULL.
 *
 * Use \ref ini_free_long_config_array() to free the array after use.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
long *ini_get_long_config_array(struct value_obj *vo,
                                int *size,
                                int *error);

/**
 * @brief Convert value to an array of floating point values.
 *
 * This is a conversion function.
 * It converts the value read from the INI file
 * and stored in the configuration element
 * into an array of floating point values. Any of the conversion
 * functions can be used to try to convert the value
 * stored as a string inside the value object.
 * The result can be different depending upon
 * how the caller tries to interpret the value.
 *
 * Separators inside the string are detected automatically.
 * The spaces are trimmed automatically around separators
 * in the string.
 *
 * The length of the allocated array is returned in "size".
 * Size parameter can't be NULL.
 *
 * Use \ref ini_free_double_config_array() to free the array after use.
 *
 * @param[in]  vo               Value object to interpret.
 *                              It must be retrieved using
 *                              \ref ini_get_config_valueobj().
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
double *ini_get_double_config_array(struct value_obj *vo,
                                    int *size,
                                    int *error);

/**
 * @brief Free array of string values.
 *
 * Use this function to free the array returned by
 * \ref ini_get_string_config_array() or by
 * \ref ini_get_raw_string_config_array().
 *
 * @param[in] str_config        Array of string values.
 */
void ini_free_string_config_array(char **str_config);

/**
 * @brief Free array of long values.
 *
 * Use this function to free the array returned by
 * \ref ini_get_long_config_array().
 *
 * @param[in] array         Array of long values.
 */
void ini_free_long_config_array(long *array);
/**
 * @brief Free array of floating pointer values.
 *
 * Use this function to free the array returned by
 * \ref ini_get_double_config_array().
 *
 * @param[in] array         Array of floating pointer values.
 */
void ini_free_double_config_array(double *array);

/**
 * @}
 */

#endif
