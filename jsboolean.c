#include "jsi.h"
#include "jsvalue.h"
#include "jsbuiltin.h"

static void Bp_new_Boolean(js_State *J)
{
	js_new_bool(J, js_toboolean(J, 1));
}

static void Bp_Boolean(js_State *J)
{
	js_push_bool(J, js_toboolean(J, 1));
}

static void Bp_toString(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	if (self->type != JS_CBOOLEAN)
        js_error_type(J, "not a boolean");
	js_push_literal(J, self->u.boolean ? "true" : "false");
}

static void Bp_valueOf(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	if (self->type != JS_CBOOLEAN)
        js_error_type(J, "not a boolean");
	js_push_bool(J, self->u.boolean);
}

void jb_initboolean(js_State *J)
{
	J->Boolean_prototype->u.boolean = 0;

	js_push_object(J, J->Boolean_prototype);
	{
		jb_prop_func(J, "Boolean.prototype.toString", Bp_toString, 0);
		jb_prop_func(J, "Boolean.prototype.valueOf", Bp_valueOf, 0);
	}
	js_new_cctor(J, Bp_Boolean, Bp_new_Boolean, "Boolean", 1);
	js_defglobal(J, "Boolean", JS_DONTENUM);
}
