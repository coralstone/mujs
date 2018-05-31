#include "jsi.h"
#include "jslex.h"
#include "jscompile.h"
#include "jsvalue.h"
#include "jsbuiltin.h"


void js_cfunction(js_State *J, js_CFunction cfun, const char *name, int length)
{
	js_Object *obj = jsV_newobject(J, JS_CCFUNCTION, J->Function_prototype);
	obj->u.c.name = name;
	obj->u.c.function = cfun;
	obj->u.c.constructor = NULL;
	obj->u.c.length = length;
	js_push_object(J, obj);
	{
		js_push_number(J, length);
		js_def_prop(J, -2, "length", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
		js_new_object(J);
		{
			js_copy(J, -2);
			js_def_prop(J, -2, "constructor", JS_DONTENUM);
		}
		js_def_prop(J, -2, "prototype", JS_DONTCONF);
	}
}

/* prototype -- constructor */
void js_new_cctor(js_State *J, js_CFunction cfun, js_CFunction ccon, const char *name, int length)
{
	js_Object *obj = jsV_newobject(J, JS_CCFUNCTION, J->Function_prototype);
	obj->u.c.name = name;
	obj->u.c.function = cfun;
	obj->u.c.constructor = ccon;
	obj->u.c.length = length;
	js_push_object(J, obj); /* proto obj */
	{
		js_push_number(J, length);
		js_def_prop(J, -2, "length", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
		js_rot2(J); /* obj proto */
		js_copy(J, -2); /* obj proto obj */
		js_def_prop(J, -2, "constructor", JS_DONTENUM);
		js_def_prop(J, -2, "prototype", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
	}
}

static void jb_global_func(js_State *J, const char *name, js_CFunction cfun, int n)
{
	js_cfunction(J, cfun, name, n);
	js_defglobal(J, name, JS_DONTENUM);
}

void jb_prop_func(js_State *J, const char *name, js_CFunction cfun, int n)
{
	const char *pname = strrchr(name, '.');
	pname = pname ? pname + 1 : name;
	js_cfunction(J, cfun, name, n);
	js_def_prop(J, -2, pname, JS_DONTENUM);
}

void jb_prop_num(js_State *J, const char *name, double number)
{
	js_push_number(J, number);
	js_def_prop(J, -2, name, JS_READONLY | JS_DONTENUM | JS_DONTCONF);
}

void jb_prop_str(js_State *J, const char *name, const char *string)
{
	js_push_literal(J, string);
	js_def_prop(J, -2, name, JS_DONTENUM);
}

static void jsB_parseInt(js_State *J)
{
	const char *s = js_tostring(J, 1);
	int radix = js_is_def(J, 2) ? js_tointeger(J, 2) : 10;
	double sign = 1;
	double n;
	char *e;

	while (jsY_iswhite(*s) || jsY_isnewline(*s))
		++s;
	if (*s == '-') {
		++s;
		sign = -1;
	} else if (*s == '+') {
		++s;
	}
	if (radix == 0) {
		radix = 10;
		if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
			s += 2;
			radix = 16;
		}
	} else if (radix < 2 || radix > 36) {
		js_push_number(J, NAN);
		return;
	}
	n = strtol(s, &e, radix);
	if (s == e)
		js_push_number(J, NAN);
	else
		js_push_number(J, n * sign);
}

static void jsB_parseFloat(js_State *J)
{
	const char *s = js_tostring(J, 1);
	char *e;
	double n;

	while (jsY_iswhite(*s) || jsY_isnewline(*s)) ++s;
	if (!strncmp(s, "Infinity", 8))
		js_push_number(J, INFINITY);
	else if (!strncmp(s, "+Infinity", 9))
		js_push_number(J, INFINITY);
	else if (!strncmp(s, "-Infinity", 9))
		js_push_number(J, -INFINITY);
	else {
		n = js_stof(s, &e);
		if (e == s)
			n = NAN;

        js_push_number(J, n);
	}
}

static void jsB_isNaN(js_State *J)
{
	double n = js_tonumber(J, 1);
	js_push_bool(J, isnan(n));
}

static void jsB_isFinite(js_State *J)
{
	double n = js_tonumber(J, 1);
	js_push_bool(J, isfinite(n));
}

static void Encode(js_State *J, const char *str, const char *unescaped)
{
	js_Buffer *sb = NULL;

	static const char *HEX = "0123456789ABCDEF";

	if (js_try(J)) {
		js_free(J, sb);
		js_throw(J);
	}

	while (*str) {
		int c = (unsigned char) *str++;
		if (strchr(unescaped, c))
			js_putc(J, &sb, c);
		else {
			js_putc(J, &sb, '%');
			js_putc(J, &sb, HEX[(c >> 4) & 0xf]);
			js_putc(J, &sb, HEX[c & 0xf]);
		}
	}
	js_putc(J, &sb, 0);

	js_push_string(J, sb ? sb->s : "");
	js_endtry(J);
	js_free(J, sb);
}

static void Decode(js_State *J, const char *str, const char *reserved)
{
	js_Buffer *sb = NULL;
	int a, b;

	if (js_try(J)) {
		js_free(J, sb);
		js_throw(J);
	}

	while (*str) {
		int c = (unsigned char) *str++;
		if (c != '%')
			js_putc(J, &sb, c);
		else {
			if (!str[0] || !str[1])
				js_error_uri(J, "truncated escape sequence");
			a = *str++;
			b = *str++;
			if (!jsY_ishex(a) || !jsY_ishex(b))
				js_error_uri(J, "invalid escape sequence");
			c = jsY_tohex(a) << 4 | jsY_tohex(b);
			if (!strchr(reserved, c))
				js_putc(J, &sb, c);
			else {
				js_putc(J, &sb, '%');
				js_putc(J, &sb, a);
				js_putc(J, &sb, b);
			}
		}
	}
	js_putc(J, &sb, 0);

	js_push_string(J, sb ? sb->s : "");
	js_endtry(J);
	js_free(J, sb);
}

#define URIRESERVED ";/?:@&=+$,"
#define URIALPHA "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define URIDIGIT "0123456789"
#define URIMARK "-_.!~*`()"
#define URIUNESCAPED URIALPHA URIDIGIT URIMARK

static void jsB_decodeURI(js_State *J)
{
	Decode(J, js_tostring(J, 1), URIRESERVED "#");
}

static void jsB_decodeURIComponent(js_State *J)
{
	Decode(J, js_tostring(J, 1), "");
}

static void jsB_encodeURI(js_State *J)
{
	Encode(J, js_tostring(J, 1), URIUNESCAPED URIRESERVED "#");
}

static void jsB_encodeURIComponent(js_State *J)
{
	Encode(J, js_tostring(J, 1), URIUNESCAPED);
}

void jb_init(js_State *J)
{
	/* Create the prototype objects here, before the constructors */
	J->Object_prototype   = jsV_newobject(J, JS_COBJECT, NULL);
	J->Array_prototype    = jsV_newobject(J, JS_CARRAY, J->Object_prototype);
	J->Function_prototype = jsV_newobject(J, JS_CCFUNCTION, J->Object_prototype);
	J->Boolean_prototype  = jsV_newobject(J, JS_CBOOLEAN, J->Object_prototype);
	J->Number_prototype   = jsV_newobject(J, JS_CNUMBER, J->Object_prototype);
	J->String_prototype   = jsV_newobject(J, JS_CSTRING, J->Object_prototype);
	J->RegExp_prototype   = jsV_newobject(J, JS_COBJECT, J->Object_prototype);
	J->Date_prototype     = jsV_newobject(J, JS_CDATE, J->Object_prototype);

	/* All the native error types */
	J->Error_prototype      = jsV_newobject(J, JS_CERROR, J->Object_prototype);
	J->EvalError_prototype  = jsV_newobject(J, JS_CERROR, J->Error_prototype);
	J->RangeError_prototype = jsV_newobject(J, JS_CERROR, J->Error_prototype);
	J->ReferenceError_prototype = jsV_newobject(J, JS_CERROR, J->Error_prototype);
	J->SyntaxError_prototype    = jsV_newobject(J, JS_CERROR, J->Error_prototype);
	J->TypeError_prototype      = jsV_newobject(J, JS_CERROR, J->Error_prototype);
	J->URIError_prototype       = jsV_newobject(J, JS_CERROR, J->Error_prototype);

	/* Create the constructors and fill out the prototype objects */
	jb_initobject(J);
	jb_initarray(J);
	jb_initfunction(J);
	jb_initboolean(J);
	jb_initnumber(J);
	jb_initstring(J);
	jb_initregexp(J);
	jb_initdate(J);
	jb_initerror(J);
	jb_initmath(J);
	jb_initjson(J);

	/* Initialize the global object */
	js_push_number(J, NAN);
	js_defglobal(J, "NaN", JS_READONLY | JS_DONTENUM | JS_DONTCONF);

	js_push_number(J, INFINITY);
	js_defglobal(J, "Infinity", JS_READONLY | JS_DONTENUM | JS_DONTCONF);

	js_push_undef(J);
	js_defglobal(J, "undefined", JS_READONLY | JS_DONTENUM | JS_DONTCONF);

	jb_global_func(J, "parseInt", jsB_parseInt, 1);
	jb_global_func(J, "parseFloat", jsB_parseFloat, 1);
	jb_global_func(J, "isNaN", jsB_isNaN, 1);
	jb_global_func(J, "isFinite", jsB_isFinite, 1);

	jb_global_func(J, "decodeURI", jsB_decodeURI, 1);
	jb_global_func(J, "decodeURIComponent", jsB_decodeURIComponent, 1);
	jb_global_func(J, "encodeURI", jsB_encodeURI, 1);
	jb_global_func(J, "encodeURIComponent", jsB_encodeURIComponent, 1);
}
