#include "jsi.h"
#include "jsvalue.h"
#include "jsbuiltin.h"

static void Math_abs(js_State *J)
{
	js_push_number(J, fabs(js_tonumber(J, 1)));
}

static void Math_acos(js_State *J)
{
	js_push_number(J, acos(js_tonumber(J, 1)));
}

static void Math_asin(js_State *J)
{
	js_push_number(J, asin(js_tonumber(J, 1)));
}

static void Math_atan(js_State *J)
{
	js_push_number(J, atan(js_tonumber(J, 1)));
}

static void Math_atan2(js_State *J)
{
	double y = js_tonumber(J, 1);
	double x = js_tonumber(J, 2);
	js_push_number(J, atan2(y, x));
}

static void Math_ceil(js_State *J)
{
	js_push_number(J, ceil(js_tonumber(J, 1)));
}

static void Math_cos(js_State *J)
{
	js_push_number(J, cos(js_tonumber(J, 1)));
}

static void Math_exp(js_State *J)
{
	js_push_number(J, exp(js_tonumber(J, 1)));
}

static void Math_floor(js_State *J)
{
	js_push_number(J, floor(js_tonumber(J, 1)));
}

static void Math_log(js_State *J)
{
	js_push_number(J, log(js_tonumber(J, 1)));
}

static void Math_pow(js_State *J)
{
	double x = js_tonumber(J, 1);
	double y = js_tonumber(J, 2);
	double v ;
	if (!isfinite(y) && fabs(x) == 1)
		v = NAN ;
	else
        v = pow(x,y);

    js_push_number(J, v);
}

static void Math_random(js_State *J)
{
	js_push_number(J, rand() / (RAND_MAX + 1.0));
}

static double do_round(double x)
{
	if (isnan(x)) return x;
	if (isinf(x)) return x;
	if (x == 0) return x;
	if (x > 0 && x < 0.5) return 0;
	if (x < 0 && x >= -0.5) return -0;
	return floor(x + 0.5);
}

static void Math_round(js_State *J)
{
	double x = js_tonumber(J, 1);
	js_push_number(J, do_round(x));
}

static void Math_sin(js_State *J)
{
	js_push_number(J, sin(js_tonumber(J, 1)));
}

static void Math_sqrt(js_State *J)
{
	js_push_number(J, sqrt(js_tonumber(J, 1)));
}

static void Math_tan(js_State *J)
{
	js_push_number(J, tan(js_tonumber(J, 1)));
}

static void Math_max(js_State *J)
{
	int i, n = js_gettop(J);
	double x = -INFINITY;
	for (i = 1; i < n; ++i) {
		double y = js_tonumber(J, i);
		if (isnan(y)) {
			x = y;
			break;
		}
		if (signbit(x) == signbit(y))
			x = x > y ? x : y;
		else if (signbit(x))
			x = y;
	}
	js_push_number(J, x);
}

static void Math_min(js_State *J)
{
	int i, n = js_gettop(J);
	double x = INFINITY;
	for (i = 1; i < n; ++i) {
		double y = js_tonumber(J, i);
		if (isnan(y)) {
			x = y;
			break;
		}
		if (signbit(x) == signbit(y))
			x = x < y ? x : y;
		else if (signbit(y))
			x = y;
	}
	js_push_number(J, x);
}

void jb_initmath(js_State *J)
{
	js_push_object(J, js_newobject(J, JS_CMATH, J->Object_prototype));
	{
		jb_prop_num(J, "E", 2.7182818284590452354);
		jb_prop_num(J, "LN10", 2.302585092994046);
		jb_prop_num(J, "LN2", 0.6931471805599453);
		jb_prop_num(J, "LOG2E", 1.4426950408889634);
		jb_prop_num(J, "LOG10E", 0.4342944819032518);
		jb_prop_num(J, "PI", 3.1415926535897932);
		jb_prop_num(J, "SQRT1_2", 0.7071067811865476);
		jb_prop_num(J, "SQRT2", 1.4142135623730951);

		jb_prop_func(J, "Math.abs", Math_abs, 1);
		jb_prop_func(J, "Math.acos", Math_acos, 1);
		jb_prop_func(J, "Math.asin", Math_asin, 1);
		jb_prop_func(J, "Math.atan", Math_atan, 1);
		jb_prop_func(J, "Math.atan2", Math_atan2, 2);
		jb_prop_func(J, "Math.ceil", Math_ceil, 1);
		jb_prop_func(J, "Math.cos", Math_cos, 1);
		jb_prop_func(J, "Math.exp", Math_exp, 1);
		jb_prop_func(J, "Math.floor", Math_floor, 1);
		jb_prop_func(J, "Math.log", Math_log, 1);
		jb_prop_func(J, "Math.max", Math_max, 0); /* 2 */
		jb_prop_func(J, "Math.min", Math_min, 0); /* 2 */
		jb_prop_func(J, "Math.pow", Math_pow, 2);
		jb_prop_func(J, "Math.random", Math_random, 0);
		jb_prop_func(J, "Math.round", Math_round, 1);
		jb_prop_func(J, "Math.sin", Math_sin, 1);
		jb_prop_func(J, "Math.sqrt", Math_sqrt, 1);
		jb_prop_func(J, "Math.tan", Math_tan, 1);
	}
	js_def_global(J, "Math", JS_DONTENUM);
}
