#ifndef mujs_h
#define mujs_h

#include <setjmp.h> /* required for setjmp in fz_try macro */

#ifdef __cplusplus
extern "C" {
#endif

/* noreturn is a GCC extension */
#ifdef __GNUC__
#define JS_NORETURN __attribute__((noreturn))
#else
#ifdef _MSC_VER
#define JS_NORETURN __declspec(noreturn)
#else
#define JS_NORETURN
#endif
#endif

/* GCC can do type checking of printf strings */
#ifdef __printflike
#define JS_PRINTFLIKE __printflike
#else
#if __GNUC__ > 2 || __GNUC__ == 2 && __GNUC_MINOR__ >= 7
#define JS_PRINTFLIKE(fmtarg, firstvararg) \
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
#define JS_PRINTFLIKE(fmtarg, firstvararg)
#endif
#endif

/*
#ifndef uint
typedef unsigned int uint
#endif

#ifndef ushort
typedef unsigned short ushort
#endif

#ifndef ulong
typedef unsigned long ulong
#endif
*/
typedef unsigned int   uint;
typedef unsigned short ushort;
typedef unsigned long  ulong;
typedef struct js_State js_State;

typedef void*(*js_Alloc)(void *memctx, void *ptr, int size);
typedef void (*js_Panic)(js_State *J);
typedef void (*js_CFunction)(js_State *J);
typedef void (*js_Finalize)(js_State *J, void *p);
typedef int  (*js_HasProperty)(js_State *J, void *p, const char *name);
typedef int  (*js_Put)(js_State *J, void *p, const char *name);
typedef int  (*js_Delete)(js_State *J, void *p, const char *name);
typedef void (*js_Report)(js_State *J, const char *message);

/* Basic functions */
js_State *js_newstate(js_Alloc alloc, void *actx, int flags);
void      js_setcontext(js_State *J, void *uctx);
void     *js_getcontext(js_State *J);
void      js_setreport(js_State *J, js_Report report);
js_Panic  js_atpanic(js_State *J, js_Panic panic);
void      js_freestate(js_State *J);
void      js_gc(js_State *J, int report);

int js_dostring(js_State *J, const char *source);
int js_dofile(js_State *J, const char *filename);
int js_ploadstring(js_State *J, const char *filename, const char *source);
int js_ploadfile(js_State *J, const char *filename);
int js_pcall(js_State *J, int n);
int js_pconstruct(js_State *J, int n);

/* Exception handling */

void *js_savetry(js_State *J); /* returns a jmp_buf */

#define js_try(J) 	setjmp(js_savetry(J))

void js_endtry(js_State *J);

/* State constructor flags */
enum {
	JS_STRICT = 1,
};

/* RegExp flags */
enum {
	JS_REGEXP_G = 1,
	JS_REGEXP_I = 2,
	JS_REGEXP_M = 4,
};

/* Property attribute flags */
enum {
	JS_READONLY = 1,
	JS_DONTENUM = 2,
	JS_DONTCONF = 4,
};

void js_report(js_State *J, const char *message);

/*
void js_newerror(js_State *J, const char *message);
void js_newevalerror(js_State *J, const char *message);
void js_newrangeerror(js_State *J, const char *message);
void js_newreferenceerror(js_State *J, const char *message);
void js_newerror_syntax(js_State *J, const char *message);
void js_newtypeerror(js_State *J, const char *message);
void js_newurierror(js_State *J, const char *message);
*/

JS_NORETURN void js_error(js_State *J, const char *fmt, ...)       JS_PRINTFLIKE(2,3);
JS_NORETURN void js_error_eval(js_State *J, const char *fmt, ...)  JS_PRINTFLIKE(2,3);
JS_NORETURN void js_error_range(js_State *J, const char *fmt, ...) JS_PRINTFLIKE(2,3);
JS_NORETURN void js_error_ref(js_State *J, const char *fmt, ...)   JS_PRINTFLIKE(2,3);
JS_NORETURN void js_error_syntax(js_State *J, const char *fmt, ...) JS_PRINTFLIKE(2,3);
JS_NORETURN void js_error_type(js_State *J, const char *fmt, ...)  JS_PRINTFLIKE(2,3);
JS_NORETURN void js_error_uri(js_State *J, const char *fmt, ...)   JS_PRINTFLIKE(2,3);
JS_NORETURN void js_throw(js_State *J);


#define JS_CHECK_OBJ(J,idx) if (!js_is_object(J, idx)) {js_error_type(J, "not an object");}

void js_loadstring(js_State *J, const char *filename, const char *source);
void js_loadfile(js_State *J, const char *filename);
void js_loadeval(js_State *J, const char *filename, const char *source);

void js_eval(js_State *J);
void js_call(js_State *J, int n);
void js_construct(js_State *J, int n);
/*
const char *js_ref(js_State *J);
void js_unref(js_State *J, const char *ref);
*/
void js_get_registry(js_State *J, const char *name);
void js_set_registry(js_State *J, const char *name);
void js_del_registry(js_State *J, const char *name);

void js_get_global(js_State *J, const char *name);
void js_set_global(js_State *J, const char *name);
void js_def_global(js_State *J, const char *name, int atts);

int  js_has_prop(js_State *J, int idx, const char *name);
void js_get_prop(js_State *J, int idx, const char *name);
void js_set_prop(js_State *J, int idx, const char *name);
void js_def_prop(js_State *J, int idx, const char *name, int atts);
void js_del_prop(js_State *J, int idx, const char *name);
void js_def_accessor(js_State *J, int idx, const char *name, int atts);

int  js_get_length(js_State *J, int idx);
void js_set_length(js_State *J, int idx, int len);
int  js_has_index(js_State *J, int idx, int i);
void js_get_index(js_State *J, int idx, int i);
void js_set_index(js_State *J, int idx, int i);
void js_del_index(js_State *J, int idx, int i);

void js_cur_function(js_State *J);
void js_push_global(js_State *J);
void js_push_undef(js_State *J);
void js_push_null(js_State *J);
void js_push_bool(js_State *J, int v);
void js_push_number(js_State *J, double v);
void js_push_string(js_State *J, const char *v);
void js_push_lstr(js_State *J, const char *v, int len);
void js_push_literal(js_State *J, const char *v);

void js_push_iterator(js_State *J, int idx, int own);
const char *js_next_iterator(js_State *J, int idx);

void js_new_objectx(js_State *J);
void js_new_object(js_State *J);
void js_new_array(js_State *J);
void js_new_bool(js_State *J, int v);
void js_new_number(js_State *J, double v);
void js_new_string(js_State *J, const char *v);
void js_new_regexp(js_State *J, const char *pattern, int flags);


void js_new_user_data(js_State *J, const char *tag, void *data, js_Finalize finalize);
void js_new_user_datax(js_State *J, const char *tag, void *data,
     js_HasProperty has, js_Put put, js_Delete del, js_Finalize finalize);




int js_is_def(js_State *J, int idx);
int js_is_undef(js_State *J, int idx);
int js_is_null(js_State *J, int idx);
int js_is_bool(js_State *J, int idx);
int js_is_number(js_State *J, int idx);
int js_is_string(js_State *J, int idx);
int js_is_primitve(js_State *J, int idx);
int js_is_object(js_State *J, int idx);
int js_is_array(js_State *J, int idx);
int js_is_regexp(js_State *J, int idx);
int js_is_coercible(js_State *J, int idx);
int js_is_callable(js_State *J, int idx);
int js_is_userdata(js_State *J, int idx, const char *tag);

int         js_tobool(js_State *J, int idx);
double      js_tonumber(js_State *J, int idx);
const char *js_tostring(js_State *J, int idx);
void *      js_touserdata(js_State *J, int idx, const char *tag);

const char *js_trystring(js_State *J, int idx, const char *error);

//int   js_tointeger(js_State *J, int idx);
int   js_toi32(js_State *J, int idx);
uint  js_tou32(js_State *J, int idx);
//short js_toint16(js_State *J, int idx);
//ushort js_touint16(js_State *J, int idx);

int  js_gettop(js_State *J);
void js_settop(js_State *J, int idx);
void js_pop(js_State *J, int n);
void js_rot(js_State *J, int n);
void js_copy(js_State *J, int idx);
void js_remove(js_State *J, int idx);
void js_insert(js_State *J, int idx);
void js_replace(js_State* J, int idx);

void js_dup(js_State *J);
void js_dup2(js_State *J);
void js_rot2(js_State *J);
void js_rot3(js_State *J);
void js_rot4(js_State *J);
void js_rot2pop1(js_State *J);
void js_rot3pop2(js_State *J);

void js_concat(js_State *J);
int  js_compare(js_State *J, int *okay);
int  js_equal(js_State *J);
int  js_equal_strict(js_State *J);
int  js_instanceof(js_State *J);

#ifdef __cplusplus
}
#endif

#endif
