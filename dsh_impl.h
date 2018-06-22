/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

#ifndef _SQL_DSH_IMPL_H_
#define _SQL_DSH_IMPL_H_
#include "sql.h"

#ifdef WIN32
#ifndef LIBDSH
#define dsh_export extern __declspec(dllimport)
#else
#define dsh_export extern __declspec(dllexport)
#endif
#else
#define dsh_export extern
#endif

/* avg window function */
#define DECL_AVG_CACHE(TYPE) \
    struct avg_cache_ ## TYPE { \
        TYPE sum; \
        lng count; \
    };
DECL_AVG_CACHE(bte)
DECL_AVG_CACHE(sht)
DECL_AVG_CACHE(int)
DECL_AVG_CACHE(lng)
DECL_AVG_CACHE(dbl)
DECL_AVG_CACHE(flt)
DECL_AVG_CACHE(hge)
#undef DECL_AVG_CACHE

dsh_export str DSHwin_fun_sum_bte(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_sum_sht(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_sum_int(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_sum_lng(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_sum_hge(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_sum_flt(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_sum_dbl(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);

dsh_export str DSHwin_fun_min_bit(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_min_bte(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_min_sht(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_min_int(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_min_lng(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_min_hge(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_min_flt(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_min_dbl(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);

dsh_export str DSHwin_fun_max_bit(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_max_bte(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_max_sht(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_max_int(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_max_lng(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_max_hge(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_max_flt(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_max_dbl(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);

dsh_export str DSHwin_fun_avg_bte(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_avg_sht(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_avg_int(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_avg_lng(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_avg_hge(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_avg_flt(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
dsh_export str DSHwin_fun_avg_dbl(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);

dsh_export str DSHwin_fun_first_value_any(BAT * o, const BAT * d, const BAT * i, const lng cnt, lng w);
#endif

