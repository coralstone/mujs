#ifndef js_value_h
#define js_value_h

typedef struct js_Property js_Property;
typedef struct js_Iterator js_Iterator;

/* Hint to ToPrimitive() */
enum {
	JS_HNONE,
	JS_HNUMBER,
	JS_HSTRING
};

enum js_Type {
	JS_TSHRSTR, /* type tag doubles as string zero-terminator */
	JS_TUNDEFINED,
	JS_TNULL,
	JS_TBOOLEAN,
	JS_TNUMBER,
	JS_TLITSTR,
	JS_TMEMSTR,
	JS_TOBJECT,
};

enum js_Class {
	JS_COBJECT,
	JS_CARRAY,
	JS_CFUNCTION,
	JS_CSCRIPT, /* function created from global/eval code */
	JS_CCFUNCTION, /* built-in function */
	JS_CERROR,
	JS_CBOOLEAN,
	JS_CNUMBER,
	JS_CSTRING,
	JS_CREGEXP,
	JS_CDATE,
	JS_CMATH,
	JS_CJSON,
	JS_CITERATOR,
	JS_CUSERDATA,
};

/*
	Short strings abuse the js_Value struct. By putting the type tag in the
	last byte, and using 0 as the tag for short strings, we can use the
	entire js_Value as string storage by letting the type tag serve double
	purpose as the string zero terminator.
*/

struct js_Value
{
	union {
		int    boolean;
		double number;
		char   shrstr[8];
		const  char *litstr;
		js_String *memstr;
		js_Object *object;
	} u;
	char pad[7]; /* extra storage for shrstr */
	char type; /* type tag and zero terminator for shrstr */
};

struct js_String
{
	js_String *gcnext;
	char gcmark;
	char p[1];
};

struct js_Regexp
{
	void *prog;
	char *source;
	unsigned short flags;
	unsigned short last;
};

struct js_Object
{
	enum js_Class type;
	int extensible;
	js_Property *properties;
	int count; /* number of properties, for array sparseness check */
	js_Object *prototype;
	union {
		int boolean;
		double number;
		struct {
			const char *string;
			int length;
		} s;
		struct {
			int length;
		} a;
		struct {
			js_Function *function;
			js_Env *scope;
		} f;
		struct {
			const char *name;
			js_CFunction function;
			js_CFunction constructor;
			int length;
		} c;
		js_Regexp r;
		struct {
			js_Object *target;
			js_Iterator *head;
		} iter;
		struct {
			const char *tag;
			void *data;
			js_HasProperty has;
			js_Put put;
			js_Delete delete;
			js_Finalize finalize;
		} user;
	} u;
	js_Object *gcnext;
	int gcmark;
};

struct js_Property
{
	const char *name;
	js_Property *left, *right;
	int level;
	int atts;
	js_Value value;
	js_Object *getter;
	js_Object *setter;
};

struct js_Iterator
{
	const char *name;
	js_Iterator *next;
};



void       js_toprimitive(js_State *J, int idx, int hint);
js_Value  *js_tovalue(js_State *J, int idx);
js_Object *js_toobject(js_State *J, int idx);
void       js_pushvalue(js_State *J, js_Value v);
void       js_push_object(js_State *J, js_Object *v);

/* jsvalue.c */
int         jv_toboolean(js_State *J, js_Value *v);
double      jv_tonumber(js_State *J, js_Value *v);
int         jv_tointeger(js_State *J, js_Value *v);
const char *jv_tostring(js_State *J, js_Value *v);
js_Object * jv_toobject(js_State *J, js_Value *v);
void        jv_toprimitive(js_State *J, js_Value *v, int preferred);

const char *jv_ntos(js_State *J, char buf[32], double n);
double      jv_ston(js_State *J, const char *str);

const char *js_itoa(char buf[32], int a);
double      js_atod(const char *s, char **ep);
int         js_ntoi(double);
int         js_noti32(double);

js_String   *jv_memstring(js_State *J, const char *s, int n);
/* jsproperty.c */
js_Object   *jp_newobject(js_State *J, enum js_Class type, js_Object *prototype);
#define js_newobject jp_newobject

js_Object  *jp_newiterator(js_State *J, js_Object *obj, int own);
const char *jp_nextiterator(js_State *J, js_Object *iter);
void        jp_resizearray(js_State *J, js_Object *obj, int newlen);

/* jsdump.c */
void js_dumpobject(js_State *J, js_Object *obj);
void js_dumpvalue(js_State *J, js_Value v);

#endif
