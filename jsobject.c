#include "jsi.h"
#include "jsvalue.h"
#include "jsbuiltin.h"

static void jsB_new_Object(js_State *J)
{
	if (js_is_undef(J, 1) || js_is_null(J, 1))
		js_new_object(J);
	else
		js_push_object(J, js_toobject(J, 1));
}

static void jsB_Object(js_State *J)
{
	if (js_is_undef(J, 1) || js_is_null(J, 1))
		js_new_object(J);
	else
		js_push_object(J, js_toobject(J, 1));
}

static void Op_toString(js_State *J)
{
	if (js_is_undef(J, 0))
		js_push_literal(J, "[object Undefined]");
	else if (js_is_null(J, 0))
		js_push_literal(J, "[object Null]");
	else {
		js_Object *self = js_toobject(J, 0);
		switch (self->type) {
		case JS_COBJECT:    js_push_literal(J, "[object Object]"); break;
		case JS_CARRAY:     js_push_literal(J, "[object Array]"); break;
		case JS_CFUNCTION:  js_push_literal(J, "[object Function]"); break;
		case JS_CSCRIPT:    js_push_literal(J, "[object Function]"); break;
		case JS_CCFUNCTION: js_push_literal(J, "[object Function]"); break;
		case JS_CERROR:     js_push_literal(J, "[object Error]"); break;
		case JS_CBOOLEAN:   js_push_literal(J, "[object Boolean]"); break;
		case JS_CNUMBER:    js_push_literal(J, "[object Number]"); break;
		case JS_CSTRING:    js_push_literal(J, "[object String]"); break;
		case JS_CREGEXP:    js_push_literal(J, "[object RegExp]"); break;
		case JS_CDATE:      js_push_literal(J, "[object Date]"); break;
		case JS_CMATH:      js_push_literal(J, "[object Math]"); break;
		case JS_CJSON:      js_push_literal(J, "[object JSON]"); break;
		case JS_CITERATOR:  js_push_literal(J, "[Iterator]"); break;
		case JS_CUSERDATA:
				   js_push_literal(J, "[object ");
				   js_push_literal(J, self->u.user.tag);
				   js_concat(J);
				   js_push_literal(J, "]");
				   js_concat(J);
				   break;
		}
	}
}

static void Op_valueOf(js_State *J)
{
	js_copy(J, 0);
}

static void Op_hasOwnProperty(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	const char *name = js_tostring(J, 1);
	js_Property *ref = jp_getownproperty(J, self, name);
	js_push_bool(J, ref != NULL);
}

static void Op_isPrototypeOf(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	if (js_is_object(J, 1)) {
		js_Object *V = js_toobject(J, 1);
		do {
			V = V->prototype;
			if (V == self) {
				js_push_bool(J, 1);
				return;
			}
		} while (V);
	}
	js_push_bool(J, 0);
}

static void Op_propertyIsEnumerable(js_State *J)
{
	js_Object *self = js_toobject(J, 0);
	const char *name = js_tostring(J, 1);
	js_Property *ref = jp_getownproperty(J, self, name);
	js_push_bool(J, ref && !(ref->atts & JS_DONTENUM));
}

static void O_getPrototypeOf(js_State *J)
{


	JS_CHECK_OBJ(J, 1);

	js_Object *obj = js_toobject(J, 1);
	if (obj->prototype)
		js_push_object(J, obj->prototype);
	else
		js_push_null(J);
}

static void O_getOwnPropertyDescriptor(js_State *J)
{
	js_Object *obj;
	js_Property *ref;

	JS_CHECK_OBJ(J, 1);

	obj = js_toobject(J, 1);
	ref = jp_getproperty(J, obj, js_tostring(J, 2));
	if (!ref)
		js_push_undef(J);
	else {
		js_new_object(J);
		if (!ref->getter && !ref->setter) {
			js_push_value(J, ref->value);
			js_set_prop(J, -2, "value");
			js_push_bool(J, !(ref->atts & JS_READONLY));
			js_set_prop(J, -2, "writable");
		} else {
			if (ref->getter)
				js_push_object(J, ref->getter);
			else
				js_push_undef(J);
			js_set_prop(J, -2, "get");
			if (ref->setter)
				js_push_object(J, ref->setter);
			else
				js_push_undef(J);
			js_set_prop(J, -2, "set");
		}
		js_push_bool(J, !(ref->atts & JS_DONTENUM));
		js_set_prop(J, -2, "enumerable");
		js_push_bool(J, !(ref->atts & JS_DONTCONF));
		js_set_prop(J, -2, "configurable");
	}
}

static int O_getOwnPropertyNames_walk(js_State *J, js_Property *ref, int i)
{
	if (ref->left->level)
		i = O_getOwnPropertyNames_walk(J, ref->left, i);
	js_push_literal(J, ref->name);
	js_set_index(J, -2, i++);
	if (ref->right->level)
		i = O_getOwnPropertyNames_walk(J, ref->right, i);
	return i;
}

static void O_getOwnPropertyNames(js_State *J)
{
	js_Object *obj;
	int k,i;

	JS_CHECK_OBJ(J, 1);

	obj = js_toobject(J, 1);

	js_new_array(J);

	if (obj->properties->level)
		i = O_getOwnPropertyNames_walk(J, obj->properties, 0);
	else
		i = 0;

	if (obj->type == JS_CARRAY) {
		js_push_literal(J, "length");
		js_set_index(J, -2, i++);
	}

	if (obj->type == JS_CSTRING) {
		js_push_literal(J, "length");
		js_set_index(J, -2, i++);
		for (k = 0; k < obj->u.s.length; ++k) {
			js_push_number(J, k);
			js_set_index(J, -2, i++);
		}
	}

	if (obj->type == JS_CREGEXP) {
		js_push_literal(J, "source");
		js_set_index(J, -2, i++);
		js_push_literal(J, "global");
		js_set_index(J, -2, i++);
		js_push_literal(J, "ignoreCase");
		js_set_index(J, -2, i++);
		js_push_literal(J, "multiline");
		js_set_index(J, -2, i++);
		js_push_literal(J, "lastIndex");
		js_set_index(J, -2, i++);
	}
}

static void ToPropertyDescriptor(js_State *J, js_Object *obj, const char *name, js_Object *desc)
{
	int haswritable = 0;
	int hasvalue = 0;
	int enumerable = 0;
	int configurable = 0;
	int writable = 0;
	int atts = 0;

	js_push_object(J, obj);
	js_push_object(J, desc);

	if (js_has_prop(J, -1, "writable")) {
		haswritable = 1;
		writable = js_toboolean(J, -1);
		js_pop(J, 1);
	}
	if (js_has_prop(J, -1, "enumerable")) {
		enumerable = js_toboolean(J, -1);
		js_pop(J, 1);
	}
	if (js_has_prop(J, -1, "configurable")) {
		configurable = js_toboolean(J, -1);
		js_pop(J, 1);
	}
	if (js_has_prop(J, -1, "value")) {
		hasvalue = 1;
		js_set_prop(J, -3, name);
	}

	if (!writable) atts |= JS_READONLY;
	if (!enumerable) atts |= JS_DONTENUM;
	if (!configurable) atts |= JS_DONTCONF;

	if (js_has_prop(J, -1, "get")) {
		if (haswritable || hasvalue)
			js_error_type(J, "value/writable and get/set attributes are exclusive");
	} else {
		js_push_undef(J);
	}

	if (js_has_prop(J, -2, "set")) {
		if (haswritable || hasvalue)
			js_error_type(J, "value/writable and get/set attributes are exclusive");
	} else {
		js_push_undef(J);
	}

	js_def_accessor(J, -4, name, atts);

	js_pop(J, 2);
}

static void O_defineProperty(js_State *J)
{
	JS_CHECK_OBJ(J, 1);
	JS_CHECK_OBJ(J, 3);

	ToPropertyDescriptor(J, js_toobject(J, 1), js_tostring(J, 2), js_toobject(J, 3));
	js_copy(J, 1);
}

static void O_defineProperties_walk(js_State *J, js_Property *ref)
{
	if (ref->left->level)
		O_defineProperties_walk(J, ref->left);
	if (!(ref->atts & JS_DONTENUM)) {
		js_push_value(J, ref->value);
		ToPropertyDescriptor(J, js_toobject(J, 1), ref->name, js_toobject(J, -1));
		js_pop(J, 1);
	}
	if (ref->right->level)
		O_defineProperties_walk(J, ref->right);
}

static void O_defineProperties(js_State *J)
{
	js_Object *props;

	JS_CHECK_OBJ(J, 1) ;
	JS_CHECK_OBJ(J, 2) ;

	props = js_toobject(J, 2);
	if (props->properties->level)
		O_defineProperties_walk(J, props->properties);

	js_copy(J, 1);
}

static void O_create_walk(js_State *J, js_Object *obj, js_Property *ref)
{
	if (ref->left->level)
		O_create_walk(J, obj, ref->left);
	if (!(ref->atts & JS_DONTENUM)) {
		if (ref->value.type != JS_TOBJECT)
			js_error_type(J, "not an object");
		ToPropertyDescriptor(J, obj, ref->name, ref->value.u.object);
	}
	if (ref->right->level)
		O_create_walk(J, obj, ref->right);
}

static void O_create(js_State *J)
{
	js_Object *obj;
	js_Object *proto;
	js_Object *props;

	if (js_is_object(J, 1))
		proto = js_toobject(J, 1);
	else if (js_is_null(J, 1))
		proto = NULL;
	else
		js_error_type(J, "not an object or null");

	obj = js_newobject(J, JS_COBJECT, proto);
	js_push_object(J, obj);

	if (js_is_def(J, 2)) {

		JS_CHECK_OBJ(J, 2);

		props = js_toobject(J, 2);
		if (props->properties->level)
			O_create_walk(J, obj, props->properties);
	}
}

static int O_keys_walk(js_State *J, js_Property *ref, int i)
{
	if (ref->left->level)
		i = O_keys_walk(J, ref->left, i);
	if (!(ref->atts & JS_DONTENUM)) {
		js_push_literal(J, ref->name);
		js_set_index(J, -2, i++);
	}
	if (ref->right->level)
		i = O_keys_walk(J, ref->right, i);
	return i;
}

static void O_keys(js_State *J)
{
	js_Object *obj;
	int i, k;

	JS_CHECK_OBJ(J, 1) ;
	obj = js_toobject(J, 1);

	js_new_array(J);

	if (obj->properties->level)
		i = O_keys_walk(J, obj->properties, 0);
	else
		i = 0;

	if (obj->type == JS_CSTRING) {
		for (k = 0; k < obj->u.s.length; ++k) {
			js_push_number(J, k);
			js_set_index(J, -2, i++);
		}
	}
}

static void O_preventExtensions(js_State *J)
{
	JS_CHECK_OBJ(J, 1)  ;
	js_toobject(J, 1)->extensible = 0;
	js_copy(J, 1);
}

static void O_isExtensible(js_State *J)
{
	JS_CHECK_OBJ(J, 1) ;
	js_push_bool(J, js_toobject(J, 1)->extensible);
}

static void O_seal_walk(js_State *J, js_Property *ref)
{
	if (ref->left->level)
		O_seal_walk(J, ref->left);
	ref->atts |= JS_DONTCONF;
	if (ref->right->level)
		O_seal_walk(J, ref->right);
}

static void O_seal(js_State *J)
{
	js_Object *obj;

	JS_CHECK_OBJ(J, 1) ;

	obj = js_toobject(J, 1);
	obj->extensible = 0;

	if (obj->properties->level)
		O_seal_walk(J, obj->properties);

	js_copy(J, 1);
}

static int O_isSealed_walk(js_State *J, js_Property *ref)
{
	if (ref->left->level)
		if (!O_isSealed_walk(J, ref->left))
			return 0;
	if (!(ref->atts & JS_DONTCONF))
		return 0;
	if (ref->right->level)
		if (!O_isSealed_walk(J, ref->right))
			return 0;
	return 1;
}

static void O_isSealed(js_State *J)
{
	js_Object *obj;

	JS_CHECK_OBJ(J, 1) ;

	obj = js_toobject(J, 1);
	if (obj->extensible) {
		js_push_bool(J, 0);
		return;
	}

	if (obj->properties->level)
		js_push_bool(J, O_isSealed_walk(J, obj->properties));
	else
		js_push_bool(J, 1);
}

static void O_freeze_walk(js_State *J, js_Property *ref)
{
	if (ref->left->level)
		O_freeze_walk(J, ref->left);
	ref->atts |= JS_READONLY | JS_DONTCONF;
	if (ref->right->level)
		O_freeze_walk(J, ref->right);
}

static void O_freeze(js_State *J)
{
	js_Object *obj;

	JS_CHECK_OBJ(J, 1) ;

	obj = js_toobject(J, 1);
	obj->extensible = 0;

	if (obj->properties->level)
		O_freeze_walk(J, obj->properties);

	js_copy(J, 1);
}

static int O_isFrozen_walk(js_State *J, js_Property *ref)
{
	if (ref->left->level)
		if (!O_isFrozen_walk(J, ref->left))
			return 0;
	if (!(ref->atts & (JS_READONLY | JS_DONTCONF)))
		return 0;
	if (ref->right->level)
		if (!O_isFrozen_walk(J, ref->right))
			return 0;
	return 1;
}

static void O_isFrozen(js_State *J)
{
	js_Object *obj;

	JS_CHECK_OBJ(J, 1) ;

	obj = js_toobject(J, 1);
	if (obj->extensible) {
		js_push_bool(J, 0);
		return;
	}

	if (obj->properties->level)
		js_push_bool(J, O_isFrozen_walk(J, obj->properties));
	else
		js_push_bool(J, 1);
}

void jb_initobject(js_State *J)
{
	js_push_object(J, J->Object_prototype);
	{
		jb_prop_func(J, "Object.prototype.toString", Op_toString, 0);
		jb_prop_func(J, "Object.prototype.toLocaleString", Op_toString, 0);
		jb_prop_func(J, "Object.prototype.valueOf", Op_valueOf, 0);
		jb_prop_func(J, "Object.prototype.hasOwnProperty", Op_hasOwnProperty, 1);
		jb_prop_func(J, "Object.prototype.isPrototypeOf", Op_isPrototypeOf, 1);
		jb_prop_func(J, "Object.prototype.propertyIsEnumerable", Op_propertyIsEnumerable, 1);
	}
	js_new_cctor(J, jsB_Object, jsB_new_Object, "Object", 1);
	{
		/* ES5 */
		jb_prop_func(J, "Object.getPrototypeOf", O_getPrototypeOf, 1);
		jb_prop_func(J, "Object.getOwnPropertyDescriptor", O_getOwnPropertyDescriptor, 2);
		jb_prop_func(J, "Object.getOwnPropertyNames", O_getOwnPropertyNames, 1);
		jb_prop_func(J, "Object.create", O_create, 2);
		jb_prop_func(J, "Object.defineProperty", O_defineProperty, 3);
		jb_prop_func(J, "Object.defineProperties", O_defineProperties, 2);
		jb_prop_func(J, "Object.seal", O_seal, 1);
		jb_prop_func(J, "Object.freeze", O_freeze, 1);
		jb_prop_func(J, "Object.preventExtensions", O_preventExtensions, 1);
		jb_prop_func(J, "Object.isSealed", O_isSealed, 1);
		jb_prop_func(J, "Object.isFrozen", O_isFrozen, 1);
		jb_prop_func(J, "Object.isExtensible", O_isExtensible, 1);
		jb_prop_func(J, "Object.keys", O_keys, 1);
	}
	js_def_global(J, "Object", JS_DONTENUM);
}
