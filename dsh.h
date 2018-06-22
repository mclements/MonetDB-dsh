/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2016 MonetDB B.V.
 */

/* In your own module, replace "UDF" & "udf" by your module's name */

#ifndef _SQL_DSH_H_
#define _SQL_DSH_H_
#include "sql.h"
#include <string.h>

/* This is required as-is (except from renaming "UDF" & "udf" as suggested
 * above) for all modules for correctly exporting function on Unix-like and
 * Windows systems. */

#ifdef WIN32
#ifndef LIBDSH
#define dsh_export extern __declspec(dllimport)
#else
#define dsh_export extern __declspec(dllexport)
#endif
#else
#define dsh_export extern
#endif

/* implementations for special physical-order-based functions */
dsh_export char * DSHoid_segment_append(bat *sideid, bat *lcandid, bat *rcandid, const bat * lid, const bat * rid);
dsh_export char * DSHoid_segment_append2(bat *postproj, bat *lcandid, bat *rcandid, const bat * lid, const bat * rid);
dsh_export char * DSHbitmerge(bat *res, const bat *side, const bat *left, const bat *right);

dsh_export char * DSHoid_segment_zip(bat *lcand, bat *rcand, const bat *left, const bat *right);

dsh_export char * DSHreverse(bat *out, const bat *in);
dsh_export char * DSHoid_segment_reverse(bat *out, const bat *in);

/* window functions */
dsh_export char * DSHwin_fun_sum(bat *o, const bat *d, const bat *i, const lng * w);
dsh_export char * DSHwin_fun_min(bat *o, const bat *d, const bat *i, const lng * w);
dsh_export char * DSHwin_fun_max(bat *o, const bat *d, const bat *i, const lng * w);
dsh_export char * DSHwin_fun_avg(bat *o, const bat *d, const bat *i, const lng * w);
dsh_export char * DSHwin_fun_first_value(bat *o, const bat *d, const bat *i, const lng * w);

#endif /* _SQL_DSH_H_ */
