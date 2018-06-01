#include "jsi.h"

/* Dynamically grown string buffer */

void js_putc(js_State *J, js_Buffer **sbp, int c)
{
	js_Buffer *sb = *sbp;
	if (!sb) {
		sb = js_malloc(J, sizeof *sb);
		sb->n = 0;
		sb->m = sizeof sb->s;
		*sbp = sb;
	} else if (sb->n == sb->m) {
		sb = js_realloc(J, sb, (sb->m *= 2) + soffsetof(js_Buffer, s));
		*sbp = sb;
	}
	sb->s[sb->n++] = c;
}

void js_puts(js_State *J, js_Buffer **sb, const char *s)
{
	while (*s)
		js_putc(J, sb, *s++);
}

void js_putm(js_State *J, js_Buffer **sb, const char *s, const char *e)
{
	while (s < e)
		js_putc(J, sb, *s++);
}

/* Use an AA-tree to quickly look up interned strings. */

struct js_StringNode
{
	js_StringNode *left, *right;
	int level;
	char string[1];
};

#define CHECK_STR_NODE(node) (node && node != &jstr_null)
static js_StringNode jstr_null = { &jstr_null, &jstr_null, 0, ""};

static js_StringNode *jn_newstring(js_State *J, const char *string, const char **result)
{
	int n = strlen(string);
	js_StringNode *node = js_malloc(J, soffsetof(js_StringNode, string) + n + 1);
	node->left = node->right = &jstr_null;
	node->level = 1;
	memcpy(node->string, string, n + 1);
	return *result = node->string, node;
}

static js_StringNode *jn_skew(js_StringNode *node)
{
	if (node->left->level == node->level) {
		js_StringNode *temp = node;
		node = node->left;
		temp->left = node->right;
		node->right = temp;
	}
	return node;
}

static js_StringNode *jn_split(js_StringNode *node)
{
	if (node->right->right->level == node->level) {
		js_StringNode *temp = node;
		node = node->right;
		temp->right = node->left;
		node->left = temp;
		++node->level;
	}
	return node;
}

static js_StringNode *jn_insert(js_State *J, js_StringNode *node, const char *string, const char **result)
{
	if (CHECK_STR_NODE(node)) {
		int c = strcmp(string, node->string);
		if (c < 0)
			node->left  = jn_insert(J, node->left, string, result);
		else if (c > 0)
			node->right = jn_insert(J, node->right, string, result);
		else
			return *result = node->string, node;
		node = jn_skew(node);
		node = jn_split(node);
		return node;
	}
	return jn_newstring(J, string, result);
}

static void dump_node(js_StringNode *node, int level)
{
    int i;
	if (CHECK_STR_NODE(node->left))
		dump_node(node->left, level + 1);
	printf("%d: ", node->level);
	for (i = 0; i < level; ++i)
		putchar('\t');
	printf("'%s'\n", node->string);

	if (CHECK_STR_NODE(node->right))
		dump_node(node->right, level + 1);
}

void jn_dumpstrings(js_State *J)
{
	js_StringNode *root = J->strings;
	printf("interned strings {\n");
	if (CHECK_STR_NODE(root))
		dump_node(root, 1);
	printf("}\n");
}

static void jn_free_str_node(js_State *J, js_StringNode *node)
{
	if (CHECK_STR_NODE(node->left))
        jn_free_str_node(J, node->left);
	if (CHECK_STR_NODE(node->right))
	    jn_free_str_node(J, node->right);
	js_free(J, node);
}

void jn_free_strings(js_State *J)
{
	if (CHECK_STR_NODE(J->strings))
		jn_free_str_node(J, J->strings);
}

const char *js_intern(js_State *J, const char *s)
{
	const char *result;
	if (!J->strings)
		 J->strings = &jstr_null;
	J->strings = jn_insert(J, J->strings, s, &result);
	return result;
}
