/*
 * Copyright 2020-2025 Hewlett Packard Enterprise Development LP
 * Copyright 2004-2019 Cray Inc.
 * Other additional copyright holders may be indicated within.
 *
 * The entirety of this work is licensed under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "primitive.h"

#include "DecoratedClassType.h"
#include "expr.h"
#include "iterator.h"
#include "stringutil.h"
#include "type.h"
#include "resolution.h"
#include "wellknown.h"

#include "global-ast-vecs.h"

static QualifiedType
returnInfoUnknown(CallExpr* call) {
  return QualifiedType(dtUnknown);
}

static QualifiedType
returnInfoVoid(CallExpr* call) {
  return QualifiedType(dtVoid, QUAL_VAL);
}

static QualifiedType
returnInfoCVoidPtr(CallExpr* call) {
  return QualifiedType(dtCVoidPtr, QUAL_VAL);
}

static QualifiedType
returnInfoBool(CallExpr* call) {
  return QualifiedType(dtBool, QUAL_VAL);
}

static QualifiedType
returnInfoString(CallExpr* call) {
  return QualifiedType(dtString, QUAL_VAL);
}

static QualifiedType
returnInfoBytes(CallExpr* call) {
  return QualifiedType(dtBytes, QUAL_VAL);
}

static QualifiedType
returnInfoStringC(CallExpr* call) {
  return QualifiedType(dtStringC, QUAL_VAL);
}

static QualifiedType
returnInfoWideMake(CallExpr* call) {
  Type* t1 = call->get(1)->typeInfo();
  if (t1->symbol->hasFlag(FLAG_REF))
    INT_FATAL("ref not supported here yet");

  if (wideClassMap.get(t1))
    t1 = wideClassMap.get(t1);

  return QualifiedType(t1, QUAL_VAL);
}


static QualifiedType
returnInfoLocaleID(CallExpr* call) {
  return QualifiedType(dtLocaleID, QUAL_VAL);
}

static QualifiedType
returnInfoNodeID(CallExpr* call) {
  return QualifiedType(NODE_ID_TYPE, QUAL_VAL);
}

static QualifiedType
returnInfoUInt8(CallExpr* call) {
  return QualifiedType(dtUInt[INT_SIZE_8], QUAL_VAL);
}

static QualifiedType
returnInfoInt32(CallExpr* call) {
  return QualifiedType(dtInt[INT_SIZE_32], QUAL_VAL);
}

static QualifiedType
returnInfoInt64(CallExpr* call) {
  return QualifiedType(dtInt[INT_SIZE_64], QUAL_VAL);
}

static QualifiedType
returnInfoUInt64(CallExpr* call) {
  return QualifiedType(dtUInt[INT_SIZE_64], QUAL_VAL);
}

static QualifiedType
returnInfoSizeType(CallExpr* call) {
  return QualifiedType(SIZE_TYPE, QUAL_VAL);
}

//
// Since 'int' is equivalent to 'int(64)' currently, this doesn't do
// anything different than returnInfoInt64 does, but it's intended to
// be used in cases where a primitive returns a type that ought to
// track the default 'int' size rather than being hard-coded to a
// specific bit-width.
//
static QualifiedType
returnInfoDefaultInt(CallExpr* call) {
  return returnInfoInt64(call);
}

static QualifiedType
returnInfoUInt32(CallExpr* call) { // unexecuted none/gasnet on 4/25/08
  return QualifiedType(dtUInt[INT_SIZE_32], QUAL_VAL);
}

static QualifiedType
returnInfoReal32(CallExpr* call) {
  return QualifiedType(dtReal[FLOAT_SIZE_32], QUAL_VAL);
}

static QualifiedType
returnInfoReal64(CallExpr* call) {
  return QualifiedType(dtReal[FLOAT_SIZE_64], QUAL_VAL);
}

static QualifiedType
returnInfoComplexField(CallExpr* call) {  // for get real/imag primitives
  Type *t = call->get(1)->getValType();
  if (t == dtComplex[COMPLEX_SIZE_64]) {
    return QualifiedType(dtReal[FLOAT_SIZE_32]->refType, QUAL_REF);
  } else if (t == dtComplex[COMPLEX_SIZE_128]) {
    return QualifiedType(dtReal[FLOAT_SIZE_64]->refType, QUAL_REF);
  } else {
    INT_FATAL( call, "unsupported complex size");
  }
  return QualifiedType(dtUnknown);
}

static QualifiedType
returnInfoAbs(CallExpr* call) {
  Type *t = call->get(1)->getValType();
  if (t == dtComplex[COMPLEX_SIZE_64]) {
    return QualifiedType(dtReal[FLOAT_SIZE_32], QUAL_VAL);
  } else if (t == dtComplex[COMPLEX_SIZE_128]) {
    return QualifiedType(dtReal[FLOAT_SIZE_64], QUAL_VAL);
  } else if (t == dtImag[FLOAT_SIZE_32]) {
    return QualifiedType(dtReal[FLOAT_SIZE_32], QUAL_VAL);
  } else if (t == dtImag[FLOAT_SIZE_64]) {
    return QualifiedType(dtReal[FLOAT_SIZE_64], QUAL_VAL);
  }

  return QualifiedType(t, QUAL_VAL);
}


static QualifiedType
returnInfoFirst(CallExpr* call) {
  return call->get(1)->qualType();
}

static QualifiedType
returnInfoSecond(CallExpr* call) {
  return call->get(2)->qualType();
}

static QualifiedType
returnInfoFirstAsValue(CallExpr* call) {
  return QualifiedType(Qualifier::QUAL_CONST_VAL, call->get(1)->qualType().type());
}

static QualifiedType
returnInfoFirstDeref(CallExpr* call) {
  QualifiedType tmp = call->get(1)->qualType();
  Type* type = tmp.type()->getValType();
  // if it's a tuple, also remove references in the elements
  if (type->symbol->hasFlag(FLAG_TUPLE)) {
    AggregateType* tupleType = toAggregateType(type);
    INT_ASSERT(tupleType);

    type = computeNonRefTuple(tupleType);
  }
  return QualifiedType(type, QUAL_VAL);
}

static QualifiedType
returnInfoFirstDerefNoRtti(CallExpr* call) {
  auto qt = returnInfoFirstDeref(call);
  if (qt.type()->symbol->hasFlag(FLAG_HAS_RUNTIME_TYPE)) {
    USR_FATAL(call, "static variables do not support types with"
                    "runtime type information (such as arrays)");
  }
  return qt;
}

static QualifiedType
returnInfoScalarPromotionType(CallExpr* call) {
  QualifiedType tmp = call->get(1)->qualType();
  Type* type = tmp.type()->getValType();

  if (type->scalarPromotionType)
    type = type->scalarPromotionType;

  return QualifiedType(type, QUAL_VAL);
}

static QualifiedType
returnInfoThunkResultType(CallExpr* call) {
  QualifiedType tmp = call->get(1)->qualType();
  AggregateType* type = toAggregateType(tmp.type()->getValType());

  INT_ASSERT(type);
  INT_ASSERT(type->thunkInvoke);

  return QualifiedType(type->thunkInvoke->retType, QUAL_VAL);
}

static QualifiedType
returnInfoStaticFieldType(CallExpr* call) {
  // The code below is not very general. It can be extended as needed.
  // The first argument is the variable or the type whose field is queried.
  Type* type = call->get(1)->getValType();
  INT_ASSERT(! type->symbol->hasFlag(FLAG_TUPLE)); // not implemented
  AggregateType* at = toAggregateType(canonicalDecoratedClassType(type));
  INT_ASSERT(at); // caller's responsibility
  // The second argument is the name of the field.
  VarSymbol* nameSym = toVarSymbol(toSymExpr(call->get(2))->symbol());
  // caller's responsibility
  INT_ASSERT(nameSym->immediate->const_kind == CONST_KIND_STRING);
  Symbol* field = at->getField(nameSym->immediate->v_string.c_str(), true);
  return field->qualType().toVal();
}


static QualifiedType
returnInfoCast(CallExpr* call) {
  Type* t1 = call->get(1)->typeInfo();
  Type* t2 = call->get(2)->typeInfo();
  if (t2->symbol->hasFlag(FLAG_WIDE_CLASS)) {
    if (wideClassMap.get(t1))
      t1 = wideClassMap.get(t1);
  }
  if (t2->symbol->hasFlag(FLAG_WIDE_REF)) {
    if (wideRefMap.get(t1))
      t1 = wideRefMap.get(t1);
  }
  INT_ASSERT(!isReferenceType(t1));
  return QualifiedType(t1, QUAL_VAL);
}

static QualifiedType
returnInfoForProcTypeConverter(CallExpr* call) {
  Type* t = call->get(1)->typeInfo();
  int64_t mask = 0;

  // Get the second arg as a 'param int' if it exists.
  auto arg2 = call->numActuals() >= 2 ? call->get(2) : nullptr;
  bool got = get_int(arg2, &mask);
  INT_ASSERT(got || !arg2);

  Type* typeToUse = dtUnknown;
  if (auto ft1 = toFunctionType(t)) {
    bool maskConflicts = false;
    typeToUse = ft1->getWithMask(mask, maskConflicts);
    std::ignore = maskConflicts;
  }

  return QualifiedType(typeToUse, QUAL_VAL);
}

static QualifiedType
returnInfoVal(CallExpr* call) {
  AggregateType* ct = toAggregateType(call->get(1)->typeInfo());

  if (call->get(1)->isRefOrWideRef()) {
    return QualifiedType(call->get(1)->getValType(), QUAL_VAL);
  } else if (ct && ct->symbol->hasFlag(FLAG_WIDE_CLASS)) {
    return QualifiedType(ct, QUAL_VAL);
  }

  INT_FATAL(call, "attempt to get value type of non-reference type");
  return QualifiedType(NULL);
}

// BHARSH TODO: merge this with returnInfoAsRef and fix the WIDE_REF case...
static QualifiedType
returnInfoRef(CallExpr* call) {
  Type* t = call->get(1)->getValType();
  if(t->symbol->hasFlag(FLAG_GENERIC))
    // this will result in an error later on
    return QualifiedType(t, QUAL_REF);
  if (!t->refType)
    INT_FATAL(call, "invalid attempt to get reference type");
  return QualifiedType(t->refType, QUAL_REF);
}

static QualifiedType
returnInfoAsRef(CallExpr* call) {
  Type* t = call->get(1)->typeInfo();
  if (isReferenceType(t)) {
    return QualifiedType(t, QUAL_REF);
  }
  else if (t->symbol->hasFlag(FLAG_WIDE_REF)) {
    return QualifiedType(t, QUAL_WIDE_REF);
  }
  else {
    if (!t->refType)
      INT_FATAL(call, "invalid attempt to get reference type");
    return QualifiedType(t->refType, QUAL_REF);
  }
}

// NEEDS TO BE FINISHED WHEN PRIMITIVES ARE REDONE
static QualifiedType
returnInfoNumericUp(CallExpr* call) {
  Type* t1 = call->get(1)->typeInfo()->getValType();
  Type* t2 = call->get(2)->typeInfo()->getValType();
  if (is_int_type(t1) && is_real_type(t2))
    return QualifiedType(t2, QUAL_VAL);
  if (is_real_type(t1) && is_int_type(t2))
    return QualifiedType(t1, QUAL_VAL);
  if (is_int_type(t1) && is_bool_type(t2))
    return QualifiedType(t1, QUAL_VAL);
  if (is_bool_type(t1) && is_int_type(t2))
    return QualifiedType(t2, QUAL_VAL);
  return QualifiedType(t1, QUAL_VAL);
}

static QualifiedType
returnInfoArrayIndexValue(CallExpr* call) {
  Type* type = call->get(1)->getValType();
  if (type->symbol->hasFlag(FLAG_WIDE_CLASS))
    type = type->getField("addr")->type;
  // Is this conditional necessary?  Can just assume condition is true?
  if (type->symbol->hasFlag(FLAG_DATA_CLASS) ||
      type->symbol->hasFlag(FLAG_C_ARRAY)) {
    TypeSymbol* eltTypeSymbol = getDataClassType(type->symbol);
    INT_ASSERT(eltTypeSymbol);
    return QualifiedType(eltTypeSymbol->type, QUAL_VAL);
  } else {
    INT_FATAL("unsupported case");
    return QualifiedType(NULL);
  }
}

static QualifiedType
returnInfoArrayIndex(CallExpr* call) {
  QualifiedType tmp = returnInfoArrayIndexValue(call);
  return QualifiedType(tmp.type()->refType, QUAL_REF);
}

static QualifiedType
returnInfoGetMember(CallExpr* call) {
  AggregateType* ct = toAggregateType(call->get(1)->typeInfo());
  if (!ct)
    INT_FATAL(call, "bad member primitive");
  if (ct->symbol->hasFlag(FLAG_REF))
    ct = toAggregateType(ct->getValType());
  SymExpr* sym = toSymExpr(call->get(2));
  if (!sym)
    INT_FATAL(call, "bad member primitive");
  VarSymbol* var = toVarSymbol(sym->symbol());
  if (!var)
    INT_FATAL(call, "bad member primitive");
  if (Immediate* imm = var->immediate) {
    Symbol* field = NULL;
    if (imm->const_kind == CONST_KIND_STRING)
    {
      field = ct->getField(var->immediate->v_string.c_str());
    }
    if (imm->const_kind == NUM_KIND_INT)
    {
      int64_t i = imm->int_value();
      field = ct->getField(i);
    }
    INT_ASSERT(field);

    return field->qualType();
  } else
    return sym->qualType();
  INT_FATAL(call, "bad member primitive");
  return QualifiedType(NULL);
}

static QualifiedType
returnInfoGetTupleMember(CallExpr* call) {
  AggregateType* ct = toAggregateType(call->get(1)->getValType());
  INT_ASSERT(ct);
  if (!ct->symbol->hasFlag(FLAG_STAR_TUPLE)) {
    USR_FATAL(call, "invalid access of non-homogeneous tuple by runtime value");
  }
  return ct->getField("x0")->qualType();
}

static QualifiedType
returnInfoGetTupleMemberRef(CallExpr* call) {
  Type* type = returnInfoGetTupleMember(call).type();
  if (type->refType)
    type = type->refType;
  Qualifier q = QUAL_REF;
  if (call->get(1)->isWideRef()) {
    q = QUAL_WIDE_REF;
    if (Type* t = wideRefMap.get(type)) {
      type = t;
    }
  }
  return QualifiedType(type, q);
}

static QualifiedType
returnInfoGetMemberRef(CallExpr* call) {
  Type* t = canonicalDecoratedClassType(call->get(1)->getValType());
  AggregateType* ct = toAggregateType(t);
  INT_ASSERT(ct);
  SymExpr* se = toSymExpr(call->get(2));
  INT_ASSERT(se);
  VarSymbol* var = toVarSymbol(se->symbol());
  INT_ASSERT(var);
  Type* retType = NULL;
  if (Immediate* imm = var->immediate)
  {
    Symbol* field = NULL;
    if (imm->const_kind == CONST_KIND_STRING)
    {
      field = ct->getField(var->immediate->v_string.c_str());
    }
    if (imm->const_kind == NUM_KIND_INT)
    {
      int64_t i = imm->int_value();
      if (ct->symbol->hasFlag(FLAG_ITERATOR_CLASS)) {
        // Handle a peculiar intra-pass state where we attempt to get the
        // type of an iterator class field when what we really want is the
        // type of the corresponding formal of the iterator function.
        FnSymbol* fn = getTheIteratorFn(ct);
        field = fn->getFormal(i);
      } else {
        field = ct->getField(i);
      }
    }
    INT_ASSERT(field);
    makeRefType(field->type);
    retType = field->type->refType ? field->type->refType : field->type;
  } else {
    makeRefType(var->type);
    retType = var->type->refType ? var->type->refType : var->type;
  }
  Qualifier q = QUAL_REF;
  if (call->get(1)->isWideRef()) {
    q = QUAL_WIDE_REF;
    if (Type* t = wideRefMap.get(retType)) {
      retType = t;
    }
  }
  return QualifiedType(retType, q);
}

static QualifiedType
returnInfoEndCount(CallExpr* call) {
  static Type* endCountType = NULL;
  if (endCountType == NULL) {
    // Look for the type var `_remoteEndCountType` in ChapelBase.
    const char* searchAstr = astr("_remoteEndCountType");
    forv_Vec(VarSymbol, var, gVarSymbols) {
      if (var->name == searchAstr) {
        endCountType = var->type;
        break;
      }
    }
  }
  return QualifiedType(endCountType, QUAL_VAL);
}

static QualifiedType
returnInfoError(CallExpr* call) {
  AggregateType* at = toAggregateType(dtError);
  INT_ASSERT(isClass(at));
  Type* unmanaged =
    at->getDecoratedClass(ClassTypeDecorator::UNMANAGED); // TODO: nilable
  INT_ASSERT(unmanaged);
  return QualifiedType(unmanaged, QUAL_VAL);
}

static QualifiedType
returnInfoVirtualMethodCall(CallExpr* call) {
  SymExpr* se = toSymExpr(call->get(1));
  INT_ASSERT(se);
  FnSymbol* fn = toFnSymbol(se->symbol());
  INT_ASSERT(fn);
  return fn->getReturnQualType();
}

static QualifiedType
returnInfoCoerce(CallExpr* call) {
  Type* t = call->get(2)->getValType();

  if (t->symbol->hasFlag(FLAG_GENERIC)) {
    // Try to figure out what instantiation type we would use
    // and return that type.
    SymExpr* actualOne = toSymExpr(call->get(1));
    SymExpr* actualTwo = toSymExpr(call->get(2));
    t = getInstantiationType(actualOne->getValType(), actualOne->symbol(),
                             t, actualTwo->symbol(), call);
    if (t == nullptr)
      USR_FATAL(call, "could not coerce a value of type '%s' to type '%s'",
        actualOne->getValType()->symbol->name, actualTwo->symbol()->name);
  }

  return QualifiedType(t, QUAL_VAL);
}

static QualifiedType
returnInfoIteratorRecordFieldValueByFormal(CallExpr* call) {
  QualifiedType t = call->get(2)->qualType();
  return t;
}

static QualifiedType
returnInfoToUnmanaged(CallExpr* call) {
  Type* t = call->get(1)->getValType();

  ClassTypeDecoratorEnum decorator = ClassTypeDecorator::UNMANAGED;
  if (isNilableClassType(t))
    decorator = ClassTypeDecorator::UNMANAGED_NILABLE;
  else if (isNonNilableClassType(t))
    decorator = ClassTypeDecorator::UNMANAGED_NONNIL;

  if (AggregateType* at = toAggregateType(canonicalClassType(t))) {
    if (isClass(at)) {
      t = at->getDecoratedClass(decorator);
    }
  }
  return QualifiedType(t, QUAL_VAL);
}

static QualifiedType returnInfoSecondActualTypeSymbol(CallExpr* call) {
  auto ret = QualifiedType(dtUnknown, QUAL_VAL);

  if (call->numActuals() >= 2) {
    if (SymExpr* se = toSymExpr(call->get(2))) {
      if (auto ts = toTypeSymbol(se->symbol())) {
        Type* t = ts->type;
        ret = QualifiedType(t, QUAL_VAL);
      }
    }
  }

  return ret;
}

static QualifiedType
returnInfoToBorrowed(CallExpr* call) {
  Type* t = call->get(1)->getValType();

  ClassTypeDecoratorEnum decorator = ClassTypeDecorator::BORROWED;
  if (isNilableClassType(t))
    decorator = ClassTypeDecorator::BORROWED_NILABLE;
  else if (isNonNilableClassType(t))
    decorator = ClassTypeDecorator::BORROWED_NONNIL;

  if (AggregateType* at = toAggregateType(canonicalClassType(t)))
    if (isClass(at))
      t = at->getDecoratedClass(decorator);

  return QualifiedType(t, QUAL_VAL);
}

static QualifiedType
returnInfoToUndecorated(CallExpr* call) {
  Type* t = call->get(1)->getValType();

  ClassTypeDecoratorEnum decorator = ClassTypeDecorator::GENERIC;
  if (isNilableClassType(t))
    decorator = ClassTypeDecorator::GENERIC_NILABLE;
  else if (isNonNilableClassType(t))
    decorator = ClassTypeDecorator::GENERIC_NONNIL;

  if (AggregateType* at = toAggregateType(canonicalClassType(t)))
    if (isClass(at))
      t = at->getDecoratedClass(decorator);

  return QualifiedType(t, QUAL_VAL);
}


static QualifiedType
returnInfoToNilable(CallExpr* call) {
  Type* t = call->get(1)->getValType();

  if (isClassLikeOrManaged(t)) {
    ClassTypeDecoratorEnum decorator = classTypeDecorator(t);
    decorator = addNilableToDecorator(decorator);

    if (isManagedPtrType(t)) {
      AggregateType* manager = getManagedPtrManagerType(t);
      AggregateType* at = toAggregateType(canonicalClassType(t));
      if (at == NULL) {
        // e.g. _to_nonnil(owned)
        t = getDecoratedClass(manager, decorator);
      } else {
        // e.g. _to_nonnil(owned C)
        t = computeDecoratedManagedType(at, decorator, manager, call);
      }
    } else {
      if (AggregateType* at = toAggregateType(canonicalClassType(t)))
        if (isClass(at))
          t = at->getDecoratedClass(decorator);
    }
  }

  return QualifiedType(t, QUAL_VAL);
}

static QualifiedType
returnInfoToNonNilable(CallExpr* call) {
  Type* t = call->get(1)->getValType();

  if (isClassLikeOrManaged(t)) {
    ClassTypeDecoratorEnum decorator = classTypeDecorator(t);
    decorator = addNonNilToDecorator(decorator);

    if (isManagedPtrType(t)) {
      AggregateType* manager = getManagedPtrManagerType(t);
      AggregateType* at = toAggregateType(canonicalClassType(t));
      if (at == NULL) {
        // e.g. _to_nonnil(owned)
        t = getDecoratedClass(manager, decorator);
      } else {
        // e.g. _to_nonnil(owned C)
        t = computeDecoratedManagedType(at, decorator, manager, call);
      }
    } else {
      if (AggregateType* at = toAggregateType(canonicalClassType(t)))
        if (isClass(at))
          t = at->getDecoratedClass(decorator);
    }
  }

  return QualifiedType(t, QUAL_VAL);
}

static QualifiedType
returnInfoRuntimeTypeField(CallExpr* call) {
  bool isType = false;
  Type* t = getPrimGetRuntimeTypeFieldReturnType(call, isType);
  return QualifiedType(t, QUAL_VAL);
}



// print the number of each type of primitive present in the AST
void printPrimitiveCounts(const char* passName) {
  int primCounts[NUM_KNOWN_PRIMS];
  for(int i=0; i<NUM_KNOWN_PRIMS; i++) {
    primCounts[i] = 0;
  }

  forv_Vec(CallExpr, call, gCallExprs) {
    if (call->baseExpr == NULL) {
      if (call->primitive) {
        primCounts[call->primitive->tag] += 1;
      }
    }
  }

  printf("NUM_KNOWN_PRIMS = %d\n", NUM_KNOWN_PRIMS);
  for(int i=1; i<NUM_KNOWN_PRIMS; i++) {
    if (primitives[i])
      printf("%s prim[%d] %s %d\n", passName, i, primitives[i]->name, primCounts[i]);
    else
      printf("%s prim[%d] *** UNKNOWN *** %d\n", passName, i, primCounts[i]);
  }

}

HashMap<const char *, StringHashFns, PrimitiveOp *> primitives_map;

PrimitiveOp* primitives[NUM_KNOWN_PRIMS];

PrimitiveOp::PrimitiveOp(PrimitiveTag atag,
                         const char *aname,
                         QualifiedType (*areturnInfo)(CallExpr*)) :
  tag(atag),
  name(aname),
  returnInfo(areturnInfo),
  isEssential(false),
  passLineno(false)
{
  primitives_map.put(name, this);
}

/* Primitive names appear both in prim-ops-list.h as well as
   here. This routine checks that the name matches in both.
 */
static void checkPrimName(PrimitiveTag tag, const char* name) {
  // Check name matches the string in prim-ops-list.h
  switch (tag) {
#define PRIMITIVE(macroTag, macroName) \
    case PRIM_ ## macroTag: \
      INT_ASSERT(0 == strcmp(name, macroName)); \
      break;

#define PRIMITIVE_R(macroTag, macroName) PRIMITIVE(macroTag, macroName)
#define PRIMITIVE_G(macroTag, macroName) PRIMITIVE(macroTag, macroName)

#include "chpl/uast/prim-ops-list.h"
#undef PRIMITIVE
#undef PRIMITIVE_R
#undef PRIMITIVE_G

    default:
      INT_FATAL("case not handled");
  }
}


static void
prim_def(PrimitiveTag tag, const char* name, QualifiedType (*returnInfo)(CallExpr*),
         bool isEssential = false, bool passLineno = false) {
  checkPrimName(tag, name);
  primitives[tag] = new PrimitiveOp(tag, name, returnInfo);
  primitives[tag]->isEssential = isEssential;
  primitives[tag]->passLineno = passLineno;
}

/*
 * The routine below, using the routines just above, define primitives
 * for use by the compiler.  Each primitive definition takes:
 *
 * - (optionally) the primitive's enum
 * - its string name
 * - a function pointer indicating the type it returns/evaluates to
 * - (optionally) a boolean saying whether or not it's essential (i.e.,
 *   should not be dead code eliminated)
 * - (optionally) a boolean saying whether or not it expects to receive
 *   filename/lineno arguments
 *
 * Primitives may be defined by the compiler directly (e.g., rewritten
 * during function resolution) or implemented in the runtime.  In the
 * latter case, not surprisingly, it is important to make sure that
 * the return type of the primitive as specified here matches that of
 * the runtime function implementing the primitive.
 */

void
initPrimitive() {
  // use for any primitives not in this list
  primitives[PRIM_UNKNOWN] = NULL;

  prim_def(PRIM_INNERMOST_CONTEXT, "innermost context", returnInfoFirstAsValue);
  prim_def(PRIM_OUTER_CONTEXT, "outer context", returnInfoFirst);
  prim_def(PRIM_HOIST_TO_CONTEXT, "hoist to context", returnInfoVoid);

  prim_def(PRIM_CREATE_THUNK, "create thunk", returnInfoUnknown);
  prim_def(PRIM_THUNK_RESULT, "thunk result", returnInfoFirst);
  prim_def(PRIM_FORCE_THUNK, "force thunk", returnInfoUnknown);
  prim_def(PRIM_THUNK_RESULT_TYPE, "thunk result type", returnInfoThunkResultType);

  prim_def(PRIM_ACTUALS_LIST, "actuals list", returnInfoVoid);
  prim_def(PRIM_NOOP, "noop", returnInfoVoid);
  // dst, src. PRIM_MOVE can set a reference.
  prim_def(PRIM_MOVE, "move", returnInfoVoid, false);

  // dst, type to default-init
  prim_def(PRIM_DEFAULT_INIT_VAR, "default init var", returnInfoVoid);
  // dst, type to noinit-init
  prim_def(PRIM_NOINIT_INIT_VAR, "noinit init var", returnInfoVoid);

  // fn->_this, the name of the field, value/type, optional declared type
  prim_def(PRIM_INIT_FIELD, "init field", returnInfoVoid, false);

  // dst, init-expr, optional declared type
  prim_def(PRIM_INIT_VAR,   "init var",   returnInfoVoid);

  // indicates split initialization point of declaration
  // The value may not be used until a later PRIM_INIT_VAR_SPLIT_INIT.
  //
  // dst, optional type
  prim_def(PRIM_INIT_VAR_SPLIT_DECL, "init var split decl", returnInfoVoid, false);

  // indicates split initialization point of initialization
  // dst, init-expr, optional type
  //
  // if the optional type is provided, it should match PRIM_INIT_VAR_SPLIT_DECL.
  prim_def(PRIM_INIT_VAR_SPLIT_INIT, "init var split init",   returnInfoVoid);

  prim_def(PRIM_SPLIT_INIT_UPDATE_TYPE, "split init update type",
           returnInfoVoid);

  prim_def(PRIM_INIT_REF_DECL, "init ref decl", returnInfoVoid);

  // indicates the body of the initializer is now in Phase 2
  prim_def(PRIM_INIT_DONE, "init done", returnInfoVoid);

  prim_def(PRIM_REF_TO_STRING, "ref to string", returnInfoStringC);
  prim_def(PRIM_RETURN, "return", returnInfoFirst, true);
  prim_def(PRIM_THROW, "throw", returnInfoFirst, true, true);
  prim_def(PRIM_TRY_EXPR, "try-expr", returnInfoFirst);
  prim_def(PRIM_TRYBANG_EXPR, "try!-expr", returnInfoFirst);
  prim_def(PRIM_YIELD, "yield", returnInfoFirst, true);

  // Represents a Chapel reduce expression "OP reduce DATA".
  // Args: (OP, DATA, is zippered (gTrue/gFalse)).
  // Lowered during resolution.
  prim_def(PRIM_REDUCE, "reduce", returnInfoVoid, true);

  prim_def(PRIM_UNARY_MINUS, "u-", returnInfoFirstDeref);
  prim_def(PRIM_UNARY_PLUS, "u+", returnInfoFirstDeref);
  prim_def(PRIM_UNARY_NOT, "u~", returnInfoFirstDeref);
  prim_def(PRIM_UNARY_LNOT, "u!", returnInfoBool);
  prim_def(PRIM_SQRT, "sqrt", returnInfoFirst);
  prim_def(PRIM_ABS, "abs", returnInfoAbs);
  prim_def(PRIM_ADD, "+", returnInfoNumericUp);
  prim_def(PRIM_SUBTRACT, "-", returnInfoNumericUp);
  prim_def(PRIM_MULT, "*", returnInfoNumericUp);
  prim_def(PRIM_DIV, "/", returnInfoNumericUp, true); // div by zero is visible
  prim_def(PRIM_MOD, "%", returnInfoFirstDeref); // mod by zero?
  prim_def(PRIM_FMA, "fma", returnInfoFirstDeref);
  prim_def(PRIM_LSH, "<<", returnInfoFirstDeref);
  prim_def(PRIM_RSH, ">>", returnInfoFirstDeref);
  prim_def(PRIM_EQUAL, "==", returnInfoBool);
  prim_def(PRIM_NOTEQUAL, "!=", returnInfoBool);
  prim_def(PRIM_LESSOREQUAL, "<=", returnInfoBool);
  prim_def(PRIM_GREATEROREQUAL, ">=", returnInfoBool);
  prim_def(PRIM_LESS, "<", returnInfoBool);
  prim_def(PRIM_GREATER, ">", returnInfoBool);
  prim_def(PRIM_AND, "&", returnInfoFirstDeref);
  prim_def(PRIM_OR, "|", returnInfoFirstDeref);
  prim_def(PRIM_XOR, "^", returnInfoFirstDeref);
  prim_def(PRIM_POW, "**", returnInfoNumericUp);

  // dst, src. PRIM_ASSIGN with reference dst sets *dst
  prim_def(PRIM_ASSIGN, "=", returnInfoVoid, true);
  // like PRIM_ASSIGN but indicates an elided copy
  // which means that the RHS cannot be used again after this call.
  prim_def(PRIM_ASSIGN_ELIDED_COPY, "elided-copy=", returnInfoVoid, true);
  // like PRIM_ASSIGN but the operation can be put off until end of
  // the enclosing task or forall.
  prim_def(PRIM_UNORDERED_ASSIGN, "unordered=", returnInfoVoid, true, true);

  prim_def(PRIM_ADD_ASSIGN, "+=", returnInfoVoid, true);
  prim_def(PRIM_SUBTRACT_ASSIGN, "-=", returnInfoVoid, true);
  prim_def(PRIM_MULT_ASSIGN, "*=", returnInfoVoid, true);
  prim_def(PRIM_DIV_ASSIGN, "/=", returnInfoVoid, true);
  prim_def(PRIM_MOD_ASSIGN, "%=", returnInfoVoid, true);
  prim_def(PRIM_LSH_ASSIGN, "<<=", returnInfoVoid, true);
  prim_def(PRIM_RSH_ASSIGN, ">>=", returnInfoVoid, true);
  prim_def(PRIM_AND_ASSIGN, "&=", returnInfoVoid, true);
  prim_def(PRIM_OR_ASSIGN, "|=", returnInfoVoid, true);
  prim_def(PRIM_XOR_ASSIGN, "^=", returnInfoVoid, true);
  prim_def(PRIM_LOGICALAND_ASSIGN, "&&=", returnInfoVoid, true);
  prim_def(PRIM_LOGICALOR_ASSIGN, "||=", returnInfoVoid, true);
  prim_def(PRIM_REDUCE_ASSIGN, "reduce=", returnInfoVoid, true);

  prim_def(PRIM_MIN, "_min", returnInfoFirst);
  prim_def(PRIM_MAX, "_max", returnInfoFirst);

  prim_def(PRIM_SETCID, "setcid", returnInfoVoid, true);
  prim_def(PRIM_TESTCID, "testcid", returnInfoBool, false);
  prim_def(PRIM_GETCID, "getcid", returnInfoInt32, false);
  prim_def(PRIM_SET_UNION_ID, "set_union_id", returnInfoVoid, true);
  prim_def(PRIM_GET_UNION_ID, "get_union_id", returnInfoDefaultInt, false);

  // PRIM_GET_MEMBER(_VALUE): aggregate, field
  // if the field is a ref:
  //   GET_MEMBER is invalid AST
  //   GET_MEMBER_VALUE returns the reference
  // if the field is not a ref
  //   GET_MEMBER returns a reference to the field
  //   GET_MEMBER_VALUE returns the field value
  prim_def(PRIM_GET_MEMBER, ".", returnInfoGetMemberRef);
  prim_def(PRIM_GET_MEMBER_VALUE, ".v", returnInfoGetMember, false);

  // PRIM_SET_MEMBER: base, field, value
  // if the field is a ref, and the value is a ref, sets the ptr.
  // if the field is a ref, and the value is a not ref, invalid AST
  // if the field is not ref, and the value is a ref, derefs value first
  // if neither are references, sets the field
  prim_def(PRIM_SET_MEMBER, ".=", returnInfoVoid, true);

  prim_def(PRIM_CHECK_NIL, "_check_nil", returnInfoVoid, true, true);

  // PRIM_IF_VAR: DefExpr of NAME, RHS expr
  // represents the condition in "if var NAME = RHS then ..."
  prim_def(PRIM_IF_VAR, "if var", returnInfoVoid);

  // new keyword
  prim_def(PRIM_NEW, "new", returnInfoFirst);
  prim_def(PRIM_NEW_WITH_ALLOCATOR, "new with allocator", returnInfoSecond);
  // given a complex value, produce a reference to the real component
  prim_def(PRIM_GET_REAL, "complex_get_real", returnInfoComplexField);
  // given a complex value, produce a reference to the imag component
  prim_def(PRIM_GET_IMAG, "complex_get_imag", returnInfoComplexField);
  // query expression primitive
  prim_def(PRIM_QUERY, "query", returnInfoUnknown);
  prim_def(PRIM_QUERY_PARAM_FIELD, "query param field", returnInfoGetMemberRef);
  prim_def(PRIM_QUERY_TYPE_FIELD, "query type field", returnInfoGetMember);

  // get a reference to a value
  prim_def(PRIM_ADDR_OF, "addr of", returnInfoRef);
  // dereference a reference
  prim_def(PRIM_DEREF,   "deref",   returnInfoVal, false);
  // If the argument is a reference, simply return it. Otherwise, return a
  // ref to the arg. The result is always a reference.
  prim_def(PRIM_SET_REFERENCE, "set reference", returnInfoAsRef);

  // local block primitives
  // assert that a wide ref is on this locale
  prim_def(PRIM_LOCAL_CHECK, "local_check", returnInfoVoid, true, true);
  prim_def(PRIM_IS_LOCAL, "is_local", returnInfoBool, true, false);

  // get/set end count for 'begin' -
  // manipulates task-local storage
  prim_def(PRIM_GET_END_COUNT, "get end count", returnInfoEndCount);
  prim_def(PRIM_SET_END_COUNT, "set end count", returnInfoVoid, true);

  prim_def(PRIM_GET_DYNAMIC_END_COUNT, "get dynamic end count", returnInfoEndCount);
  prim_def(PRIM_SET_DYNAMIC_END_COUNT, "set dynamic end count", returnInfoVoid, true);

  // this requires sizes in all 3D to be specified. For 2D launches, 1 can be
  // passed as one or more of these arguments.
  prim_def(PRIM_GPU_KERNEL_LAUNCH, "gpu kernel launch", returnInfoVirtualMethodCall, true);

  // Primitive functions to access thread, block, and grid information:
  //
  // Threads, blocks, and grids are CUDA terminology; the corresponding terms
  // in OpenCL are: workitems, workgroups, and n-dimensional ranges.
  //
  // Threads correspond to a stream of instructions to execute on one core
  // of a GPU. A streaming multiprocessor (SM) may operate on multiple threads
  // simultaneously in a SIMT (single instruction multiple threads) manner.
  //
  // When launching a kernel the user specifies a 3-dimensional block size.
  // Blocks are groups of threads. Grids are groups of blocks and when
  // launching a kernel the user specifies a 3-dimensional grid size.
  //
  // Threads can be indexed as a 3-dimensional location within a block,
  // which itself can be indexed as a 3-dimensional location within a grid.
  prim_def(PRIM_GPU_THREADIDX_X, "gpu threadIdx x", returnInfoInt32, true);
  prim_def(PRIM_GPU_THREADIDX_Y, "gpu threadIdx y", returnInfoInt32, true);
  prim_def(PRIM_GPU_THREADIDX_Z, "gpu threadIdx z", returnInfoInt32, true);
  prim_def(PRIM_GPU_BLOCKIDX_X, "gpu blockIdx x", returnInfoInt32, true);
  prim_def(PRIM_GPU_BLOCKIDX_Y, "gpu blockIdx y", returnInfoInt32, true);
  prim_def(PRIM_GPU_BLOCKIDX_Z, "gpu blockIdx z", returnInfoInt32, true);
  prim_def(PRIM_GPU_BLOCKDIM_X, "gpu blockDim x", returnInfoInt32, true);
  prim_def(PRIM_GPU_BLOCKDIM_Y, "gpu blockDim y", returnInfoInt32, true);
  prim_def(PRIM_GPU_BLOCKDIM_Z, "gpu blockDim z", returnInfoInt32, true);
  prim_def(PRIM_GPU_GRIDDIM_X, "gpu gridDim x", returnInfoInt32, true);
  prim_def(PRIM_GPU_GRIDDIM_Y, "gpu gridDim y", returnInfoInt32, true);
  prim_def(PRIM_GPU_GRIDDIM_Z, "gpu gridDim z", returnInfoInt32, true);

  prim_def(PRIM_GPU_INIT_KERNEL_CFG, "gpu init kernel cfg", returnInfoCVoidPtr, true, true);
  prim_def(PRIM_GPU_INIT_KERNEL_CFG_3D, "gpu init kernel cfg 3d", returnInfoCVoidPtr, true, true);
  prim_def(PRIM_GPU_DEINIT_KERNEL_CFG, "gpu deinit kernel cfg", returnInfoVoid, true);
  prim_def(PRIM_GPU_ARG, "gpu arg", returnInfoVoid, true);
  prim_def(PRIM_GPU_PID_OFFLOAD, "gpu pid offload", returnInfoVoid, true);
  prim_def(PRIM_GPU_BLOCK_REDUCE, "gpu block reduce", returnInfoVoid, true);

  // Marker for a scopeless block that denotes the "scope" of code to which
  // GPU attributes are being applied. The block contains the primitives as the
  // last child, and every promoted expression and foreach loop within this
  // "scope" should have these primitives inserted.
  //
  // The marker prevents the block from being flattened by other passes.
  prim_def(PRIM_GPU_ATTRIBUTE_BLOCK, "gpu attribute block", returnInfoVoid, true);

  // Marker for a scopeless block that contains the primitives generated by
  // GPU attributes.
  //
  // The marker prevents the block from being flattened by other passes.
  prim_def(PRIM_GPU_PRIMITIVE_BLOCK, "gpu primitive block", returnInfoVoid, true);

  // allocate data into shared memory (takes one parameter: number of bytes to allocate)
  // and returns a raw_c_void_ptr
  prim_def(PRIM_GPU_ALLOC_SHARED, "gpu allocShared", returnInfoCVoidPtr, true);

  // synchronize threads in a GPU kernel (equivalent to CUDA __syncThreads)
  prim_def(PRIM_GPU_SYNC_THREADS, "gpu syncThreads", returnInfoVoid, true);

  // If embedded inside a gpuizable loop will set the blocksize when the kernel
  // for that loop launches.
  prim_def(PRIM_GPU_SET_BLOCKSIZE, "gpu set blockSize", returnInfoVoid, true);

  // If embedded inside a gpuizable loop, perform this many loop iterations
  // within a single thread of the kernel.
  prim_def(PRIM_GPU_SET_ITERS_PER_THREAD, "gpu set itersPerThread", returnInfoVoid, true);

  // Generates call that produces runtime error when not run by a GPU
  prim_def(PRIM_ASSERT_ON_GPU, "chpl_assert_on_gpu", returnInfoVoid, true, true);
  prim_def(PRIM_ASSERT_GPU_ELIGIBLE, "assert gpu eligible", returnInfoVoid, true, true);
  prim_def(PRIM_GPU_ELIGIBLE, "gpu eligible", returnInfoVoid, true, true);
  prim_def(PRIM_GPU_REDUCE_WRAPPER, "gpu reduce wrapper", returnInfoVoid, true);

  // task primitives
  // get serial state
  prim_def(PRIM_GET_SERIAL, "task_get_serial", returnInfoBool);
  // set serial state to true or false
  prim_def(PRIM_SET_SERIAL, "task_set_serial", returnInfoVoid, true);

  // These are used for task bundles and for allocating class instances.
  prim_def(PRIM_SIZEOF_BUNDLE, "sizeof_bundle", returnInfoSizeType);

  // sizeof(_ddata.eltType)
  prim_def(PRIM_SIZEOF_DDATA_ELEMENT, "sizeof_ddata_element", returnInfoSizeType);

  // initialize fields of a temporary record
  prim_def(PRIM_INIT_FIELDS, "chpl_init_record", returnInfoVoid, true);
  prim_def(PRIM_PTR_EQUAL, "ptr_eq", returnInfoBool);
  prim_def(PRIM_PTR_NOTEQUAL, "ptr_neq", returnInfoBool);
  // PRIM_IS_SUBTYPE arguments are (parent, sub) and it checks
  // if sub is a sub-type of parent.
  prim_def(PRIM_IS_SUBTYPE, "is_subtype", returnInfoBool);
  // for arguments (generic, sub), checks if sub is an instantiation of generic
  // sub can be a value as a convenience for normalization of type queries
  prim_def(PRIM_IS_INSTANTIATION_ALLOW_VALUES, "is_instantiation_allow_values", returnInfoBool);
  // same as above but excludes same type
  prim_def(PRIM_IS_PROPER_SUBTYPE, "is_proper_subtype", returnInfoBool);
  // accepts two arguments: A class/record type expression and a param string for the field name
  prim_def(PRIM_IS_BOUND, "is bound", returnInfoBool);
  // PRIM_IS_COERCIBLE arguments are (source type, target type)
  prim_def(PRIM_IS_COERCIBLE, "is_coercible", returnInfoBool);
  // PRIM_CAST arguments are (type to cast to, value to cast)
  prim_def(PRIM_CAST, "cast", returnInfoCast, false, true);
  prim_def(PRIM_DYNAMIC_CAST, "dynamic_cast", returnInfoCast, false);

  // PRIM_LIFETIME_OF represents a query of a lifetime to inform the lifetime
  // checker.
  prim_def(PRIM_LIFETIME_OF, "lifetime_of", returnInfoInt64);

  // PRIM_TYPEOF of an array returns a runtime type (containing its domain)
  // For values without a runtime type component, it works the same as
  // PRIM_STATIC_TYPEOF
  prim_def(PRIM_TYPEOF, "typeof", returnInfoFirstDeref);


  // Return the compile-time component of a type (ignoring runtime types)
  // For an array, returns the compile-time type only.
  // There might be uninitialized memory if the run-time type is used.
  prim_def(PRIM_STATIC_TYPEOF, "static typeof", returnInfoFirstDeref);

  prim_def(PRIM_STATIC_FUNCTION_VAR, "static function var", returnInfoFirst);
  prim_def(PRIM_STATIC_FUNCTION_VAR_VALIDATE_TYPE, "static function validate type", returnInfoFirstDerefNoRtti);
  prim_def(PRIM_STATIC_FUNCTION_VAR_WRAPPER, "static function var wrapper", returnInfoVoid);


  // As with PRIM_STATIC_TYPEOF, returns a compile-time component of
  // a type only. Returns the scalar promotion type (i.e. the type of the
  // elements that iterating over it would yield)
  // There might be uninitialized memory if the run-time type is used.
  prim_def(PRIM_SCALAR_PROMOTION_TYPE, "scalar promotion type", returnInfoScalarPromotionType);

  // As with PRIM_STATIC_TYPEOF, returns a compile-time component of
  // the type of the field.
  // There might be uninitialized memory if the run-time type is used.
  // Args are variable, field name.
  prim_def(PRIM_STATIC_FIELD_TYPE, "static field type", returnInfoStaticFieldType);

  // used modules in BlockStmt::useList
  prim_def(PRIM_USED_MODULES_LIST, "used modules list", returnInfoVoid);
  // modules named explicitly in BlockStmt::modRefs
  prim_def(PRIM_REFERENCED_MODULES_LIST, "referenced modules list",
           returnInfoVoid);
  prim_def(PRIM_TUPLE_EXPAND, "expand_tuple", returnInfoVoid);

  // Direct calls to the Chapel comm layer
  prim_def(PRIM_CHPL_COMM_GET, "chpl_comm_get", returnInfoVoid, true, true);
  prim_def(PRIM_CHPL_COMM_PUT, "chpl_comm_put", returnInfoVoid, true, true);
  prim_def(PRIM_CHPL_COMM_ARRAY_GET, "chpl_comm_array_get", returnInfoVoid, true, true);
  prim_def(PRIM_CHPL_COMM_ARRAY_PUT, "chpl_comm_array_put", returnInfoVoid, true, true);
  prim_def(PRIM_CHPL_COMM_REMOTE_PREFETCH, "chpl_comm_remote_prefetch", returnInfoVoid, true, true);
  // Direct calls to the Chapel comm layer for strided comm
  prim_def(PRIM_CHPL_COMM_GET_STRD, "chpl_comm_get_strd", returnInfoVoid, true, true);
  prim_def(PRIM_CHPL_COMM_PUT_STRD, "chpl_comm_put_strd", returnInfoVoid, true, true);

  prim_def(PRIM_ARRAY_SHIFT_BASE_POINTER, "shift_base_pointer", returnInfoVoid, true);

  // PRIM_ARRAY_GET arguments
  //  base pointer
  //  index
  //  no alias set
  // This is similar to A[i] in C
  prim_def(PRIM_ARRAY_GET, "array_get", returnInfoArrayIndex, false);
  // PRIM_ARRAY_SET is unused by compiler, runtime, modules
  // PRIM_ARRAY_SET / PRIM_ARRAY_SET_FIRST have these arguments
  //   base pointer
  //   index
  //   value
  prim_def(PRIM_ARRAY_SET, "array_set", returnInfoVoid, true);
  prim_def(PRIM_ARRAY_SET_FIRST, "array_set_first", returnInfoVoid, true);

  prim_def(PRIM_MAYBE_LOCAL_THIS, "may be local access", returnInfoUnknown);
  prim_def(PRIM_MAYBE_LOCAL_ARR_ELEM, "may be local array element", returnInfoUnknown);
  prim_def(PRIM_MAYBE_AGGREGATE_ASSIGN, "may be aggregated assignment", returnInfoUnknown);

  prim_def(PRIM_PROTO_SLICE_ASSIGN, "assign proto slices", returnInfoVoid);

  prim_def(PRIM_ERROR, "error", returnInfoVoid, true);
  prim_def(PRIM_WARNING, "warning", returnInfoVoid, true);
  prim_def(PRIM_WHEN, "when case expressions", returnInfoVoid);
  prim_def(PRIM_TYPE_TO_STRING, "typeToString", returnInfoString);

  // These are the block info primitives.
  // BlockStmt::blockInfo - param for loop (index, low, high, stride)
  prim_def(PRIM_BLOCK_PARAM_LOOP, "param loop", returnInfoVoid);
  // BlockStmt::blockInfo - while do loop (cond)
  prim_def(PRIM_BLOCK_WHILEDO_LOOP, "while...do loop", returnInfoVoid);
  // BlockStmt::blockInfo - do while loop (cond)
  prim_def(PRIM_BLOCK_DOWHILE_LOOP, "do...while loop", returnInfoVoid);
  // BlockStmt::blockInfo - for loop (index, iterator)
  prim_def(PRIM_BLOCK_FOR_LOOP, "for loop", returnInfoVoid);
  // BlockStmt::blockInfo - C for loop (initExpr, testExpr, incrExpr)
  prim_def(PRIM_BLOCK_C_FOR_LOOP, "C for loop", returnInfoVoid);
  // BlockStmt::blockInfo - begin block
  prim_def(PRIM_BLOCK_BEGIN, "begin block", returnInfoVoid);
  // BlockStmt::blockInfo - cobegin block
  prim_def(PRIM_BLOCK_COBEGIN, "cobegin block", returnInfoVoid);
  // BlockStmt::blockInfo - coforall block
  prim_def(PRIM_BLOCK_COFORALL, "coforall loop", returnInfoVoid);
  // BlockStmt::blockInfo - on block
  prim_def(PRIM_BLOCK_ON, "on block", returnInfoVoid);
  // BlockStmt::blockInfo - elided on block
  //   (i.e. an on block that not needed for single-locale compilation)
  prim_def(PRIM_BLOCK_ELIDED_ON, "elided on block", returnInfoVoid);
  // BlockStmt::blockInfo - begin on block
  prim_def(PRIM_BLOCK_BEGIN_ON, "begin on block", returnInfoVoid);
  // BlockStmt::blockInfo - cobegin on block
  prim_def(PRIM_BLOCK_COBEGIN_ON, "cobegin on block", returnInfoVoid);
  // BlockStmt::blockInfo - coforall on block
  prim_def(PRIM_BLOCK_COFORALL_ON, "coforall on block", returnInfoVoid);
  // BlockStmt::blockInfo - local block
  prim_def(PRIM_BLOCK_LOCAL, "local block", returnInfoVoid);
  // BlockStmt::blockInfo - unlocal local block
  prim_def(PRIM_BLOCK_UNLOCAL, "unlocal block", returnInfoVoid);

  // The arg is an iterator record (or iterator class) temp or iterator call.
  // Indicates whether the iterator has the corresponding leader iterator.
  prim_def(PRIM_HAS_LEADER, "has leader", returnInfoBool);

  prim_def(PRIM_TO_LEADER, "to leader", returnInfoVoid);
  prim_def(PRIM_TO_FOLLOWER, "to follower", returnInfoVoid);
  prim_def(PRIM_TO_STANDALONE, "to standalone", returnInfoVoid);

  // call destructor on type (do not free)
  prim_def(PRIM_CALL_DESTRUCTOR, "call destructor", returnInfoVoid, true);

  // Help fold logical && and ||
  prim_def(PRIM_LOGICAL_FOLDER, "_paramFoldLogical", returnInfoBool);

  // create a wide pointer from (type, localeID, addr)
  prim_def(PRIM_WIDE_MAKE, "_wide_make", returnInfoWideMake, false);
  // Returns the "locale" portion of a wide pointer.
  prim_def(PRIM_WIDE_GET_LOCALE, "_wide_get_locale", returnInfoLocaleID, false);
  // MPF - 10/9/2015 - neither _wide_get_node nor _wide_get_addr
  // is used in the module or test code. insertWideReferences uses
  // PRIM_WIDE_GET_NODE. It might make sense to keep both of these
  // functions for debugging.
  // Get just the node portion of a wide pointer.
  prim_def(PRIM_WIDE_GET_NODE, "_wide_get_node", returnInfoNodeID, false);
  // Get just the address portion of a wide pointer.
  prim_def(PRIM_WIDE_GET_ADDR, "_wide_get_addr", returnInfoCVoidPtr, false);
  // Returns true if the symbol is represented by a wide pointer.
  prim_def(PRIM_IS_WIDE_PTR, "is wide pointer", returnInfoBool);

  // specify a particular localeID for an on clause.
  prim_def(PRIM_ON_LOCALE_NUM, "chpl_on_locale_num", returnInfoLocaleID);

  // call the 'chpl_task_getRequestedSubloc' runtime function
  prim_def(PRIM_GET_REQUESTED_SUBLOC, "chpl_task_getRequestedSubloc", returnInfoInt64);

  prim_def(PRIM_REGISTER_GLOBAL_VAR, "_register_global_var", returnInfoVoid, true, true);
  prim_def(PRIM_BROADCAST_GLOBAL_VARS, "_broadcast_global_vars", returnInfoVoid, true, true);
  // ('_private_broadcast' sym)
  // Later, a structure index is inserted ahead
  // of the symbol, so it ends up as
  // ('_private_broadcast' index sym).
  prim_def(PRIM_PRIVATE_BROADCAST, "_private_broadcast", returnInfoVoid, true);

  prim_def(PRIM_INT_ERROR, "_internal_error", returnInfoVoid, true);

  prim_def(PRIM_CAPTURE_FN, "capture fn", returnInfoVoid);
  prim_def(PRIM_CAPTURE_FN_TO_CLASS, "capture fn to class", returnInfoVoid);
  prim_def(PRIM_CREATE_FN_TYPE, "create fn type", returnInfoVoid);

  prim_def(PRIM_STRING_COMPARE, "string_compare", returnInfoDefaultInt, true);
  prim_def(PRIM_STRING_CONTAINS, "string_contains", returnInfoBool, true);
  prim_def(PRIM_STRING_CONCAT, "string_concat", returnInfoStringC, true, true);
  prim_def(PRIM_STRING_LENGTH_BYTES, "string_length_bytes", returnInfoDefaultInt);
  prim_def(PRIM_STRING_LENGTH_CODEPOINTS, "string_length_codepoints", returnInfoDefaultInt);
  prim_def(PRIM_ASCII, "ascii", returnInfoUInt8);
  prim_def(PRIM_STRING_ITEM, "string item", returnInfoString);
  prim_def(PRIM_BYTES_ITEM, "bytes item", returnInfoBytes);
  prim_def(PRIM_STRING_INDEX, "string_index", returnInfoStringC, true, true);
  prim_def(PRIM_STRING_COPY, "string_copy", returnInfoStringC, false, true);
  // Cast the object argument to void*.
  prim_def(PRIM_CAST_TO_VOID_STAR, "cast_to_void_star", returnInfoCVoidPtr, true, false);
  // Cast to the second argument. The conversion is done at codegen time.
  // In most cases the cast is effectively a bitcast/'reinterpret_cast'.
  prim_def(PRIM_CAST_TO_TYPE, "cast_to_type", returnInfoSecondActualTypeSymbol, true, false);
  prim_def(PRIM_STRING_SELECT, "string_select", returnInfoStringC, true, true);
  prim_def(PRIM_SLEEP, "sleep", returnInfoVoid, true);
  prim_def(PRIM_REAL_TO_INT, "real2int", returnInfoDefaultInt);
  prim_def(PRIM_OBJECT_TO_INT, "object2int", returnInfoDefaultInt);
  prim_def(PRIM_CHPL_EXIT_ANY, "chpl_exit_any", returnInfoVoid, true);

  prim_def(PRIM_RT_ERROR, "chpl_error", returnInfoVoid, true, true);
  prim_def(PRIM_RT_WARNING, "chpl_warning", returnInfoVoid, true, true);
  prim_def(PRIM_RT_GPU_HALT, "chpl_gpu_halt", returnInfoVoid, true, true);

  prim_def(PRIM_NEW_PRIV_CLASS, "chpl_newPrivatizedClass", returnInfoVoid);

  prim_def(PRIM_GET_USER_LINE, "_get_user_line", returnInfoDefaultInt, true, true);
  prim_def(PRIM_GET_USER_FILE, "_get_user_file", returnInfoInt32, true, true);

  // A marker that goes into a module init function. Indicates that
  // its argument symbol, defined directly in the module scope,
  // needs to be resolved at this point during resolution of the init function.
  prim_def(PRIM_RESOLUTION_POINT, "resolution point", returnInfoVoid, false);

  prim_def(PRIM_FTABLE_CALL, "call ftable function", returnInfoVoid, true);

  prim_def(PRIM_IS_TUPLE_TYPE, "is tuple type", returnInfoBool);
  prim_def(PRIM_IS_STAR_TUPLE_TYPE, "is star tuple type", returnInfoBool);
  // base, index, value
  prim_def(PRIM_SET_SVEC_MEMBER, "set svec member", returnInfoVoid, true);
  prim_def(PRIM_GET_SVEC_MEMBER, "get svec member", returnInfoGetTupleMemberRef);
  prim_def(PRIM_GET_SVEC_MEMBER_VALUE, "get svec member value", returnInfoGetTupleMember, false);

  prim_def(PRIM_VIRTUAL_METHOD_CALL, "virtual method call", returnInfoVirtualMethodCall, true);

  prim_def(PRIM_SIMPLE_TYPE_NAME, "simple type name", returnInfoString);
  prim_def(PRIM_NUM_FIELDS, "num fields", returnInfoInt32);
  prim_def(PRIM_FIELD_NUM_TO_NAME, "field num to name", returnInfoString);
  prim_def(PRIM_FIELD_NAME_TO_NUM, "field name to num", returnInfoInt32);
  prim_def(PRIM_FIELD_BY_NUM, "field by num", returnInfoUnknown);
  prim_def(PRIM_CLASS_NAME_BY_ID, "class name by id", returnInfoStringC);

  prim_def(PRIM_ITERATOR_RECORD_FIELD_VALUE_BY_FORMAL, "iterator record field value by formal", returnInfoIteratorRecordFieldValueByFormal);
  prim_def(PRIM_ITERATOR_RECORD_SET_SHAPE, "iterator record set shape", returnInfoVoid);
  prim_def(PRIM_IS_GENERIC_TYPE, "is generic type", returnInfoBool);
  prim_def(PRIM_IS_CLASS_TYPE, "is class type", returnInfoBool);
  prim_def(PRIM_IS_NILABLE_CLASS_TYPE, "is nilable class type", returnInfoBool);
  prim_def(PRIM_IS_NON_NILABLE_CLASS_TYPE, "is non nilable class type", returnInfoBool);
  prim_def(PRIM_IS_RECORD_TYPE, "is record type", returnInfoBool);
  prim_def(PRIM_IS_PROC_TYPE, "is proc type", returnInfoBool);
  prim_def(PRIM_TO_PROC_TYPE, "to proc type", returnInfoForProcTypeConverter);
  prim_def(PRIM_IS_UNION_TYPE, "is union type", returnInfoBool);
  prim_def(PRIM_IS_EXTERN_UNION_TYPE, "is extern union type", returnInfoBool);
  prim_def(PRIM_IS_ATOMIC_TYPE, "is atomic type", returnInfoBool);
  prim_def(PRIM_IS_REF_ITER_TYPE, "is ref iter type", returnInfoBool);
  prim_def(PRIM_IS_EXTERN_TYPE, "is extern type", returnInfoBool);
  prim_def(PRIM_IS_BORROWED_CLASS_TYPE, "is borrowed class type", returnInfoBool);
  prim_def(PRIM_IS_ABS_ENUM_TYPE, "is abstract enum type", returnInfoBool);

  prim_def(PRIM_IS_POD, "is pod type", returnInfoBool);
  prim_def(PRIM_IS_COPYABLE, "is copyable type", returnInfoBool);
  prim_def(PRIM_IS_CONST_COPYABLE, "is const copyable type", returnInfoBool);
  prim_def(PRIM_IS_ASSIGNABLE, "is assignable type", returnInfoBool);
  prim_def(PRIM_IS_CONST_ASSIGNABLE, "is const assignable type", returnInfoBool);
  prim_def(PRIM_HAS_DEFAULT_VALUE, "type has default value", returnInfoBool);

  // This primitive allows normalize to request function resolution
  // coerce a return value to the declared return type, even though
  // the declared return type is not really known until function
  // resolution.
  // It coerces its first argument to the type stored in the second argument.
  prim_def(PRIM_COERCE, "coerce", returnInfoCoerce);

  // Arguments to these are the actual arguments to try resolving.
  // May or may not end up resolving the called fn.
  prim_def(PRIM_CALL_RESOLVES, "call resolves", returnInfoBool);
  prim_def(PRIM_METHOD_CALL_RESOLVES, "method call resolves", returnInfoBool);
  // Like the previous two but also always attempts to resolve the called fn
  prim_def(PRIM_CALL_AND_FN_RESOLVES, "call and fn resolves", returnInfoBool);
  prim_def(PRIM_METHOD_CALL_AND_FN_RESOLVES, "method call and fn resolves", returnInfoBool);
  prim_def(PRIM_RESOLVES, "resolves", returnInfoBool);

  prim_def(PRIM_IMPLEMENTS_INTERFACE, "implements interface", returnInfoInt64);

  prim_def(PRIM_START_RMEM_FENCE, "chpl_rmem_consist_acquire", returnInfoVoid, true, true);
  prim_def(PRIM_FINISH_RMEM_FENCE, "chpl_rmem_consist_release", returnInfoVoid, true, true);

  // Given an index, get a given filename (c_string)
  prim_def(PRIM_LOOKUP_FILENAME, "chpl_lookupFilename", returnInfoStringC, false, false);

  prim_def(PRIM_GET_COMPILER_VAR, "get compiler variable", returnInfoString);
  prim_def(PRIM_GET_VISIBLE_SYMBOLS, "get visible symbols", returnInfoVoid);

  // Allocate a class instance on the stack (where normally it
  // would be allocated on the heap). The only argument is the class type.
  prim_def(PRIM_STACK_ALLOCATE_CLASS, "stack allocate class", returnInfoFirst);

  // zero the memory a variable points to. only argument is the variable
  prim_def(PRIM_ZERO_VARIABLE, "zero variable", returnInfoVoid, true);

  prim_def(PRIM_ZIP, "zip", returnInfoVoid, false, false);
  prim_def(PRIM_REQUIRE, "require", returnInfoVoid, false, false);

  // used in error-handling conditional. args: error variable
  prim_def(PRIM_CHECK_ERROR, "check error", returnInfoVoid, false, false);
  // used before error handling is lowered to represent the current error
  prim_def(PRIM_CURRENT_ERROR, "current error", returnInfoError, false, false);

  // return the unmanaged class variant, error if decorated
  prim_def(PRIM_TO_UNMANAGED_CLASS_CHECKED, "to unmanaged class from unknown", returnInfoToUnmanaged, false, false);
  // return the borrowed class variant, error if decorated
  prim_def(PRIM_TO_BORROWED_CLASS_CHECKED, "to borrowed class from unknown", returnInfoToBorrowed, false, false);
  // return the nilable class variant, error if argument is not a type
  prim_def(PRIM_TO_NILABLE_CLASS_CHECKED, "to nilable class from type", returnInfoToNilable, false, false);

  // These return the (non-nil) class variant
  prim_def(PRIM_TO_UNMANAGED_CLASS, "to unmanaged class", returnInfoToUnmanaged, false, false);
  // borrowed class type currently == canonical class type
  prim_def(PRIM_TO_BORROWED_CLASS, "to borrowed class", returnInfoToBorrowed, false, false);
  // return the undecorated class type
  prim_def(PRIM_TO_UNDECORATED_CLASS, "to undecorated class", returnInfoToUndecorated, false, false);
  // Returns the nilable class type
  prim_def(PRIM_TO_NILABLE_CLASS, "to nilable class", returnInfoToNilable, false, false);
  prim_def(PRIM_TO_NON_NILABLE_CLASS, "to non nilable class", returnInfoToNonNilable, false, false);

  prim_def(PRIM_SET_ALIASING_ARRAY_ON_TYPE, "set aliasing array on type", returnInfoVoid, false, false);

  prim_def(PRIM_NEEDS_AUTO_DESTROY, "needs auto destroy", returnInfoBool, false, false);

  // if the argument is a value, mark it with "no auto destroy"
  // (no effect on a reference)
  prim_def(PRIM_STEAL, "steal", returnInfoFirst, false, false);

  // Indicates the end of a statement. This is important for the
  // deinitialization location for some variables.
  // Any arguments are SymExprs to user variables that should be alive until
  // after this primitive.
  prim_def(PRIM_END_OF_STATEMENT, "end of statement", returnInfoVoid, false, false);

  prim_def(PRIM_AUTO_DESTROY_RUNTIME_TYPE, "auto destroy runtime type", returnInfoVoid, false, false);

  // Accepts 2 arguments:
  // 1) type variable that will become the _RuntimeTypeInfo
  // 2) field symbol or param-string name of the field in the _RuntimeTypeInfo
  prim_def(PRIM_GET_RUNTIME_TYPE_FIELD, "get runtime type field", returnInfoRuntimeTypeField, false, false);

  // Corresponds to LLVM's invariant start
  // takes in a pointer/reference argument that is the invariant thing
  prim_def(PRIM_INVARIANT_START, "invariant start", returnInfoVoid, false, false);

  // variable number of arguments
  // 1st argument is a SymExpr referring to a Symbol indicating which
  //  alias set this is (e.g. the owning array).
  // Remaining arguments are SymExprs referring to Symbols that this one
  // does not alias.
  // Translates to llvm alias.scope and noalias metadata:
  //    - alias.scope with metadata corresponding to the 1st symbol
  //    - noalias with metadata corresponding to the remaining symbols
  prim_def(PRIM_NO_ALIAS_SET, "no alias set", returnInfoUnknown, false, false);
  // 1st argument is symbol to set alias set
  // 2nd argument is symbol to base it on
  prim_def(PRIM_COPIES_NO_ALIAS_SET, "copies no alias set", returnInfoUnknown, false, false);

  // Argument is a symbol and we attach flags to that symbol to
  // indicate optimization information.
  // That symbol includes FLAG_OPT_INFO_... flags.
  prim_def(PRIM_OPTIMIZATION_INFO, "optimization info", returnInfoVoid, true, false);

  prim_def(PRIM_GATHER_TESTS, "gather tests", returnInfoDefaultInt);
  prim_def(PRIM_GET_TEST_BY_NAME, "get test by name", returnInfoVoid);
  prim_def(PRIM_GET_TEST_BY_INDEX, "get test by index", returnInfoVoid);

  // version info for 'chpl'
  prim_def(PRIM_VERSION_MAJOR, "version major", returnInfoDefaultInt);
  prim_def(PRIM_VERSION_MINOR, "version minor", returnInfoDefaultInt);
  prim_def(PRIM_VERSION_UPDATE, "version update", returnInfoDefaultInt);
  prim_def(PRIM_VERSION_SHA, "version sha", returnInfoString);

  prim_def(PRIM_REF_DESERIALIZE, "deserialize for ref fields", returnInfoCVoidPtr);

  prim_def(PRIM_UINT32_AS_REAL32, "uint32 as real32", returnInfoReal32);
  prim_def(PRIM_UINT64_AS_REAL64, "uint64 as real64", returnInfoReal64);
  prim_def(PRIM_REAL32_AS_UINT32, "real32 as uint32", returnInfoUInt32);
  prim_def(PRIM_REAL64_AS_UINT64, "real64 as uint64", returnInfoUInt64);

  prim_def(PRIM_DEBUG_TRAP, "debug trap", returnInfoVoid, true);

  // Expects a single argument, which will be passed by pointer to an underlying
  // runtime function, so that the memory can be hashed.
  prim_def(PRIM_CONST_ARG_HASH, "hash const arguments", returnInfoUInt64, true);
  // Expects five arguments:
  // 1. hash of the const argument at the start of the function
  // 2. hash of the const argument at the end of the function
  // 3. the name of the const argument
  // 4. and 5. the line number and file name where the argument was defined.
  //
  // The latter two will be inserted by the compiler before code generation
  //
  // 1 and 2 are used to determine if the argument has been indirectly modified,
  // 3-5 are used to generate a warning message if it was.
  prim_def(PRIM_CHECK_CONST_ARG_HASH, "check hashes of const arguments", returnInfoVoid, true, true);

  // we need to carry information about 'in' intents lowered from foreach loops
  // until gpu transforms. To do that we add an assignment
  //   `taskIndVar = TASK_PRIVATE_SVAR_CAPTURE(capturedVar)` into the AST.
  prim_def(PRIM_TASK_PRIVATE_SVAR_CAPTURE, "task private svar capture", returnInfoUnknown);
}

static Map<const char*, VarSymbol*> memDescsMap;
static Map<int, VarSymbol*> memDescsNodeMap;  // key is the Type node's ID
Vec<const char*> memDescsVec;
static int64_t memDescInt = 0;

VarSymbol* newMemDesc(const char* str) {
  const char* s = astr(str);
  if (VarSymbol* v = memDescsMap.get(s))
    return v;
  VarSymbol* memDescVar = new_IntSymbol(memDescInt++, INT_SIZE_16);
  memDescsMap.put(s, memDescVar);
  memDescsVec.add(s);
  return memDescVar;
}

VarSymbol* newMemDesc(Type* type) {
  if (VarSymbol* v = memDescsNodeMap.get(type->id))
    return v;
  VarSymbol* memDescVar = new_IntSymbol(memDescInt++, INT_SIZE_16);
  memDescsNodeMap.put(type->id, memDescVar);
  memDescsVec.add(type->symbol->name);
  return memDescVar;
}

bool getSettingPrimitiveDstSrc(CallExpr* call, Expr** dest, Expr** src)
{
  if (call->isPrimitive(PRIM_MOVE) || call->isPrimitive(PRIM_ASSIGN)) {
    // dest, src
    *dest = call->get(1);
    *src = call->get(2);
    return true;
  }

  if (call->isPrimitive(PRIM_SET_MEMBER) ||
      call->isPrimitive(PRIM_SET_SVEC_MEMBER)) {
    // base, field/index, value
    *dest = call->get(1);
    *src = call->get(3);
    return true;
  }

  return false;
}

void registerPrimitiveCodegen(PrimitiveTag tag, void (*fn)(CallExpr*, GenRet&)) {
  for (int i = 0; i < NUM_KNOWN_PRIMS; i++ ) {
    if (primitives[i] && primitives[i]->tag == tag) {
      primitives[i]->codegenFn = fn;
      return;
    }
  }
  INT_FATAL("failed to find primitive");
}
