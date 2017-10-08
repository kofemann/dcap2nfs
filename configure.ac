
AC_INIT([dcap2nfs], [0.1.0], [support@dcache.org])
AM_INIT_AUTOMAKE

AC_PROG_CC
CPPFLAGS=-I/usr/include/tirpc

AC_SEARCH_LIBS([pthread_join], [pthread], [PTHREADS="-lpthread"],[
AC_MSG_ERROR([pthread was not found])
])

AC_SEARCH_LIBS([clnt_tli_create], [tirpc], [TIRPC="-ltirpc"],[
AC_MSG_ERROR([clnt_tli_create was not found])
])

AC_CHECK_HEADERS([rpc/clnt.h], [], [
AC_MSG_ERROR([missing rpc/clnt.h])
], [])

AC_CHECK_HEADERS([rpc/clnt.h], [], [
AC_MSG_ERROR([missing rpc/clnt.h])
], [])

AC_CHECK_HEADERS([netconfig.h], [], [
AC_MSG_ERROR([missing netconfig.h])
], [])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile])

AC_OUTPUT
