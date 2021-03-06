/**
 * This file is part of DBrew, the dynamic binary rewriting library.
 *
 * (c) 2015-2016, Josef Weidendorfer <josef.weidendorfer@gmx.de>
 *
 * DBrew is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License (LGPL)
 * as published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * DBrew is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with DBrew.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "common.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static
void cc_init(CaptureConfig* cc)
{
    for(int i=0; i < CC_MAXPARAM; i++)
        initMetaState(&(cc->par_state[i]), CS_DYNAMIC);
    for(int i=0; i < CC_MAXPARAM; i++)
        cc->par_name[i] = 0;
    for(int i=0; i < CC_MAXCALLDEPTH; i++)
        cc->force_unknown[i] = false;
    cc->hasReturnFP = false;
    cc->parCount = -1; // unknown
    cc->branches_known = false;
    cc->function_configs = 0;

}

static
void cc_free(CaptureConfig* cc)
{
    if (!cc) return;

    for(int i=0; i < CC_MAXPARAM; i++)
        free(cc->par_name[i]);

    FunctionConfig* fc = cc->function_configs;
    while(fc) {
        FunctionConfig* next = fc->next;
        free(fc);
        fc = next;
    }
    free(cc);
}

static
CaptureConfig* cc_new(void)
{
    CaptureConfig* cc;

    cc = (CaptureConfig*) malloc(sizeof(CaptureConfig));
    cc_init(cc);

    return cc;
}

// allocate a configuration for the rewriter if not already existing
static
CaptureConfig* cc_get(Rewriter* r)
{
    if (r->cc == 0)
        r->cc = cc_new();

    return r->cc;
}

static
FunctionConfig* fc_new(uint64_t func, char* name, int size,
                       FunctionConfig* next)
{
    FunctionConfig* fc;

    fc = (FunctionConfig*) malloc(sizeof(FunctionConfig));
    fc->name = (name == 0) ? 0 : strdup(name);
    fc->func = func;
    fc->size = size;
    fc->next = next;

    return fc;
}

static
FunctionConfig* fc_find(CaptureConfig* cc, uint64_t func)
{
    FunctionConfig* fc = cc->function_configs;
    while(fc) {
        // on exact match, size does not matter
        if (func == fc->func) return fc;
        // check if we fall into address range
        if ((func > fc->func) && (func < fc->func+fc->size)) return fc;
        fc = fc->next;
    }
    return 0;
}

static
FunctionConfig* fc_get(CaptureConfig* cc, uint64_t func)
{
    FunctionConfig* fc;

    fc = fc_find(cc, func);
    if (!fc) {
        fc = fc_new(func, 0, 0, cc->function_configs);
        cc->function_configs = fc;
    }

    return fc;
}

// DBrew internal, called by other modules

FunctionConfig* config_find_function(Rewriter* r, uint64_t f)
{
    CaptureConfig* cc = cc_get(r);
    return fc_find(cc, f);
}


//---------------------------------------------------------------------
// DBrew API functions for configuration

void dbrew_config_reset(Rewriter* r)
{
    if (r->cc)
        cc_free(r->cc);

    r->cc = cc_new();
}

void dbrew_config_staticpar(Rewriter* r, int staticParPos)
{
    CaptureConfig* cc = cc_get(r);

    assert((staticParPos >= 0) && (staticParPos < CC_MAXPARAM));
    initMetaState(&(cc->par_state[staticParPos]), CS_STATIC2);
}

void dbrew_config_par_setname(Rewriter* c, int par, char* name)
{
    CaptureConfig* cc = cc_get(c);

    assert((par >= 0) && (par < CC_MAXPARAM));
    cc->par_name[par] = strdup(name);
}

/**
 * This allows to specify for a given function inlining depth that
 * values produced by binary operations always should be forced to unknown.
 * Thus, when result is known, it is converted to unknown state with
 * the value being loaded as immediate into destination.
 *
 * Brute force approach to prohibit loop unrolling.
 */
void dbrew_config_force_unknown(Rewriter* r, int depth)
{
    CaptureConfig* cc = cc_get(r);

    assert((depth >= 0) && (depth < CC_MAXCALLDEPTH));
    cc->force_unknown[depth] = true;
}

void dbrew_config_returnfp(Rewriter* r)
{
    CaptureConfig* cc = cc_get(r);
    cc->hasReturnFP = true;
}

void dbrew_config_parcount(Rewriter* r, int parCount)
{
    CaptureConfig* cc = cc_get(r);
    cc->parCount = parCount;
}

void dbrew_config_branches_known(Rewriter* r, bool b)
{
    CaptureConfig* cc = cc_get(r);
    cc->branches_known = b;
}

void dbrew_config_function_setname(Rewriter* r, uint64_t f, const char* name)
{
    CaptureConfig* cc = cc_get(r);
    FunctionConfig* fc = fc_get(cc, f);
    fc->name = strdup(name);
}

void dbrew_config_function_setsize(Rewriter* r, uint64_t f, int size)
{
    CaptureConfig* cc = cc_get(r);
    FunctionConfig* fc = fc_get(cc, f);
    fc->size = size;
}
