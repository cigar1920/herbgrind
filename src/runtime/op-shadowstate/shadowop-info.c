/*--------------------------------------------------------------------*/
/*--- Herbgrind: a valgrind tool for Herbie             shadowop.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of Herbgrind, a valgrind tool for diagnosing
   floating point accuracy problems in binary programs and extracting
   problematic expressions.

   Copyright (C) 2016-2017 Alex Sanchez-Stern

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307, USA.

   The GNU General Public License is contained in the file COPYING.
*/

#include "shadowop-info.h"
#include "pub_tool_mallocfree.h"
#include "pub_tool_debuginfo.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcbase.h"
#include "../../helper/ir-info.h"
#include "../../helper/bbuf.h"
#include "../../helper/runtime-util.h"
#include "../shadowop/mathreplace.h"

#include <math.h>
#include <stdint.h>

VgHashTable* mathreplaceOpInfoMap = NULL;
VgHashTable* semanticOpInfoMap = NULL;

void initOpShadowState(void){
  mathreplaceOpInfoMap = VG_(HT_construct)("call map mathreplace");
  semanticOpInfoMap = VG_(HT_construct)("call map semantic op");
  markMap = VG_(HT_construct)("mark map");
  intMarkMap = VG_(HT_construct)("int mark map");
}

ShadowOpInfo* mkShadowOpInfo(IROp_Extended op_code, OpType type,
                             Addr op_addr, Addr block_addr,
                             int nargs){
  ShadowOpInfo* result = VG_(perm_malloc)(sizeof(ShadowOpInfo), vg_alignof(ShadowOpInfo));
  result->op_code = op_code;
  result->op_addr = op_addr;
  result->block_addr = block_addr;
  result->op_type = type;

  result->expr = NULL;
  if (nargs != numFloatArgs(result)){
    printOpInfo(result);
    VG_(printf)("\n");
  }
  tl_assert2(nargs == numFloatArgs(result),
             "nargs and numArgs don't match! nargs is %d, but numArgs returns %d",
             nargs, numFloatArgs(result));
  initializeAggregate(&(result->agg), nargs);
  return result;
}

void initializeErrorAggregate(ErrorAggregate* error_agg){
  error_agg->max_error = -1;
  error_agg->total_error = 0;
  error_agg->num_evals = 0;
}

void initializeAggregate(Aggregate* agg, int nargs){
  initializeErrorAggregate(&(agg->global_error));
  initializeErrorAggregate(&(agg->local_error));
  agg->inputs.range_records = VG_(malloc)("input ranges", nargs * sizeof(RangeRecord));
  for(int i = 0; i < nargs; ++i){
    initRange(&(agg->inputs.range_records[i].pos_range));
    if (detailed_ranges){
      initRange(&(agg->inputs.range_records[i].neg_range));
    }
  }
}

void ppAddr(Addr addr){
  const HChar* src_filename;
  const HChar* fnname;
  UInt src_line;
  if (VG_(get_filename_linenum)(VG_(current_DiEpoch)(), addr, &src_filename,
                                NULL, &src_line)){
    VG_(get_fnname)(VG_(current_DiEpoch)(), addr, &fnname);
    VG_(printf)("%s:%u in %s (addr %lX)",
                src_filename, src_line, fnname, addr);
  } else {
    if (VG_(get_fnname)(VG_(current_DiEpoch)(), addr, &fnname)){
      VG_(printf)("%s (addr %lX)", fnname, addr);
    } else {
      VG_(printf)("addr %lX", addr);
    }
  }
  if (print_object_files){
    const HChar* objname;
    if (!VG_(get_objname)(VG_(current_DiEpoch)(), addr, &objname)){
      objname = "Unknown Object";
    }
    VG_(printf)(" in %s", objname);
  }
}
#define MAX_ADDR_STRING_SIZE 300
char* getAddrString(Addr addr){
  const HChar* src_filename;
  const HChar* fnname;
  UInt src_line;
  char _buf[MAX_ADDR_STRING_SIZE];
  BBuf* buf = mkBBuf(MAX_ADDR_STRING_SIZE, _buf);

  if (VG_(get_filename_linenum)(VG_(current_DiEpoch)(), addr, &src_filename,
                                NULL, &src_line)){
    fnname = getFnName(addr);
    printBBuf(buf, "%s:%u in %s (addr %lX)",
              src_filename, src_line, fnname, addr);
  } else {
    printBBuf(buf, "addr %lX", addr);
  }
  if (print_object_files){
    const HChar* objname;
    if (!VG_(get_objname)(VG_(current_DiEpoch)(), addr, &objname)){
      objname = "Unknown Object";
    }
    printBBuf(buf, " in %s", objname);
  }
  char* result = VG_(malloc)("addr string", MAX_ADDR_STRING_SIZE - buf->bound + 1);
  VG_(strcpy)(result, _buf);
  return result;
}

void printOpInfo(ShadowOpInfo* opinfo){
  if (opinfo->op_code == 0){
    VG_(printf)("%s", getWrappedName(opinfo->op_type));
  } else {
    ppIROp_Extended(opinfo->op_code);
  }
  VG_(printf)(" at ");
  ppAddr(opinfo->op_addr);
}

void updateInputRecords(InputsRecord* record, ShadowValue** args, int nargs){
  for (int i = 0; i < nargs; ++i){
    updateRangeRecord(record->range_records + i, getDouble(args[i]->real));
  }
}

int numFloatArgs(ShadowOpInfo* opinfo){
  if (opinfo->op_code == 0){
    return getWrappedNumArgs(opinfo->op_type);
  } else {
    return getNativeNumFloatArgs(opinfo->op_code);
  }
}

// WARNING: this function never frees its result, call sparingly
const char* getFnName(Addr addr){
  const char* fnname;
  VG_(get_fnname)(VG_(current_DiEpoch)(), addr, &fnname);
  if (isPrefix("caml", fnname)){
    char* demangledFnname = VG_(perm_malloc)(sizeof(char) * VG_(strlen)(fnname), 1);
    int n = 0;
    for(const char* p = fnname + 4; *p != '\0'; ++p){
      if (p[0] == '_' && p[1] == '_'){
        demangledFnname[n++] = '.';
        p++;
        continue;
      }
      if (p[0] == '_'){
        Bool restIsTag = True;
        for (const char* q = p + 1; *q != '\0'; ++q){
          if (!VG_(isdigit)(*q)){
            restIsTag = False;
            break;
          }
        }
        if (restIsTag){
          demangledFnname[n++] = '\0';
          break;
        }
      }
      demangledFnname[n++] = *p;
    }
    return demangledFnname;
  }
  return fnname;
}

int cmpInfo(ShadowOpInfo* info1, ShadowOpInfo* info2){
  /* if (info1->agg.local_error.max_error > */
  /*     info2->agg.local_error.max_error){ */
  /*   return 1; */
  /* } else if (info1->agg.local_error.max_error < */
  /*            info2->agg.local_error.max_error){ */
  /*   return -1; */
  /* } else if (info1->agg.local_error.total_error / info1->agg.local_error.num_evals > */
  /*            info2->agg.local_error.total_error / info2->agg.local_error.num_evals){ */
  /*   return 1; */
  /* } else if (info1->agg.local_error.total_error / info1->agg.local_error.num_evals > */
  /*            info2->agg.local_error.total_error / info2->agg.local_error.num_evals){ */
  /*   return -1; */
  /* } else */ if ((uintptr_t)info1 > (uintptr_t)info2){
    return 1;
  } else if ((uintptr_t)info1 < (uintptr_t)info2){
    return -1;
  } else {
    tl_assert(info1 == info2);
    return 0;
  }
}
