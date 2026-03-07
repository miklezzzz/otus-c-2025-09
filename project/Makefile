MODULES = auth_k8s
PG_CONFIG = /usr/lib/postgresql/18/bin/pg_config

override CFLAGS += -Wall -Wextra -Wpedantic -std=gnu11 \
                   -Wno-unused-parameter -Wno-declaration-after-statement \
                   -fno-stack-protector -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=0 -fPIC

override CPPFLAGS += -D_GNU_SOURCE -include $(CURDIR)/glibc_compat.h \
              -isystem /usr/local/include/kubernetes/src/generic \
              -isystem /usr/local/include/kubernetes/include \
              -isystem /usr/local/include/kubernetes/api \
              -isystem /usr/local/include/kubernetes/config

PG_LDFLAGS = -Wl,--whole-archive \
             /usr/local/lib/libkubernetes.a \
             /usr/local/lib/libyaml.a \
             /usr/local/lib/libwebsockets.a \
             /usr/local/lib/libcurl.a \
             -Wl,--no-whole-archive \
             /usr/local/lib64/libssl.a \
             /usr/local/lib64/libcrypto.a

SHLIB_LINK = -lpthread -lm -lz -ldl

PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
