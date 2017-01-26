/*--------------------------------------------------------------------*/
/*--- HerbGrind: a valgrind tool for Herbie    value-shadowstate.c ---*/
/*--------------------------------------------------------------------*/

/*
   This file is part of HerbGrind, a valgrind tool for diagnosing
   floating point accuracy problems in binary programs and extracting
   problematic expressions.

   Copyright (C) 2016 Alex Sanchez-Stern

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

#include "value-shadowstate.h"

#include "pub_tool_hashtable.h"
#include "pub_tool_libcprint.h"
#include "pub_tool_libcassert.h"
#include "pub_tool_libcbase.h"
#include "pub_tool_threadstate.h"

#include "../../options.h"

ShadowTemp* shadowTemps[MAX_TEMPS];
ShadowValue* shadowThreadState[MAX_THREADS][MAX_REGISTERS];
VgHashTable* shadowMemory = NULL;

Stack* freedTemps[MAX_TEMP_SHADOWS];
Stack* freedVals;

void initValueShadowState(void){
  for(int i = 0; i < MAX_TEMP_SHADOWS; ++i){
    freedTemps[i] = mkStack();
  }
  freedVals = mkStack();
}

VG_REGPARM(2) void dynamicCleanup(int nentries, IRTemp* entries){
  Bool hasEntriesToCleanup = False;
  for(int i = 0; i < nentries; ++i){
    ShadowTemp* temp = shadowTemps[entries[i]];
    if (temp == NULL) continue;
    for(int j = 0; j < temp->num_vals; ++j){
    if (print_moves){
      if (!hasEntriesToCleanup){
        VG_(printf)("Freeing temp(s) %p", shadowTemps[entries[i]]);
        hasEntriesToCleanup = True;
      } else {
        VG_(printf)(", %p", shadowTemps[entries[i]]);
      }
      disownShadowValue(temp->values[j]);
    }
    freeShadowTemp(temp);
    shadowTemps[entries[i]] = NULL;
  }
}
inline
ShadowValue* getTS(Int idx){
  return shadowThreadState[VG_(get_running_tid)()][idx];
}
VG_REGPARM(2) void dynamicPut(Int tsDest, ShadowTemp* st){
  for(int i = 0; i < st->num_vals; ++i){
    ShadowValue* val = st->values[i];
    int size = val->type == Ft_Single ? sizeof(float) : sizeof(double);
    shadowThreadState[VG_(get_running_tid)()][tsDest + (i * size)] =
      val;
    if (val->type == Ft_Double){
      shadowThreadState[VG_(get_running_tid)()]
        [tsDest + ((i + 1) * size)] =
        NULL;
    }
    ownShadowValue(val);
  }
}
VG_REGPARM(1) ShadowTemp* dynamicGet64(Int tsSrc, UWord tsBytes){
  ShadowValue* firstValue = getTS(tsSrc);
  if (firstValue == NULL){
    ShadowValue* secondValue = getTS(tsSrc + sizeof(float));
    if (secondValue == NULL || secondValue->type != Ft_Single){
      return NULL;
    }
    ShadowTemp* temp = mkShadowTemp(2);
    float firstValueBytes;
    VG_(memcpy)(&firstValueBytes, &tsBytes, sizeof(float));
    firstValue = mkShadowValue(Ft_Single, firstValueBytes);
    temp->values[0] = firstValue;
    temp->values[1] = secondValue;
    ownShadowValue(secondValue);
    return temp;
  } if (firstValue->type == Ft_Double){
    ShadowTemp* temp = mkShadowTemp(1);
    temp->values[0] = firstValue;
    ownShadowValue(firstValue);
    return temp;
  } else {
    ShadowValue* secondValue = getTS(tsSrc + sizeof(float));
    if (secondValue == NULL){
      float secondValueBytes;
      VG_(memcpy)(&secondValueBytes, (&tsBytes) + sizeof(float),
                  sizeof(float));
      secondValue = mkShadowValue(Ft_Single, secondValueBytes);
    } else {
      ownShadowValue(secondValue);
    ownShadowValue(firstValue);
    tl_assert(secondValue->type == Ft_Single);
    ShadowTemp* temp = mkShadowTemp(2);
    temp->values[0] = firstValue;
    temp->values[1] = secondValue;
    return temp;
  }
}
VG_REGPARM(1) ShadowTemp* dynamicGet128(Int tsSrc,
                                        UWord bytes1,
                                        UWord bytes2){
  FloatType valType = Ft_Unknown;
  int setIndex = 0;
  ShadowValue* setValue = NULL;
  for(int i = 0; i < 4; ++i){
    ShadowValue* value = getTS(tsSrc + sizeof(float) * i);
    if (value != NULL){
      tl_assert2(value->type == valType || valType == Ft_Unknown,
                 "Mismatched values! "
                 "TS(%d) (%p) has type %d, "
                 "but TS(%d) (%p) has type %d!\n",
                 tsSrc + sizeof(float) * setIndex, setValue, valType,
                 tsSrc + sizeof(float) * i, value, value->type);
      valType = value->type;
      setIndex = i;
      setValue = value;
    }
  }
  if (valType == Ft_Unknown){
    return NULL;
  } else if (valType == Ft_Double){
    ShadowTemp* temp = mkShadowTemp(2);
    for(int i = 0; i < 2; ++i){
      Int tsAddr = tsSrc + sizeof(double) * i;
      temp->values[i] = getTS(tsAddr);
      if (temp->values[i] == NULL){
        double valBytes;
        VG_(memcpy)(&valBytes, i == 0 ? &bytes1 : &bytes2,
                    sizeof(double));
        temp->values[i] = mkShadowValue(Ft_Double, valBytes);
      } else {
        ownShadowValue(temp->values[i]);
      }
    }
    return temp;
  } else {
    ShadowTemp* temp = mkShadowTemp(4);
    for(int i = 0; i < 4; ++i){
      Int tsAddr = tsSrc + sizeof(float) * i;
      temp->values[i] = getTS(tsAddr);
      if (temp->values[i] == NULL){
        float valBytes;
        VG_(memcpy)(&valBytes,
                    (i < 3 ? &bytes1 : &bytes2) + (i % 2) * sizeof(float),
                    sizeof(float));
        temp->values[i] = mkShadowValue(Ft_Single, valBytes);
      } else {
        ownShadowValue(temp->values[i]);
      }
    }
    return temp;
  }
}
void freeShadowTemp(ShadowTemp* temp){
  stack_push(freedTemps[temp->num_vals - 1], (void*)temp);
}

inline
ShadowTemp* mkShadowTemp(UWord num_vals){
  ShadowTemp* result;
  if (stack_empty(freedTemps[num_vals - 1])){
    result = newShadowTemp(num_vals);
  } else {
    result = (void*)stack_pop(freedTemps[num_vals - 1]);
  }
  return result;
}
inline
void stack_push_fast(Stack* s, StackNode* item_node){
  item_node->next = s->head;
  s->head = item_node;
}
void freeShadowValue(ShadowValue* val){
  if (val->influences != NULL){
    VG_(deleteXA)(val->influences);
    val->influences = NULL;
  }
  stack_push_fast(freedVals, (void*)val);
}

inline
StackNode* stack_pop_fast(Stack* s){
  StackNode* oldHead = s->head;
  s->head = oldHead->next;
  return oldHead;
}

inline
ShadowValue* mkShadowValueBare(FloatType type){
  ShadowValue* result;
  if (stack_empty(freedVals)){
    result = newShadowValue(type);
  } else {
    result = (void*)stack_pop_fast(freedVals);
    result->type = type;
  }
  result->ref_count = 1;
  return result;
}
inline
ShadowValue* mkShadowValue(FloatType type, double value){
  ShadowValue* result = mkShadowValueBare(type);
  setReal(result->real, value);
  return result;
}

VG_REGPARM(1) ShadowTemp* copyShadowTemp(ShadowTemp* temp){
  ShadowTemp* result = mkShadowTemp(temp->num_vals);
  for(int i = 0; i < temp->num_vals; ++i){
    ownShadowValue(temp->values[i]);
    result->values[i] = temp->values[i];
    }
  }
  return result;
}
VG_REGPARM(1) ShadowTemp* deepCopyShadowTemp(ShadowTemp* temp){
  ShadowTemp* result = mkShadowTemp(temp->num_vals);
  for(int i = 0; i < temp->num_vals; ++i){
    result->values[i] = copyShadowValue(temp->values[i]);
  }
  return result;
}
inline
void disownShadowTemp(ShadowTemp* temp){
  for(int i = 0; i < temp->num_vals; ++i){
    disownShadowValue(temp->values[i]);
  }
  freeShadowTemp(temp);
}
VG_REGPARM(1) void disownShadowTempNonNullDynamic(IRTemp idx){
  disownShadowTemp(shadowTemps[idx]);
}
VG_REGPARM(1) void disownShadowTempDynamic(IRTemp idx){
  if (shadowTemps[idx] != NULL){
    disownShadowTemp(shadowTemps[idx]);
  }
}
void disownShadowValue(ShadowValue* val){
  if (val == NULL) return;
  if (val->ref_count < 2){
    freeShadowValue(val);
  } else {
    (val->ref_count)--;
  }
}
void ownShadowValue(ShadowValue* val){
  if (val == NULL) return;
  (val->ref_count)++;
}

VG_REGPARM(1) ShadowTemp* mkShadowTempOneDouble(double value){
  ShadowTemp* result = mkShadowTemp(1);
  result->values[0] = mkShadowValue(Ft_Double, value);
  if (print_moves){
    VG_(printf)("Made one double %p\n", result);
  }
  return result;
}
VG_REGPARM(1) ShadowTemp* mkShadowTempTwoDoubles(double* values){
  ShadowTemp* result = mkShadowTemp(2);
  result->values[0] =
    mkShadowValue(Ft_Double, values[0]);
  result->values[1] =
    mkShadowValue(Ft_Double, values[1]);
  return result;
}
VG_REGPARM(1) ShadowTemp* mkShadowTempOneSingle(double value){
  ShadowTemp* result = mkShadowTemp(1);
  result->values[0] = mkShadowValue(Ft_Single, value);
  return result;
}
VG_REGPARM(1) ShadowTemp* mkShadowTempTwoSingles(UWord values){
  ShadowTemp* result = mkShadowTemp(2);
  float floatValues[2];
  VG_(memcpy)(floatValues, &values, sizeof(floatValues));
  result->values[0] = mkShadowValue(Ft_Single, floatValues[0]);
  result->values[1] = mkShadowValue(Ft_Single, floatValues[1]);
  return result;
}
inline
VG_REGPARM(1) ShadowTemp* mkShadowTempFourSingles(float* values){
  ShadowTemp* result = mkShadowTemp(4);
  result->values[0] =
    mkShadowValue(Ft_Single, values[0]);
  result->values[1] =
    mkShadowValue(Ft_Single, values[1]);
  result->values[2] =
    mkShadowValue(Ft_Single, values[2]);
  result->values[3] =
    mkShadowValue(Ft_Single, values[3]);
  return result;
}
VG_REGPARM(1) ShadowTemp* mkShadowTempFourSinglesG(UWord guard, float* values){
  if (!guard) return NULL;
  return mkShadowTempFourSingles(values);
}
