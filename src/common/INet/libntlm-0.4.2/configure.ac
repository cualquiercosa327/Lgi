dnl Process this file with autoconf to produce a configure script.

dnl Copyright (C) 2002, 2004, 2005, 2006, 2007, 2008 Simon Josefsson.
dnl
dnl This file is part of Libntlm.
dnl
dnl Libntlm is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU Lesser General Public License as
dnl published by the Free Software Foundation; either version 2.1 of
dnl the License, or (at your option) any later version.
dnl
dnl Libntlm is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU Lesser General Public License for more details.
dnl
dnl You should have received a copy of the GNU Lesser General Public
dnl License along with Libntlm; if not, write to the Free Software
dnl Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
dnl 02110-1301, USA

AC_PREREQ(2.61)
AC_INIT(libntlm, 0.4.2, libntlm@josefsson.org)
AC_CONFIG_AUX_DIR([build-aux])

AC_CONFIG_SRCDIR(ntlm.h.in)

# Interfaces removed:    CURRENT++, AGE=0, REVISION=0
# Interfaces added:      CURRENT++, AGE++, REVISION=0
# No interfaces changed:                   REVISION++
AC_SUBST(LT_CURRENT, 0)
AC_SUBST(LT_AGE, 0)
AC_SUBST(LT_REVISION, 14)

AM_INIT_AUTOMAKE([1.10 gnits])
AM_CONFIG_HEADER(config.h)

# Checks for programs.
AC_PROG_CC
AB_INIT
gl_EARLY
AC_LIBTOOL_WIN32_DLL
AC_PROG_LIBTOOL

# For gnulib.
gl_INIT
AC_DEFINE(check_version, ntlm_check_version, [Rename to public API name.])

# Checks for machine architecture characteristics.
AC_C_BIGENDIAN

# Run self-tests under valgrind?
if test "$cross_compiling" = no; then
  AC_CHECK_PROGS(VALGRIND, valgrind)
fi
if test -n "$VALGRIND" && $VALGRIND true > /dev/null 2>&1; then
  opt_valgrind_tests=yes
else
  opt_valgrind_tests=no
  VALGRIND=
fi 
AC_MSG_CHECKING([whether self tests are run under valgrind])
AC_ARG_ENABLE(valgrind-tests,
	AS_HELP_STRING([--enable-valgrind-tests],
                       [run self tests under valgrind]),
  opt_valgrind_tests=$enableval)
AC_MSG_RESULT($opt_valgrind_tests)

AC_CONFIG_FILES([Makefile lib/Makefile tests/Makefile libntlm.pc \
                 libntlm.spec ntlm.h])
AC_OUTPUT
