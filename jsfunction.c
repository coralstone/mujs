#include "jsi.h"
#include "jsparse.h"
#include "jscompile.h"
#include "jsvalue.h"
#include "jsbuiltin.h"



static void Fp_toString(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	js_Buffer *sb = NULL;
	int i;

	if (!js_is_callable(J, 0))
		js_error_type(J, "not a function");

	if (self->type == JS_CFUNCTION || self->type == JS_CSCRIPT) {
		js_Function *F = self->u.f.function;

		if (js_try(J)) {
			js_free(J, sb);
			js_throw(J);
		}

		js_puts(J, &sb, "function ");
		js_puts(J, &sb, F->name);
		js_putc(J, &sb, '(');
		for (i = 0; i < F->numparams; ++i) {
			if (i > 0) js_putc(J, &sb, ',');
			js_puts(J, &sb, F->vartab[i]);
		}
		js_puts(J, &sb, ") { ... }");

		js_push_string(J, sb->s);
		js_endtry(J);
		js_free(J, sb);
	} else if (self->type == JS_CCFUNCTION) {
		if (js_try(J)) {
			js_free(J, sb);
			js_throw(J);
		}

		js_puts(J, &sb, "function ");
		js_puts(J, &sb, self->u.c.name);
		js_puts(J, &sb, "() { ... }");

		js_push_string(J, sb->s);
		js_endtry(J);
		js_free(J, sb);
	} else {
		js_push_literal(J, "function () { ... }");
	}
}

static void Fp_apply(js_State *J)
{
	int i, n;

	if (!js_is_callable(J, 0))
		js_error_type(J, "not a function");

	js_copy(J, 0);
	js_copy(J, 1);

	if (js_is_null(J, 2) || js_is_undef(J, 2)) {
		n = 0;
	} else {
		n = js_get_length(J, 2);
		for (i = 0; i < n; ++i)
			js_get_index(J, 2, i);
	}

	js_call(J, n);
}

static void Fp_call(js_State *J)
{
	int i, top = js_gettop(J);

	if (!js_is_callable(J, 0))
		js_error_type(J, "not a function");

	for (i = 0; i < top; ++i)
		js_copy(J, i);

	js_call(J, top - 2);
}

static void callbound(js_State *J)
{
	int top = js_gettop(J);
	int i, fun, args, n;

	fun = js_gettop(J);
	js_cur_function(J);
	js_get_prop(J, fun, "__TargetFunction__");
	js_get_prop(J, fun, "__BoundThis__");

	args = js_gettop(J);
	js_get_prop(J, fun, "__BoundArguments__");
	n = js_get_length(J, args);
	for (i = 0; i < n; ++i)
		js_get_index(J, args, i);
	js_remove(J, args);

	for (i = 1; i < top; ++i)
		js_copy(J, i);

	js_call(J, n + top - 1);
}

static void constructbound(js_State *J)
{
	int top = js_gettop(J);
	int i, fun, args, n;

	fun = js_gettop(J);
	js_cur_function(J);
	js_get_prop(J, fun, "__TargetFunction__");

	args = js_gettop(J);
	js_get_prop(J, fun, "__BoundArguments__");
	n = js_get_length(J, args);
	for (i = 0; i < n; ++i)
		js_get_index(J, args, i);
	js_remove(J, args);

	for (i = 1; i < top; ++i)
		js_copy(J, i);

	js_construct(J, n + top - 1);
}

static void Fp_bind(js_State *J)
{
	int i, top = js_gettop(J);
	int n;

	if (!js_is_callable(J, 0))
		js_error_type(J, "not a function");

	n = js_get_length(J, 0);
	if (n > top - 2)
		n -= top - 2;
	else
		n = 0;

	/* Reuse target function's prototype for HasInstance check. */
	js_get_prop(J, 0, "prototype");
	js_new_cctor(J, callbound, constructbound, "[bind]", n);

	/* target function */
	js_copy(J, 0);
	js_def_prop(J, -2, "__TargetFunction__", JS_READONLY | JS_DONTENUM | JS_DONTCONF);

	/* bound this */
	js_copy(J, 1);
	js_def_prop(J, -2, "__BoundThis__", JS_READONLY | JS_DONTENUM | JS_DONTCONF);

	/* bound arguments */
	js_new_array(J);
	for (i = 2; i < top; ++i) {
		js_copy(J, i);
		js_set_index(J, -2, i - 2);
	}
	js_def_prop(J, -2, "__BoundArguments__", JS_READONLY | JS_DONTENUM | JS_DONTCONF);
}

static void Fp_Function(js_State *J)
{
	int i, top = js_gettop(J);
	js_Buffer *sb = NULL;
	const char *body;
	js_Ast *parse;
	js_Function *fun;

	if (js_try(J)) {
		js_free(J, sb);
		jsP_freeparse(J);
		js_throw(J);
	}

	/* p1, p2, ..., pn */
	if (top > 2) {
		for (i = 1; i < top - 1; ++i) {
			if (i > 1)
				js_putc(J, &sb, ',');
			js_puts(J, &sb, js_tostring(J, i));
		}
		js_putc(J, &sb, ')');
	}

	/* body */
	body = js_is_def(J, top - 1) ? js_tostring(J, top - 1) : "";

	parse = jsP_parsefunction(J, "[string]", sb ? sb->s : NULL, body);
	fun = jsC_compilefunction(J, parse);

	js_endtry(J);
	js_free(J, sb);
	jsP_freeparse(J);

	js_new_function(J, fun, J->GE);
}

static void Fp_func_prototype(js_State *J)
{
	js_push_undef(J);
}

void jb_initfunction(js_State *J)
{
	J->Function_prototype->u.c.function = Fp_func_prototype;
	J->Function_prototype->u.c.constructor = NULL;

	js_push_object(J, J->Function_prototype);
	{
		jb_prop_func(J, "Function.prototype.toString", Fp_toString, 2);
		jb_prop_func(J, "Function.prototype.apply", Fp_apply, 2);
		jb_prop_func(J, "Function.prototype.call", Fp_call, 1);
		jb_prop_func(J, "Function.prototype.bind", Fp_bind, 1);
	}
	js_new_cctor(J, Fp_Function,Fp_Function, "Function", 1);
	js_defglobal(J, "Function", JS_DONTENUM);
}
