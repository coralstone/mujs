#ifndef js_run_h
#define js_run_h

js_Env  *jsR_newenvironment(js_State *J, js_Object *variables, js_Env *outer);

struct js_Environment
{
	js_Env    *outer;
	js_Object *variables;

	js_Env    *gcnext;
	int gcmark;
};

#endif
