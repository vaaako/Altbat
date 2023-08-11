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



// enum of possible vval (vispy value) types
enum {
	VVAL_NUM,
	VVAL_ERR
};

// num types
enum VnumTypes {
	INT,
	FLOAT
};

// New vval struct
typedef struct {
	int type;
	// long num;
	double num;
	int err;
} vval;

// enum of possible error
enum ErrorsTypes {
	VERR_DIV_ZERO,
	VERR_BAD_OP,
	VERR_BAD_NUM
};

// Struct to errors responses
struct ErrorEntry {
	enum ErrorsTypes type;
	char* response;
} errors_map[] = {
	{ VERR_DIV_ZERO, "Error: Division By Zero!" },
	{ VERR_BAD_OP,   "Error: Bad Operator!" },
	{ VERR_BAD_NUM,  "Error: Bad number!" }
};


// Create new number type vval
vval vval_num(double x) {
	vval v;
	v.type = VVAL_NUM;
	v.num  = x;
	return v;
}

// Create new error type vval
vval vval_err(int x) {
	vval v;
	v.type = VVAL_ERR;
	v.err  = x;
	return v;
}

// Print a vval
void vval_print(vval v) {


	switch (v.type) {
		// If is a number, print
		case VVAL_NUM:
			// printf("%li", v.num);
			if(v.num == (long)v.num) // Check if v.num is integer number	
				printf("%li", (long)v.num);
			else
				printf("%.1lf", v.num);
			break;
		case VVAL_ERR:
			// const int erros_size = sizeof(errors_map) / sizeof(errors_map[0]); // Find array lenght
			for(int i=0; i<sizeof(errors_map) / sizeof(errors_map[0]); i++) {
				if(v.err == errors_map[i].type) {
					puts(errors_map[i].response);
					break;
				}
			}
			break;
	}
}

// Print vval followed by a newline
void vval_println(vval v) {
	vval_print(v);
	putchar('\n');
}





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

vval eval_op(vval x, char* op, vval y) {
	// Map operators
	const int num_operators = sizeof(operators) / sizeof(operators[0]); // Find array lenght

	// Get operator index
	enum OperatorCode operator_code = OP_UNKNOWN;
	for(int i=0; i<num_operators; i++) {
		if(strcmp(op, operators[i].name)==0) {
			operator_code = operators[i].code;
			break;
		}
	}


	switch (operator_code) {
		case OP_ADD: // "+", "add"
			return vval_num(x.num + y.num);
		case OP_SUB: // "-", "sub"
			return vval_num(x.num - y.num);
		case OP_MUL: // "*", "mul"
			return vval_num(x.num * y.num);
		case OP_DIV: // "/", "div"
			return (y.num==0) ? vval_err(VERR_DIV_ZERO)
				: vval_num(x.num / y.num);
			// return (y != 0) ? x / y : 0;
		case OP_RES: // "%", "rem"
			/* Catch error later */
			return (y.num==0) ? vval_err(VERR_DIV_ZERO)
				: vval_num((long)x.num % (long)y.num);
			// return (y != 0) ? x % y : 0;
		case OP_POW: // "^", "pow"
			return vval_num((long)pow(x.num, y.num));
		case OP_MIN: // "min"
			return vval_num((x.num < y.num) ? x.num : y.num);
		case OP_MAX: // "max"
			return vval_num((x.num > y.num) ? x.num : y.num);
		default:
			return vval_err(VERR_BAD_OP);
	}
}


vval eval(mpc_ast_t* t) {
	// If tagged as number return it directly
	if(strstr(t->tag, "number")) { // t->tag has "number"
		// Check if there is some error in conversion
		errno = 0;
		// long x = strtol(t->contents, NULL, 10); // String -> Long
		double x = atof(t->contents); // String -> Long

		return (errno != ERANGE) ? vval_num(x) : vval_err(VERR_BAD_NUM);

		// return atoi(t->contents); // parse to int (if is only numbers)
	}

	// The operator is always second child
	char* op = t->children[1]->contents;

	// Store the third child in 'x'
	vval x = eval(t->children[2]);

	// Iterate the remaining children and combining
	int i = 3;
	while(strstr(t->children[i]->tag, "expr")) {
		x = eval_op(x, op, eval(t->children[i]));
		i++;
	}

	return x;
}

int main(int argc, char const *argv[]) {
	// Create some parses
	mpc_parser_t* Number    = mpc_new("number");
	mpc_parser_t* Operator  = mpc_new("operator");
	mpc_parser_t* Expr      = mpc_new("expr");
	mpc_parser_t* Vispy     = mpc_new("vispy");

	// Define them with the following Language
	mpca_lang(MPCA_LANG_DEFAULT,
		// Pass regex of what consists each element of a "Expression"
		// INT and FLOAT = [-+]?(([0-9]+(\\.[0-9]*)?)|(\\.[0-9]+))
		"                                                                \
			number    : /[-+]?(([0-9]+(\\.[0-9]*)?)|(\\.[0-9]+))/;       \
			operator  : '+' | '-' | '*' | '/' | '%' | '^'                \
					  | \"add\" | \"sub\" | \"mul\" | \"div\"            \
					  | \"res\" | \"pow\" | \"min\" | \"max\";           \
			expr      : <number> | '(' <operator> <expr>+ ')';           \
			vispy     : /^/ <operator> <expr>+ /$/;   		",
		Number, Operator, Expr, Vispy);

	puts("Vispy Version 0.0.4");
	puts("Press Ctrl+c to Exit");
	puts("Type \"author\" for more information.\n");


	while(1) {
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

			vval result = eval(r.output);
			vval_println(result);

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
	mpc_cleanup(4, Number, Operator, Expr, Vispy);
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