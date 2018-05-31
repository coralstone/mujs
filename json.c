#include "jsi.h"
#include "jslex.h"
#include "jsvalue.h"
#include "jsbuiltin.h"

#include "utf.h"

static void jsonnext(js_State *J)
{
	J->lookahead = jsY_lexjson(J);
}

static int jsonaccept(js_State *J, int t)
{
	if (J->lookahead == t) {
		jsonnext(J);
		return 1;
	}
	return 0;
}

static void jsonexpect(js_State *J, int t)
{
	if (!jsonaccept(J, t))
		js_error_syntax(J, "JSON: unexpected token: %s (expected %s)",
				jsY_tokenstring(J->lookahead), jsY_tokenstring(t));
}

static void jsonvalue(js_State *J)
{
	int i;
	const char *name;

	switch (J->lookahead) {
	case TK_STRING:
		js_push_string(J, J->text);
		jsonnext(J);
		break;

	case TK_NUMBER:
		js_push_number(J, J->number);
		jsonnext(J);
		break;

	case '{':
		js_new_object(J);
		jsonnext(J);
		if (jsonaccept(J, '}'))
			return;
		do {
			if (J->lookahead != TK_STRING)
				js_error_syntax(J, "JSON: unexpected token: %s (expected string)", jsY_tokenstring(J->lookahead));
			name = J->text;
			jsonnext(J);
			jsonexpect(J, ':');
			jsonvalue(J);
			js_set_prop(J, -2, name);
		} while (jsonaccept(J, ','));
		jsonexpect(J, '}');
		break;

	case '[':
		js_new_array(J);
		jsonnext(J);
		i = 0;
		if (jsonaccept(J, ']'))
			return;
		do {
			jsonvalue(J);
			js_set_index(J, -2, i++);
		} while (jsonaccept(J, ','));
		jsonexpect(J, ']');
		break;

	case TK_TRUE:
		js_push_bool(J, 1);
		jsonnext(J);
		break;

	case TK_FALSE:
		js_push_bool(J, 0);
		jsonnext(J);
		break;

	case TK_NULL:
		js_push_null(J);
		jsonnext(J);
		break;

	default:
		js_error_syntax(J, "JSON: unexpected token: %s", jsY_tokenstring(J->lookahead));
	}
}

static void jsonrevive(js_State *J, const char *name)
{
	const char *key;
	char buf[32];

	/* revive is in 2 */
	/* holder is in -1 */

	js_get_prop(J, -1, name); /* get value from holder */

	if (js_is_object(J, -1)) {
		if (js_is_array(J, -1)) {
			int i = 0;
			int n = js_get_length(J, -1);
			for (i = 0; i < n; ++i) {
				jsonrevive(J, js_itoa(buf, i));
				if (js_is_undef(J, -1)) {
					js_pop(J, 1);
					js_del_prop(J, -1, buf);
				} else {
					js_set_prop(J, -2, buf);
				}
			}
		} else {
			js_push_iterator(J, -1, 1);
			while ((key = js_next_iterator(J, -1))) {
				js_rot2(J);
				jsonrevive(J, key);
				if (js_is_undef(J, -1)) {
					js_pop(J, 1);
					js_del_prop(J, -1, key);
				} else {
					js_set_prop(J, -2, key);
				}
				js_rot2(J);
			}
			js_pop(J, 1);
		}
	}

	js_copy(J, 2); /* reviver function */
	js_copy(J, -3); /* holder as this */
	js_push_string(J, name); /* name */
	js_copy(J, -4); /* value */
	js_call(J, 2);
	js_rot2pop1(J); /* pop old value, leave new value on stack */
}

static void JSON_parse(js_State *J)
{
	const char *source = js_tostring(J, 1);
	jsY_initlex(J, "JSON", source);
	jsonnext(J);

	if (js_is_callable(J, 2)) {
		js_new_object(J);
		jsonvalue(J);
		js_def_prop(J, -2, "", 0);
		jsonrevive(J, "");
	} else {
		jsonvalue(J);
	}
}

static void fmtnum(js_State *J, js_Buffer **sb, double n)
{
	if (isnan(n)) js_puts(J, sb, "null");
	else if (isinf(n)) js_puts(J, sb, "null");
	else if (n == 0) js_puts(J, sb, "0");
	else {
		char buf[40];
		js_puts(J, sb, jsV_ntos(J, buf, n));
	}
}

static void fmtstr(js_State *J, js_Buffer **sb, const char *s)
{
	static const char *HEX = "0123456789ABCDEF";
	Rune c;
	js_putc(J, sb, '"');
	while (*s) {
		s += chartorune(&c, s);
		switch (c) {
		case '"': js_puts(J, sb, "\\\""); break;
		case '\\': js_puts(J, sb, "\\\\"); break;
		case '\b': js_puts(J, sb, "\\b"); break;
		case '\f': js_puts(J, sb, "\\f"); break;
		case '\n': js_puts(J, sb, "\\n"); break;
		case '\r': js_puts(J, sb, "\\r"); break;
		case '\t': js_puts(J, sb, "\\t"); break;
		default:
			if (c < ' ' || c > 127) {
				js_puts(J, sb, "\\u");
				js_putc(J, sb, HEX[(c>>12)&15]);
				js_putc(J, sb, HEX[(c>>8)&15]);
				js_putc(J, sb, HEX[(c>>4)&15]);
				js_putc(J, sb, HEX[c&15]);
			} else {
				js_putc(J, sb, c); break;
			}
		}
	}
	js_putc(J, sb, '"');
}

static void fmtindent(js_State *J, js_Buffer **sb, const char *gap, int level)
{
	js_putc(J, sb, '\n');
	while (level--)
		js_puts(J, sb, gap);
}

static int fmtvalue(js_State *J, js_Buffer **sb, const char *key, const char *gap, int level);

static void fmtobject(js_State *J, js_Buffer **sb, js_Object *obj, const char *gap, int level)
{
	const char *key;
	int save;
	int i, n;

	n = js_gettop(J) - 1;
	for (i = 4; i < n; ++i)
		if (js_is_object(J, i))
			if (js_toobject(J, i) == js_toobject(J, -1))
				js_error_type(J, "cyclic object value");

	n = 0;
	js_putc(J, sb, '{');
	js_push_iterator(J, -1, 1);
	while ((key = js_next_iterator(J, -1))) {
		save = (*sb)->n;
		if (n) js_putc(J, sb, ',');
		if (gap) fmtindent(J, sb, gap, level + 1);
		fmtstr(J, sb, key);
		js_putc(J, sb, ':');
		if (gap)
			js_putc(J, sb, ' ');
		js_rot2(J);
		if (!fmtvalue(J, sb, key, gap, level + 1))
			(*sb)->n = save;
		else
			++n;
		js_rot2(J);
	}
	js_pop(J, 1);
	if (gap && n) fmtindent(J, sb, gap, level);
	js_putc(J, sb, '}');
}

static void fmtarray(js_State *J, js_Buffer **sb, const char *gap, int level)
{
	int n, i;
	char buf[32];

	n = js_gettop(J) - 1;
	for (i = 4; i < n; ++i)
		if (js_is_object(J, i))
			if (js_toobject(J, i) == js_toobject(J, -1))
				js_error_type(J, "cyclic object value");

	js_putc(J, sb, '[');
	n = js_get_length(J, -1);
	for (i = 0; i < n; ++i) {
		if (i) js_putc(J, sb, ',');
		if (gap) fmtindent(J, sb, gap, level + 1);
		if (!fmtvalue(J, sb, js_itoa(buf, i), gap, level + 1))
			js_puts(J, sb, "null");
	}
	if (gap && n) fmtindent(J, sb, gap, level);
	js_putc(J, sb, ']');
}

static int fmtvalue(js_State *J, js_Buffer **sb, const char *key, const char *gap, int level)
{
	/* replacer is in 2 */
	/* holder is in -1 */

	js_get_prop(J, -1, key);

	if (js_is_object(J, -1)) {
		if (js_has_prop(J, -1, "toJSON")) {
			if (js_is_callable(J, -1)) {
				js_copy(J, -2);
				js_push_string(J, key);
				js_call(J, 1);
				js_rot2pop1(J);
			} else {
				js_pop(J, 1);
			}
		}
	}

	if (js_is_callable(J, 2)) {
		js_copy(J, 2); /* replacer function */
		js_copy(J, -3); /* holder as this */
		js_push_string(J, key); /* name */
		js_copy(J, -4); /* old value */
		js_call(J, 2);
		js_rot2pop1(J); /* pop old value, leave new value on stack */
	}

	if (js_is_object(J, -1) && !js_is_callable(J, -1)) {
		js_Object *obj = js_toobject(J, -1);
		switch (obj->type) {
		case JS_CNUMBER: fmtnum(J, sb, obj->u.number); break;
		case JS_CSTRING: fmtstr(J, sb, obj->u.s.string); break;
		case JS_CBOOLEAN: js_puts(J, sb, obj->u.boolean ? "true" : "false"); break;
		case JS_CARRAY: fmtarray(J, sb, gap, level); break;
		default: fmtobject(J, sb, obj, gap, level); break;
		}
	}
	else if (js_is_bool(J, -1))
		js_puts(J, sb, js_toboolean(J, -1) ? "true" : "false");
	else if (js_is_number(J, -1))
		fmtnum(J, sb, js_tonumber(J, -1));
	else if (js_is_string(J, -1))
		fmtstr(J, sb, js_tostring(J, -1));
	else if (js_is_null(J, -1))
		js_puts(J, sb, "null");
	else {
		js_pop(J, 1);
		return 0;
	}

	js_pop(J, 1);
	return 1;
}

static void JSON_stringify(js_State *J)
{
	js_Buffer *sb = NULL;
	char buf[12];
	const char *s, *gap;
	int n;

	gap = NULL;

	if (js_is_number(J, 3)) {
		n = js_tointeger(J, 3);
		if (n < 0) n = 0;
		if (n > 10) n = 10;
		memset(buf, ' ', n);
		buf[n] = 0;
		if (n > 0) gap = buf;
	} else if (js_is_string(J, 3)) {
		s = js_tostring(J, 3);
		n = strlen(s);
		if (n > 10) n = 10;
		memcpy(buf, s, n);
		buf[n] = 0;
		if (n > 0) gap = buf;
	}

	if (js_try(J)) {
		js_free(J, sb);
		js_throw(J);
	}

	js_new_object(J); /* wrapper */
	js_copy(J, 1);
	js_def_prop(J, -2, "", 0);
	if (!fmtvalue(J, &sb, "", gap, 0)) {
		js_push_undef(J);
	} else {
		js_putc(J, &sb, 0);
		js_push_string(J, sb ? sb->s : "");
		js_rot2pop1(J);
	}

	js_endtry(J);
	js_free(J, sb);
}

void jb_initjson(js_State *J)
{
	js_push_object(J, jsV_newobject(J, JS_CJSON, J->Object_prototype));
	{
		jb_prop_func(J, "JSON.parse", JSON_parse, 2);
		jb_prop_func(J, "JSON.stringify", JSON_stringify, 3);
	}
	js_defglobal(J, "JSON", JS_DONTENUM);
}