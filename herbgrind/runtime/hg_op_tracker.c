#include "hg_op_tracker.h"

#include "../include/hg_options.h"
#include "../types/hg_opinfo.h"
#include "../types/hg_ast.h"

#include "pub_tool_vki.h"
#include "pub_tool_libcfile.h"
#include "pub_tool_libcprint.h"

XArray* tracked_ops;

// How many characters are going to be allowed in each entry.
#define ENTRY_BUFFER_SIZE 512

void startTrackingOp(Op_Info* opinfo){
  if (tracked_ops == NULL){
    tracked_ops = VG_(newXA)(VG_(malloc), "op tracker",
                             VG_(free), sizeof(Op_Info*));
    VG_(setCmpFnXA)(tracked_ops, cmp_debuginfo);
  }
  VG_(addToXA)(tracked_ops, &opinfo);
}

// Assumes no duplicates. Will result in NULL's in the tracked ops
// list, does not actually remove from the list, just sets matching op
// to NULL.
void clearTrackedOp(Op_Info* opinfo){
  for(int i = 0; i < VG_(sizeXA)(tracked_ops); ++i){
    Op_Info** entry = VG_(indexXA)(tracked_ops, i);
    if (*entry == NULL) continue;
    if (*entry == opinfo){
      *entry = NULL;
      return;
    }
  }
}
void recursivelyClearChildren(OpASTNode* node){
  if (node->tag != Node_Branch) return;
  for(int i = 0; i < node->nd.Branch.nargs; ++i){
    OpASTNode* child = node->nd.Branch.args[i];
    recursivelyClearChildren(child);
    if (child->tag == Node_Branch)
      clearTrackedOp(child->nd.Branch.op);
  }
}

Int cmp_debuginfo(const void* a, const void* b){
  return ((const Op_Info*)b)->evalinfo.max_error -
    ((const Op_Info*)a)->evalinfo.max_error;
}

void writeReport(const HChar* filename){
  HChar buf[ENTRY_BUFFER_SIZE];
  // Try to open the filename they gave us.
  SysRes file_result = VG_(open)(filename,
                                 VKI_O_CREAT | VKI_O_TRUNC | VKI_O_WRONLY,
                                 VKI_S_IRUSR | VKI_S_IWUSR);
  if(sr_isError(file_result)){
    VG_(printf)("Couldn't open output file!\n");
    return;
  }
  Int file_d = sr_Res(file_result);

  if (tracked_ops == NULL){
    VG_(write)(file_d, "No errors found.\n", 18);
    VG_(close)(file_d);
    return;
  }

  if (report_exprs)
    // For each expression, counting from the back where the bigger
    // expressions should be, eliminate subexpressions from the list
    // for reporting.
    for(int i = VG_(sizeXA)(tracked_ops) - 1; i >= 0; --i){
      Op_Info** entry = VG_(indexXA)(tracked_ops, i);
      Op_Info* opinfo = *entry;
      if (opinfo == NULL) continue;
      recursivelyClearChildren(opinfo->ast);
    }

  // Sort the entries by maximum error.
  VG_(sortXA)(tracked_ops);

  // Write out an entry for each tracked op.
  for(int i = 0; i < VG_(sizeXA)(tracked_ops); ++i){
    Op_Info* opinfo = *(Op_Info**)VG_(indexXA)(tracked_ops, i);

    if (opinfo == NULL) continue;

    UInt entry_len;
    char* astString = opASTtoString(opinfo->ast);
    if (human_readable){
      entry_len = VG_(snprintf)(buf, ENTRY_BUFFER_SIZE,
                                "%s\n"
                                "%s in %s at %s:%u (address %lX)\n"
                                "%f bits average error\n"
                                "%f bits max error\n"
                                "Aggregated over %lu instances\n\n",
                                astString,
                                opinfo->debuginfo.plain_opname,
                                opinfo->debuginfo.fnname,
                                opinfo->debuginfo.src_filename,
                                opinfo->debuginfo.src_line,
                                opinfo->debuginfo.op_addr,
                                opinfo->evalinfo.total_error / opinfo->evalinfo.num_calls,
                                opinfo->evalinfo.max_error,
                                opinfo->evalinfo.num_calls);
    } else {
      entry_len = VG_(snprintf)(buf, ENTRY_BUFFER_SIZE,
                                "((expr %s) "
                                 "(plain-name \"%s\") "
                                 "(function \"%s\") "
                                 "(filename \"%s\") "
                                 "(line-num %u) "
                                 "(instr-addr %lX) "
                                 "(avg-error %f) "
                                 "(max-error %f) "
                                 "(num-calls %lu))\n",
                                astString,
                                opinfo->debuginfo.plain_opname,
                                opinfo->debuginfo.fnname,
                                opinfo->debuginfo.src_filename,
                                opinfo->debuginfo.src_line,
                                opinfo->debuginfo.op_addr,
                                opinfo->evalinfo.total_error / opinfo->evalinfo.num_calls,
                                opinfo->evalinfo.max_error,
                                opinfo->evalinfo.num_calls);
    }
    VG_(free)(astString);
    VG_(write)(file_d, buf, entry_len);
  }

  // Finally, close up the file.
  VG_(close)(file_d);
}
