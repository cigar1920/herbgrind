/*--------------------------------------------------------------------*/
/*--- Herbgrind: a valgrind tool for Herbie            ownership.h ---*/
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
#ifndef _OWNERSHIP_H
#define _OWNERSHIP_H

#include "pub_tool_basics.h"
#include "pub_tool_tooliface.h"
#include "pub_tool_xarray.h"

extern XArray* tempDebt;

void initOwnership(void);
void cleanupBlockOwnership(IRSB* sbOut, IRExpr* guard);
void resetOwnership(IRSB* sbOut);
void cleanupAtEndOfBlock(IRSB* sbOut, IRTemp shadowed_temp);
void addDynamicDisown(IRSB* sbOut, IRTemp idx);
void addDynamicDisownNonNull(IRSB* sbOut, IRTemp idx);
void addDynamicDisownNonNullDetached(IRSB* sbOut, IRExpr* st);
void addDisownNonNull(IRSB* sbOut, IRExpr* shadow_temp, int num_vals);
void addDisown(IRSB* sbOut, IRExpr* shadow_temp, int num_vals);
void addDisownG(IRSB* sbOut, IRExpr* guard, IRExpr* shadow_temp, int num_vals);
void addSVDisown(IRSB* sbOut, IRExpr* sv);
void addSVDisownNonNull(IRSB* sbOut, IRExpr* sv);
void addSVDisownNonNullG(IRSB* sbOut, IRExpr* guard, IRExpr* sv);
void addSVDisownG(IRSB* sbOut, IRExpr* guard, IRExpr* sv);
void addSVOwn(IRSB* sbOut, IRExpr* sv);
void addSVOwnG(IRSB* sbOut, IRExpr* guard, IRExpr* sv);
void addSVOwnNonNullG(IRSB* sbOut, IRExpr* guard, IRExpr* sv);
void addSVOwnNonNull(IRSB* sbOut, IRExpr* sv);
void addExprDisownG(IRSB* sbOut, IRExpr* guard, IRExpr* expr);
void addClear(IRSB* sbOut, IRTemp shadowed_temp, int num_vals);

#endif
