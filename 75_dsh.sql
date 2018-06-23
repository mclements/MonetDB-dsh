-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0.  If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.
--
-- Copyright 1997 - July 2008 CWI, August 2008 - 2017 MonetDB B.V.

-- (co) ???
-- 

create schema dsh;

-- diff
create function dsh.diff(x double)
returns bool
external name sql."diff";

-- oid_segment_append
create function dsh.oid_segment_append(l oid, r oid)
returns table (side oid, lcand oid, rcand oid)
external name dsh."oid_segment_append";

-- oid_segment_append2
create function dsh.oid_segment_append2(l oid, r oid)
returns table (postproj oid, lcand oid, rcand oid)
external name dsh."oid_segment_append2";

-- bitmerge - left and right are polymorphic
create function dsh.bitmerge(side bool, l double, r double)
returns double
external name dsh."bitmerge";
-- TODO: other cases

-- oid_segment_zip
create function dsh.oid_segment_zip(leftoid oid, rightoid oid, rvalue double)
returns table(lcand oid, rcand oid)
external name dsh."oid_segment_zip";

-- reverse - i is polymorphic
create function dsh.reverse(i double)
returns oid
external name dsh."reverse";
-- TODO: other cases

-- oid_segment_reverse
create function dsh.oid_segment_reverse(i oid)
returns oid
external name dsh."oid_segment_reverse";

-- win_fun_sum - polymorphic for i
create function dsh.win_fun_sum(d bool, i double, w integer)
returns double
external name dsh."win_fun_sum";
-- TODO: other cases

-- win_fun_min - polymorphic for i
create function dsh.win_fun_min(d bool, i double, w integer)
returns double
external name dsh."win_fun_min";
-- TODO: other cases

-- win_fun_max - polymorphic for i
create function dsh.win_fun_max(d bool, i double, w integer)
returns double
external name dsh."win_fun_max";
-- TODO: other cases

-- win_fun_avg - polymorphic for i
create function dsh.win_fun_avg(d bool, i double, w integer)
returns double
external name dsh."win_fun_avg";
-- TODO: other cases

-- win_fun_count
create function dsh.win_fun_count(d bool, w integer)
returns integer
external name dsh."win_fun_count";

-- win_fun_first_value - polymorphic for i
create function dsh.win_fun_first_value(d bool, i double, w integer)
returns double
external name dsh."win_fun_first_value";
-- TODO: other cases
