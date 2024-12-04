include $(top_srcdir)/Make_global.am

sbin_PROGRAMS = nftlb

nftlb_SOURCES = config.c	\
		main.c		\
		objects.c	\
		farms.c		\
		backends.c	\
		nft.c		\
		events.c	\
		network.c	\
		server.c	\
		policies.c	\
		elements.c	\
		farmpolicy.c \
		sessions.c	\
		checksum.c	\
		addresses.c \
		farmaddress.c \
		addresspolicy.c \
		nftst.c \
		../utils/src/u_backtrace.c \
		../utils/src/u_log.c \
		../utils/src/u_network.c \
		../utils/src/u_sbuffer.c \
		../utils/src/u_http.c \
		../utils/src/u_string.c
nftlb_LDADD = ${LIBNFTABLES_LIBS} ${LIBJSON_LIBS} ${LIBMNL_LIBS} -lev
