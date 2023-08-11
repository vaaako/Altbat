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
 * BVAL stands for Altbat Value
 * 
 * */

/*******************/
/* CONSTRUCTORS    */
/*******************/

/**
 * Adding new types follows this basic steps:
 * 1. Add on enum of types
 * 	1.1. If necessary add a new field to struct bval
 * 2. Add a type cosntructor
 * 3. Add deletion to bval_del()
 * 4. Add to bval_print()
 * 5. Add to bval_copy()
 * 
 * */

// Foward declarations
struct bval;
struct benv;
typedef struct bval bval;
typedef struct benv benv; // Environment

// Visp Value

// enum of possible bval (bat value) types
enum BTypes {
	BVAL_ERR,
	BVAL_NUM,
	BVAL_SYM,

	BVAL_FUN,
	BVAL_SEXPR,
	BVAL_QEXPR
};

// Function pointer type
// "To get an bval* we dereference bbuiltin and call it with a benv* and a bval*"
typedef bval*(*bbuiltin)(benv*, bval*);

// New bval struct
struct bval {
	int type;

	/* Basic */
	double num;
	// Error and symbol types have some string data
	char* err;
	char* sym;

	/* Function */
	bbuiltin builtin;
	benv* env;
	bval* formals;
	bval* body;

	/* Expression */
	// Count and Pointer to a list of "bval*"
	// We will also need to keep track of how many lval* are in this list
	int count; // e.g. list {1 2} -> count = 1, because "list" is the operator, so it doens't counts and {1 2} is a QExpr, so counts as one cell (with two cell of type BVAL_NUM inside)
	struct bval** cell; /* which points to a location where we store a list of lval*. More specifically pointers to the other individual bval \
	└ e.g. (list {1 2}) -> "{ 1 2 }" is the first cell, it's count is 2 (because have two cells of type BVAL_NUM inside) */
};


// We can change our lval construction functions to return pointers to an lval, rather than one directly
// Construct a pointer to a new number bval
bval* bval_num(double x) {
	bval* v = malloc(sizeof(bval));
	v->type = BVAL_NUM;
	v->num  = x;
	return v;
}

// Construct a pointer to a new Error bval
bval* bval_err(char* fmt, ...) {
	bval* v = malloc(sizeof(bval));
	v->type = BVAL_ERR;

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


// BVAL_ERR,
// BVAL_NUM,
// BVAL_SYM,

// BVAL_FUN,
// BVAL_SEXPR,
// BVAL_QEXPR
struct VTypeMap {
	enum BTypes btype;
	char* name;
} btypes_map[] = { // Same order as BTypes
	{ BVAL_ERR, "Error" },
	{ BVAL_NUM, "Number" },
	{ BVAL_SYM, "Symbol" },
	{ BVAL_FUN, "Function" },
	{ BVAL_SEXPR, "S-Expression" },
	{ BVAL_QEXPR, "Q-Expression" }
};

char* btype_name(int t) {
	if(btypes_map[t].btype == t) {
		return btypes_map[t].name;
	} else {
		return "Unknown";
	}
}

// Construct a pointer to a new Symbol vvfal
bval* bval_sym(char* s) {
	bval* v = malloc(sizeof(bval));
	v->type = BVAL_SYM;
	v->sym  = malloc(strlen(s) +1);
	strcpy(v->sym, s);
	return v;
}

// A pointer to a new empty Sexpr bval
bval* bval_sexpr(void) {
	bval* v  = malloc(sizeof(bval));
	v->type  = BVAL_SEXPR;
	v->count = 0;
	v->cell  = NULL;
	return v;
}

// A pointer to a new empty Qexpr bval
bval* bval_qexpr(void) {
	bval* v  = malloc(sizeof(bval));
	v->type  = BVAL_QEXPR;
	v->count = 0;
	v->cell  = NULL;
	return v;
}

// A pointer to a new function bval
bval* bval_fun(bbuiltin func) {
	bval* v = malloc(sizeof(bval));
	v->type = BVAL_FUN;
	v->builtin  = func;
	return v;
}




benv* benv_new();
void benv_del(benv*);
benv* benv_copy(benv*);

bval* bval_lambda(bval* formals, bval* body) {
	bval* v = malloc(sizeof(bval));
	v->type = BVAL_FUN;

	// Set builtin to null
	v->builtin = NULL;

	// Build new environent
	v->env = benv_new();

	// Set formals and body
	v->formals = formals;
	v->body = body;
	return v;
}

void bval_del(bval* v) {
	switch (v->type) {
		// Do nothing special for number and function type
		case BVAL_NUM: break;
		case BVAL_FUN:
			if(!v->builtin) {
				benv_del(v->env);
				bval_del(v->formals);
				bval_del(v->body);
			}
		break;

		// For Err or Sym free the string data
		case BVAL_ERR: free(v->err); break;
		case BVAL_SYM: free(v->sym); break;


		// If Sexpr or Qexpr then delete all elements inside
		case BVAL_SEXPR:
		case BVAL_QEXPR:
			for(int i=0; i < v->count; i++) {
				bval_del(v->cell[i]);
			}

			// Also free the memory allocated to contain the pointer
			free(v->cell);
			break;
	}

	// Free the memory allocated for the bval struct itself
	free(v);
}

bval* bval_copy(bval* v) {
	bval* x = malloc(sizeof(bval));
	x->type = v->type;

	switch (v->type) {
		// Copy Functions and Numbers directly
		case BVAL_NUM: x->num = v->num; break;
		case BVAL_FUN:
			if(v->builtin) {
				x->builtin = v->builtin;
			}
			else {
				x->builtin = NULL;
				x->env     = benv_copy(v->env);
				x->formals = bval_copy(v->formals);
				x->body    = bval_copy(v->body);
			}
			break;

		// Copy Strings using malloc and strcpy
		case BVAL_ERR:
			x->err = malloc(strlen(v->err) + 1);
			strcpy(x->err, v->err);
			break;

		case BVAL_SYM:
			x->sym = malloc(strlen(v->sym) + 1);
			strcpy(x->sym, v->sym);
			break;

		// Copy Lists by copying each sub-expression
		case BVAL_SEXPR:
		case BVAL_QEXPR:
			x->count = v->count;
			x->cell = malloc(sizeof(bval*) * x->count);
			for(int i=0; i<x->count; i++)
				x->cell[i] = bval_copy(v->cell[i]);
			break;
	}

	return x;
}

/**************
 * ENVIORONMENT
 * ************/
struct benv {
	benv* par; // Parent
	int count;
	char** syms;
	bval** vals;
};

/**
 * The way the parent environment works is simple.
 * If someone calls benv_get on the environment,
 * and the symbol cannot be found. It will look then
 * in any parent environment to see if the named value
 * exists there, and repeat the process till either the
 * variable is found or there are no more parents.
 * To signify that an environment has no parent we set
 * the reference to NULL.
 * */

benv* benv_new(void) {
	benv* e  = malloc(sizeof(benv));
	e->par   = NULL;
	e->count = 0;
	e->syms  = NULL;
	e->vals  = NULL;
	return e;
}

void benv_del(benv* e) {
	for(int i=0; i<e->count; i++) {
		free(e->syms[i]);
		bval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

bval* benv_get(benv* e, bval* k) {
	// Iterate over all items in enviroment
	for(int i=0; i<e->count; i++) {
		// Check if the stored string matches the symbol string
		// If it does, return a copy of the value
		if(strcmp(e->syms[i], k->sym)==0) return bval_copy(e->vals[i]);
	}

	// If no symbol found in parent otherwise error
	if(e->par)
		return benv_get(e->par, k);
	else return bval_err("Unbound symbol '%s'!", k->sym);
}


/**
 * Because we have a new lval type that has its
 * own environment we need a function for copying
 * environments, to use for when we copy lval structs.
 * */
benv* benv_copy(benv* e) {
	benv* n  = malloc(sizeof(benv));
	n->par   = e->par;
	n->count = e->count;
	n->syms  = malloc(sizeof(char*) * n->count);
	n->vals  = malloc(sizeof(bval*) * n->count);

	for(int i=0; i<e->count; i++) {
		n->syms[i] = malloc(strlen(e->syms[i]) + 1);
		strcpy(n->syms[i], e->syms[i]);
		n->vals[i] = bval_copy(e->vals[i]);
	}
	return n;
}


/* If no existing value is found with that name, we need to
 * allocate some more space to put it in */
void benv_put(benv* e, bval* k, bval* v) {
	// Iterate over all items in environment
	// This is to see if variable alredy exists
	for(int i=0; i<e->count; i++) {
		// If variable is found delete item at that position
		// And replace with variable supplied by user
		if(strcmp(e->syms[i], k->sym)==0) {
			bval_del(e->vals[i]);
			e->vals[i] = bval_copy(v);
			return;
		}
	}

	// If no existing entry found allocate space for new entry
	e->count++;
	e->vals = realloc(e->vals, sizeof(bval*) * e->count);
	e->syms = realloc(e->syms, sizeof(char*) * e->count);

	// Copy contents of bval and symbol stirng into new location
	e->vals[e->count-1] = bval_copy(v);
	e->syms[e->count-1] = malloc(strlen(k->sym)+1);
	strcpy(e->syms[e->count-1], k->sym);
}

void benv_def(benv* e, bval* k, bval* v) {
	// Iterate till e has no parent
	while(e->par)
		e = e->par;

	// Pu value in e
	benv_put(e, k, v);
}




/*******************
 * PRINT
 *******************/
void bval_print(bval* v);
// Print bval followed by a newline
void bval_expr_print(bval* v, char open, char close) {
	putchar(open);

	for(int i=0; i < v->count; i++) {
		// Print Value contanined withi
		bval_print(v->cell[i]);

		// Don't print trailing space if last element
		if(i != (v->count-1))
			putchar(' ');
	}
	putchar(close);
}

// Print a bval
void bval_print(bval* v) {
	switch (v->type) {
		case BVAL_NUM:
			if(v->num == (long)v->num)
				printf("%li", (long)v->num);
			else
				printf("%.1lf", v->num);
			break;
		case BVAL_ERR: printf("Error: %s", v->err); break;
		case BVAL_SYM: printf("%s", v->sym); break;
		case BVAL_SEXPR: bval_expr_print(v, '(', ')'); break;
		case BVAL_QEXPR: bval_expr_print(v, '{', '}'); break;
		case BVAL_FUN:
			if(v->builtin) {
				printf("<builtin>");
			} else {
				printf("(\\ "); bval_print(v->formals);
				putchar(' '); bval_print(v->body); putchar(')');
			}
		break;

	}
}

void bval_println(bval* v) {
	bval_print(v);
	putchar('\n');
}







/*******************/
/* Utility         */
/*******************/




/**
 * 
 * This function increases the count of the Expression list by one, and then uses
 * realloc to reallocate the amount of space required by v->cell.
 * */
bval* bval_add(bval* v, bval* x) {
	v->count++;
	v->cell = realloc(v->cell, sizeof(bval*) * v->count);
	v->cell[v->count-1] = x;
	return v;
}

/**
 * The lval_pop function extracts a single element from
 * an S-Expression at index i and shifts the rest of the list
 * backward so that it no longer contains that bval* 
 * */
bval* bval_pop(bval* v, int i) {
	// Find the item at "i"
	bval* x = v->cell[i];

	// Shift memory after the item at "i" over
	// memmove -> Move memory block to another address reagion
	memmove(&v->cell[i], &v->cell[i+1], sizeof(bval*) * (v->count-i-1)); // dest, src, size

	// Decrease the count of items in the list
	v->count--;

	// Reallocate the memory used
	v->cell = realloc(v->cell, sizeof(bval*) * v->count);
	return x;
}

/**
 * The bval_take function is similar to bval_pop but it
 * deletes the list it has extracted the element from
 * */
bval* bval_take(bval* v, int i) {
	bval* x = bval_pop(v, i);
	bval_del(v);
	return x;
}


bval* bval_join(bval* x, bval* y) {
	// For each cell in 'y' add it to 'x'
	while(y->count)
		x = bval_add(x, bval_pop(y, 0));

	// Delete the empty 'y' and return 'x'
	bval_del(y);
	return x;
}






/*************************/
/* BUILT IN METHODS      */
/*************************/

// Macro to help with error conditions
#define BASSERT(args, cond, fmt, ...) \
	if(!(cond)) { \
		bval* err = bval_err(fmt, ##__VA_ARGS__); \
		bval_del(args); \
		return err; \
	}

#define BASSERT_TYPE(func, args, index, expect) \
	BASSERT(args, args->cell[index]->type == expect, \
		"Function '%s' Got %s type for argument %i, Expected %s.", \
		func, index, btype_name(args->cell[index]->type), btype_name(expect))

#define BASSERT_NUM(func, args, num) \
	BASSERT(args, args->count == num, \
		"Function '%s' passed %i arguments, Expected %i.", \
		func, args->count, num)

#define BASSERT_NOT_EMPTY(func, args, index) \
	BASSERT(args, args->cell[index]->count != 0, \
	"Function '%s' passed {} for argument %i.", func, index)



bval* bval_eval(benv* e, bval* v);
// Takes one or more arguments and returns a new Q-Expression containing the arguments
bval* builtin_list(benv* e, bval* a) {
	a->type = BVAL_QEXPR;
	return a;
}

// Takes a Q-Expression and returns a Q-Expression with only the first element
bval* builtin_head(benv* e, bval* a) {
	// Check error conditions
	BASSERT_NUM("head", a, 1);
	BASSERT_TYPE("head", a, 0, BVAL_QEXPR);
	BASSERT_NOT_EMPTY("head", a, 0);
	
	// Otherwise take first argument
	bval* v = bval_take(a, 0);

	// Delete all elements that are not head and return
	while(v->count > 1) bval_del(bval_pop(v, 1));
	return v;
}

// Takes a Q-Expression and returns a Q-Expression with the first element removed
bval* builtin_tail(benv* e, bval* a) {
	// Check error conditions
	BASSERT_NUM("tail", a, 1);
	BASSERT_TYPE("tail", a, 0, BVAL_QEXPR);
	BASSERT_NOT_EMPTY("tail", a, 0);

	// Take first argument
	bval* v = bval_take(a, 0);

	// Delete first element and return
	bval_del(bval_pop(v, 0));
	return v;
}



// Takes a Q-Expression and evaluates it as if it were a S-Expression
bval* builtin_eval(benv* e, bval* a) {
	BASSERT_NUM("eval", a, 1);
	BASSERT_TYPE("eval", a, 0, BVAL_QEXPR);

	bval* x = bval_take(a, 0);
	x->type = BVAL_SEXPR;
	return bval_eval(e, x);
}



// Takes one or more Q-Expressions and returns a Q-Expression of them conjoined together
bval* builtin_join(benv* e, bval* a) {
	for(int i=0; i<a->count; i++) {
		BASSERT_TYPE("join", a, i, BVAL_QEXPR);
	}

	bval* x = bval_pop(a, 0);

	while(a->count) {
		bval* y = bval_pop(a, 0);
		x = bval_join(x, y);
	}

	bval_del(a);
	return x;
}


// Returns the number of elements in a Q-Expression
bval* builtin_len(benv* e, bval* a) {
	BASSERT_NUM("len", a, 1);
	BASSERT_TYPE("len", a, 0, BVAL_QEXPR);

	a->type = BVAL_NUM;
	a->num = a->cell[0]->count;
	return a;
}


// Takes a value and a Q-Expression and appends it to the front
bval* builtin_cons(benv* e, bval* a) {
	BASSERT_NUM("cons", a, 1);
	BASSERT_TYPE("cons", a, 0, BVAL_QEXPR);
	BASSERT_TYPE("cons", a, 1, BVAL_QEXPR);

	// printf("%f\n", a->cell[1]->cell[0]->num); // Print first number of second cell

	bval* x = bval_pop(a, 1); // Get qexpr to extract numbers from

	bval* new = bval_qexpr(); // Make a new cell of type qexpr
	new = bval_add(new, bval_take(a, 0)); // Add first element as the number passed (and delete to remove it from memory, no longer necessary)

	// Add to new all elements from x
	while(x->count)
		new = bval_add(new, bval_pop(x, 0));
	bval_del(x); // No longer necessary

	return new;
}


bval* builtin_env(benv* e, bval* a) {
	BASSERT_NUM("env", a, 0);

	for(int i=0; i<e->count; i++) {
		printf("%s\n",e->syms[i]);
	}

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

bval* builtin_op(benv* e, bval* a, char* op) {
	// Ensure all arguments are numbers
	for(int i=0; i < a->count; i++) {
		if(a->cell[i]->type != BVAL_NUM) {
			bval_del(a);
			return bval_err("Cannot operate non-numbers!");
		}
	}

	// Pop the first element
	bval* x = bval_pop(a, 0);

	// If no arguments and sub then perform unary negation
	if((strcmp(op, "-")==0) && a->count == 0)
		x->num = -x->num;

	// While there are still elements remaining
	while(a->count > 0) {
		int err = 0; // Check for error from inside switch

		// Pop the next element
		bval* y = bval_pop(a, 0);

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
					bval_del(x); bval_del(y);
					x = bval_err("Division by Zero!");

					err=1; break; // Error
				}
				x->num /= y->num;
				break;
			case OP_RES:
				if(y->num==0) {
					bval_del(x); bval_del(y);
					x = bval_err("Division by Zero!");

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
				bval_del(x); bval_del(y);
				x = bval_err("Bad Operator!");
				
				err=1; break; // Erro
		}
		if(err) break; // Error occurred, break while loop


		bval_del(y);
	}

	bval_del(a); return x;
}


bval* builtin_lambda(benv* e, bval* a) {
	// Check two arguments, each of wich are Q-Expressions
	BASSERT_NUM("\\", a, 2);
	BASSERT_TYPE("\\", a, 0, BVAL_QEXPR);
	BASSERT_TYPE("\\", a, 1, BVAL_QEXPR);

	// Check first Q-Expression contains only symbols
	for(int i=0; i < a->cell[0]->count; i++) {
		BASSERT(a, (a->cell[0]->cell[i]->type == BVAL_SYM),
			"Cannot define non-symbol. Got %s, Expected %s.",
			btype_name(a->cell[0]->cell[i]->type), btype_name(BVAL_SYM));
	}

	// Pop first to arguments and pass them to bval_lambda
	bval* formals = bval_pop(a, 0);
	bval* body    = bval_pop(a, 0);
	bval_del(a);

	return bval_lambda(formals, body);
}

/**
 * There are two ways we could define a variable now.
 * Either we could define it in the local, innermost environment,
 * or we could define it in the global, outermost environment.
 * We will add functions to do both. We'll leave the benv_put method
 * the same. It can be used for definition in the local environment.
 * But we'll add a new function benv_def for definition in the global
 * environment. This works by simply following the parent chain up before
 * using benv_put to define locally 
 * */
bval* builtin_var(benv* e, bval* a, char* func) {
	BASSERT_TYPE(func, a, 0, BVAL_QEXPR);

	bval* syms = a->cell[0];
	for(int i=0; i < syms->count; i++) {
		BASSERT(a, (syms->cell[i]->type == BVAL_SYM),
			"Function '%s' cannot define non-symbol. "
			"Got %s, Expected %s.", func,
			btype_name(syms->cell[i]->type),
			btype_name(BVAL_SYM));
	}

	BASSERT(a, (syms->count == a->count-1),
		"Function '%s' passed too many arguments for symbols. "
		"Got %i, Expected %i.", func, syms->count, a->count-1);

	for(int i=0; i < syms->count; i++) {
		// If 'def' define in globally. If 'put' define in locally
		if(strcmp(func, "def")==0)
			benv_def(e, syms->cell[i], a->cell[i+1]);

		if(strcmp(func, "=")==0)
			benv_put(e, syms->cell[i], a->cell[i+1]);
	}

	bval_del(a);
	return bval_sexpr();
}

bval* builtin_def(benv* e, bval* a) {
	return builtin_var(e, a, "def");
}

bval* builtin_put(benv* e, bval* a) {
	return builtin_var(e, a, "=");
}


bval* builtin_add(benv* e, bval* a) {
	return builtin_op(e, a, "+");
}

bval* builtin_sub(benv* e, bval* a) {
	return builtin_op(e, a, "-");
}

bval* builtin_mul(benv* e, bval* a) {
	return builtin_op(e, a, "*");
}

bval* builtin_div(benv* e, bval* a) {
	return builtin_op(e, a, "/");
}

bval* builtin_res(benv* e, bval* a) {
	return builtin_op(e, a, "%");
}

bval* builtin_pow(benv* e, bval* a) {
	return builtin_op(e, a, "^");
}

bval* builtin_min(benv* e, bval* a) {
	return builtin_op(e, a, "<");
}

bval* builtin_max(benv* e, bval* a) {
	return builtin_op(e, a, ">");
}


/**
 * For each builtin we want to create a function lval and
 * symbol lval with the given name. We then register these
 * with the environment using benv_put.
 * The environment always takes or returns copies
 * of a values, so we need to remember to delete these
 * two lval after registration as we won't need them any more.
 * */
void benv_add_builtin(benv* e, char* name, bbuiltin func) {
	bval* k = bval_sym(name);
	bval* v = bval_fun(func);
	benv_put(e, k, v);
	bval_del(k); bval_del(v);
}

void benv_add_builtins(benv* e) {
	// Variable functions
	benv_add_builtin(e, "def", builtin_def);
	benv_add_builtin(e, "\\", builtin_lambda);
	benv_add_builtin(e, "def", builtin_def);
	benv_add_builtin(e, "=",   builtin_put);

	// List Functions
	benv_add_builtin(e, "list", builtin_list);
	benv_add_builtin(e, "head", builtin_head);
	benv_add_builtin(e, "tail", builtin_tail);
	benv_add_builtin(e, "eval", builtin_eval);
	benv_add_builtin(e, "join", builtin_join);
	benv_add_builtin(e, "len", builtin_len);
	benv_add_builtin(e, "cons", builtin_cons);
	benv_add_builtin(e, "env", builtin_env);

	// Mathematical Functions
	benv_add_builtin(e, "+", builtin_add);
	benv_add_builtin(e, "add", builtin_add);

	benv_add_builtin(e, "-", builtin_sub);
	benv_add_builtin(e, "sub", builtin_sub);

	benv_add_builtin(e, "*", builtin_mul);
	benv_add_builtin(e, "mul", builtin_mul);

	benv_add_builtin(e, "/", builtin_div);
	benv_add_builtin(e, "div", builtin_div);

	benv_add_builtin(e, "%", builtin_res);
	benv_add_builtin(e, "res", builtin_res);

	benv_add_builtin(e, "^", builtin_pow);
	benv_add_builtin(e, "pow", builtin_pow);


	benv_add_builtin(e, "<", builtin_min);
	benv_add_builtin(e, "min", builtin_min);
	benv_add_builtin(e, ">", builtin_max);
	benv_add_builtin(e, "max", builtin_max);

}

/**
 * First it iterates over the passed in arguments attempting
 * to place each one in the environment.
 * Then it checks if the environment is full,
 * and if so evaluates, otherwise returns a copy
 * of itself with some arguments filled
 * */
bval* bval_call(benv* e, bval* f, bval* a) {
	// If builtin then simply apply that
	if(f->builtin)
		return f->builtin(e, a);


	// Record argument counts
	int given  = a->count;
	int total = f->formals->count;

	// While arguments still remain to be processed
	while(a->count) {
		// If ran out of formal arguments to build
		if(f->formals->count == 0) {
			bval_del(a);
			return bval_err(
				"Function passed too many arguments. "
				"Got %i, Expected %i.", given, total);
		}

		// Pop the first symbol from the formals
		bval* sym = bval_pop(f->formals, 0);

		// Special case to deal with '&'
		if(strcmp(sym->sym, "&")==0
			&& strcmp(f->formals->cell[0]->sym, "&")==0) {

			// Check to ensure that & is not passed invadily
			if(f->formals->count != 2)
				return bval_err("Function format invalid. "
					"Symbol '&' not followed by single symbol");

			// Pop and delete '&' symbol
			bval_del(bval_pop(f->formals, 0));

			// Pop next symbol and create empty list
			bval* sym = bval_pop(f->formals, 0);
			bval* val = bval_qexpr();

			// Bind to environment and delete
			benv_put(f->env, sym, val);
			bval_del(sym); bval_del(val);
		}

		// Pop the next argument from the list
		bval* val = bval_pop(a, 0);

		// Bind a copy into the function's environment
		benv_put(f->env, sym, val);

		// Delete symbol and value
		bval_del(sym); bval_del(val);
	}

	// Argument list is now bound so can be cleaned up
	bval_del(a);

	// If all formals have been bound evaluate
	if(f->formals->count == 0) {
		// Set environment parent to evaluation environment
		f->env->par = e;

		// Evaluate and return
		return builtin_eval(f->env,
			bval_add(bval_sexpr(), bval_copy(f->body)));
	} else {
		// Otherwise return partially evaluated function
		return bval_copy(f);
	}
}


// Define struct of methods
struct BuiltinMethods {
	char* name;
	bval* (*func)(benv*, bval*);
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

bval* builtin(benv* e, bval* a, char* func) {
	int num_methods = sizeof(bultin_map) / sizeof(bultin_map[0]); // Find array lenght

	// Get operator index
	// bval* builtin;
	for(int i=0; i<num_methods; i++) {
		if(strcmp(func, bultin_map[i].name)==0)
			return bultin_map[i].func(e, a);
		else if(strstr("+-*/%^ \
						add sub mul div res pow min max", func))
			return builtin_op(e, a, func);
	}

	bval_del(a);
	return bval_err("Unknown Function!");
}


bval* bval_eval_sexpr(benv* e, bval* v) {
	// Evaluate Children
	for(int i=0; i < v->count; i++) {
		v->cell[i] = bval_eval(e, v->cell[i]);
	}

	// Error Checking
	for(int i=0; i < v->count; i++) {
		if(v->cell[i]->type == BVAL_ERR) return bval_take(v, i); 
	}

	// Empty Expression
	if(v->count == 0) return v;

	// Single Expression
	if(v->count == 1) return bval_take(v, 0);

	// Ensure First Element is a function after evaluation
	bval* f = bval_pop(v, 0);
	if(f->type != BVAL_FUN) {
		bval* err = bval_err(
			"S-Expression starts with incorrect type"
			"Got %s, Expected %s",
			btype_name(f->type), btype_name(BVAL_FUN));

		bval_del(v); bval_del(f);
		return err;
	}

	// If so call function to get result
	// bval* result = f->builtin(e, v);
	// bval_del(f);
	// return result;
	bval* result = bval_call(e, f, v);
	return result;
}

bval* bval_eval(benv* e, bval* v) {
	// Evaluate Sexpressions
	if(v->type == BVAL_SYM) {
		bval* x = benv_get(e, v);
		bval_del(v);
		return x;
	}
	if(v->type == BVAL_SEXPR) return bval_eval_sexpr(e, v);
	return v;

	// All other bval types ramin the same

}




/*************************/
/* READ                  */
/*************************/

bval* bval_read_num(mpc_ast_t* t) {
	errno = 0;
	double x = atof(t->contents);
	return (errno != ERANGE) ? bval_num(x)
		: bval_err("Error: Invalid Number!");
}

bval* bval_read(mpc_ast_t* t) {
	// If Symbol or Number return conversion to that type
	if(strstr(t->tag, "number")) return bval_read_num(t);
	if(strstr(t->tag, "symbol")) return bval_sym(t->contents);

	// If root (>) or sexpr then create empty list
	bval* x = NULL;
	if(strcmp(t->tag, ">")==0) x = bval_sexpr();
	if(strstr(t->tag, "sexpr")) x = bval_sexpr();
	if(strstr(t->tag, "qexpr")) x = bval_qexpr();

	// Fill this list with any valid expression contained within
	for(int i=0; i < t->children_num; i++) {
		if(strcmp(t->children[i]->contents, "(")==0
			|| strcmp(t->children[i]->contents, ")")==0
			|| strcmp(t->children[i]->contents, "{")==0
			|| strcmp(t->children[i]->contents, "}")==0
			|| strcmp(t->children[i]->tag, "regex")==0) continue;

		x = bval_add(x, bval_read(t->children[i]));
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
	mpc_parser_t* Altbat  = mpc_new("altbat");

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
			altbat     : /^/ <expr>+ /$/ ; ",
		Number, Symbol, Sexpr, Qexpr, Expr, Altbat);

	// Vakon
	puts("Altbat Version 0.1.3");
	puts("Press Ctrl+c to Exit");
	puts("Type \"author\" for more information.\n");




	benv* e = benv_new();
	benv_add_builtins(e);

	while(1) {
		// Read user's input
		char *input = readline("Altbat> ");

		// Add input to history
		add_history(input);

		if(strcmp(input, "author")==0) {
			puts("Author: Vako");
			free(input);
			continue;
		}

		// Attempt to parse the user input
		mpc_result_t r;
		if(mpc_parse("<stdin>", input, Altbat, &r)) {
			// Sucess

			bval* x = bval_eval(e, bval_read(r.output));
			bval_println(x);
			bval_del(x);

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

	benv_del(e);

	// Undefine and Delete Parsers
	mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Altbat);
}






/*

			number    : /[-+]?(([0-9]+(\\.[0-9]*)?)|(\\.[0-9]+))/;       \
			operator  : '+' | '-' | '*' | '/' | '%' | '^'                \
						| \"add\" | \"sub\" | \"mul\" | \"div\"            \
						| \"res\" | \"pow\" | \"min\" | \"max\";           \
			term      : <number> | '(' <expr> ')' ;                      \
			expr      : <term> ((<operator>) <term>)*;                   \
			altbat     : /^/ <expr>+ /$/;                                 \

*/