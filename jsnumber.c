#include "jsi.h"
#include "jsvalue.h"
#include "jsbuiltin.h"

#if defined(_MSC_VER) && (_MSC_VER < 1700) /* VS2012 has stdint.h */
typedef unsigned __int64 uint64_t;
#else
#include <stdint.h>
#endif

static void jsB_new_Number(js_State *J)
{
	js_new_number(J, js_gettop(J) > 1 ? js_tonumber(J, 1) : 0);
}

static void jsB_Number(js_State *J)
{
	js_push_number(J, js_gettop(J) > 1 ? js_tonumber(J, 1) : 0);
}

static void Np_valueOf(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	if (self->type != JS_CNUMBER)
        js_error_type(J, "not a number");
	js_push_number(J, self->u.number);
}

static void Np_toString(js_State *J)
{
	char buf[32];
	js_Object *self = js_toobject(J, 0);
	int radix = js_is_undef(J, 1) ? 10 : js_tointeger(J, 1);
	if (self->type != JS_CNUMBER)
		js_error_type(J, "not a number");
	if (radix == 10) {
		js_push_string(J, jsV_ntos(J, buf, self->u.number));
		return;
	}
	if (radix < 2 || radix > 36)
		js_error_range(J, "invalid radix");

	/* lame number to string conversion for any radix from 2 to 36 */
	{
		static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
		char buf[100];
		double number = self->u.number;
		int sign = self->u.number < 0;
		js_Buffer *sb = NULL;
		uint64_t u, limit = ((uint64_t)1<<52);

		int ndigits, exp, point;

		if (number == 0)   { js_push_string(J, "0"); return; }
		if (isnan(number)) { js_push_string(J, "NaN"); return; }
		if (isinf(number)) { js_push_string(J, sign ? "-Infinity" : "Infinity"); return; }

		if (sign)
			number = -number;

		/* fit as many digits as we want in an int */
		exp = 0;
		while (number * pow(radix, exp) > limit)
			--exp;
		while (number * pow(radix, exp+1) < limit)
			++exp;
		u = number * pow(radix, exp) + 0.5;

		/* trim trailing zeros */
		while (u > 0 && (u % radix) == 0) {
			u /= radix;
			--exp;
		}

		/* serialize digits */
		ndigits = 0;
		while (u > 0) {
			buf[ndigits++] = digits[u % radix];
			u /= radix;
		}
		point = ndigits - exp;

		if (js_try(J)) {
			js_free(J, sb);
			js_throw(J);
		}

		if (sign)
			js_putc(J, &sb, '-');

		if (point <= 0) {
			js_putc(J, &sb, '0');
			js_putc(J, &sb, '.');
			while (point++ < 0)
				js_putc(J, &sb, '0');
			while (ndigits-- > 0)
				js_putc(J, &sb, buf[ndigits]);
		} else {
			while (ndigits-- > 0) {
				js_putc(J, &sb, buf[ndigits]);
				if (--point == 0 && ndigits > 0)
					js_putc(J, &sb, '.');
			}
			while (point-- > 0)
				js_putc(J, &sb, '0');
		}

		js_putc(J, &sb, 0);
		js_push_string(J, sb->s);

		js_endtry(J);
		js_free(J, sb);
	}
}

/* Customized ToString() on a number */
static void numtostr(js_State *J, const char *fmt, int w, double n)
{
	char buf[32], *e;
	if (isnan(n))      js_push_literal(J, "NaN");
	else if (isinf(n)) js_push_literal(J, n < 0 ? "-Infinity" : "Infinity");
	else if (n == 0)   js_push_literal(J, "0");
	else {
		if (w < 1) w = 1;
		if (w > 17) w = 17;
		sprintf(buf, fmt, w, n);
		e = strchr(buf, 'e');
		if (e) {
			int exp = atoi(e+1);
			sprintf(e, "e%+d", exp);
		}
		js_push_string(J, buf);
	}
}

static void Np_toFixed(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	int width = js_tointeger(J, 1);
	if (self->type != JS_CNUMBER)
        js_error_type(J, "not a number");
	numtostr(J, "%.*f", width, self->u.number);
}

static void Np_toExponential(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	int width = js_tointeger(J, 1);
	if (self->type != JS_CNUMBER)
        js_error_type(J, "not a number");
	numtostr(J, "%.*e", width, self->u.number);
}

static void Np_toPrecision(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	int width = js_tointeger(J, 1);
	if (self->type != JS_CNUMBER)
        js_error_type(J, "not a number");
	numtostr(J, "%.*g", width, self->u.number);
}

void jb_initnumber(js_State *J)
{
	J->Number_prototype->u.number = 0;

	js_push_object(J, J->Number_prototype);
	{
		jb_prop_func(J, "Number.prototype.valueOf", Np_valueOf, 0);
		jb_prop_func(J, "Number.prototype.toString", Np_toString, 1);
		jb_prop_func(J, "Number.prototype.toLocaleString", Np_toString, 0);
		jb_prop_func(J, "Number.prototype.toFixed", Np_toFixed, 1);
		jb_prop_func(J, "Number.prototype.toExponential", Np_toExponential, 1);
		jb_prop_func(J, "Number.prototype.toPrecision", Np_toPrecision, 1);
	}
	js_new_cctor(J, jsB_Number, jsB_new_Number, "Number", 0); /* 1 */
	{
		jb_prop_num(J, "MAX_VALUE", 1.7976931348623157e+308);
		jb_prop_num(J, "MIN_VALUE", 5e-324);
		jb_prop_num(J, "NaN", NAN);
		jb_prop_num(J, "NEGATIVE_INFINITY", -INFINITY);
		jb_prop_num(J, "POSITIVE_INFINITY", INFINITY);
	}
	js_defglobal(J, "Number", JS_DONTENUM);
}
