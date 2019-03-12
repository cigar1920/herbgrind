/*--------------------------------------------------------------------*/
/*--- Herbgrind: a valgrind tool for Herbie              hg_main.c ---*/
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

#include "hg_main.h"
#include "include/herbgrind.h"
#include "include/mathreplace-funcs.h"
#include "options.h"
#include "instrument/instrument.h"
#include "runtime/shadowop/mathreplace.h"
#include "runtime/shadowop/influence-op.h"
#include "runtime/op-shadowstate/marks.h"
#include "runtime/op-shadowstate/output.h"

#include "helper/mpfr-valgrind-glue.h"

// This handles client requests, the macros that client programs stick
// in to send messages to the tool.
static Bool hg_handle_client_request(ThreadId tid, UWord* arg, UWord* ret) {
  if (!VG_IS_TOOL_USERREQ('H', 'B', arg[0])){
    return False;
  }
  switch(arg[0]) {
  case VG_USERREQ__BEGIN:
    running_depth++;
    break;
  case VG_USERREQ__END:
    running_depth--;
    break;
  case VG_USERREQ__PERFORM_OP:
    performWrappedOp((OpType)arg[1], (double*)arg[2], (double*)arg[3]);
    break;
  case VG_USERREQ__PERFORM_OPF:
    {
      double double_args[3];
      double double_result;
      for (int i = 0; i < getWrappedNumArgs((OpType)arg[1]); ++i){
        double_args[i] = ((float*)arg[3])[i];
      }
      performWrappedOp((OpType)arg[1], &double_result, double_args);
      *(float*)arg[2] = double_result;
    }
    break;
  case VG_USERREQ__PERFORM_SPECIAL_OP:
    performSpecialWrappedOp((SpecialOpType)arg[1], (double*)arg[2],
                            (double*)arg[3], (double*)arg[4]);
    break;
  case VG_USERREQ__MARK_IMPORTANT:
    markImportant(getMemShadow((Addr)arg[1]),
                  *(double*)(Addr)arg[1], 0, 1);
    break;
  case VG_USERREQ__MAYBE_MARK_IMPORTANT:
    maybeMarkImportant(getMemShadow((Addr)arg[1]),
                       *(double*)(Addr)arg[1], 0, 1);
    break;
  case VG_USERREQ__MAYBE_MARK_IMPORTANT_WITH_INDEX:
    maybeMarkImportant(getMemShadow((Addr)arg[1]),
                       *(double*)(Addr)arg[1], (int)arg[2], (int)arg[3]);
    break;
  case VG_USERREQ__FORCE_TRACK:
    forceTrack((Addr)arg[1]);
    break;
  default:
    return False;
  }
  *ret = 0;
  return True;
}

// This is called after the program exits, for cleanup and such.
static void hg_fini(Int exitcode){
  finish_instrumentation();
  writeOutput();
}
// This does any initialization that needs to be done after command
// line processing.
static void hg_post_clo_init(void){
  init_instrumentation();
}

// This is where we initialize everything
static void hg_pre_clo_init(void)
{
   VG_(details_name)            ("Herbgrind");
   VG_(details_version)         (NULL);
   VG_(details_description)     ("a valgrind tool for Herbie");
   VG_(details_copyright_author)("Copyright (C) 2016-2017, and GNU GPL'd, by Alex Sanchez-Stern");
   VG_(details_bug_reports_to)  (VG_BUGS_TO);

   VG_(details_avg_translation_sizeB) ( 275 );

   VG_(basic_tool_funcs)        (hg_post_clo_init,
                                 hg_instrument,
                                 hg_fini);

   VG_(needs_client_requests) (hg_handle_client_request);
   VG_(needs_command_line_options)(hg_process_cmd_line_option,
                                   hg_print_usage,
                                   hg_print_debug_usage);
   setup_mpfr_valgrind_glue();
}

VG_DETERMINE_INTERFACE_VERSION(hg_pre_clo_init)

/*--------------------------------------------------------------------*/
/*--- end                                                          ---*/
/*--------------------------------------------------------------------*/
