#include "jsi.h"
#include "jsvalue.h"
#include "jsbuiltin.h"

int js_get_length(js_State *J, int idx)
{
	int len;
	js_get_prop(J, idx, "length");
	len = js_tointeger(J, -1);
	js_pop(J, 1);
	return len;
}

void js_set_length(js_State *J, int idx, int len)
{
	js_push_number(J, len);
	js_set_prop(J, idx < 0 ? idx - 1 : idx, "length");
}

int js_has_index(js_State *J, int idx, int i)
{
	char buf[32];
	return js_has_prop(J, idx, js_itoa(buf, i));
}

void js_get_index(js_State *J, int idx, int i)
{
	char buf[32];
	js_get_prop(J, idx, js_itoa(buf, i));
}

void js_set_index(js_State *J, int idx, int i)
{
	char buf[32];
	js_set_prop(J, idx, js_itoa(buf, i));
}

void js_del_index(js_State *J, int idx, int i)
{
	char buf[32];
	js_del_prop(J, idx, js_itoa(buf, i));
}

static void Ap_new_Array(js_State *J)
{
	int i, top = js_gettop(J);

	js_new_array(J);

	if (top == 2) {
		if (js_is_number(J, 1)) {
			js_copy(J, 1);
			js_set_prop(J, -2, "length");
		} else {
			js_copy(J, 1);
			js_set_index(J, -2, 0);
		}
	} else {
		for (i = 1; i < top; ++i) {
			js_copy(J, i);
			js_set_index(J, -2, i - 1);
		}
	}
}

static void Ap_concat(js_State *J)
{
	int i, top = js_gettop(J);
	int n, k, len;

	js_new_array(J);
	n = 0;

	for (i = 0; i < top; ++i) {
		js_copy(J, i);
		if (js_is_array(J, -1)) {
			len = js_get_length(J, -1);
			for (k = 0; k < len; ++k)
				if (js_has_index(J, -1, k))
					js_set_index(J, -3, n++);
			js_pop(J, 1);
		} else {
			js_set_index(J, -2, n++);
		}
	}
}

static void Ap_join(js_State *J)
{
	char * volatile out = NULL;
	const char *sep;
	const char *r;
	int seplen;
	int k, n, len;

	len = js_get_length(J, 0);

	if (js_is_def(J, 1)) {
		sep = js_tostring(J, 1);
		seplen = strlen(sep);
	} else {
		sep = ",";
		seplen = 1;
	}

	if (len == 0) {
		js_push_literal(J, "");
		return;
	}

	if (js_try(J)) {
		js_free(J, out);
		js_throw(J);
	}

	n = 1;
	for (k = 0; k < len; ++k) {
		js_get_index(J, 0, k);
		if (js_is_undef(J, -1) || js_is_null(J, -1))
			r = "";
		else
			r = js_tostring(J, -1);
		n += strlen(r);

		if (k == 0) {
			out = js_malloc(J, n);
			strcpy(out, r);
		} else {
			n += seplen;
			out = js_realloc(J, out, n);
			strcat(out, sep);
			strcat(out, r);
		}

		js_pop(J, 1);
	}

	js_push_string(J, out);
	js_endtry(J);
	js_free(J, out);
}

static void Ap_pop(js_State *J)
{
	int n;

	n = js_get_length(J, 0);

	if (n > 0) {
		js_get_index(J, 0, n - 1);
		js_del_index(J, 0, n - 1);
		js_set_length(J, 0, n - 1);
	} else {
		js_set_length(J, 0, 0);
		js_push_undef(J);
	}
}

static void Ap_push(js_State *J)
{
	int i, top = js_gettop(J);
	int n;

	n = js_get_length(J, 0);

	for (i = 1; i < top; ++i, ++n) {
		js_copy(J, i);
		js_set_index(J, 0, n);
	}

	js_set_length(J, 0, n);

	js_push_number(J, n);
}

static void Ap_reverse(js_State *J)
{
	int len, middle, lower;

	len = js_get_length(J, 0);
	middle = len / 2;
	lower = 0;

	while (lower != middle) {
		int upper = len - lower - 1;
		int haslower = js_has_index(J, 0, lower);
		int hasupper = js_has_index(J, 0, upper);
		if (haslower && hasupper) {
			js_set_index(J, 0, lower);
			js_set_index(J, 0, upper);
		} else if (hasupper) {
			js_set_index(J, 0, lower);
			js_del_index(J, 0, upper);
		} else if (haslower) {
			js_set_index(J, 0, upper);
			js_del_index(J, 0, lower);
		}
		++lower;
	}

	js_copy(J, 0);
}

static void Ap_shift(js_State *J)
{
	int k, len;

	len = js_get_length(J, 0);

	if (len == 0) {
		js_set_length(J, 0, 0);
		js_push_undef(J);
		return;
	}

	js_get_index(J, 0, 0);

	for (k = 1; k < len; ++k) {
		if (js_has_index(J, 0, k))
			js_set_index(J, 0, k - 1);
		else
			js_del_index(J, 0, k - 1);
	}

	js_del_index(J, 0, len - 1);
	js_set_length(J, 0, len - 1);
}

static void Ap_slice(js_State *J)
{
	int len, s, e, n;
	double sv, ev;

	js_new_array(J);

	len = js_get_length(J, 0);
	sv = js_tointeger(J, 1);
	ev = js_is_def(J, 2) ? js_tointeger(J, 2) : len;

	if (sv < 0) sv = sv + len;
	if (ev < 0) ev = ev + len;

	s = sv < 0 ? 0 : sv > len ? len : sv;
	e = ev < 0 ? 0 : ev > len ? len : ev;

	for (n = 0; s < e; ++s, ++n)
		if (js_has_index(J, 0, s))
			js_set_index(J, -2, n);
}

static int compare(js_State *J, int x, int y, int *hasx, int *hasy, int hasfn)
{
	const char *sx, *sy;
	int c;

	*hasx = js_has_index(J, 0, x);
	*hasy = js_has_index(J, 0, y);

	if (*hasx && *hasy) {
		int unx = js_is_undef(J, -2);
		int uny = js_is_undef(J, -1);
		if (unx && uny) return 0;
		if (unx) return 1;
		if (uny) return -1;

		if (hasfn) {
			js_copy(J, 1); /* copy function */
			js_push_undef(J); /* set this object */
			js_copy(J, -4); /* copy x */
			js_copy(J, -4); /* copy y */
			js_call(J, 2);
			c = js_tonumber(J, -1);
			js_pop(J, 1);
			return c;
		}

		sx = js_tostring(J, -2);
		sy = js_tostring(J, -1);
		return strcmp(sx, sy);
	}

	if (*hasx) return -1;
	if (*hasy) return 1;
	return 0;
}

static void Ap_sort(js_State *J)
{
	int len, i, k;
	int hasx, hasy, hasfn;

	len = js_get_length(J, 0);

	hasfn = js_is_callable(J, 1);
	hasx = hasy = 0;

	for (i = 1; i < len; ++i) {
		k = i;
		while (k > 0 && compare(J, k - 1, k, &hasx, &hasy, hasfn) > 0) {
			if (hasx && hasy) {
				js_set_index(J, 0, k - 1);
				js_set_index(J, 0, k);
			} else if (hasx) {
				js_del_index(J, 0, k - 1);
				js_set_index(J, 0, k);
			} else if (hasy) {
				js_set_index(J, 0, k - 1);
				js_del_index(J, 0, k);
			}
			hasx = hasy = 0;
			--k;
		}
		if (hasx + hasy > 0)
			js_pop(J, hasx + hasy);
	}

	js_copy(J, 0);
}

static void Ap_splice(js_State *J)
{
	int top = js_gettop(J);
	int len, start, del, add, k;
	double f;

	js_new_array(J);

	len = js_get_length(J, 0);

	f = js_tointeger(J, 1);
	if (f < 0) f = f + len;
	start = f < 0 ? 0 : f > len ? len : f;

	f = js_tointeger(J, 2);
	del = f < 0 ? 0 : f > len - start ? len - start : f;

	/* copy deleted items to return array */
	for (k = 0; k < del; ++k)
		if (js_has_index(J, 0, start + k))
			js_set_index(J, -2, k);
	js_set_length(J, -1, del);

	/* shift the tail to resize the hole left by deleted items */
	add = top - 3;
	if (add < del) {
		for (k = start; k < len - del; ++k) {
			if (js_has_index(J, 0, k + del))
				js_set_index(J, 0, k + add);
			else
				js_del_index(J, 0, k + add);
		}
		for (k = len; k > len - del + add; --k)
			js_del_index(J, 0, k - 1);
	} else if (add > del) {
		for (k = len - del; k > start; --k) {
			if (js_has_index(J, 0, k + del - 1))
				js_set_index(J, 0, k + add - 1);
			else
				js_del_index(J, 0, k + add - 1);
		}
	}

	/* copy new items into the hole */
	for (k = 0; k < add; ++k) {
		js_copy(J, 3 + k);
		js_set_index(J, 0, start + k);
	}

	js_set_length(J, 0, len - del + add);
}

static void Ap_unshift(js_State *J)
{
	int i, top = js_gettop(J);
	int k, len;

	len = js_get_length(J, 0);

	for (k = len; k > 0; --k) {
		int from = k - 1;
		int to = k + top - 2;
		if (js_has_index(J, 0, from))
			js_set_index(J, 0, to);
		else
			js_del_index(J, 0, to);
	}

	for (i = 1; i < top; ++i) {
		js_copy(J, i);
		js_set_index(J, 0, i - 1);
	}

	js_set_length(J, 0, len + top - 1);

	js_push_number(J, len + top - 1);
}

static void Ap_toString(js_State *J)
{
	int top = js_gettop(J);
	js_pop(J, top - 1);
	Ap_join(J);
}

static void Ap_indexOf(js_State *J)
{
	int k, len, from;

	len = js_get_length(J, 0);
	from = js_is_def(J, 2) ? js_tointeger(J, 2) : 0;
	if (from < 0) from = len + from;
	if (from < 0) from = 0;

	js_copy(J, 1);
	for (k = from; k < len; ++k) {
		if (js_has_index(J, 0, k)) {
			if (js_equal_strict(J)) {
				js_push_number(J, k);
				return;
			}
			js_pop(J, 1);
		}
	}

	js_push_number(J, -1);
}

static void Ap_lastIndexOf(js_State *J)
{
	int k, len, from;

	len = js_get_length(J, 0);
	from = js_is_def(J, 2) ? js_tointeger(J, 2) : len - 1;
	if (from > len - 1) from = len - 1;
	if (from < 0) from = len + from;

	js_copy(J, 1);
	for (k = from; k >= 0; --k) {
		if (js_has_index(J, 0, k)) {
			if (js_equal_strict(J)) {
				js_push_number(J, k);
				return;
			}
			js_pop(J, 1);
		}
	}

	js_push_number(J, -1);
}

static void Ap_every(js_State *J)
{
	int hasthis = js_gettop(J) >= 3;
	int k, len;

	if (!js_is_callable(J, 1))
		js_error_type(J, "callback is not a function");

	len = js_get_length(J, 0);
	for (k = 0; k < len; ++k) {
		if (js_has_index(J, 0, k)) {
			js_copy(J, 1);
			if (hasthis)
				js_copy(J, 2);
			else
				js_push_undef(J);
			js_copy(J, -3);
			js_push_number(J, k);
			js_copy(J, 0);
			js_call(J, 3);
			if (!js_toboolean(J, -1))
				return;
			js_pop(J, 2);
		}
	}

	js_push_bool(J, 1);
}

static void Ap_some(js_State *J)
{
	int hasthis = js_gettop(J) >= 3;
	int k, len;

	if (!js_is_callable(J, 1))
		js_error_type(J, "callback is not a function");

	len = js_get_length(J, 0);
	for (k = 0; k < len; ++k) {
		if (js_has_index(J, 0, k)) {
			js_copy(J, 1);
			if (hasthis)
				js_copy(J, 2);
			else
				js_push_undef(J);
			js_copy(J, -3);
			js_push_number(J, k);
			js_copy(J, 0);
			js_call(J, 3);
			if (js_toboolean(J, -1))
				return;
			js_pop(J, 2);
		}
	}

	js_push_bool(J, 0);
}

static void Ap_forEach(js_State *J)
{
	int hasthis = js_gettop(J) >= 3;
	int k, len;

	if (!js_is_callable(J, 1))
		js_error_type(J, "callback is not a function");

	len = js_get_length(J, 0);
	for (k = 0; k < len; ++k) {
		if (js_has_index(J, 0, k)) {
			js_copy(J, 1);
			if (hasthis)
				js_copy(J, 2);
			else
				js_push_undef(J);
			js_copy(J, -3);
			js_push_number(J, k);
			js_copy(J, 0);
			js_call(J, 3);
			js_pop(J, 2);
		}
	}

	js_push_undef(J);
}

static void Ap_map(js_State *J)
{
	int hasthis = js_gettop(J) >= 3;
	int k, len;

	if (!js_is_callable(J, 1))
		js_error_type(J, "callback is not a function");

	js_new_array(J);

	len = js_get_length(J, 0);
	for (k = 0; k < len; ++k) {
		if (js_has_index(J, 0, k)) {
			js_copy(J, 1);
			if (hasthis)
				js_copy(J, 2);
			else
				js_push_undef(J);
			js_copy(J, -3);
			js_push_number(J, k);
			js_copy(J, 0);
			js_call(J, 3);
			js_set_index(J, -3, k);
			js_pop(J, 1);
		}
	}
}

static void Ap_filter(js_State *J)
{
	int hasthis = js_gettop(J) >= 3;
	int k, to, len;

	if (!js_is_callable(J, 1))
		js_error_type(J, "callback is not a function");

	js_new_array(J);
	to = 0;

	len = js_get_length(J, 0);
	for (k = 0; k < len; ++k) {
		if (js_has_index(J, 0, k)) {
			js_copy(J, 1);
			if (hasthis)
				js_copy(J, 2);
			else
				js_push_undef(J);
			js_copy(J, -3);
			js_push_number(J, k);
			js_copy(J, 0);
			js_call(J, 3);
			if (js_toboolean(J, -1)) {
				js_pop(J, 1);
				js_set_index(J, -2, to++);
			} else {
				js_pop(J, 2);
			}
		}
	}
}

static void Ap_reduce(js_State *J)
{
	int hasinitial = js_gettop(J) >= 3;
	int k, len;

	if (!js_is_callable(J, 1))
		js_error_type(J, "callback is not a function");

	len = js_get_length(J, 0);
	k = 0;

	if (len == 0 && !hasinitial)
		js_error_type(J, "no initial value");

	/* initial value of accumulator */
	if (hasinitial)
		js_copy(J, 2);
	else {
		while (k < len)
			if (js_has_index(J, 0, k++))
				break;
		if (k == len)
			js_error_type(J, "no initial value");
	}

	while (k < len) {
		if (js_has_index(J, 0, k)) {
			js_copy(J, 1);
			js_push_undef(J);
			js_rot(J, 4); /* accumulator on top */
			js_rot(J, 4); /* property on top */
			js_push_number(J, k);
			js_copy(J, 0);
			js_call(J, 4); /* calculate new accumulator */
		}
		++k;
	}

	/* return accumulator */
}

static void Ap_reduceRight(js_State *J)
{
	int hasinitial = js_gettop(J) >= 3;
	int k, len;

	if (!js_is_callable(J, 1))
		js_error_type(J, "callback is not a function");

	len = js_get_length(J, 0);
	k = len - 1;

	if (len == 0 && !hasinitial)
		js_error_type(J, "no initial value");

	/* initial value of accumulator */
	if (hasinitial)
		js_copy(J, 2);
	else {
		while (k >= 0)
			if (js_has_index(J, 0, k--))
				break;
		if (k < 0)
			js_error_type(J, "no initial value");
	}

	while (k >= 0) {
		if (js_has_index(J, 0, k)) {
			js_copy(J, 1);
			js_push_undef(J);
			js_rot(J, 4); /* accumulator on top */
			js_rot(J, 4); /* property on top */
			js_push_number(J, k);
			js_copy(J, 0);
			js_call(J, 4); /* calculate new accumulator */
		}
		--k;
	}

	/* return accumulator */
}

static void A_isArray(js_State *J)
{

	if (!js_is_object(J, 1)) {
         js_push_bool(J, 0);
         return;
	}

    js_Object *T = js_toobject(J, 1);
    js_push_bool(J, T->type == JS_CARRAY);
}

void jb_initarray(js_State *J)
{
	js_push_object(J, J->Array_prototype);
	{
		jb_prop_func(J, "Array.prototype.toString", Ap_toString, 0);
		jb_prop_func(J, "Array.prototype.concat", Ap_concat, 0); /* 1 */
		jb_prop_func(J, "Array.prototype.join", Ap_join, 1);
		jb_prop_func(J, "Array.prototype.pop", Ap_pop, 0);
		jb_prop_func(J, "Array.prototype.push", Ap_push, 0); /* 1 */
		jb_prop_func(J, "Array.prototype.reverse", Ap_reverse, 0);
		jb_prop_func(J, "Array.prototype.shift", Ap_shift, 0);
		jb_prop_func(J, "Array.prototype.slice", Ap_slice, 2);
		jb_prop_func(J, "Array.prototype.sort", Ap_sort, 1);
		jb_prop_func(J, "Array.prototype.splice", Ap_splice, 0); /* 2 */
		jb_prop_func(J, "Array.prototype.unshift", Ap_unshift, 0); /* 1 */

		/* ES5 */
		jb_prop_func(J, "Array.prototype.indexOf", Ap_indexOf, 1);
		jb_prop_func(J, "Array.prototype.lastIndexOf", Ap_lastIndexOf, 1);
		jb_prop_func(J, "Array.prototype.every", Ap_every, 1);
		jb_prop_func(J, "Array.prototype.some", Ap_some, 1);
		jb_prop_func(J, "Array.prototype.forEach", Ap_forEach, 1);
		jb_prop_func(J, "Array.prototype.map", Ap_map, 1);
		jb_prop_func(J, "Array.prototype.filter", Ap_filter, 1);
		jb_prop_func(J, "Array.prototype.reduce", Ap_reduce, 1);
		jb_prop_func(J, "Array.prototype.reduceRight", Ap_reduceRight, 1);
	}
	js_new_cctor(J, Ap_new_Array, Ap_new_Array, "Array", 0); /* 1 */
	{
		/* ES5 */
		jb_prop_func(J, "Array.isArray", A_isArray, 1);
	}
	js_defglobal(J, "Array", JS_DONTENUM);
}
