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

struct OperatorEntry { // Define struct of operators
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

long eval_op(long x, char* op, long y) {
	// Map operators
	// const char* operators[] = { "+", "add", "-", "sub", "*", "mul", "/", "div", "%", "rem", "^", "pow", "min", "max" };
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
			return x + y;
		case OP_SUB: // "-", "sub"
			return x - y;
		case OP_MUL: // "*", "mul"
			return x * y;
		case OP_DIV: // "/", "div"
			return (y != 0) ? x / y : 0;
		case OP_RES: // "%", "rem"
			return (y != 0) ? x % y : 0;
		case OP_POW: // "^", "pow"
			return (long)pow(x, y);
		case OP_MIN: // "min"
			return (x < y) ? x : y;
		case OP_MAX: // "max"
			return (x > y) ? x : y;
		default:
			return 0;
	}

	// if(strcmp(op, "+")==0 || strcmp(op, "add")==0) return x + y;
	// if(strcmp(op, "-")==0 || strcmp(op, "sub")==0) return x - y;
	// if(strcmp(op, "*")==0 || strcmp(op, "mul")==0) return x * y;
	// if(strcmp(op, "/")==0 || strcmp(op, "div")==0) return x / y;
	// if(strcmp(op, "%")==0 || strcmp(op, "rem")==0) return x % y;
	// if(strcmp(op, "^")==0 || strcmp(op, "pow")==0) return x^y;
	// if(strcmp(op, "min")==0) return (x<y) ? x : y;
	// if(strcmp(op, "max")==0) return (x>y) ? x : y;

	// return 0;
}

long eval(mpc_ast_t* t) {
	// If tagged as number return it directly
	if(strstr(t->tag, "number")) // t->tag has "number"
		return atoi(t->contents); // parse to int (if is only numbers)

	// The operator is always second child
	char* op = t->children[1]->contents;

	// Store the third child in 'x'
	long x = eval(t->children[2]);

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
			vispy     : /^/ <operator> <expr>+ /$/;                      \
		",
		Number, Operator, Expr, Vispy);

	puts("Vispy Version 0.0.3");
	puts("Press Ctrl+c to Exit");
	puts("Type \"author\" for more information.\n");


	while(1) {
		// fputs("Vispy> ", stdout);
		char *input = readline("Vispy> ");

		// Add input to history
		add_history(input);

		if(strcmp(input, "author")==0) {
			puts("Author: Vako");
			free(input);		
			continue;
		}


		// Read a line of user input of maximum size 2048
		// fgets(input, 2048, stdin);
		// printf("Command \"%s\" not found \n", input);

		// Attempt to parse the user input
		mpc_result_t r;
		if(mpc_parse("<stdin>", input, Vispy, &r)) {
			// Sucess

			const long result = eval(r.output);
			printf("%li\n", result);
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
