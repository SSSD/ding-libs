m4_include([version.m4])
AC_INIT([ding-libs],
        VERSION_NUMBER,
        [sssd-devel@lists.fedorahosted.org])
AC_CONFIG_SRCDIR([README])
AC_CONFIG_AUX_DIR([build])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CC
AC_PROG_LIBTOOL
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_INSTALL

AM_CONDITIONAL([HAVE_GCC], [test "$ac_cv_prog_gcc" = yes])

PKG_CHECK_MODULES([CHECK], [check >= 0.9.5], [have_check=1], [have_check=])
if test x$have_check = x; then
    AC_MSG_WARN([Without the 'CHECK' libraries, you will be unable to run all tests in the 'make check' suite])
fi
AM_CONDITIONAL([HAVE_CHECK], [test x$have_check != x])

m4_pattern_allow([AM_SILENT_RULES])
AM_SILENT_RULES

AC_CONFIG_HEADERS([config.h])

AC_SUBST([PRERELEASE_VERSION],
         PRERELEASE_VERSION_NUMBER)

AC_SUBST([PATH_UTILS_VERSION], PATH_UTILS_VERSION_NUMBER)
AC_SUBST([DHASH_VERSION], DHASH_VERSION_NUMBER)
AC_SUBST([COLLECTION_VERSION], COLLECTION_VERSION_NUMBER)
AC_SUBST([REF_ARRAY_VERSION], REF_ARRAY_VERSION_NUMBER)
AC_SUBST([BASICOBJECTS_VERSION], BASICOBJECTS_VERSION_NUMBER)
AC_SUBST([INI_CONFIG_VERSION], INI_CONFIG_VERSION_NUMBER)

AM_CONDITIONAL([GIT_CHECKOUT], [git log -1 &>/dev/null])

AC_PATH_PROG([DOXYGEN], [doxygen], [false])
AM_CONDITIONAL([HAVE_DOXYGEN], [test x$DOXYGEN != xfalse ])

# Enable trace build
AC_ARG_ENABLE([trace],
              [AS_HELP_STRING([--enable-trace[=LEVEL]],[build with low level tracing enabled])],
              [trace_level="$enableval"],
              [trace_level="0"])
AS_IF([test ["$trace_level" -gt "0"] -a ["$trace_level" -lt "8"] ],[AC_SUBST([TRACE_VAR],["-DTRACE_LEVEL=$trace_level"])])

AC_CHECK_SIZEOF([long])
AC_CHECK_SIZEOF([long long])

AC_CHECK_FUNC([strcasestr],
              AC_DEFINE([HAVE_STRCASESTR],
                        [1],
                        [Define if strcasestr exists]),
              AC_MSG_ERROR("Platform must support strcasestr"))

AC_CHECK_FUNC([getline],
              AC_DEFINE([HAVE_GETLINE],
                        [1],
                        [Define if getline() exists]),
              AC_MSG_ERROR("Platform must support getline()"))

AC_DEFINE([COL_MAX_DATA], [65535], [Max length of the data block allowed in the collection value.])

AC_DEFINE([MAX_KEY], [1024], [Max length of the key in the INI file.])

#Support old versions of autotools that don't provide docdir
AC_SUBST([docdir])
if test x$docdir = x; then
    AC_SUBST([docdir], ${datadir}/doc/AC_PACKAGE_NAME)
fi

AC_CONFIG_FILES([Makefile contrib/ding-libs.spec
                 path_utils/path_utils.cfg.doxy path_utils/path_utils.pc
                 dhash/dhash.pc
                 collection/collection.cfg.doxy collection/collection.pc
                 refarray/ref_array.cfg.doxy refarray/ref_array.pc
                 basicobjects/basicobjects.cfg.doxy
                 basicobjects/basicobjects.pc
                 ini/ini_config.cfg.doxy ini/ini_config.pc])
AC_OUTPUT
