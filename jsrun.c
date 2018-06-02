#include "jsi.h"
#include "jscompile.h"
#include "jsvalue.h"
#include "jsrun.h"

#include "utf.h"

static void jsR_run(js_State *J, js_Function *F);

/* Push values on stack */

#define STACK (J->stack)
#define TOP   (J->top)
#define BOT   (J->bot)

static void js_stackoverflow(js_State *J)
{
	STACK[TOP].type = JS_TLITSTR;
	STACK[TOP].u.litstr = "stack overflow";
	++TOP;
	js_throw(J);
}

static void js_outofmemory(js_State *J)
{
	STACK[TOP].type = JS_TLITSTR;
	STACK[TOP].u.litstr = "out of memory";
	++TOP;
	js_throw(J);
}

void *js_malloc(js_State *J, int size)
{
	void *ptr = J->alloc(J->actx, NULL, size);
	if (!ptr)
		js_outofmemory(J);
	return ptr;
}

void *js_realloc(js_State *J, void *ptr, int size)
{
	ptr = J->alloc(J->actx, ptr, size);
	if (!ptr)
		js_outofmemory(J);
	return ptr;
}

char *js_strdup(js_State *J, const char *s)
{
	int n = strlen(s) + 1;
	char *p = js_malloc(J, n);
	memcpy(p, s, n);
	return p;
}

void js_free(js_State *J, void *ptr)
{
	J->alloc(J->actx, ptr, 0);
}



#define CHECKSTACK(n) if (TOP + n >= JS_STACKSIZE) js_stackoverflow(J)

void js_push_value(js_State *J, js_Value v)
{
	CHECKSTACK(1);
	STACK[TOP] = v;
	++TOP;
}

void js_push_undef(js_State *J)
{
	CHECKSTACK(1);
	STACK[TOP].type = JS_TUNDEFINED;
	++TOP;
}

void js_push_null(js_State *J)
{
	CHECKSTACK(1);
	STACK[TOP].type = JS_TNULL;
	++TOP;
}

void js_push_bool(js_State *J, int v)
{
	CHECKSTACK(1);
	STACK[TOP].type = JS_TBOOLEAN;
	STACK[TOP].u.boolean = !!v;
	++TOP;
}

void js_push_number(js_State *J, double v)
{
	CHECKSTACK(1);
	STACK[TOP].type = JS_TNUMBER;
	STACK[TOP].u.number = v;
	++TOP;
}

void js_push_string(js_State *J, const char *v)
{
	int n = strlen(v);
	CHECKSTACK(1);
	if (n <= soffsetof(js_Value, type)) {
		char *s = STACK[TOP].u.shrstr;
		while (n--) *s++ = *v++;
		*s = 0;
		STACK[TOP].type = JS_TSHRSTR;
	} else {
		STACK[TOP].type = JS_TMEMSTR;
		STACK[TOP].u.memstr = jv_memstring(J, v, n);
	}
	++TOP;
}

void js_push_lstr(js_State *J, const char *v, int n)
{
	CHECKSTACK(1);
	if (n <= soffsetof(js_Value, type)) {
		char *s = STACK[TOP].u.shrstr;
		while (n--) *s++ = *v++;
		*s = 0;
		STACK[TOP].type = JS_TSHRSTR;
	} else {
		STACK[TOP].type = JS_TMEMSTR;
		STACK[TOP].u.memstr = jv_memstring(J, v, n);
	}
	++TOP;
}

void js_push_literal(js_State *J, const char *v)
{
	CHECKSTACK(1);
	STACK[TOP].type = JS_TLITSTR;
	STACK[TOP].u.litstr = v;
	++TOP;
}

void js_push_object(js_State *J, js_Object *v)
{
	CHECKSTACK(1);
	STACK[TOP].type = JS_TOBJECT;
	STACK[TOP].u.object = v;
	++TOP;
}

void js_push_global(js_State *J)
{
	js_push_object(J, J->G);
}

void js_cur_function(js_State *J)
{
	CHECKSTACK(1);
	STACK[TOP] = STACK[BOT-1];
	++TOP;
}

/* Read values from stack */

static js_Value *stackidx(js_State *J, int idx)
{
	static js_Value undefined = { {0}, {0}, JS_TUNDEFINED };
	idx = idx < 0 ? TOP + idx : BOT + idx;
	if (idx < 0 || idx >= TOP)
		return &undefined;
	return STACK + idx;
}



int js_is_def(js_State *J, int idx) { return stackidx(J, idx)->type != JS_TUNDEFINED; }
int js_is_undef(js_State *J, int idx) { return stackidx(J, idx)->type == JS_TUNDEFINED; }
int js_is_null(js_State *J, int idx) { return stackidx(J, idx)->type == JS_TNULL; }
int js_is_bool(js_State *J, int idx) { return stackidx(J, idx)->type == JS_TBOOLEAN; }
int js_is_number(js_State *J, int idx) { return stackidx(J, idx)->type == JS_TNUMBER; }
int js_is_string(js_State *J, int idx) { enum js_Type t = stackidx(J, idx)->type; return t == JS_TSHRSTR || t == JS_TLITSTR || t == JS_TMEMSTR; }
int js_is_primitive(js_State *J, int idx) { return stackidx(J, idx)->type != JS_TOBJECT; }
int js_is_object(js_State *J, int idx) { return stackidx(J, idx)->type == JS_TOBJECT; }
int js_is_coercible(js_State *J, int idx) { js_Value *v = stackidx(J, idx); return v->type != JS_TUNDEFINED && v->type != JS_TNULL; }

int js_is_callable(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	if (v->type == JS_TOBJECT)
		return v->u.object->type == JS_CFUNCTION ||
			v->u.object->type == JS_CSCRIPT ||
			v->u.object->type == JS_CCFUNCTION;
	return 0;
}

int js_is_array(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	return v->type == JS_TOBJECT && v->u.object->type == JS_CARRAY;
}

int js_is_regexp(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	return v->type == JS_TOBJECT && v->u.object->type == JS_CREGEXP;
}

int js_is_userdata(js_State *J, int idx, const char *tag)
{
	js_Value *v = stackidx(J, idx);
	if (v->type == JS_TOBJECT && v->u.object->type == JS_CUSERDATA)
		return !strcmp(tag, v->u.object->u.user.tag);
	return 0;
}

static const char *js_typeof(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	switch (v->type) {
	default:
	case JS_TSHRSTR: return "string";
	case JS_TUNDEFINED: return "undefined";
	case JS_TNULL: return "object";
	case JS_TBOOLEAN: return "boolean";
	case JS_TNUMBER: return "number";
	case JS_TLITSTR: return "string";
	case JS_TMEMSTR: return "string";
	case JS_TOBJECT:
		if (v->u.object->type == JS_CFUNCTION || v->u.object->type == JS_CCFUNCTION)
			return "function";
		return "object";
	}
}

js_Value *js_tovalue(js_State *J, int idx)
{
	return stackidx(J, idx);
}

int js_toboolean(js_State *J, int idx)
{
	return jv_toboolean(J, stackidx(J, idx));
}

double js_tonumber(js_State *J, int idx)
{
	return jv_tonumber(J, stackidx(J, idx));
}

int js_tointeger(js_State *J, int idx)
{
	return jv_tointeger(J, stackidx(J, idx));
}

int js_toi32(js_State *J, int idx)
{
	return js_ntoi32(js_tonumber(J, idx));
}

unsigned int js_tou32(js_State *J, int idx)
{
	return (unsigned int)js_toi32(J, idx);
}
/*
short js_toint16(js_State *J, int idx)
{
	return jsV_ntoi16(jsV_tonumber(J, stackidx(J, idx)));
}

unsigned short js_touint16(js_State *J, int idx)
{
	return jsV_ntou16(jsV_tonumber(J, stackidx(J, idx)));
}
*/

const char *js_tostring(js_State *J, int idx)
{
	return jv_tostring(J, stackidx(J, idx));
}

js_Object *js_toobject(js_State *J, int idx)
{
	return jv_toobject(J, stackidx(J, idx));
}

void js_toprimitive(js_State *J, int idx, int hint)
{
	jv_toprimitive(J, stackidx(J, idx), hint);
}

js_Regexp *js_toregexp(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	if (v->type == JS_TOBJECT && v->u.object->type == JS_CREGEXP)
		return &v->u.object->u.r;
	js_error_type(J, "not a regexp");
}

void *js_touserdata(js_State *J, int idx, const char *tag)
{
	js_Value *v = stackidx(J, idx);
	if (v->type == JS_TOBJECT && v->u.object->type == JS_CUSERDATA)
		if (!strcmp(tag, v->u.object->u.user.tag))
			return v->u.object->u.user.data;
	js_error_type(J, "not a %s", tag);
}

static js_Object *jsR_tofunction(js_State *J, int idx)
{
	js_Value *v = stackidx(J, idx);
	if (v->type == JS_TUNDEFINED || v->type == JS_TNULL)
		return NULL;
	if (v->type == JS_TOBJECT)
		if (v->u.object->type == JS_CFUNCTION || v->u.object->type == JS_CCFUNCTION)
			return v->u.object;
	js_error_type(J, "not a function");
}

/* Stack manipulation */

int js_gettop(js_State *J)
{
	return TOP - BOT;
}

void js_pop(js_State *J, int n)
{
	TOP -= n;
	if (TOP < BOT) {
		TOP = BOT;
		js_error(J, "stack underflow!");
	}
}

void js_remove(js_State *J, int idx)
{
	idx = idx < 0 ? TOP + idx : BOT + idx;
	if (idx < BOT || idx >= TOP)
		js_error(J, "stack error!");
	for (;idx < TOP - 1; ++idx)
		STACK[idx] = STACK[idx+1];
	--TOP;
}

void js_insert(js_State *J, int idx)
{
	js_error(J, "not implemented yet");
}

void js_replace(js_State* J, int idx)
{
	idx = idx < 0 ? TOP + idx : BOT + idx;
	if (idx < BOT || idx >= TOP)
		js_error(J, "stack error!");
	STACK[idx] = STACK[--TOP];
}

void js_copy(js_State *J, int idx)
{
	CHECKSTACK(1);
	STACK[TOP] = *stackidx(J, idx);
	++TOP;
}

void js_dup(js_State *J)
{
	CHECKSTACK(1);
	STACK[TOP] = STACK[TOP-1];
	++TOP;
}

void js_dup2(js_State *J)
{
	CHECKSTACK(2);
	STACK[TOP] = STACK[TOP-2];
	STACK[TOP+1] = STACK[TOP-1];
	TOP += 2;
}

void js_rot2(js_State *J)
{
	/* A B -> B A */
	js_Value tmp = STACK[TOP-1];	/* A B (B) */
	STACK[TOP-1] = STACK[TOP-2];	/* A A */
	STACK[TOP-2] = tmp;		/* B A */
}

void js_rot3(js_State *J)
{
	/* A B C -> C A B */
	js_Value tmp = STACK[TOP-1];	/* A B C (C) */
	STACK[TOP-1] = STACK[TOP-2];	/* A B B */
	STACK[TOP-2] = STACK[TOP-3];	/* A A B */
	STACK[TOP-3] = tmp;		/* C A B */
}

void js_rot4(js_State *J)
{
	/* A B C D -> D A B C */
	js_Value tmp = STACK[TOP-1];	/* A B C D (D) */
	STACK[TOP-1] = STACK[TOP-2];	/* A B C C */
	STACK[TOP-2] = STACK[TOP-3];	/* A B B C */
	STACK[TOP-3] = STACK[TOP-4];	/* A A B C */
	STACK[TOP-4] = tmp;		/* D A B C */
}

void js_rot2pop1(js_State *J)
{
	/* A B -> B */
	STACK[TOP-2] = STACK[TOP-1];
	--TOP;
}

void js_rot3pop2(js_State *J)
{
	/* A B C -> C */
	STACK[TOP-3] = STACK[TOP-1];
	TOP -= 2;
}

void js_rot(js_State *J, int n)
{
	int i;
	js_Value tmp = STACK[TOP-1];
	for (i = 1; i < n; ++i)
		STACK[TOP-i] = STACK[TOP-i-1];
	STACK[TOP-i] = tmp;
}

/* Property access that takes care of attributes and getters/setters */

int js_is_arr_index(js_State *J, const char *p, int *idx)
{
	int n = 0;
	while (*p) {
		int c = *p++;
		if (c >= '0' && c <= '9') {
			if (n > INT_MAX / 10 - 1)
				return 0;
			n = n * 10 + (c - '0');
		} else {
			return 0;
		}
	}
	return *idx = n, 1;
}

static void js_pushrune(js_State *J, Rune rune)
{
	char buf[UTFmax + 1];
	if (rune > 0) {
		buf[runetochar(buf, &rune)] = 0;
		js_push_string(J, buf);
	} else {
		js_push_undef(J);
	}
}

static int jr_hasproperty(js_State *J, js_Object *obj, const char *name)
{
	js_Property *ref;
	int k;

	if (obj->type == JS_CARRAY) {
		if (!strcmp(name, "length")) {
			js_push_number(J, obj->u.a.length);
			return 1;
		}
	}

	else if (obj->type == JS_CSTRING) {
		if (!strcmp(name, "length")) {
			js_push_number(J, obj->u.s.length);
			return 1;
		}
		if (js_is_arr_index(J, name, &k)) {
			if (k >= 0 && k < obj->u.s.length) {
				js_pushrune(J, js_runeat(J, obj->u.s.string, k));
				return 1;
			}
		}
	}

	else if (obj->type == JS_CREGEXP) {
		if (!strcmp(name, "source")) {
			js_push_literal(J, obj->u.r.source);
			return 1;
		}
		if (!strcmp(name, "global")) {
			js_push_bool(J, obj->u.r.flags & JS_REGEXP_G);
			return 1;
		}
		if (!strcmp(name, "ignoreCase")) {
			js_push_bool(J, obj->u.r.flags & JS_REGEXP_I);
			return 1;
		}
		if (!strcmp(name, "multiline")) {
			js_push_bool(J, obj->u.r.flags & JS_REGEXP_M);
			return 1;
		}
		if (!strcmp(name, "lastIndex")) {
			js_push_number(J, obj->u.r.last);
			return 1;
		}
	}

	else if (obj->type == JS_CUSERDATA) {
		if (obj->u.user.has && obj->u.user.has(J, obj->u.user.data, name))
			return 1;
	}

	ref = jp_getproperty(J, obj, name);
	if (ref) {
		if (ref->getter) {
			js_push_object(J, ref->getter);
			js_push_object(J, obj);
			js_call(J, 0);
		} else {
			js_push_value(J, ref->value);
		}
		return 1;
	}

	return 0;
}

static void jr_getproperty(js_State *J, js_Object *obj, const char *name)
{
	if (!jr_hasproperty(J, obj, name))
		js_push_undef(J);
}

static void jr_setproperty(js_State *J, js_Object *obj, const char *name)
{
	js_Value *value = stackidx(J, -1);
	js_Property *ref;
	int k;
	int own;

	if (obj->type == JS_CARRAY) {
		if (!strcmp(name, "length")) {
			double rawlen = jv_tonumber(J, value);
			int newlen = js_ntoi(rawlen);
			if (newlen != rawlen || newlen < 0)
				js_error_range(J, "array length");
			jp_resizearray(J, obj, newlen);
			return;
		}
		if (js_is_arr_index(J, name, &k))
			if (k >= obj->u.a.length)
				obj->u.a.length = k + 1;
	}

	else if (obj->type == JS_CSTRING) {
		if (!strcmp(name, "length"))
			goto readonly;
		if (js_is_arr_index(J, name, &k))
			if (k >= 0 && k < obj->u.s.length)
				goto readonly;
	}

	else if (obj->type == JS_CREGEXP) {
		if (!strcmp(name, "source")) goto readonly;
		if (!strcmp(name, "global")) goto readonly;
		if (!strcmp(name, "ignoreCase")) goto readonly;
		if (!strcmp(name, "multiline")) goto readonly;
		if (!strcmp(name, "lastIndex")) {
			obj->u.r.last = jv_tointeger(J, value);
			return;
		}
	}

	else if (obj->type == JS_CUSERDATA) {
		if (obj->u.user.put && obj->u.user.put(J, obj->u.user.data, name))
			return;
	}

	/* First try to find a setter in prototype chain */
	ref = jp_getpropertyx(J, obj, name, &own);
	if (ref) {
		if (ref->setter) {
			js_push_object(J, ref->setter);
			js_push_object(J, obj);
			js_push_value(J, *value);
			js_call(J, 1);
			js_pop(J, 1);
			return;
		} else {
			if (J->strict)
				if (ref->getter)
					js_error_type(J, "setting property '%s' that only has a getter", name);
		}
	}

	/* Property not found on this object, so create one */
	if (!ref || !own)
		ref = jp_setproperty(J, obj, name);

	if (ref) {
		if (!(ref->atts & JS_READONLY))
			ref->value = *value;
		else
			goto readonly;
	}

	return;

readonly:
	if (J->strict)
		js_error_type(J, "'%s' is read-only", name);
}

static void jr_defproperty(js_State *J, js_Object *obj, const char *name,
	int atts, js_Value *value, js_Object *getter, js_Object *setter)
{
	js_Property *ref;
	int k;

	if (obj->type == JS_CARRAY) {
		if (!strcmp(name, "length"))
			goto readonly;
	}

	else if (obj->type == JS_CSTRING) {
		if (!strcmp(name, "length"))
			goto readonly;
		if (js_is_arr_index(J, name, &k))
			if (k >= 0 && k < obj->u.s.length)
				goto readonly;
	}

	else if (obj->type == JS_CREGEXP) {
		if (!strcmp(name, "source")) goto readonly;
		if (!strcmp(name, "global")) goto readonly;
		if (!strcmp(name, "ignoreCase")) goto readonly;
		if (!strcmp(name, "multiline")) goto readonly;
		if (!strcmp(name, "lastIndex")) goto readonly;
	}

	else if (obj->type == JS_CUSERDATA) {
		if (obj->u.user.put && obj->u.user.put(J, obj->u.user.data, name))
			return;
	}

	ref = jp_setproperty(J, obj, name);
	if (ref) {
		if (value) {
			if (!(ref->atts & JS_READONLY))
				ref->value = *value;
			else if (J->strict)
				js_error_type(J, "'%s' is read-only", name);
		}
		if (getter) {
			if (!(ref->atts & JS_DONTCONF))
				ref->getter = getter;
			else if (J->strict)
				js_error_type(J, "'%s' is non-configurable", name);
		}
		if (setter) {
			if (!(ref->atts & JS_DONTCONF))
				ref->setter = setter;
			else if (J->strict)
				js_error_type(J, "'%s' is non-configurable", name);
		}
		ref->atts |= atts;
	}

	return;

readonly:
	if (J->strict)
		js_error_type(J, "'%s' is read-only or non-configurable", name);
}

static int jr_delproperty(js_State *J, js_Object *obj, const char *name)
{
	js_Property *ref;
	int k;

	if (obj->type == JS_CARRAY) {
		if (!strcmp(name, "length"))
			goto dontconf;
	}

	else if (obj->type == JS_CSTRING) {
		if (!strcmp(name, "length"))
			goto dontconf;
		if (js_is_arr_index(J, name, &k))
			if (k >= 0 && k < obj->u.s.length)
				goto dontconf;
	}

	else if (obj->type == JS_CREGEXP) {
		if (!strcmp(name, "source")) goto dontconf;
		if (!strcmp(name, "global")) goto dontconf;
		if (!strcmp(name, "ignoreCase")) goto dontconf;
		if (!strcmp(name, "multiline")) goto dontconf;
		if (!strcmp(name, "lastIndex")) goto dontconf;
	}

	else if (obj->type == JS_CUSERDATA) {
		if (obj->u.user.delete && obj->u.user.delete(J, obj->u.user.data, name))
			return 1;
	}

	ref = jp_getownproperty(J, obj, name);
	if (ref) {
		if (ref->atts & JS_DONTCONF)
			goto dontconf;
		jp_delproperty(J, obj, name);
	}
	return 1;

dontconf:
	if (J->strict)
		js_error_type(J, "'%s' is non-configurable", name);
	return 0;
}

/* Registry, global and object property accessors */

const char *js_ref(js_State *J)
{
	js_Value *v = stackidx(J, -1);
	const char *s;
	char buf[32];
	switch (v->type) {
	case JS_TUNDEFINED: s = "_Undefined"; break;
	case JS_TNULL: s = "_Null"; break;
	case JS_TBOOLEAN:
		s = v->u.boolean ? "_True" : "_False";
		break;
	case JS_TOBJECT:
		sprintf(buf, "%p", (void*)v->u.object);
		s = js_intern(J, buf);
		break;
	default:
		sprintf(buf, "%d", J->nextref++);
		s = js_intern(J, buf);
		break;
	}
	js_set_registry(J, s);
	return s;
}
/*
void js_unref(js_State *J, const char *ref)
{
	js_delregistry(J, ref);
}
*/
void js_get_registry(js_State *J, const char *name)
{
	jr_getproperty(J, J->R, name);
}

void js_set_registry(js_State *J, const char *name)
{
	jr_setproperty(J, J->R, name);
	js_pop(J, 1);
}

void js_del_registry(js_State *J, const char *name)
{
	jr_delproperty(J, J->R, name);
}

void js_get_global(js_State *J, const char *name)
{
	jr_getproperty(J, J->G, name);
}

void js_set_global(js_State *J, const char *name)
{
	jr_setproperty(J, J->G, name);
	js_pop(J, 1);
}

void js_def_global(js_State *J, const char *name, int atts)
{
	jr_defproperty(J, J->G, name, atts, stackidx(J, -1), NULL, NULL);
	js_pop(J, 1);
}

void js_get_prop(js_State *J, int idx, const char *name)
{
	jr_getproperty(J, js_toobject(J, idx), name);
}

void js_set_prop(js_State *J, int idx, const char *name)
{
	jr_setproperty(J, js_toobject(J, idx), name);
	js_pop(J, 1);
}

void js_def_prop(js_State *J, int idx, const char *name, int atts)
{
	jr_defproperty(J, js_toobject(J, idx), name, atts, stackidx(J, -1), NULL, NULL);
	js_pop(J, 1);
}

void js_del_prop(js_State *J, int idx, const char *name)
{
	jr_delproperty(J, js_toobject(J, idx), name);
}

void js_def_accessor(js_State *J, int idx, const char *name, int atts)
{
	jr_defproperty(J, js_toobject(J, idx), name, atts, NULL, jsR_tofunction(J, -2), jsR_tofunction(J, -1));
	js_pop(J, 2);
}

int js_has_prop(js_State *J, int idx, const char *name)
{
	return jr_hasproperty(J, js_toobject(J, idx), name);
}

/* Iterator */

void js_push_iterator(js_State *J, int idx, int own)
{
	js_push_object(J, jp_newiterator(J, js_toobject(J, idx), own));
}

const char *js_next_iterator(js_State *J, int idx)
{
	return jp_nextiterator(J, js_toobject(J, idx));
}

/* Environment records */

js_Env *jsR_newenvironment(js_State *J, js_Object *vars, js_Env *outer)
{
	js_Env *E = js_malloc(J, sizeof *E);
	E->gcmark = 0;
	E->gcnext = J->gcenv;
	J->gcenv = E;
	++J->gccounter;

	E->outer     = outer;
	E->variables = vars;
	return E;
}

static void js_initvar(js_State *J, const char *name, int idx)
{
	jr_defproperty(J, J->E->variables, name, JS_DONTENUM | JS_DONTCONF, stackidx(J, idx), NULL, NULL);
}

static void js_defvar(js_State *J, const char *name)
{
	jr_defproperty(J, J->E->variables, name, JS_DONTENUM | JS_DONTCONF, NULL, NULL, NULL);
}

static int js_hasvar(js_State *J, const char *name)
{
	js_Env *E = J->E;
	do {
		js_Property *ref = jp_getproperty(J, E->variables, name);
		if (ref) {
			if (ref->getter) {
				js_push_object(J, ref->getter);
				js_push_object(J, E->variables);
				js_call(J, 0);
			} else {
				js_push_value(J, ref->value);
			}
			return 1;
		}
		E = E->outer;
	} while (E);
	return 0;
}

static void js_setvar(js_State *J, const char *name)
{
	js_Env *E = J->E;
	do {
		js_Property *ref = jp_getproperty(J, E->variables, name);
		if (ref) {
			if (ref->setter) {
				js_push_object(J, ref->setter);
				js_push_object(J, E->variables);
				js_copy(J, -3);
				js_call(J, 1);
				js_pop(J, 1);
				return;
			}
			if (!(ref->atts & JS_READONLY))
				ref->value = *stackidx(J, -1);
			else if (J->strict)
				js_error_type(J, "'%s' is read-only", name);
			return;
		}
		E = E->outer;
	} while (E);
	if (J->strict)
		js_error_ref(J, "assignment to undeclared variable '%s'", name);
	jr_setproperty(J, J->G, name);
}

static int js_delvar(js_State *J, const char *name)
{
	js_Env *E = J->E;
	do {
		js_Property *ref = jp_getownproperty(J, E->variables, name);
		if (ref) {
			if (ref->atts & JS_DONTCONF) {
				if (J->strict)
					js_error_type(J, "'%s' is non-configurable", name);
				return 0;
			}
			jp_delproperty(J, E->variables, name);
			return 1;
		}
		E = E->outer;
	} while (E);
	return jr_delproperty(J, J->G, name);
}

/* Function calls */

static void jsR_savescope(js_State *J, js_Env *newE)
{
	if (J->envtop + 1 >= JS_ENVLIMIT)
		js_stackoverflow(J);
	J->envstack[J->envtop++] = J->E;
	J->E = newE;
}

static void jsR_restorescope(js_State *J)
{
	J->E = J->envstack[--J->envtop];
}

static void jsR_calllwfunction(js_State *J, int n, js_Function *F, js_Env *scope)
{
	js_Value v;
	int i;

	jsR_savescope(J, scope);

	if (n > F->numparams) {
		js_pop(J, n - F->numparams);
		n = F->numparams;
	}
	for (i = n; i < F->varlen; ++i)
		js_push_undef(J);

	jsR_run(J, F);
	v = *stackidx(J, -1);
	TOP = --BOT; /* clear stack */
	js_push_value(J, v);

	jsR_restorescope(J);
}

static void jsR_callfunction(js_State *J, int n, js_Function *F, js_Env *scope)
{
	js_Value v;
	int i;

	scope = jsR_newenvironment(J, js_newobject(J, JS_COBJECT, NULL), scope);

	jsR_savescope(J, scope);

	if (F->arguments) {
		js_new_object(J);
		if (!J->strict) {
			js_cur_function(J);
			js_def_prop(J, -2, "callee", JS_DONTENUM);
		}
		js_push_number(J, n);
		js_def_prop(J, -2, "length", JS_DONTENUM);
		for (i = 0; i < n; ++i) {
			js_copy(J, i + 1);
			js_set_index(J, -2, i);
		}
		js_initvar(J, "arguments", -1);
		js_pop(J, 1);
	}

	for (i = 0; i < F->numparams; ++i) {
		if (i < n)
			js_initvar(J, F->vartab[i], i + 1);
		else {
			js_push_undef(J);
			js_initvar(J, F->vartab[i], -1);
			js_pop(J, 1);
		}
	}
	js_pop(J, n);

	jsR_run(J, F);
	v = *stackidx(J, -1);
	TOP = --BOT; /* clear stack */
	js_push_value(J, v);

	jsR_restorescope(J);
}

static void jsR_callscript(js_State *J, int n, js_Function *F, js_Env *scope)
{
	js_Value v;

	if (scope)
		jsR_savescope(J, scope);

	js_pop(J, n);
	jsR_run(J, F);
	v = *stackidx(J, -1);
	TOP = --BOT; /* clear stack */
	js_push_value(J, v);

	if (scope)
		jsR_restorescope(J);
}

static void jsR_callcfunction(js_State *J, int n, int min, js_CFunction F)
{
	int i;
	js_Value v;

	for (i = n; i < min; ++i)
		js_push_undef(J);

	F(J);
	v = *stackidx(J, -1);
	TOP = --BOT; /* clear stack */
	js_push_value(J, v);
}

static void jsR_pushtrace(js_State *J, const char *name, const char *file, int line)
{
	if (J->tracetop + 1 == JS_ENVLIMIT)
		js_error(J, "call stack overflow");
	++J->tracetop;
	J->trace[J->tracetop].name = name;
	J->trace[J->tracetop].file = file;
	J->trace[J->tracetop].line = line;
}

void js_call(js_State *J, int n)
{
	js_Object *obj;
	int savebot;

	if (!js_is_callable(J, -n-2))
		js_error_type(J, "called object is not a function");

	obj = js_toobject(J, -n-2);

	savebot = BOT;
	BOT = TOP - n - 1;

	if (obj->type == JS_CFUNCTION) {
		jsR_pushtrace(J, obj->u.f.function->name, obj->u.f.function->filename, obj->u.f.function->line);
		if (obj->u.f.function->lightweight)
			jsR_calllwfunction(J, n, obj->u.f.function, obj->u.f.scope);
		else
			jsR_callfunction(J, n, obj->u.f.function, obj->u.f.scope);
		--J->tracetop;
	} else if (obj->type == JS_CSCRIPT) {
		jsR_pushtrace(J, obj->u.f.function->name, obj->u.f.function->filename, obj->u.f.function->line);
		jsR_callscript(J, n, obj->u.f.function, obj->u.f.scope);
		--J->tracetop;
	} else if (obj->type == JS_CCFUNCTION) {
		jsR_pushtrace(J, obj->u.c.name, "native", 0);
		jsR_callcfunction(J, n, obj->u.c.length, obj->u.c.function);
		--J->tracetop;
	}

	BOT = savebot;
}

void js_construct(js_State *J, int n)
{
	js_Object *obj;
	js_Object *prototype;
	js_Object *newobj;

	if (!js_is_callable(J, -n-1))
		js_error_type(J, "called object is not a function");

	obj = js_toobject(J, -n-1);

	/* built-in constructors create their own objects, give them a 'null' this */
	if (obj->type == JS_CCFUNCTION && obj->u.c.constructor) {
		int savebot = BOT;
		js_push_null(J);
		if (n > 0)
			js_rot(J, n + 1);
		BOT = TOP - n - 1;

		jsR_pushtrace(J, obj->u.c.name, "native", 0);
		jsR_callcfunction(J, n, obj->u.c.length, obj->u.c.constructor);
		--J->tracetop;

		BOT = savebot;
		return;
	}

	/* extract the function object's prototype property */
	js_get_prop(J, -n - 1, "prototype");
	if (js_is_object(J, -1))
		prototype = js_toobject(J, -1);
	else
		prototype = J->Object_prototype;
	js_pop(J, 1);

	/* create a new object with above prototype, and shift it into the 'this' slot */
	newobj = js_newobject(J, JS_COBJECT, prototype);
	js_push_object(J, newobj);
	if (n > 0)
		js_rot(J, n + 1);

	/* call the function */
	js_call(J, n);

	/* if result is not an object, return the original object we created */
	if (!js_is_object(J, -1)) {
		js_pop(J, 1);
		js_push_object(J, newobj);
	}
}

void js_eval(js_State *J)
{
	if (!js_is_string(J, -1))
		return;
	js_loadeval(J, "(eval)", js_tostring(J, -1));
	js_rot2pop1(J);
	js_copy(J, 0); /* copy 'this' */
	js_call(J, 0);
}

int js_pconstruct(js_State *J, int n)
{
	int savetop = TOP - n - 2;
	if (js_try(J)) {
		/* clean up the stack to only hold the error object */
		STACK[savetop] = STACK[TOP-1];
		TOP = savetop + 1;
		return 1;
	}
	js_construct(J, n);
	js_endtry(J);
	return 0;
}

int js_pcall(js_State *J, int n)
{
	int savetop = TOP - n - 2;
	if (js_try(J)) {
		/* clean up the stack to only hold the error object */
		STACK[savetop] = STACK[TOP-1];
		TOP = savetop + 1;
		return 1;
	}
	js_call(J, n);
	js_endtry(J);
	return 0;
}

/* Exceptions */

void *js_savetrypc(js_State *J, js_Instruction *pc)
{
	if (J->trytop == JS_TRYLIMIT)
		js_error(J, "try: exception stack overflow");
	J->trybuf[J->trytop].E = J->E;
	J->trybuf[J->trytop].envtop = J->envtop;
	J->trybuf[J->trytop].tracetop = J->tracetop;
	J->trybuf[J->trytop].top = J->top;
	J->trybuf[J->trytop].bot = J->bot;
	J->trybuf[J->trytop].strict = J->strict;
	J->trybuf[J->trytop].pc = pc;
	return J->trybuf[J->trytop++].buf;
}

void *js_savetry(js_State *J)
{
	if (J->trytop == JS_TRYLIMIT)
		js_error(J, "try: exception stack overflow");
	J->trybuf[J->trytop].E = J->E;
	J->trybuf[J->trytop].envtop = J->envtop;
	J->trybuf[J->trytop].tracetop = J->tracetop;
	J->trybuf[J->trytop].top = J->top;
	J->trybuf[J->trytop].bot = J->bot;
	J->trybuf[J->trytop].strict = J->strict;
	J->trybuf[J->trytop].pc = NULL;
	return J->trybuf[J->trytop++].buf;
}

void js_endtry(js_State *J)
{
	if (J->trytop == 0)
		js_error(J, "endtry: exception stack underflow");
	--J->trytop;
}

void js_throw(js_State *J)
{
	if (J->trytop > 0) {
		js_Value v = *stackidx(J, -1);
		--J->trytop;
		J->E = J->trybuf[J->trytop].E;
		J->envtop = J->trybuf[J->trytop].envtop;
		J->tracetop = J->trybuf[J->trytop].tracetop;
		J->top = J->trybuf[J->trytop].top;
		J->bot = J->trybuf[J->trytop].bot;
		J->strict = J->trybuf[J->trytop].strict;
		js_push_value(J, v);
		longjmp(J->trybuf[J->trytop].buf, 1);
	}
	if (J->panic)
		J->panic(J);
	abort();
}

/* Main interpreter loop */

static void jr_dump_stack(js_State *J)
{
	int i;
	printf("stack {\n");
	for (i = 0; i < TOP; ++i) {
		putchar(i == BOT ? '>' : ' ');
		printf("% 4d: ", i);
		js_dumpvalue(J, STACK[i]);
		putchar('\n');
	}
	printf("}\n");
}

static void jr_dump_env(js_State *J, js_Env *E, int d)
{
	printf("scope %d ", d);
	js_dumpobject(J, E->variables);
	if (E->outer)
		jr_dump_env(J, E->outer, d+1);
}

void js_stacktrace(js_State *J)
{
	int n;
	printf("stack trace:\n");
	for (n = J->tracetop; n >= 0; --n) {
		const char *name = J->trace[n].name;
		const char *file = J->trace[n].file;
		int line = J->trace[n].line;
		if (line > 0) {
			if (name[0])
				printf("\tat %s (%s:%d)\n", name, file, line);
			else
				printf("\tat %s:%d\n", file, line);
		} else
			printf("\tat %s (%s)\n", name, file);
	}
}

void js_trap(js_State *J, int pc)
{
	if (pc > 0) {
		js_Function *F = STACK[BOT-1].u.object->u.f.function;
		printf("trap at %d in function ", pc);
		jc_dump_function(J, F);
	}
	jr_dump_stack(J);
	jr_dump_env(J, J->E, 0);
	js_stacktrace(J);
}

static void jsR_run(js_State *J, js_Function *F)
{
	js_Function **FT = F->funtab;
	double *NT = F->numtab;
	const char **ST = F->strtab;
	js_Instruction *pcstart = F->code;
	js_Instruction *pc = F->code;
	enum js_OpCode opcode;
	int offset;
	int savestrict;

	const char *str;
	js_Object *obj;
	double x, y;
	unsigned int ux, uy;
	int ix, iy, okay;
	int b;

	savestrict = J->strict;
	J->strict  = F->strict;

	while (1) {
		if (J->gccounter > JS_GCLIMIT) {
			J->gccounter = 0;
			js_gc(J, 0);
		}

		opcode = *pc++;
		switch (opcode) {
		case OP_POP:  js_pop(J, 1); break;
		case OP_DUP:  js_dup(J); break;
		case OP_DUP2: js_dup2(J); break;
		case OP_ROT2: js_rot2(J); break;
		case OP_ROT3: js_rot3(J); break;
		case OP_ROT4: js_rot4(J); break;

		case OP_NUMBER_0:   js_push_number(J, 0); break;
		case OP_NUMBER_1:   js_push_number(J, 1); break;
		case OP_NUMBER_POS: js_push_number(J, *pc++); break;
		case OP_NUMBER_NEG: js_push_number(J, -(*pc++)); break;
		case OP_NUMBER:     js_push_number(J, NT[*pc++]); break;
		case OP_STRING:     js_push_literal(J, ST[*pc++]); break;

		case OP_CLOSURE:   js_new_function(J, FT[*pc++], J->E); break;
		case OP_NEWOBJECT: js_new_object(J); break;
		case OP_NEWARRAY:  js_new_array(J); break;
		case OP_NEWREGEXP: js_new_regexp(J, ST[pc[0]], pc[1]); pc += 2; break;

		case OP_UNDEF: js_push_undef(J); break;
		case OP_NULL:  js_push_null(J); break;
		case OP_TRUE:  js_push_bool(J, 1); break;
		case OP_FALSE: js_push_bool(J, 0); break;

		case OP_THIS:
			if (J->strict) {
				js_copy(J, 0);
			} else {
				if (js_is_coercible(J, 0))
					js_copy(J, 0);
				else
					js_push_global(J);
			}
			break;

		case OP_CURRENT:
			js_cur_function(J);
			break;

		case OP_INITLOCAL:
			STACK[BOT + *pc++] = STACK[--TOP];
			break;

		case OP_GETLOCAL:
			CHECKSTACK(1);
			STACK[TOP++] = STACK[BOT + *pc++];
			break;

		case OP_SETLOCAL:
			STACK[BOT + *pc++] = STACK[TOP-1];
			break;

		case OP_DELLOCAL:
			++pc;
			js_push_bool(J, 0);
			break;

		case OP_INITVAR:
			js_initvar(J, ST[*pc++], -1);
			js_pop(J, 1);
			break;

		case OP_DEFVAR:
			js_defvar(J, ST[*pc++]);
			break;

		case OP_GETVAR:
			str = ST[*pc++];
			if (!js_hasvar(J, str))
				js_error_ref(J, "'%s' is not defined", str);
			break;

		case OP_HASVAR:
			if (!js_hasvar(J, ST[*pc++]))
				js_push_undef(J);
			break;

		case OP_SETVAR:
			js_setvar(J, ST[*pc++]);
			break;

		case OP_DELVAR:
			b = js_delvar(J, ST[*pc++]);
			js_push_bool(J, b);
			break;

		case OP_IN:
			str = js_tostring(J, -2);
			if (!js_is_object(J, -1))
				js_error_type(J, "operand to 'in' is not an object");
			b = js_has_prop(J, -1, str);
			js_pop(J, 2 + b);
			js_push_bool(J, b);
			break;

		case OP_INITPROP:
			obj = js_toobject(J, -3);
			str = js_tostring(J, -2);
			jr_setproperty(J, obj, str);
			js_pop(J, 2);
			break;

		case OP_INITGETTER:
			obj = js_toobject(J, -3);
			str = js_tostring(J, -2);
			jr_defproperty(J, obj, str, 0, NULL, jsR_tofunction(J, -1), NULL);
			js_pop(J, 2);
			break;

		case OP_INITSETTER:
			obj = js_toobject(J, -3);
			str = js_tostring(J, -2);
			jr_defproperty(J, obj, str, 0, NULL, NULL, jsR_tofunction(J, -1));
			js_pop(J, 2);
			break;

		case OP_GETPROP:
			str = js_tostring(J, -1);
			obj = js_toobject(J, -2);
			jr_getproperty(J, obj, str);
			js_rot3pop2(J);
			break;

		case OP_GETPROP_S:
			str = ST[*pc++];
			obj = js_toobject(J, -1);
			jr_getproperty(J, obj, str);
			js_rot2pop1(J);
			break;

		case OP_SETPROP:
			str = js_tostring(J, -2);
			obj = js_toobject(J, -3);
			jr_setproperty(J, obj, str);
			js_rot3pop2(J);
			break;

		case OP_SETPROP_S:
			str = ST[*pc++];
			obj = js_toobject(J, -2);
			jr_setproperty(J, obj, str);
			js_rot2pop1(J);
			break;

		case OP_DELPROP:
			str = js_tostring(J, -1);
			obj = js_toobject(J, -2);
			b = jr_delproperty(J, obj, str);
			js_pop(J, 2);
			js_push_bool(J, b);
			break;

		case OP_DELPROP_S:
			str = ST[*pc++];
			obj = js_toobject(J, -1);
			b = jr_delproperty(J, obj, str);
			js_pop(J, 1);
			js_push_bool(J, b);
			break;

		case OP_ITERATOR:
			if (!js_is_undef(J, -1) && !js_is_null(J, -1)) {
				obj = jp_newiterator(J, js_toobject(J, -1), 0);
				js_pop(J, 1);
				js_push_object(J, obj);
			}
			break;

		case OP_NEXTITER:
			obj = js_toobject(J, -1);
			str = jp_nextiterator(J, obj);
			if (str) {
				js_push_literal(J, str);
				js_push_bool(J, 1);
			} else {
				js_pop(J, 1);
				js_push_bool(J, 0);
			}
			break;

		/* Function calls */

		case OP_EVAL:
			js_eval(J);
			break;

		case OP_CALL:
			js_call(J, *pc++);
			break;

		case OP_NEW:
			js_construct(J, *pc++);
			break;

		/* Unary operators */

		case OP_TYPEOF:
			str = js_typeof(J, -1);
			js_pop(J, 1);
			js_push_literal(J, str);
			break;

		case OP_POS:
			x = js_tonumber(J, -1);
			js_pop(J, 1);
			js_push_number(J, x);
			break;

		case OP_NEG:
			x = js_tonumber(J, -1);
			js_pop(J, 1);
			js_push_number(J, -x);
			break;

		case OP_BITNOT:
			ix = js_tointeger(J, -1);
			js_pop(J, 1);
			js_push_number(J, ~ix);
			break;

		case OP_LOGNOT:
			b = js_toboolean(J, -1);
			js_pop(J, 1);
			js_push_bool(J, !b);
			break;

		case OP_INC:
			x = js_tonumber(J, -1);
			js_pop(J, 1);
			js_push_number(J, x + 1);
			break;

		case OP_DEC:
			x = js_tonumber(J, -1);
			js_pop(J, 1);
			js_push_number(J, x - 1);
			break;

		case OP_POSTINC:
			x = js_tonumber(J, -1);
			js_pop(J, 1);
			js_push_number(J, x + 1);
			js_push_number(J, x);
			break;

		case OP_POSTDEC:
			x = js_tonumber(J, -1);
			js_pop(J, 1);
			js_push_number(J, x - 1);
			js_push_number(J, x);
			break;

		/* Multiplicative operators */

		case OP_MUL:
			x = js_tonumber(J, -2);
			y = js_tonumber(J, -1);
			js_pop(J, 2);
			js_push_number(J, x * y);
			break;

		case OP_DIV:
			x = js_tonumber(J, -2);
			y = js_tonumber(J, -1);
			js_pop(J, 2);
			js_push_number(J, x / y);
			break;

		case OP_MOD:
			x = js_tonumber(J, -2);
			y = js_tonumber(J, -1);
			js_pop(J, 2);
			js_push_number(J, fmod(x, y));
			break;

		/* Additive operators */

		case OP_ADD:
			js_concat(J);
			break;

		case OP_SUB:
			x = js_tonumber(J, -2);
			y = js_tonumber(J, -1);
			js_pop(J, 2);
			js_push_number(J, x - y);
			break;

		/* Shift operators */

		case OP_SHL:
			ix = js_toi32(J, -2);
			uy = js_tou32(J, -1);
			js_pop(J, 2);
			js_push_number(J, ix << (uy & 0x1F));
			break;

		case OP_SHR:
			ix = js_toi32(J, -2);
			uy = js_tou32(J, -1);
			js_pop(J, 2);
			js_push_number(J, ix >> (uy & 0x1F));
			break;

		case OP_USHR:
			ux = js_tou32(J, -2);
			uy = js_tou32(J, -1);
			js_pop(J, 2);
			js_push_number(J, ux >> (uy & 0x1F));
			break;

		/* Relational operators */

		case OP_LT: b = js_compare(J, &okay); js_pop(J, 2); js_push_bool(J, okay && b < 0); break;
		case OP_GT: b = js_compare(J, &okay); js_pop(J, 2); js_push_bool(J, okay && b > 0); break;
		case OP_LE: b = js_compare(J, &okay); js_pop(J, 2); js_push_bool(J, okay && b <= 0); break;
		case OP_GE: b = js_compare(J, &okay); js_pop(J, 2); js_push_bool(J, okay && b >= 0); break;

		case OP_INSTANCEOF:
			b = js_instanceof(J);
			js_pop(J, 2);
			js_push_bool(J, b);
			break;

		/* Equality */

		case OP_EQ: b = js_equal(J); js_pop(J, 2); js_push_bool(J, b); break;
		case OP_NE: b = js_equal(J); js_pop(J, 2); js_push_bool(J, !b); break;
		case OP_STRICTEQ: b = js_equal_strict(J); js_pop(J, 2); js_push_bool(J, b); break;
		case OP_STRICTNE: b = js_equal_strict(J); js_pop(J, 2); js_push_bool(J, !b); break;

		case OP_JCASE:
			offset = *pc++;
			b = js_equal_strict(J);
			if (b) {
				js_pop(J, 2);
				pc = pcstart + offset;
			} else {
				js_pop(J, 1);
			}
			break;

		/* Binary bitwise operators */

		case OP_BITAND:
			ix = js_toi32(J, -2);
			iy = js_toi32(J, -1);
			js_pop(J, 2);
			js_push_number(J, ix & iy);
			break;

		case OP_BITXOR:
			ix = js_toi32(J, -2);
			iy = js_toi32(J, -1);
			js_pop(J, 2);
			js_push_number(J, ix ^ iy);
			break;

		case OP_BITOR:
			ix = js_toi32(J, -2);
			iy = js_toi32(J, -1);
			js_pop(J, 2);
			js_push_number(J, ix | iy);
			break;

		/* Try and Catch */

		case OP_THROW:
			js_throw(J);

		case OP_TRY:
			offset = *pc++;
			if (js_trypc(J, pc)) {
				pc = J->trybuf[J->trytop].pc;
			} else {
				pc = pcstart + offset;
			}
			break;

		case OP_ENDTRY:
			js_endtry(J);
			break;

		case OP_CATCH:
			str = ST[*pc++];
			obj = js_newobject(J, JS_COBJECT, NULL);
			js_push_object(J, obj);
			js_rot2(J);
			js_set_prop(J, -2, str);
			J->E = jsR_newenvironment(J, obj, J->E);
			js_pop(J, 1);
			break;

		case OP_ENDCATCH:
			J->E = J->E->outer;
			break;

		/* With */

		case OP_WITH:
			obj = js_toobject(J, -1);
			J->E = jsR_newenvironment(J, obj, J->E);
			js_pop(J, 1);
			break;

		case OP_ENDWITH:
			J->E = J->E->outer;
			break;

		/* Branching */

		case OP_DEBUGGER:
			js_trap(J, (int)(pc - pcstart) - 1);
			break;

		case OP_JUMP:
			pc = pcstart + *pc;
			break;

		case OP_JTRUE:
			offset = *pc++;
			b = js_toboolean(J, -1);
			js_pop(J, 1);
			if (b)
				pc = pcstart + offset;
			break;

		case OP_JFALSE:
			offset = *pc++;
			b = js_toboolean(J, -1);
			js_pop(J, 1);
			if (!b)
				pc = pcstart + offset;
			break;

		case OP_RETURN:
			J->strict = savestrict;
			return;

		case OP_LINE:
			J->trace[J->tracetop].line = *pc++;
			break;
		}
	}
}
