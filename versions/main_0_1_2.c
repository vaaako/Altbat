#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "./libs/mpc/mpc.h"


// cc -std=c99 -Wall -ledit -lm main.c libs/mpc/mpc.c -o prompt


// If compiling on Windows compile these functions
#ifdef _WIN32 // _LP64 for linux
#include <string.h>

// Buffer for suer input of size 2048
static char buffer[2048];

// Fake readline function
char* readline(char* prompt) {
	fputs(prompt, stdout);
	fgets(buffer, 2048, stdin);
	char* cpy = malloc(strlen(buffer)+1); // +1 reserved to \0
	strcpy(cpy, buffer);
	cpy[strlen(cpy)-1] = '\0'; // Change last string character to \0
	return cpy;
}

// Fake add_history function (for no errors)
void add_history(char* unused) {}


// Otherwise include the editline header
#else
// To be able to use linux arrow keys
#include <editline.h>

#endif

/**
 * 
 * VVAL stands for Vasp Value
 * 
 * */

/*******************/
/* CONSTRUCTORS    */
/*******************/

/**
 * Adding new types follows this basic steps:
 * 1. Add on enum of types
 * 	1.1. If necessary add a new field to struct vval
 * 2. Add a type cosntructor
 * 3. Add deletion to vval_del()
 * 4. Add to vval_print()
 * 
 * */

// Foward declarations
struct vval;
struct venv;
typedef struct vval vval;
typedef struct venv venv; // Environment

// Visp Value

// enum of possible vval (vasp value) types
enum VTypes {
	VVAL_ERR,
	VVAL_NUM,
	VVAL_SYM,

	VVAL_FUN,
	VVAL_SEXPR,
	VVAL_QEXPR
};

// Function pointer type
// "To get an vval* we dereference vbuiltin and call it with a venv* and a vval*"
typedef vval*(*vbuiltin)(venv*, vval*);

// New vval struct
struct vval {
	int type;

	/* Basic */
	double num;
	// Error and symbol types have some string data
	char* err;
	char* sym;

	/* Function */
	vbuiltin builtin;

	/* Expression */
	// Count and Pointer to a list of "vval*"
	// We will also need to keep track of how many lval* are in this list
	int count; // e.g. list {1 2} -> count = 1, because "list" is the operator, so it doens't counts and {1 2} is a QExpr, so counts as one cell (with two cell of type VVAL_NUM inside)
	struct vval** cell; /* which points to a location where we store a list of lval*. More specifically pointers to the other individual vval \
	└ e.g. (list {1 2}) -> "{ 1 2 }" is the first cell, it's count is 2 (because have two cells of type VVAL_NUM inside) */
};


// We can change our lval construction functions to return pointers to an lval, rather than one directly
// Construct a pointer to a new number vval
vval* vval_num(double x) {
	vval* v = malloc(sizeof(vval));
	v->type = VVAL_NUM;
	v->num  = x;
	return v;
}

// Construct a pointer to a new Error vval
vval* vval_err(char* fmt, ...) {
	vval* v = malloc(sizeof(vval));
	v->type = VVAL_ERR;

	// Create a va list and initialize it
	va_list va;
	va_start(va, fmt);

	// Allocate 512 bytes of space
	v->err = malloc(512);

	// printf the error string with a maximu of 511 characters
	vsnprintf(v->err, 511, fmt, va);

	// Reallocate to number of bytes actually used
	v->err = realloc(v->err, strlen(v->err)+1);

	// Cleanup va list
	va_end(va);

	return v;
}


// VVAL_ERR,
// VVAL_NUM,
// VVAL_SYM,

// VVAL_FUN,
// VVAL_SEXPR,
// VVAL_QEXPR
struct VTypeMap {
	enum VTypes vtype;
	char* name;
} vtypes_map[] = { // Same order as VTypes
	{ VVAL_ERR, "Error" },
	{ VVAL_NUM, "Number" },
	{ VVAL_SYM, "Symbol" },
	{ VVAL_FUN, "Function" },
	{ VVAL_SEXPR, "S-Expression" },
	{ VVAL_QEXPR, "Q-Expression" }
};

char* vtype_name(int t) {
	if(vtypes_map[t].vtype == t) {
		return vtypes_map[t].name;
	} else {
		return "Unknown";
	}
}

// Construct a pointer to a new Symbol vvfal
vval* vval_sym(char* s) {
	vval* v = malloc(sizeof(vval));
	v->type = VVAL_SYM;
	v->sym  = malloc(strlen(s) +1);
	strcpy(v->sym, s);
	return v;
}

// A pointer to a new empty Sexpr vval
vval* vval_sexpr(void) {
	vval* v  = malloc(sizeof(vval));
	v->type  = VVAL_SEXPR;
	v->count = 0;
	v->cell  = NULL;
	return v;
}

// A pointer to a new empty Qexpr vval
vval* vval_qexpr(void) {
	vval* v  = malloc(sizeof(vval));
	v->type  = VVAL_QEXPR;
	v->count = 0;
	v->cell  = NULL;
	return v;
}

// A pointer to a new function vval
vval* vval_fun(vbuiltin func) {
	vval* v = malloc(sizeof(vval));
	v->type = VVAL_FUN;
	v->builtin  = func;
	return v;
}


void vval_del(vval* v) {
	switch (v->type) {
		// Do nothing special for number and function type
		case VVAL_NUM: break;
		case VVAL_FUN: break;

		// For Err or Sym free the string data
		case VVAL_ERR: free(v->err); break;
		case VVAL_SYM: free(v->sym); break;


		// If Sexpr or Qexpr then delete all elements inside
		case VVAL_SEXPR:
		case VVAL_QEXPR:
			for(int i=0; i < v->count; i++) {
				vval_del(v->cell[i]);
			}

			// Also free the memory allocated to contain the pointer
			free(v->cell);
			break;
	}

	// Free the memory allocated for the vval struct itself
	free(v);
}

vval* vval_copy(vval* v) {
	vval* x = malloc(sizeof(vval));
	x->type = v->type;

	switch (v->type) {
		// Copy Functions and Numbers directly
		case VVAL_NUM: x->num = v->num; break;
		case VVAL_FUN: x->builtin = v->builtin; break;

		// Copy Strings using malloc and strcpy
		case VVAL_ERR:
			x->err = malloc(strlen(v->err) + 1);
			strcpy(x->err, v->err);
			break;

		case VVAL_SYM:
			x->sym = malloc(strlen(v->sym) + 1);
			strcpy(x->sym, v->sym);
			break;

		// Copy Lists by copying each sub-expression
		case VVAL_SEXPR:
		case VVAL_QEXPR:
			x->count = v->count;
			x->cell = malloc(sizeof(vval*) * x->count);
			for(int i=0; i<x->count; i++)
				x->cell[i] = vval_copy(v->cell[i]);
			break;
	}

	return x;
}

/**************
 * ENVIORENMENT
 * ************/
struct venv {
	int count;
	char** syms;
	vval** vals;
};

venv* venv_new(void) {
	venv* e  = malloc(sizeof(venv));
	e->count = 0;
	e->syms  = NULL;
	e->vals  = NULL;
	return e;
}

void venv_del(venv* e) {
	for(int i=0; i<e->count; i++) {
		free(e->syms[i]);
		vval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

vval* venv_get(venv* e, vval* k) {
	// Iterate over all items in enviroment
	for(int i=0; i<e->count; i++) {
		// Check if the stored string matches the symbol string
		// If it does, return a copy of the value
		if(strcmp(e->syms[i], k->sym)==0) return vval_copy(e->vals[i]);
	}

	// If no symbol found return error
	return vval_err("Unbound symbol '%s'!", k->sym);
}

/* If no existing value is found with that name, we need to
 * allocate some more space to put it in */
void venv_put(venv* e, vval* k, vval* v) {
	// Iterate over all items in environment
	// This is to see if variable alredy exists
	for(int i=0; i<e->count; i++) {
		// If variable is found delete item at that position
		// And replace with variable supplied by user
		if(strcmp(e->syms[i], k->sym)==0) {
			vval_del(e->vals[i]);
			e->vals[i] = vval_copy(v);
			return;
		}
	}

	// If no existing entry found allocate space for new entry
	e->count++;
	e->vals = realloc(e->vals, sizeof(vval*) * e->count);
	e->syms = realloc(e->syms, sizeof(char*) * e->count);

	// Copy contents of vval and symbol stirng into new location
	e->vals[e->count-1] = vval_copy(v);
	e->syms[e->count-1] = malloc(strlen(k->sym)+1);
	strcpy(e->syms[e->count-1], k->sym);
}


void vval_print(vval* v);
// Print vval followed by a newline
void lval_expr_print(vval* v, char open, char close) {
	putchar(open);

	for(int i=0; i < v->count; i++) {
		// Print Value contanined withi
		vval_print(v->cell[i]);

		// Don't print trailing space if last element
		if(i != (v->count-1))
			putchar(' ');
	}
	putchar(close);
}

// Print a vval
void vval_print(vval* v) {
	switch (v->type) {
		case VVAL_NUM:
			if(v->num == (long)v->num)
				printf("%li", (long)v->num);
			else
				printf("%.1lf", v->num);
			break;
		case VVAL_ERR: printf("Error: %s", v->err); break;
		case VVAL_SYM: printf("%s", v->sym); break;
		case VVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
		case VVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
		case VVAL_FUN: printf("<function>"); break;

	}
}

void vval_println(vval* v) {
	vval_print(v);
	putchar('\n');
}







/*******************/
/* TOOLS (???)     */
/*******************/




/**
 * 
 * This function increases the count of the Expression list by one, and then uses
 * realloc to reallocate the amount of space required by v->cell.
 * */
vval* vval_add(vval* v, vval* x) {
	v->count++;
	v->cell = realloc(v->cell, sizeof(vval*) * v->count);
	v->cell[v->count-1] = x;
	return v;
}

/**
 * The lval_pop function extracts a single element from
 * an S-Expression at index i and shifts the rest of the list
 * backward so that it no longer contains that vval* 
 * */
vval* vval_pop(vval* v, int i) {
	// Find the item at "i"
	vval* x = v->cell[i];

	// Shift memory after the item at "i" over
	// memmove -> Move memory block to another address reagion
	memmove(&v->cell[i], &v->cell[i+1], sizeof(vval*) * (v->count-i-1)); // dest, src, size

	// Decrease the count of items in the list
	v->count--;

	// Reallocate the memory used
	v->cell = realloc(v->cell, sizeof(vval*) * v->count);
	return x;
}

/**
 * The vval_take function is similar to vval_pop but it
 * deletes the list it has extracted the element from
 * */
vval* vval_take(vval* v, int i) {
	vval* x = vval_pop(v, i);
	vval_del(v);
	return x;
}


vval* vval_join(vval* x, vval* y) {
	// For each cell in 'y' add it to 'x'
	while(y->count)
		x = vval_add(x, vval_pop(y, 0));

	// Delete the empty 'y' and return 'x'
	vval_del(y);
	return x;
}






/*************************/
/* BUILT IN METHODS      */
/*************************/

// Macro to help with error conditions
#define VASSERT(args, cond, fmt, ...) \
	if(!(cond)) { \
		vval* err = vval_err(fmt, ##__VA_ARGS__); \
		vval_del(args); \
		return err; \
	}

#define VASSERT_TYPE(func, args, index, expect) \
	VASSERT(args, args->cell[index]->type == expect, \
		"Function '%s' Got %s type for argument %i, Expected %s.", \
		func, index, vtype_name(args->cell[index]->type), vtype_name(expect))

#define VASSERT_NUM(func, args, num) \
	VASSERT(args, args->count == num, \
		"Function '%s' passed %i arguments, Expected %i.", \
		func, args->count, num)

#define VASSERT_NOT_EMPTY(func, args, index) \
  VASSERT(args, args->cell[index]->count != 0, \
	"Function '%s' passed {} for argument %i.", func, index)



vval* vval_eval(venv* e, vval* v);
// Takes one or more arguments and returns a new Q-Expression containing the arguments
vval* builtin_list(venv* e, vval* a) {
	a->type = VVAL_QEXPR;
	return a;
}

// Takes a Q-Expression and returns a Q-Expression with only the first element
vval* builtin_head(venv* e, vval* a) {
	// Check error conditions
	VASSERT_NUM("head", a, 1);
	VASSERT_TYPE("head", a, 0, VVAL_QEXPR);
	VASSERT_NOT_EMPTY("head", a, 0);
	
	// Otherwise take first argument
	vval* v = vval_take(a, 0);

	// Delete all elements that are not head and return
	while(v->count > 1) vval_del(vval_pop(v, 1));
	return v;
}

// Takes a Q-Expression and returns a Q-Expression with the first element removed
vval* builtin_tail(venv* e, vval* a) {
	// Check error conditions
	VASSERT_NUM("tail", a, 1);
	VASSERT_TYPE("tail", a, 0, VVAL_QEXPR);
	VASSERT_NOT_EMPTY("tail", a, 0);

	// Take first argument
	vval* v = vval_take(a, 0);

	// Delete first element and return
	vval_del(vval_pop(v, 0));
	return v;
}



// Takes a Q-Expression and evaluates it as if it were a S-Expression
vval* builtin_eval(venv* e, vval* a) {
	VASSERT_NUM("eval", a, 1);
	VASSERT_TYPE("eval", a, 0, VVAL_QEXPR);

	vval* x = vval_take(a, 0);
	x->type = VVAL_SEXPR;
	return vval_eval(e, x);
}



// Takes one or more Q-Expressions and returns a Q-Expression of them conjoined together
vval* builtin_join(venv* e, vval* a) {
	for(int i=0; i<a->count; i++) {
		VASSERT_TYPE("join", a, i, VVAL_QEXPR);
	}

	vval* x = vval_pop(a, 0);

	while(a->count) {
		vval* y = vval_pop(a, 0);
		x = vval_join(x, y);
	}

	vval_del(a);
	return x;
}


// Returns the number of elements in a Q-Expression
vval* builtin_len(venv* e, vval* a) {
	VASSERT_NUM("len", a, 1);
	VASSERT_TYPE("len", a, 0, VVAL_QEXPR);

	a->type = VVAL_NUM;
	a->num = a->cell[0]->count;
	return a;
}


// Takes a value and a Q-Expression and appends it to the front
vval* builtin_cons(venv* e, vval* a) {
	VASSERT_NUM("cons", a, 1);
	VASSERT_TYPE("cons", a, 0, VVAL_QEXPR);
	VASSERT_TYPE("cons", a, 1, VVAL_QEXPR);

	// printf("%f\n", a->cell[1]->cell[0]->num); // Print first number of second cell

	vval* x = vval_pop(a, 1); // Get qexpr to extract numbers from

	vval* new = vval_qexpr(); // Make a new cell of type qexpr
	new = vval_add(new, vval_take(a, 0)); // Add first element as the number passed (and delete to remove it from memory, no longer necessary)

	// Add to new all elements from x
	while(x->count)
		new = vval_add(new, vval_pop(x, 0));
	vval_del(x); // No longer necessary

	return new;
}


vval* builtin_env(venv* e, vval* a) {
	VASSERT_NUM("env", a, 0);

	for(int i=0; i<e->count; i++)
		printf("%s\n",e->syms[i]);

	return a;
}


// enum of operators_map types of eval_op
enum OperatorCode {
	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_RES,
	OP_POW,
	OP_MIN,
	OP_MAX,
	OP_UNKNOWN
};

// Define struct of operators
struct OperatorEntry {
	char* name;
	enum OperatorCode code;

} operators_map[] = { // Define operators
	{ "+",   OP_ADD },
	{ "add", OP_ADD },
	{ "-",   OP_SUB },
	{ "min", OP_SUB },
	{ "*",   OP_MUL },
	{ "mul", OP_MUL },
	{ "/",   OP_DIV },
	{ "div", OP_DIV },
	{ "%",   OP_RES },
	{ "res", OP_RES },
	{ "^",   OP_POW },
	{ "pow", OP_POW },
	{ "<", OP_MIN },
	{ "min", OP_MIN },
	{ ">", OP_MAX },
	{ "max", OP_MAX }
};

vval* builtin_op(venv* e, vval* a, char* op) {
	// Ensure all arguments are numbers
	for(int i=0; i < a->count; i++) {
		if(a->cell[i]->type != VVAL_NUM) {
			vval_del(a);
			return vval_err("Cannot operate non-numbers!");
		}
	}

	// Pop the first element
	vval* x = vval_pop(a, 0);

	// If no arguments and sub then perform unary negation
	if((strcmp(op, "-")==0) && a->count == 0)
		x->num = -x->num;

	// While there are still elements remaining
	while(a->count > 0) {
		int err = 0; // Check for error from inside switch

		// Pop the next element
		vval* y = vval_pop(a, 0);

		int num_operators = sizeof(operators_map) / sizeof(operators_map[0]); // Find array lenght

		// Get operator index
		enum OperatorCode operator_code = OP_UNKNOWN;
		for(int i=0; i<num_operators; i++) {
			if(strcmp(op, operators_map[i].name)==0) {
				operator_code = operators_map[i].code;
				break;
			}
		}


		switch (operator_code) {
			case OP_ADD:
				x->num += y->num;
				break;
			case OP_SUB:
				x->num -= y->num;
				break;
			case OP_MUL:
				x->num *= y->num;
				break;
			case OP_DIV:
				if(y->num==0) {
					vval_del(x); vval_del(y);
					x = vval_err("Division by Zero!");

					err=1; break; // Error
				}
				x->num /= y->num;
				break;
			case OP_RES:
				if(y->num==0) {
					vval_del(x); vval_del(y);
					x = vval_err("Division by Zero!");

					err=1; break; // Error
				}
				x->num = (long)x->num % (long)y->num;
				break;
			case OP_POW:
				x->num = pow(x->num, y->num);
				break;
			case OP_MIN:
				x->num = (x->num <= y->num) ? x->num : y->num;
				break;
			case OP_MAX:
				x->num = (x->num >= y->num) ? x->num : y->num;
				break;
			default:
				vval_del(x); vval_del(y);
				x = vval_err("Bad Operator!");
				
				err=1; break; // Erro
		}
		if(err) break; // Error occurred, break while loop


		vval_del(y);
	}

	vval_del(a); return x;
}



vval* builtin_def(venv* e, vval* a) {
	VASSERT_TYPE("def", a, 0, VVAL_QEXPR);

	// First argument is symbol list
	vval* syms = a->cell[0];

	// Ensure all elements of first list are symbols
	for(int i=0; i < syms->count; i++) {
		VASSERT(a, (syms->cell[i]->type == VVAL_SYM),
		  "Function 'def' cannot define non-symbol. "
		  "Got %s, Expected %s.",
		  vtype_name(syms->cell[i]->type), vtype_name(VVAL_SYM));
	}

	// Check correct number of symbols and values
	VASSERT(a, syms->count == a->count-1,
		"Function 'def' cannot define incorrect "
		"number of values to symbols");

	// Assign copies of values to symbols
	for(int i=0; i < syms->count; i++) {
		venv_put(e, syms->cell[i], a->cell[i+1]);
	}

	vval_del(a);
	return vval_sexpr();
}


vval* builtin_add(venv* e, vval* a) {
	return builtin_op(e, a, "+");
}

vval* builtin_sub(venv* e, vval* a) {
	return builtin_op(e, a, "-");
}

vval* builtin_mul(venv* e, vval* a) {
	return builtin_op(e, a, "*");
}

vval* builtin_div(venv* e, vval* a) {
	return builtin_op(e, a, "/");
}

vval* builtin_res(venv* e, vval* a) {
	return builtin_op(e, a, "%");
}

vval* builtin_pow(venv* e, vval* a) {
	return builtin_op(e, a, "^");
}

vval* builtin_min(venv* e, vval* a) {
	return builtin_op(e, a, "<");
}

vval* builtin_max(venv* e, vval* a) {
	return builtin_op(e, a, ">");
}


/**
 * For each builtin we want to create a function lval and
 * symbol lval with the given name. We then register these
 * with the environment using lenv_put.
 * The environment always takes or returns copies
 * of a values, so we need to remember to delete these
 * two lval after registration as we won't need them any more.
 * */
void venv_add_builtin(venv* e, char* name, vbuiltin func) {
	vval* k = vval_sym(name);
	vval* v = vval_fun(func);
	venv_put(e, k, v);
	vval_del(k); vval_del(v);
}

void venv_add_builtins(venv* e) {
	// Variable functions
	venv_add_builtin(e, "def", builtin_def);

	// List Functions
	venv_add_builtin(e, "list", builtin_list);
	venv_add_builtin(e, "head", builtin_head);
	venv_add_builtin(e, "tail", builtin_tail);
	venv_add_builtin(e, "eval", builtin_eval);
	venv_add_builtin(e, "join", builtin_join);
	venv_add_builtin(e, "len", builtin_len);
	venv_add_builtin(e, "cons", builtin_cons);
	venv_add_builtin(e, "env", builtin_env);

	// Mathematical Functions
	venv_add_builtin(e, "+", builtin_add);
	venv_add_builtin(e, "add", builtin_add);

	venv_add_builtin(e, "-", builtin_sub);
	venv_add_builtin(e, "sub", builtin_sub);

	venv_add_builtin(e, "*", builtin_mul);
	venv_add_builtin(e, "mul", builtin_mul);

	venv_add_builtin(e, "/", builtin_div);
	venv_add_builtin(e, "div", builtin_div);

	venv_add_builtin(e, "%", builtin_res);
	venv_add_builtin(e, "res", builtin_res);

	venv_add_builtin(e, "^", builtin_pow);
	venv_add_builtin(e, "pow", builtin_pow);


	venv_add_builtin(e, "<", builtin_min);
	venv_add_builtin(e, "min", builtin_min);
	venv_add_builtin(e, ">", builtin_max);
	venv_add_builtin(e, "max", builtin_max);

}




// Define struct of methods
struct BuiltinMethods {
	char* name;
	vval* (*func)(venv*, vval*);
} bultin_map[] = {
	{ "list",  builtin_list },
	{ "head",  builtin_head },
	{ "tail",  builtin_tail },
	{ "join",  builtin_join },
	{ "eval",  builtin_eval },
	{ "len",   builtin_len  },
	{ "cons",  builtin_cons }
	// { "+-/*", builtin_op }
};

vval* builtin(venv* e, vval* a, char* func) {
	int num_methods = sizeof(bultin_map) / sizeof(bultin_map[0]); // Find array lenght

	// Get operator index
	// vval* builtin;
	for(int i=0; i<num_methods; i++) {
		if(strcmp(func, bultin_map[i].name)==0)
			return bultin_map[i].func(e, a);
		else if(strstr("+-*/%^ \
						add sub mul div res pow min max", func))
			return builtin_op(e, a, func);
	}

	vval_del(a);
	return vval_err("Unknown Function!");
}


vval* vval_eval_sexpr(venv* e, vval* v) {
	// Evaluate Children
	for(int i=0; i < v->count; i++) {
		v->cell[i] = vval_eval(e, v->cell[i]);
	}

	// Error Checking
	for(int i=0; i < v->count; i++) {
		if(v->cell[i]->type == VVAL_ERR) return vval_take(v, i); 
	}

	// Empty Expression
	if(v->count == 0) return v;

	// Single Expression
	if(v->count == 1) return vval_take(v, 0);

	// Ensure First Element is a function after evaluation
	vval* f = vval_pop(v, 0);
	if(f->type != VVAL_FUN) {
		// vval* err = vval_err(
		// 	"S-Expression starts with incorrect type"
		// 	"Got %s, Expected %s",
		// 	vtype_name(f->type), vtype_name(VVAL_FUN));

		vval_del(v); vval_del(f);
		return vval_err("First element is not a function!");
	}

	// If so call function to get result
	vval* result = f->builtin(e, v);
	vval_del(f);
	return result;
}

vval* vval_eval(venv* e, vval* v) {
	// Evaluate Sexpressions
	if(v->type == VVAL_SYM) {
		vval* x = venv_get(e, v);
		vval_del(v);
		return x;
	}
	if(v->type == VVAL_SEXPR) return vval_eval_sexpr(e, v);
	return v;

	// All other vval types ramin the same

}




/*************************/
/* READ                  */
/*************************/

vval* vval_read_num(mpc_ast_t* t) {
	errno = 0;
	double x = atof(t->contents);
	return (errno != ERANGE) ? vval_num(x)
		: vval_err("Error: Invalid Number!");
}

vval* vval_read(mpc_ast_t* t) {
	// If Symbol or Number return conversion to that type
	if(strstr(t->tag, "number")) return vval_read_num(t);
	if(strstr(t->tag, "symbol")) return vval_sym(t->contents);

	// If root (>) or sexpr then create empty list
	vval* x = NULL;
	if(strcmp(t->tag, ">")==0) x = vval_sexpr();
	if(strstr(t->tag, "sexpr")) x = vval_sexpr();
	if(strstr(t->tag, "qexpr")) x = vval_qexpr();

	// Fill this list with any valid expression contained within
	for(int i=0; i < t->children_num; i++) {
		if(strcmp(t->children[i]->contents, "(")==0
			|| strcmp(t->children[i]->contents, ")")==0
			|| strcmp(t->children[i]->contents, "{")==0
			|| strcmp(t->children[i]->contents, "}")==0
			|| strcmp(t->children[i]->tag, "regex")==0) continue;

		x = vval_add(x, vval_read(t->children[i]));
	}

	return x;
}




int main(int argc, char const *argv[]) {
	// Create some parses
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* Sexpr  = mpc_new("sexpr"); // S-Expression (hihi)
	mpc_parser_t* Qexpr  = mpc_new("qexpr"); // Q-Expression (Quoted Expressions)
	mpc_parser_t* Expr   = mpc_new("expr");
	mpc_parser_t* Vasp  = mpc_new("vasp");

	// Define them with the following Language
	mpca_lang(MPCA_LANG_DEFAULT,
		// Pass regex of what consists each element of a "Expression"
		// INT and FLOAT = [-+]?(([0-9]+(\\.[0-9]*)?)|(\\.[0-9]+))
		// Symbol = Accept any alphanumeric and character
		"                                                                \
			number    : /[-+]?(([0-9]+(\\.[0-9]*)?)|(\\.[0-9]+))/ ;      \
			symbol    : /[a-zA-Z0-9_+\\-*^%\\/\\\\=<>!&]+/ ;               \
			sexpr     : '(' <expr>* ')' ;                                \
			qexpr     : '{' <expr>* '}' ;                                \
			expr      : <number> | <symbol> | <sexpr> | <qexpr> ;        \
			vasp     : /^/ <expr>+ /$/ ; ",
		Number, Symbol, Sexpr, Qexpr, Expr, Vasp);

	// Vakon
	puts("Vasp Version 0.1.2");
	puts("Press Ctrl+c to Exit");
	puts("Type \"author\" for more information.\n");




	venv* e = venv_new();
	venv_add_builtins(e);

	while(1) {
		// Read user's input
		char *input = readline("Vasp> ");

		// Add input to history
		add_history(input);

		if(strcmp(input, "author")==0) {
			puts("Author: Vako");
			free(input);
			continue;
		}

		// Attempt to parse the user input
		mpc_result_t r;
		if(mpc_parse("<stdin>", input, Vasp, &r)) {
			// Sucess

			vval* x = vval_eval(e, vval_read(r.output));
			vval_println(x);
			vval_del(x);

			// mpc_ast_print(r.output);
			mpc_ast_delete(r.output);
		} else {
			// Error

			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}

		// Free input memory
		free(input);
	}

	venv_del(e);

	// Undefine and Delete Parsers
	mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Vasp);
}






/*

			number    : /[-+]?(([0-9]+(\\.[0-9]*)?)|(\\.[0-9]+))/;       \
			operator  : '+' | '-' | '*' | '/' | '%' | '^'                \
					  | \"add\" | \"sub\" | \"mul\" | \"div\"            \
					  | \"res\" | \"pow\" | \"min\" | \"max\";           \
			term      : <number> | '(' <expr> ')' ;                      \
			expr      : <term> ((<operator>) <term>)*;                   \
			vasp     : /^/ <expr>+ /$/;                                 \

*/