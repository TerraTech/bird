#ifndef _BIRD_FILTER_INTERPRET_H_
#define _BIRD_FILTER_INTERPRET_H_

#include "filter/filter.h"

#define ACCESS_RTE \
  do { if (!f_rte) runtime("No route to access"); } while (0)

#define BITFIELD_MASK(what) \
  (1u << (what->a2.i >> 24))

#define ARG(n,call) \
  struct f_val v##n = call(what->a##n.p); \
  if (v##n.type & T_RETURN) \
    return v##n;

#define AI(n) ARG(n,interpret)

#define FI_INST_INTERPRET(inst) static inline struct f_val fi_interpret_##inst(const struct f_inst *what)
//#define FI_INST_PREPROCESS(inst) static struct f_val fi_preprocess_##inst(struct f_inst *what)

#define RET(ftype,member,value) return (struct f_val) { .type = ftype, .val.member = (value) }
#define RET_VOID return F_VAL_VOID
#define RETA(member,value) RET(what->aux, member, value)

/* Interpret */
#define FI_INST_NUMERIC_BINARY(name,op) \
  FI_INST_INTERPRET(name) \
  { \
    AI(1); AI(2); \
    if ((v1.type != T_INT) || (v2.type != T_INT)) \
      runtime( "Incompatible types for arithmetic operation" ); \
    RET(T_INT, i, (v1.val.i op v2.val.i)); \
  }

FI_INST_NUMERIC_BINARY(add,+)
FI_INST_NUMERIC_BINARY(subtract,-)
FI_INST_NUMERIC_BINARY(multiply,*)

FI_INST_INTERPRET(divide)
{
  AI(1); AI(2);
  if ((v1.type != T_INT) || (v2.type != T_INT))
    runtime( "Incompatible types for arithmetic operation" );
  if (v2.val.i == 0)
    runtime( "I don't believe in division by zero" );
  RET(T_INT, i, (v1.val.i / v2.val.i));
}

static inline struct f_val fi_interpret_boolbinary(const struct f_inst *what)
{
  AI(1);
  if (v1.type != T_BOOL)
    runtime ( "Incompatible type for operation &" );
  if (v1.val.i == (what->fi_code == fi_or))
    RET(T_BOOL, i, v1.val.i);
  AI(2);
  if (v2.type != T_BOOL)
    runtime ( "Incompatible type for operation &" );
  return v2;
}
#define fi_interpret_and fi_interpret_boolbinary
#define fi_interpret_or fi_interpret_boolbinary

FI_INST_INTERPRET(pair_construct)
{
  AI(1); AI(2);
  if ((v1.type != T_INT) || (v2.type != T_INT))
    runtime( "Can't operate with value of non-integer type in pair constructor" );
  unsigned u1 = v1.val.i, u2 = v2.val.i;
  if ((u1 > 0xFFFF) || (u2 > 0xFFFF))
    runtime( "Can't operate with value out of bounds in pair constructor" );
  RET(T_PAIR, i, (u1 << 16) | u2);
}

FI_INST_INTERPRET(ec_construct)
{
  AI(1); AI(2);
  int check, ipv4_used;
  u32 key, val;

  if (v1.type == T_INT) {
    ipv4_used = 0; key = v1.val.i;
  }
  else if (v1.type == T_QUAD) {
    ipv4_used = 1; key = v1.val.i;
  }
#ifndef IPV6
  /* IP->Quad implicit conversion */
  else if (v1.type == T_IP) {
    ipv4_used = 1; key = ipa_to_u32(v1.val.px.ip);
  }
#endif
  else
    runtime("Can't operate with key of non-integer/IPv4 type in EC constructor");

  if (v2.type != T_INT)
    runtime("Can't operate with value of non-integer type in EC constructor");
  val = v2.val.i;

  struct f_val res = { .type = T_EC };

  if (what->aux == EC_GENERIC) {
    check = 0; res.val.ec = ec_generic(key, val);
  }
  else if (ipv4_used) {
    check = 1; res.val.ec = ec_ip4(what->aux, key, val);
  }
  else if (key < 0x10000) {
    check = 0; res.val.ec = ec_as2(what->aux, key, val);
  }
  else {
    check = 1; res.val.ec = ec_as4(what->aux, key, val);
  }

  if (check && (val > 0xFFFF))
    runtime("Can't operate with value out of bounds in EC constructor");

  return res;
}

FI_INST_INTERPRET(lc_construct)
{
  AI(1); AI(2); AI(3);
  if ((v1.type != T_INT) || (v2.type != T_INT) || (v3.type != T_INT))
    runtime( "Can't operate with value of non-integer type in LC constructor" );

  RET(T_LC, lc, ((lcomm) { v1.val.i, v2.val.i, v3.val.i }));
}

/* Relational operators */

static inline struct f_val fi_interpret_same(const struct f_inst *what)
{
  AI(1); AI(2);
  int i = val_same(v1, v2);
  RET(T_BOOL, i, (what->fi_code == fi_eq) ? i : !i);
}

static inline struct f_val fi_interpret_compare(const struct f_inst *what)
{
  AI(1); AI(2);
  int i = val_compare(v1, v2);
  if (i == CMP_ERROR)
    runtime( "Can't compare values of incompatible types" );
  RET(T_BOOL, i, (what->fi_code == fi_lt) ? (i == -1) : (i != 1));
}

#define fi_interpret_eq fi_interpret_same
#define fi_interpret_neq fi_interpret_same
#define fi_interpret_lt fi_interpret_compare
#define fi_interpret_lte fi_interpret_compare

FI_INST_INTERPRET(not)
{
  AI(1);
  if (v1.type != T_BOOL)
    runtime( "Not applied to non-boolean" );
  RET(T_BOOL, i, !v1.val.i);
}

FI_INST_INTERPRET(match)
{
  AI(1); AI(2);
  int i = val_in_range(v1, v2);
  if (i == CMP_ERROR)
    runtime( "~ applied on unknown type pair" );
  RET(T_BOOL, i, !!i);
}

FI_INST_INTERPRET(not_match)
{
  AI(1); AI(2);
  int i = val_in_range(v1, v2);
  if (i == CMP_ERROR)
    runtime( "~ applied on unknown type pair" );
  RET(T_BOOL, i, !i);
}

FI_INST_INTERPRET(defined)
{
  AI(1);
  RET(T_BOOL, i, (v1.type != T_VOID));
}

FI_INST_INTERPRET(set)
{
  /* Set to indirect value, a1 = variable, a2 = value */
  AI(2);
  struct symbol *sym = what->a1.p;
  struct f_val *vp = sym->def;
  if ((sym->class != (SYM_VARIABLE | v2.type)) && (v2.type != T_VOID)) {
#ifndef IPV6
    /* IP->Quad implicit conversion */
    if ((sym->class == (SYM_VARIABLE | T_QUAD)) && (v2.type == T_IP)) {
      vp->type = T_QUAD;
      vp->val.i = ipa_to_u32(v2.val.px.ip);
      RET_VOID;
    }
#endif
    runtime( "Assigning to variable of incompatible type" );
  }
  *vp = v2;
  RET_VOID;
}

FI_INST_INTERPRET(constant)
{
  /* some constants have value in a2, some in *a1.p, strange. */
  /* integer (or simple type) constant, string, set, or prefix_set */
  switch (what->aux) {
    case T_PREFIX_SET:	RET(T_PREFIX_SET, ti, what->a2.p);
    case T_SET:		RET(T_SET, t, what->a2.p);
    case T_STRING:	RET(T_STRING, s, what->a2.p);
    default:		RET(what->aux, i, what->a2.i);
  }
}

FI_INST_INTERPRET(variable)
{
  return * ((struct f_val *) what->a1.p);
}

#define fi_interpret_constant_indirect fi_interpret_variable

FI_INST_INTERPRET(print)
{
  AI(1);
  val_format(v1, &f_buf);
  RET_VOID;
}

FI_INST_INTERPRET(condition)
{
  /* Structure of conditions:
   * if (CONDITION) then TRUE_BLOCK else FALSE_BLOCK
   * ... converts to this:
   * 
   * +--------------------+------------------------------------------+
   * |                    |                                          |
   * |  instruction code  |              fi_condition                |
   * |                    |                                          |
   * +--------------------+------------------------------------------+
   * |                    |                                          |
   * |                    |   +---------------+------------------+   |
   * |                    |   |               |                  |   |
   * |  argument 1        |   |  instruction  |                  |   |
   * |                    |   |      code     |   fi_condition   |   |
   * |                    |   |               |                  |   |
   * |                    |   +---------------+------------------+   |
   * |                    |   |               |                  |   |
   * |                    |   |   argument 1  |    CONDITION     |   |
   * |                    |   |               |                  |   |
   * |                    |   +---------------+------------------+   |
   * |                    |   |               |                  |   |
   * |                    |   |   argument 2  |    TRUE block    |   |
   * |                    |   |               |                  |   |
   * |                    |   +---------------+------------------+   |
   * |                    |                                          |
   * +--------------------+------------------------------------------+
   * |                    |                                          |
   * |   argument 2       |    FALSE block                           |
   * |                    |                                          |
   * +--------------------+------------------------------------------+
   *
   * Procesing works this way:
   * 1) the outer instruction is approached
   * 2) to evaluate the condition, the inner instruction is approached
   * 3) it CONDITION is true:
   *	4a) the TRUE block is executed
   *	5a) the inner instruction returns FALSE
   *	6a) the outer instruction evaluates FALSE
   *	7a) TRUE is returned
   * 3) else
   *	4b) the inner instruction returns TRUE
   *	5b) the outer instruction evaluates TRUE
   *	6b) the FALSE block is executed
   *	7b) FALSE is returned
   */

  AI(1);
  if (v1.type != T_BOOL)
    runtime( "If requires boolean expression" );
  if (v1.val.i) {
    AI(2);
    RET(T_BOOL, i, 0);
  } else
    RET(T_BOOL, i, 1);
}

FI_INST_INTERPRET(nop)
{
  debug( "No operation\n" );
  RET_VOID;
}

FI_INST_INTERPRET(print_and_die)
{
  AI(1);
  if (what->a2.i == F_NOP || (what->a2.i != F_NONL && what->a1.p))
    log_commit(*L_INFO, &f_buf);

  switch (what->a2.i) {
  case F_QUITBIRD:
    die( "Filter asked me to die" );
  case F_ACCEPT:
    /* Should take care about turning ACCEPT into MODIFY */
  case F_ERROR:
  case F_REJECT:	/* FIXME (noncritical) Should print complete route along with reason to reject route */
    RET(T_RETURN, i, what->a2.i);
  case F_NONL:
  case F_NOP:
      break;
  default:
    bug( "unknown return type: Can't happen");
  }
  RET_VOID;
}

FI_INST_INTERPRET(rta_get)
{
  ACCESS_RTE;
  struct rta *rta = (*f_rte)->attrs;

  switch (what->a2.i)
  {
  case SA_FROM:		RETA(px.ip, rta->from);
  case SA_GW:		RETA(px.ip, rta->gw);
  case SA_NET:		RETA(px, ((struct f_prefix) {
			    .ip = (*f_rte)->net->n.prefix,
			    .len = (*f_rte)->net->n.pxlen
			}));
  case SA_PROTO:	RETA(s, rta->src->proto->name);
  case SA_SOURCE:	RETA(i, rta->source);
  case SA_SCOPE:	RETA(i, rta->scope); break;
  case SA_CAST:		RETA(i, rta->cast); break;
  case SA_DEST:		RETA(i, rta->dest); break;
  case SA_IFNAME:	RETA(s, rta->iface ? rta->iface->name : "");
  case SA_IFINDEX:	RETA(i, rta->iface ? rta->iface->index : 0);

  default:
    bug("Invalid static attribute access (%x)", what->aux);
  }
  RET_VOID;
}

FI_INST_INTERPRET(rta_set)
{
  AI(1);
  ACCESS_RTE;
  if (what->aux != v1.type)
    runtime( "Attempt to set static attribute to incompatible type" );

  f_rta_cow();
  struct rta *rta = (*f_rte)->attrs;

  switch (what->a2.i)
  {
  case SA_FROM:
    rta->from = v1.val.px.ip;
    break;

  case SA_GW:
    {
      ip_addr ip = v1.val.px.ip;
      neighbor *n = neigh_find(rta->src->proto, &ip, 0);
      if (!n || (n->scope == SCOPE_HOST))
	runtime( "Invalid gw address" );

      rta->dest = RTD_ROUTER;
      rta->gw = ip;
      rta->iface = n->iface;
      rta->nexthops = NULL;
      rta->hostentry = NULL;
    }
    break;

  case SA_SCOPE:
    rta->scope = v1.val.i;
    break;

  case SA_DEST:
    switch(v1.val.i)
    {
      case RTD_BLACKHOLE:
      case RTD_UNREACHABLE:
      case RTD_PROHIBIT:
	rta->dest = v1.val.i;
	rta->gw = IPA_NONE;
	rta->iface = NULL;
	rta->nexthops = NULL;
	rta->hostentry = NULL;
	break;
      default:
	runtime( "Destination can be changed only to blackhole, unreachable or prohibit" );
    }
    break;

  default:
    bug("Invalid static attribute access (%x)", what->a2.i);
  }
  RET_VOID;
}

FI_INST_INTERPRET(ea_get)
{
  ACCESS_RTE;
  eattr *e = NULL;
  u16 code = what->a2.i;

  if (!(f_flags & FF_FORCE_TMPATTR))
    e = ea_find((*f_rte)->attrs->eattrs, code);
  if (!e)
    e = ea_find((*f_tmp_attrs), code);
  if ((!e) && (f_flags & FF_FORCE_TMPATTR))
    e = ea_find((*f_rte)->attrs->eattrs, code);

  if (!e) {
    /* A special case: undefined int_set looks like empty int_set */
    if ((what->aux & EAF_TYPE_MASK) == EAF_TYPE_INT_SET)
      RET(T_CLIST, ad, adata_empty(f_pool, 0));

    /* The same special case for ec_set */
    if ((what->aux & EAF_TYPE_MASK) == EAF_TYPE_EC_SET)
      RET(T_ECLIST, ad, adata_empty(f_pool, 0));

    /* The same special case for lc_set */
    if ((what->aux & EAF_TYPE_MASK) == EAF_TYPE_LC_SET)
      RET(T_LCLIST, ad, adata_empty(f_pool, 0));

    /* Undefined value */
    RET_VOID;
  }

  switch (what->aux & EAF_TYPE_MASK) {
  case EAF_TYPE_INT:
    RET(T_INT, i, e->u.data);
  case EAF_TYPE_ROUTER_ID:
    RET(T_QUAD, i, e->u.data);
  case EAF_TYPE_OPAQUE:
    RET(T_ENUM_EMPTY, i, 0);
  case EAF_TYPE_IP_ADDRESS:
    RET(T_IP, px.ip, * (ip_addr *) ( (struct adata *) (e->u.ptr) )->data);
  case EAF_TYPE_AS_PATH:
    RET(T_PATH, ad, e->u.ptr);
  case EAF_TYPE_BITFIELD:
    RET(T_BOOL, i, !!(e->u.data & BITFIELD_MASK(what)));
  case EAF_TYPE_INT_SET:
    RET(T_CLIST, ad, e->u.ptr);
  case EAF_TYPE_EC_SET:
    RET(T_ECLIST, ad, e->u.ptr);
  case EAF_TYPE_LC_SET:
    RET(T_LCLIST, ad, e->u.ptr);
  case EAF_TYPE_UNDEF:
    RET_VOID;
  default:
    bug("Unknown type in fi_ea_get");
  }
  RET_VOID;
}

FI_INST_INTERPRET(ea_set)
{
  ACCESS_RTE;
  AI(1);
  struct ea_list *l = lp_alloc(f_pool, sizeof(struct ea_list) + sizeof(eattr));
  u16 code = what->a2.i;

  l->next = NULL;
  l->flags = EALF_SORTED;
  l->count = 1;
  l->attrs[0].id = code;
  l->attrs[0].flags = 0;
  l->attrs[0].type = what->aux | EAF_ORIGINATED;

  switch (what->aux & EAF_TYPE_MASK) {
  case EAF_TYPE_INT:
    // Enums are also ints, so allow them in.
    if (v1.type != T_INT && (v1.type < T_ENUM_LO || v1.type > T_ENUM_HI))
      runtime( "Setting int attribute to non-int value" );
    l->attrs[0].u.data = v1.val.i;
    break;

  case EAF_TYPE_ROUTER_ID:
#ifndef IPV6
    /* IP->Quad implicit conversion */
    if (v1.type == T_IP) {
      l->attrs[0].u.data = ipa_to_u32(v1.val.px.ip);
      break;
    }
#endif
    /* T_INT for backward compatibility */
    if ((v1.type != T_QUAD) && (v1.type != T_INT))
      runtime( "Setting quad attribute to non-quad value" );
    l->attrs[0].u.data = v1.val.i;
    break;

  case EAF_TYPE_OPAQUE:
    runtime( "Setting opaque attribute is not allowed" );
    break;
  case EAF_TYPE_IP_ADDRESS:
    if (v1.type != T_IP)
      runtime( "Setting ip attribute to non-ip value" );
    int len = sizeof(ip_addr);
    struct adata *ad = lp_alloc(f_pool, sizeof(struct adata) + len);
    ad->length = len;
    (* (ip_addr *) ad->data) = v1.val.px.ip;
    l->attrs[0].u.ptr = ad;
    break;
  case EAF_TYPE_AS_PATH:
    if (v1.type != T_PATH)
      runtime( "Setting path attribute to non-path value" );
    l->attrs[0].u.ptr = v1.val.ad;
    break;
  case EAF_TYPE_BITFIELD:
    if (v1.type != T_BOOL)
      runtime( "Setting bit in bitfield attribute to non-bool value" );
    {
      /* First, we have to find the old value */
      eattr *e = NULL;
      if (!(f_flags & FF_FORCE_TMPATTR))
	e = ea_find((*f_rte)->attrs->eattrs, code);
      if (!e)
	e = ea_find((*f_tmp_attrs), code);
      if ((!e) && (f_flags & FF_FORCE_TMPATTR))
	e = ea_find((*f_rte)->attrs->eattrs, code);
      u32 data = e ? e->u.data : 0;

      if (v1.val.i)
	l->attrs[0].u.data = data | BITFIELD_MASK(what);
      else
	l->attrs[0].u.data = data & ~BITFIELD_MASK(what);;
    }
    break;
  case EAF_TYPE_INT_SET:
    if (v1.type != T_CLIST)
      runtime( "Setting clist attribute to non-clist value" );
    l->attrs[0].u.ptr = v1.val.ad;
    break;
  case EAF_TYPE_EC_SET:
    if (v1.type != T_ECLIST)
      runtime( "Setting eclist attribute to non-eclist value" );
    l->attrs[0].u.ptr = v1.val.ad;
    break;
  case EAF_TYPE_LC_SET:
    if (v1.type != T_LCLIST)
      runtime( "Setting lclist attribute to non-lclist value" );
    l->attrs[0].u.ptr = v1.val.ad;
    break;
  case EAF_TYPE_UNDEF:
    if (v1.type != T_VOID)
      runtime( "Setting void attribute to non-void value" );
    l->attrs[0].u.data = 0;
    break;
  default: bug("Unknown type in e,S");
  }

  if (!(what->aux & EAF_TEMP) && (!(f_flags & FF_FORCE_TMPATTR))) {
    f_rta_cow();
    l->next = (*f_rte)->attrs->eattrs;
    (*f_rte)->attrs->eattrs = l;
  } else {
    l->next = (*f_tmp_attrs);
    (*f_tmp_attrs) = l;
  }
  RET_VOID;
}

FI_INST_INTERPRET(pref_get) {
  ACCESS_RTE;
  RET(T_INT, i, (*f_rte)->pref);
}

FI_INST_INTERPRET(pref_set) {
  ACCESS_RTE;
  AI(1);
  if (v1.type != T_INT)
    runtime( "Can't set preference to non-integer" );
  if (v1.val.i > 0xFFFF)
    runtime( "Setting preference value out of bounds" );
  f_rte_cow();
  (*f_rte)->pref = v1.val.i;
  RET_VOID;
}

FI_INST_INTERPRET(length) {
  AI(1);
  switch(v1.type) {
  case T_PREFIX: RET(T_INT, i, v1.val.px.len);
  case T_PATH:   RET(T_INT, i, as_path_getlen(v1.val.ad));
  case T_CLIST:  RET(T_INT, i, int_set_get_size(v1.val.ad));
  case T_ECLIST: RET(T_INT, i, ec_set_get_size(v1.val.ad));
  case T_LCLIST: RET(T_INT, i, lc_set_get_size(v1.val.ad));
  default: runtime( "Prefix, path, clist or eclist expected" );
  }
  RET_VOID;
}

FI_INST_INTERPRET(ip) {
  AI(1);
  if (v1.type != T_PREFIX)
    runtime( "Prefix expected" );

  RET(T_IP, px.ip, v1.val.px.ip);
}

FI_INST_INTERPRET(as_path_first) {
  AI(1);
  if (v1.type != T_PATH)
    runtime( "AS path expected" );

  u32 as = 0;
  as_path_get_first(v1.val.ad, &as);
  RET(T_INT, i, as);
}

FI_INST_INTERPRET(as_path_last) {
  AI(1);
  if (v1.type != T_PATH)
    runtime( "AS path expected" );

  u32 as = 0;
  as_path_get_last(v1.val.ad, &as);
  RET(T_INT, i, as);
}

FI_INST_INTERPRET(as_path_last_nag) {
  AI(1);
  if (v1.type != T_PATH)
    runtime( "AS path expected" );

  RET(T_INT, i, as_path_get_last_nonaggregated(v1.val.ad));
}

FI_INST_INTERPRET(return) {
  AI(1);
  v1.type |= T_RETURN;
  return v1;
}

FI_INST_INTERPRET(call) {
  AI(1);
  struct f_val res = interpret(what->a2.p);
  if (res.type == T_RETURN) /* Exception */
    return res;

  res.type &= ~T_RETURN;
  return res;
}

FI_INST_INTERPRET(clear_local_vars) {
  struct symbol *sym;
  for (sym = what->a1.p; sym != NULL; sym = sym->aux2)
    ((struct f_val *) sym->def)->type = T_VOID;
  RET_VOID;
}

FI_INST_INTERPRET(switch) {
  AI(1);
  struct f_tree *t = find_tree(what->a2.p, v1);
  if (!t) {
    t = find_tree(what->a2.p, F_VAL_VOID);
    if (!t) {
      debug( "No else statement?\n");
      RET_VOID;
    }
  }

  /* It is actually possible to have t->data NULL */
  return interpret(t->data);
}

FI_INST_INTERPRET(ip_mask) {
  AI(1); AI(2);

  if (v2.type != T_INT)
    runtime( "Integer expected");
  if (v1.type != T_IP)
    runtime( "You can mask only IP addresses" );

  ip_addr mask = ipa_mkmask(v2.val.i);
  RET(T_IP, px.ip, ipa_and(mask, v1.val.px.ip));
}

FI_INST_INTERPRET(empty) {
  RETA(ad, adata_empty(f_pool, 0));
}

FI_INST_INTERPRET(path_prepend) {
  AI(1); AI(2);
  if (v1.type != T_PATH)
    runtime("Can't prepend to non-path");
  if (v2.type != T_INT)
    runtime("Can't prepend non-integer");

  RET(T_PATH, ad, as_path_prepend(f_pool, v1.val.ad, v2.val.i));
}

FI_INST_INTERPRET(clist_add_del) {
  AI(1); AI(2);
  if (v1.type == T_PATH)
  {
    struct f_tree *set = NULL;
    u32 key = 0;
    int pos;

    if (v2.type == T_INT)
      key = v2.val.i;
    else if ((v2.type == T_SET) && (v2.val.t->from.type == T_INT))
      set = v2.val.t;
    else
      runtime("Can't delete non-integer (set)");

    switch (what->aux)
    {
    case 'a':	runtime("Can't add to path");
    case 'd':	pos = 0; break;
    case 'f':	pos = 1; break;
    default:	bug("unknown Ca operation");
    }

    if (pos && !set)
      runtime("Can't filter integer");

    RET(T_PATH, ad, as_path_filter(f_pool, v1.val.ad, set, key, pos));
  }
  else if (v1.type == T_CLIST)
  {
    /* Community (or cluster) list */
    struct f_val dummy;
    int arg_set = 0;
    uint n = 0;

    if ((v2.type == T_PAIR) || (v2.type == T_QUAD))
      n = v2.val.i;
#ifndef IPV6
    /* IP->Quad implicit conversion */
    else if (v2.type == T_IP)
      n = ipa_to_u32(v2.val.px.ip);
#endif
    else if ((v2.type == T_SET) && clist_set_type(v2.val.t, &dummy))
      arg_set = 1;
    else if (v2.type == T_CLIST)
      arg_set = 2;
    else
      runtime("Can't add/delete non-pair");

    switch (what->aux)
    {
    case 'a':
      if (arg_set == 1)
	runtime("Can't add set");
      else if (!arg_set)
	RET(T_CLIST, ad, int_set_add(f_pool, v1.val.ad, n));
      else
	RET(T_CLIST, ad, int_set_union(f_pool, v1.val.ad, v2.val.ad));

    case 'd':
      if (!arg_set)
	RET(T_CLIST, ad, int_set_del(f_pool, v1.val.ad, n));
      else
	RET(T_CLIST, ad, clist_filter(f_pool, v1.val.ad, v2, 0));

    case 'f':
      if (!arg_set)
	runtime("Can't filter pair");
      RET(T_CLIST, ad, clist_filter(f_pool, v1.val.ad, v2, 1));

    default:
      bug("unknown Ca operation");
    }
  }
  else if (v1.type == T_ECLIST)
  {
    /* Extended community list */
    int arg_set = 0;

    /* v2.val is either EC or EC-set */
    if ((v2.type == T_SET) && eclist_set_type(v2.val.t))
      arg_set = 1;
    else if (v2.type == T_ECLIST)
      arg_set = 2;
    else if (v2.type != T_EC)
      runtime("Can't add/delete non-ec");

    switch (what->aux)
    {
    case 'a':
      if (arg_set == 1)
	runtime("Can't add set");
      else if (!arg_set)
	RET(T_ECLIST, ad, ec_set_add(f_pool, v1.val.ad, v2.val.ec));
      else
	RET(T_ECLIST, ad, ec_set_union(f_pool, v1.val.ad, v2.val.ad));

    case 'd':
      if (!arg_set)
	RET(T_ECLIST, ad, ec_set_del(f_pool, v1.val.ad, v2.val.ec));
      else
	RET(T_ECLIST, ad, eclist_filter(f_pool, v1.val.ad, v2, 0));

    case 'f':
      if (!arg_set)
	runtime("Can't filter ec");
      RET(T_ECLIST, ad, eclist_filter(f_pool, v1.val.ad, v2, 1));

    default:
      bug("unknown Ca operation");
    }
  }
  else if (v1.type == T_LCLIST)
  {
    /* Large community list */
    int arg_set = 0;

    /* v2.val is either LC or LC-set */
    if ((v2.type == T_SET) && lclist_set_type(v2.val.t))
      arg_set = 1;
    else if (v2.type == T_LCLIST)
      arg_set = 2;
    else if (v2.type != T_LC)
      runtime("Can't add/delete non-lc");

    switch (what->aux)
    {
    case 'a':
      if (arg_set == 1)
	runtime("Can't add set");
      else if (!arg_set)
	RET(T_LCLIST, ad, lc_set_add(f_pool, v1.val.ad, v2.val.lc));
      else
	RET(T_LCLIST, ad, lc_set_union(f_pool, v1.val.ad, v2.val.ad));

    case 'd':
      if (!arg_set)
	RET(T_LCLIST, ad, lc_set_del(f_pool, v1.val.ad, v2.val.lc));
      else
	RET(T_LCLIST, ad, lclist_filter(f_pool, v1.val.ad, v2, 0));

    case 'f':
      if (!arg_set)
	runtime("Can't filter lc");
      RET(T_LCLIST, ad, lclist_filter(f_pool, v1.val.ad, v2, 1));

    default:
      bug("unknown Ca operation");
    }
  }
  else
    runtime("Can't add/delete to non-[el]?clist");
}

FI_INST_INTERPRET(roa_check) {
  u32 as;

  ip_addr ip;
  int len;

  if (what->arg1)
  {
    AI(1); AI(2);
    if ((v1.type != T_PREFIX) || (v2.type != T_INT))
      runtime("Invalid argument to roa_check()");

    ip = v1.val.px.ip;
    len = v1.val.px.len;
    as = v2.val.i;
  }
  else
  {
    ACCESS_RTE;
    ip = (*f_rte)->net->n.prefix;
    len = (*f_rte)->net->n.pxlen;

    /* We ignore temporary attributes, probably not a problem here */
    /* 0x02 is a value of BA_AS_PATH, we don't want to include BGP headers */
    eattr *e = ea_find((*f_rte)->attrs->eattrs, EA_CODE(EAP_BGP, 0x02));

    if (!e || e->type != EAF_TYPE_AS_PATH)
      runtime("Missing AS_PATH attribute");

    as_path_get_last(e->u.ptr, &as);
  }

  struct roa_table_config *rtc = ((struct f_inst_roa_check *) what)->rtc;
  if (!rtc->table)
    runtime("Missing ROA table");

  RET(T_ENUM_ROA, i, roa_check(rtc->table, ip, len, as));
}


#undef ACCESS_RTE
#undef BITFIELD_MASK
#undef ARG
#undef AI
#undef FI_INST_INTERPRET
#undef RET
#undef RET_VOID
#undef RETA
#undef FI_INST_NUMERIC_BINARY

#endif