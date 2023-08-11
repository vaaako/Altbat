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


/*******************/
/* MAKE STRUCT     */
/*******************/


// enum of possible vval (vispy value) types
enum {
	VVAL_ERR,
	VVAL_NUM,
	VVAL_SYM,
	VVAL_SEXPR
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
	int count;
	struct vval** cell; // which points to a location where we store a list of lval*. More specifically pointers to the other individual lval
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

void vval_del(vval* v) {
	switch (v->type) {
		// Do nothing special for number type
		case VVAL_NUM: break;

		// For Err or Sym free the string data
		case VVAL_ERR: free(v->err); break;
		case VVAL_SYM: free(v->sym); break;

		// If Sexpr then delete all elements inside
		case VVAL_SEXPR:
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

	}
}

void vval_println(vval* v) {
	vval_print(v);
	putchar('\n');
}




/*******************/
/* EVALUATION      */
/*******************/



// enum of operators types of eval_op
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
} operators[] = { // Define operators
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

vval* vval_take(vval* v, int i) {
	vval* x = vval_pop(v, i);
	vval_del(v);
	return x;
}

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
	while(a-> count > 0) {
		int err = 0; // Check for error from inside switch

		// Pop the next element
		vval* y = vval_pop(a, 0);

		int num_operators = sizeof(operators) / sizeof(operators[0]); // Find array lenght

		// Get operator index
		enum OperatorCode operator_code = OP_UNKNOWN;
		for(int i=0; i<num_operators; i++) {
			if(strcmp(op, operators[i].name)==0) {
				operator_code = operators[i].code;
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
				x->num = (x->num < y->num) ? x->num : y->num;
				break;
			case OP_MAX:
				x->num = (x->num > y->num) ? x->num : y->num;
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


vval* vval_eval(vval* v);
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
	vval* result = builtin_op(v, f->sym);
	vval_del(f);
	return result;
}

vval* vval_eval(vval* v) {
	// Evaluate Sexpressions
	if(v->type == VVAL_SEXPR) return vval_eval_sexpr(v);

	// All other vval tyeps remain the same
	return v;
}







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

	// Fill this list with any valid expression contained within
	for(int i=0; i < t->children_num; i++) {
		if(strcmp(t->children[i]->contents, "(")==0) continue;
		if(strcmp(t->children[i]->contents, ")")==0) continue;
		if(strcmp(t->children[i]->tag, "regex")==0) continue;

		x = vval_add(x, vval_read(t->children[i]));
	}

	return x;
}

int main(int argc, char const *argv[]) {
	// Create some parses
	mpc_parser_t* Number = mpc_new("number");
	mpc_parser_t* Symbol = mpc_new("symbol");
	mpc_parser_t* Sexpr  = mpc_new("sexpr"); // S-Expression (hihi)
	mpc_parser_t* Expr   = mpc_new("expr");
	mpc_parser_t* Vispy  = mpc_new("vispy");

	// Define them with the following Language
	mpca_lang(MPCA_LANG_DEFAULT,
		// Pass regex of what consists each element of a "Expression"
		// INT and FLOAT = [-+]?(([0-9]+(\\.[0-9]*)?)|(\\.[0-9]+))
		"                                                                \
			number    : /[-+]?(([0-9]+(\\.[0-9]*)?)|(\\.[0-9]+))/;       \
			symbol  : '+' | '-' | '*' | '/' | '%' | '^'                \
					  | \"add\" | \"sub\" | \"mul\" | \"div\"            \
					  | \"res\" | \"pow\" | \"min\" | \"max\";           \
			sexpr     : '(' <expr>* ')' ;                                \
			expr      : <number> | <symbol> | <sexpr> ;                  \
			vispy     : /^/ <expr>+ /$/; ",
		Number, Symbol, Sexpr, Expr, Vispy);

	puts("Vispy Version 0.1.0");
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
	mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Vispy);
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