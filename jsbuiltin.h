#ifndef js_builtin_h
#define js_builtin_h

void jb_init(js_State *J);
void jb_initobject(js_State *J);
void jb_initarray(js_State *J);
void jb_initfunction(js_State *J);
void jb_initboolean(js_State *J);
void jb_initnumber(js_State *J);
void jb_initstring(js_State *J);
void jb_initregexp(js_State *J);
void jb_initerror(js_State *J);
void jb_initmath(js_State *J);
void jb_initjson(js_State *J);
void jb_initdate(js_State *J);

void jb_prop_func(js_State *J, const char *name, js_CFunction cfun, int n);
void jb_prop_num(js_State *J, const char *name, double number);
void jb_prop_str(js_State *J, const char *name, const char *string);

#endif
