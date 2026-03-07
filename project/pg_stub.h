// this one is required to silence some warning in psql code base
#ifndef PG_STUB_H
#define PG_STUB_H

#pragma GCC system_header

#include "postgres.h"
#include "libpq/auth.h"
#include "fmgr.h"
#include "utils/elog.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

#endif
