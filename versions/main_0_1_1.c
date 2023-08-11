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
 * VVAL stands for Vispy Value
 * 
 * */

/*******************/
/* MAKE STRUCT     */
/*******************/


// enum of possible vval (vispy value) types
enum {
	VVAL_ERR,
	VVAL_NUM,
	VVAL_SYM,
	VVAL_SEXPR,
	VVAL_QEXPR
};


// New vval struct
typedef struct vval {
	int type;
	// long num;
	double num;

	// Error and symbol types have some string data
	char* err;
	char* sym;

	// Count and Pointer to a list of "vval*"

	// We will also need to keep track of how many lval* are in this list
	int count; // e.g. list {1 2} -> count = 1, because "list" is the operator, so it doens't counts and {1 2} is a QExpr, so counts as one cell (with two cell of type VVAL_NUM inside)
	struct vval** cell; /* which points to a location where we store a list of lval*. More specifically pointers to the other individual vval \
	└ e.g. (list {1 2}) -> "{ 1 2 }" is the first cell, it's count is 2 (because have two cells of type VVAL_NUM inside) */
} vval;


// We can change our lval construction functions to return pointers to an lval, rather than one directly
// Construct a pointer to a new number vval
vval* vval_num(double x) {
	vval* v = malloc(sizeof(vval));
	v->type = VVAL_NUM;
	v->num  = x;
	return v;
}

// Construct a pointer to a new Error vval
vval* vval_err(char* m) {
	vval* v = malloc(sizeof(vval));
	v->type = VVAL_ERR;
	v->err  = malloc(strlen(m) + 1); // +1 to \0
	strcpy(v->err, m);
	return v;
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

void vval_del(vval* v) {
	switch (v->type) {
		// Do nothing special for number type
		case VVAL_NUM: break;

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






/*******************/
/* PRINT           */
/*******************/



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

	}
}

void vval_println(vval* v) {
	vval_print(v);
	putchar('\n');
}







/*******************/
/* EVALUATION      */
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








/*************************/
/* BUILT IN METHODS      */
/*************************/

// Macro to help with error conditions
#define VASSERT(args, cond, err) \
	if(!(cond)) { vval_del(args); return vval_err(err); }


vval* vval_eval(vval* v);

// Takes a Q-Expression and returns a Q-Expression with only the first element
vval* builtin_head(vval* a) {
	// Check error conditions
	VASSERT(a, a->count == 1,
		"Function 'head' passed too many arguments!");

	VASSERT(a, a->cell[0]->type == VVAL_QEXPR,
		"Function 'head' passed incorrect types!");
	
	VASSERT(a, a->cell[0]->count != 0,
		"Function 'head' passed {}!");


	// Otherwise take first argument
	vval* v = vval_take(a, 0);

	// Delete all elements that are not head and return
	while(v->count > 1) vval_del(vval_pop(v, 1));
	return v;
}

// Takes a Q-Expression and returns a Q-Expression with the first element removed
vval* builtin_tail(vval* a) {
	// Check error conditions
	VASSERT(a, a->count == 1,
		"Function 'tail' passed too many arguments!");

	VASSERT(a, a->cell[0]->type == VVAL_QEXPR,
		"Function 'tail' passed incorrect types!");

	VASSERT(a, a->cell[0]->count != 0,
		"Function 'tail' passed {}!");

	// Take first argument
	vval* v = vval_take(a, 0);

	// Delete first element and return
	vval_del(vval_pop(v, 0));
	return v;
}

// Takes one or more arguments and returns a new Q-Expression containing the arguments
vval* builtin_list(vval* a) {
	a->type = VVAL_QEXPR;
	return a;
}

// Takes a Q-Expression and evaluates it as if it were a S-Expression
vval* builtin_eval(vval* a) {
	VASSERT(a, a->count == 1, 
		"Function 'eval' passed too many arguments!");
	VASSERT(a, a->cell[0]->type == VVAL_QEXPR, 
		"Function 'eval' passed incorrect type!");

	vval* x = vval_take(a, 0);
	x->type = VVAL_SEXPR;
	return vval_eval(x);
}

// Returns the number of elements in a Q-Expression
vval* builtin_len(vval* a) {
	VASSERT(a, a->count == 1,
		"Function 'len' passed too many arguments!");
	VASSERT(a, a->cell[0]->type == VVAL_QEXPR, 
		"Function 'len' passed incorrect type!");

	a->type = VVAL_NUM;
	a->num = a->cell[0]->count;
	return a;
}


vval* vval_join(vval* x, vval* y) {
	// For each cell in 'y' add it to 'x'
	while(y->count)
		x = vval_add(x, vval_pop(y, 0));

	// Delete the empty 'y' and return 'x'
	vval_del(y);
	return x;
}

// Takes one or more Q-Expressions and returns a Q-Expression of them conjoined together
vval* builtin_join(vval* a) {
	for(int i=0; i<a->count; i++) {
		VASSERT(a, a->cell[i]->type == VVAL_QEXPR,
			"Function 'join' passed incorrect type");
	}

	vval* x = vval_pop(a, 0);

	while(a->count)
		x = vval_join(x, vval_pop(a, 0));

	vval_del(a);
	return x;
}


// Takes a value and a Q-Expression and appends it to the front
vval* builtin_cons(vval* a) {
	VASSERT(a, a->count == 2,
		"Function 'cons' passed too many arguments!");
	VASSERT(a, a->cell[0]->type == VVAL_NUM, 
		"Function 'cons' first cell passed incorrect type!");
	VASSERT(a, a->cell[1]->type == VVAL_QEXPR, 
		"Function 'cons' second cell passed incorrect type!");
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
	{ "min", OP_MIN },
	{ "max", OP_MAX }	
};

vval* builtin_op(vval* a, char* op) {
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
	if((strcmp(op, "-")==0) && a->count == 0) {
		x->num = -x->num;
	}

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


// Define struct of methods
struct BuiltinMethods {
	char* name;
	vval* (*func)(vval*);
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

vval* builtin(vval* a, char* func) {
	int num_methods = sizeof(bultin_map) / sizeof(bultin_map[0]); // Find array lenght

	// Get operator index
	// vval* fun;
	for(int i=0; i<num_methods; i++) {
		if(strcmp(func, bultin_map[i].name)==0)
			return bultin_map[i].func(a);
		else if(strstr("+-*/%^ \
						add sub mul div res pow min max", func))
			return builtin_op(a, func);
	}

	vval_del(a);
	return vval_err("Unknown Function!");
}

/*

 '+' | '-' | '*' | '/' | '%' | '^'                  \
					  | \"add\" | \"sub\" | \"mul\" | \"div\"            \
					  | \"res\" | \"pow\" | \"min\" | \"max\"   
					  */

vval* vval_eval_sexpr(vval* v) {
	// Evaluate Children
	for(int i=0; i < v->count; i++) {
		v->cell[i] = vval_eval(v->cell[i]);
	}

	// Error Checking
	for(int i=0; i < v->count; i++) {
		if(v->cell[i]->type == VVAL_ERR) return vval_take(v, i); 
	}

	// Empty Expression
	if(v->count == 0) return v;

	// Single Expression
	if(v->count == 1) return vval_take(v, 0);

	// Ensure First Element is Symbol
	vval* f = vval_pop(v, 0);
	if(f->type != VVAL_SYM) {
		vval_del(f); vval_del(v);
		return vval_err("S-Expression Does not start with symbol!");
	}

	// Call builtin with operator
	// vval* result = builtin_op(v, f->sym);
	vval* result = builtin(v, f->sym);
	vval_del(f);
	return result;
}

vval* vval_eval(vval* v) {
	// Evaluate Sexpressions
	if(v->type == VVAL_SEXPR) return vval_eval_sexpr(v);

	// All other vval tyeps remain the same
	return v;
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
	mpc_parser_t* Vispy  = mpc_new("vispy");

	// Define them with the following Language
	mpca_lang(MPCA_LANG_DEFAULT,
		// Pass regex of what consists each element of a "Expression"
		// INT and FLOAT = [-+]?(([0-9]+(\\.[0-9]*)?)|(\\.[0-9]+))
		"                                                                \
			number    : /[-+]?(([0-9]+(\\.[0-9]*)?)|(\\.[0-9]+))/;       \
			symbol  : '+' | '-' | '*' | '/' | '%' | '^'                  \
					  | \"add\" | \"sub\" | \"mul\" | \"div\"            \
					  | \"res\" | \"pow\" | \"min\" | \"max\"            \
					  | \"list\" | \"head\" | \"tail\" | \"join\"        \
					  | \"eval\" | \"len\" | \"cons\" ; \
			sexpr     : '(' <expr>* ')' ;                                \
			qexpr     : '{' <expr>* '}' ;                                \
			expr      : <number> | <symbol> | <sexpr> | <qexpr> ;        \
			vispy     : /^/ <expr>+ /$/ ; ",
		Number, Symbol, Sexpr, Qexpr, Expr, Vispy);

	// Vakon
	puts("Vispy Version 0.1.1");
	puts("Press Ctrl+c to Exit");
	puts("Type \"author\" for more information.\n");


	while(1) {
		// Read user's input
		char *input = readline("Vispy> ");

		// Add input to history
		add_history(input);

		if(strcmp(input, "author")==0) {
			puts("Author: Vako");
			free(input);		
			continue;
		}

		// Attempt to parse the user input
		mpc_result_t r;
		if(mpc_parse("<stdin>", input, Vispy, &r)) {
			// Sucess

			vval* x = vval_eval(vval_read(r.output));
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

	// Undefine and Delete Parsers
	mpc_cleanup(6, Number, Symbol, Sexpr, Qexpr, Expr, Vispy);
}






/*

			number    : /[-+]?(([0-9]+(\\.[0-9]*)?)|(\\.[0-9]+))/;       \
			operator  : '+' | '-' | '*' | '/' | '%' | '^'                \
					  | \"add\" | \"sub\" | \"mul\" | \"div\"            \
					  | \"res\" | \"pow\" | \"min\" | \"max\";           \
			term      : <number> | '(' <expr> ')' ;                      \
			expr      : <term> ((<operator>) <term>)*;                   \
			vispy     : /^/ <expr>+ /$/;                                 \

*/