#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "chunk.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct
{
	Token current;
	Token previous;
	bool hadError;
	bool panicMode;
} Parser;

typedef enum
{
	PREC_NONE,
	PREC_ASSIGNMENT,  // =
	PREC_OR,          // or
	PREC_AND,         // and
	PREC_EQUALITY,    // == !=
	PREC_COMPARISON,  // < > <= >=
	PREC_TERM,        // + -
	PREC_FACTOR,      // * /
	PREC_UNARY,       // ! -
	PREC_CALL,        // . ()
	PREC_PRIMARY
} Precedence;

typedef	void (*ParseFn)(bool canAssign);

typedef struct
{
	ParseFn prefix;
	ParseFn infix;
	Precedence precedence;
} ParseRule;

typedef struct
{
	Token name;
	int depth;
} Local;

typedef struct
{
	Local locals[UINT8_COUNT];
	int	localCount;
	int	scopeDepth; // Number of blocks that surround the code currently being compiled
} Compiler;

Parser parser;
Compiler* current = NULL;
Chunk* compilingChunk;

static Chunk* currentChunk()
{
	return compilingChunk;
}

static void errorAt(Token* token, const char* message)
{
	// Suppress errors beyond the first while in panic mode
	if (parser.panicMode)
		return;
	parser.panicMode = true;

	fprintf(stderr, "[line %d] Error", token->line);

	if (token->type == TOKEN_EOF)
	{
		fprintf(stderr, " at end");
	}
	else if (token->type == TOKEN_ERROR)
	{
		// Nothing
	}
	else
	{
		fprintf(stderr, " at '%.*s'", token->length, token->start);
	}

	fprintf(stderr, ": %s\n", message);
	parser.hadError = true;
}

static void error(const char* message)
{
	errorAt(&parser.previous, message);
}

static void	errorAtCurrent(const char* message)
{
	errorAt(&parser.current, message);
}

static void	advance()
{
	parser.previous = parser.current;

	for (;;)
	{
		parser.current = scanToken();
		if (parser.current.type != TOKEN_ERROR)
			break;

		errorAtCurrent(parser.current.start);
	}
}

static void	consume(TokenType type, const char* message)
{
	if (parser.current.type == type)
	{
		advance();
		return;
	}

	errorAtCurrent(message);
}

static bool	check(TokenType type)
{
	return parser.current.type == type;
}

static bool match(TokenType type)
{
	if (!check(type))
		return false;

	advance();
	return true;
}

static void	emitByte(uint8_t byte)
{
	writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2)
{
	emitByte(byte1);
	emitByte(byte2);
}

static void emitLoop(int loopStart)
{
	emitByte(OP_LOOP);

	int offset = currentChunk()->count - loopStart + 2; // Make sure to jump over this instruction's operands
	if (offset > UINT16_MAX)
		error("Loop body too large");

	emitByte((offset >> 8) & 0xff);
	emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction)
{
	emitByte(instruction);
	emitByte(0xff);
	emitByte(0xff);
	return currentChunk()->count - 2;
}

static void	emitReturn()
{
	emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value)
{
	int constant = addConstant(currentChunk(), value);
	if (constant > UINT8_MAX)
	{
		error("Too many constants in one chunk.");
		return 0;
	}

	return (uint8_t)constant;
}

static void emitConstant(Value value)
{
	emitBytes(OP_CONSTANT, makeConstant(value));
}

static void	patchJump(int offset)
{
	// -2 to account for the bytecode of the jump
	int jump = currentChunk()->count - offset - 2;

	if (jump > UINT16_MAX)
	{
		error("Too much code to jump over.");
	}

	// Split into 2 1-byte pieces
	currentChunk()->code[offset] = (jump >> 8) & 0xff; // Take second to last 8 bits of value
	currentChunk()->code[offset + 1] = jump & 0xff; // Take last 8 bits of the value
}

static void initCompiler(Compiler* compiler)
{
	compiler->localCount = 0;
	compiler->scopeDepth = 0;
	current = compiler;
}

static void	endCompiler()
{
	emitReturn();

#ifdef DEBUG_PRINT_CODE
	if (!parser.hadError)
	{
		disassembleChunk(currentChunk(), "code");
	}
#endif
}

static void	beginScope()
{
	current->scopeDepth++;
}

static void endScope()
{
	current->scopeDepth--;

	while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth)
	{
		emitByte(OP_POP);
		current->localCount--;
	}
}

static void expression();
static void	statement();
static void	declaration();
static uint8_t identifierConstant(Token* name);
static ParseRule* getRule(TokenType type);
static void	parsePrecedence(Precedence precedence);

static void binary(bool canAssign)
{
	TokenType operatorType = parser.previous.type;
	ParseRule* rule = getRule(operatorType);
	parsePrecedence((Precedence)rule->precedence + 1);

	switch (operatorType)
	{
	case TOKEN_BANG_EQUAL: emitBytes(OP_EQUAL, OP_NOT); break;
	case TOKEN_EQUAL_EQUAL: emitByte(OP_EQUAL); break;
	case TOKEN_GREATER: emitByte(OP_GREATER); break;
	case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
	case TOKEN_LESS: emitByte(OP_LESS); break;
	case TOKEN_LESS_EQUAL: emitBytes(OP_GREATER, OP_NOT); break;
	case TOKEN_PLUS:  emitByte(OP_ADD); break;
	case TOKEN_MINUS: emitByte(OP_SUBTRACT); break;
	case TOKEN_STAR: emitByte(OP_MULTIPLY); break;
	case TOKEN_SLASH: emitByte(OP_DIVIDE); break;
	default: return; // Unreachable
	}
}

static void literal(bool canAssign)
{
	switch (parser.previous.type)
	{
	case TOKEN_FALSE: emitByte(OP_FALSE); break;
	case TOKEN_TRUE: emitByte(OP_TRUE); break;
	case TOKEN_NIL: emitByte(OP_NIL); break;
	default: return; // Unreachable
	}
}

static void grouping(bool canAssign)
{
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign)
{
	double value = strtod(parser.previous.start, NULL);
	emitConstant(NUMBER_VAL(value));
}

static void or_(bool canAssign)
{
	int elseJump = emitJump(OP_JUMP_IF_FALSE); // Skip the OP_JUMP if false
	int endJump = emitJump(OP_JUMP);

	patchJump(elseJump);
	emitByte(OP_POP);

	parsePrecedence(PREC_OR);
	patchJump(endJump);
}

static void and_(bool canAssign)
{
	// Left operand is already on the stack
	int endJump = emitJump(OP_JUMP_IF_FALSE);

	emitByte(OP_POP); // Discard left operand
	parsePrecedence(PREC_AND); // Evaluate just the right-hand operand

	patchJump(endJump);
}

static void string(bool canAssign)
{
	// +1 and -2 trim the quotation marks
	emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void	namedVariable(Token name, bool canAssign)
{
	uint8_t	getOp, setOp;
	int	arg = resolveLocal(current, &name);

	if (arg != -1)
	{
		getOp = OP_GET_LOCAL;
		setOp = OP_SET_LOCAL;
	}
	else
	{
		arg = identifierConstant(&name);
		getOp = OP_GET_GLOBAL;
		setOp = OP_SET_GLOBAL;
	}

	if (canAssign && match(TOKEN_EQUAL))
	{
		expression();
		emitBytes(setOp, (uint8_t)arg);
	}
	else
	{
		emitBytes(getOp, (uint8_t)arg);
	}
}

static void variable(bool canAssign)
{
	namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign)
{
	TokenType operatorType = parser.previous.type;

	// Compile the operand
	parsePrecedence(PREC_UNARY); // Use PREC_UNARY to allow nesting unary operators (ex: !!a)

	// Emit the operator instruction
	switch (operatorType)
	{
	case TOKEN_BANG: emitByte(OP_NOT); break;
	case TOKEN_MINUS: emitByte(OP_NEGATE); break;
	default: return; // Unreachable
	}
}

ParseRule rules[] = {
	[TOKEN_LEFT_PAREN] = { grouping, NULL, PREC_NONE },
	[TOKEN_RIGHT_PAREN] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_LEFT_BRACE] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_RIGHT_BRACE] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_COMMA] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_DOT] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_MINUS] = {unary,    binary, PREC_TERM},
	[TOKEN_PLUS] = {NULL,     binary, PREC_TERM},
	[TOKEN_SEMICOLON] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_SLASH] = {NULL,     binary, PREC_FACTOR},
	[TOKEN_STAR] = {NULL,     binary, PREC_FACTOR},
	[TOKEN_BANG] = {unary,     NULL,   PREC_NONE},
	[TOKEN_BANG_EQUAL] = {NULL,     binary,   PREC_EQUALITY},
	[TOKEN_EQUAL] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_EQUAL_EQUAL] = {NULL,     binary,   PREC_EQUALITY},
	[TOKEN_GREATER] = {NULL,     binary,   PREC_COMPARISON},
	[TOKEN_GREATER_EQUAL] = {NULL,     binary,   PREC_COMPARISON},
	[TOKEN_LESS] = {NULL,     binary,   PREC_COMPARISON},
	[TOKEN_LESS_EQUAL] = {NULL,     binary,   PREC_COMPARISON},
	[TOKEN_IDENTIFIER] = {variable,     NULL,   PREC_NONE},
	[TOKEN_STRING] = {string,     NULL,   PREC_NONE},
	[TOKEN_NUMBER] = {number,   NULL,   PREC_NONE},
	[TOKEN_AND] = {NULL,     and_,   PREC_AND},
	[TOKEN_CLASS] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_ELSE] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_FALSE] = {literal,     NULL,   PREC_NONE},
	[TOKEN_FOR] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_FUN] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_IF] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_NIL] = {literal,     NULL,   PREC_NONE},
	[TOKEN_OR] = {NULL,     or_,   PREC_OR},
	[TOKEN_PRINT] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_RETURN] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_SUPER] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_THIS] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_TRUE] = {literal,     NULL,   PREC_NONE},
	[TOKEN_VAR] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_WHILE] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_ERROR] = {NULL,     NULL,   PREC_NONE},
	[TOKEN_EOF] = {NULL,     NULL,   PREC_NONE},
};

static void	parsePrecedence(Precedence precedence)
{
	advance();
	ParseFn	prefixRule = getRule(parser.previous.type)->prefix;
	if (prefixRule == NULL)
	{
		error("Expect expression.");
		return;
	}

	bool canAssign = precedence <= PREC_ASSIGNMENT;
	prefixRule(canAssign);

	while (precedence <= getRule(parser.current.type)->precedence)
	{
		advance();
		ParseFn	infixRule = getRule(parser.previous.type)->infix;
		infixRule(canAssign);
	}

	if (canAssign && match(TOKEN_EQUAL))
		error("Invalid assignment target.");
}

static uint8_t identifierConstant(Token* name)
{
	return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b)
{
	if (a->length != b->length)
		return false;
	return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name)
{
	for (int i = compiler->localCount - 1; i >= 0; i--)
	{
		Local* local = &compiler->locals[i];
		if (identifiersEqual(name, &local->name))
		{
			if (local->depth == -1)
			{
				error("Can't read local variable in its own initializer.");
			}

			return i;
		}
	}

	return -1; // Global
}

static void addLocal(Token name)
{
	if (current->localCount == UINT8_COUNT)
	{
		error("Too many local variables in function.");
		return;
	}

	Local* local = &current->locals[current->localCount++];
	local->name = name;
	local->depth = -1;
}

static void declareVariable()
{
	if (current->scopeDepth == 0)
		return;

	Token* name = &parser.previous;

	// Detect duplicate variable names in the same scope
	for (int i = current->localCount - 1; i >= 0; i--)
	{
		Local* local = &current->locals[i];
		if (local->depth != -1 && local->depth < current->scopeDepth)
		{
			break;
		}

		if (identifiersEqual(name, &local->name))
		{
			error("Already a variable with this name in this scope.");
		}
	}

	addLocal(*name);
}

static uint8_t parseVariable(const char* errorMessage)
{
	consume(TOKEN_IDENTIFIER, errorMessage);

	declareVariable();
	if (current->scopeDepth > 0)
		return 0; // Return dummy index, local variables aren't saved in the constant table

	return identifierConstant(&parser.previous);
}

static void markInitialized()
{
	current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global)
{
	if (current->scopeDepth > 0)
	{
		// The temporary value on the stack is the local variable.
		markInitialized();
		return;
	}

	emitBytes(OP_DEFINE_GLOBAL, global);
}

// Binary is before the rules and rules has a ref to binary, so binary can't directly have a ref to rules. Hence, this function
static ParseRule* getRule(TokenType type)
{
	return &rules[type];
}

static void expression()
{
	parsePrecedence(PREC_ASSIGNMENT);
}

static void block()
{
	while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF))
	{
		declaration();
	}

	consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void varDeclaration()
{
	uint8_t global = parseVariable("Expect variable name.");

	if (match(TOKEN_EQUAL))
	{
		expression();
	}
	else
	{
		emitByte(OP_NIL);
	}

	consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

	defineVariable(global);
}

// Expression statements are expressions followed by semicolons. Ex: execute a function, but discard its return.
static void expressionStatement()
{
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
	emitByte(OP_POP); // Discard the result
}

static void forStatement()
{
	beginScope();
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");

	if (match(TOKEN_SEMICOLON))
	{
		// No initializer
	}
	else if (match(TOKEN_VAR))
	{
		varDeclaration();
	}
	else
	{
		expressionStatement();
	}

	int loopStart = currentChunk()->count;
	int exitJump = -1;

	if (!match(TOKEN_SEMICOLON))
	{
		expression();
		consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

		// Jump when condition is false
		exitJump = emitJump(OP_JUMP_IF_FALSE);
		emitByte(OP_POP); // Always pop the condition
	}

	if (!match(TOKEN_RIGHT_PAREN))
	{
		int bodyJump = emitJump(OP_JUMP); // Jump over the incrementor
		int incrementStart = currentChunk()->count; // Jump back to the incrementor

		expression();
		emitByte(OP_POP); // Pop the expression
		consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");

		emitLoop(loopStart); // Loop after the incrementor
		loopStart = incrementStart;
		patchJump(bodyJump);
	}

	statement();
	emitLoop(loopStart);

	if (exitJump != -1)
	{
		patchJump(exitJump);
		emitByte(OP_POP); // Pop condition
	}

	endScope();
}

static void ifStatement()
{
	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	int thenJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();

	int	elseJump = emitJump(OP_JUMP);

	patchJump(thenJump); // Jump here is then is false
	emitByte(OP_POP);

	if (match(TOKEN_ELSE))
		statement();
	patchJump(elseJump);
}

static void	printStatement()
{
	expression();
	consume(TOKEN_SEMICOLON, "Expect ';' after value.");
	emitByte(OP_PRINT);
}

static void whileStatement()
{
	int loopStart = currentChunk()->count;

	consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
	expression();
	consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

	int exitJump = emitJump(OP_JUMP_IF_FALSE);
	emitByte(OP_POP);
	statement();
	emitLoop(loopStart);

	patchJump(exitJump);
	emitByte(OP_POP); // Always pop the condition value
}

static void	synchronize()
{
	parser.panicMode = false;

	// Discard tokens until we hit a statement boundary
	while (parser.current.type != TOKEN_EOF)
	{
		if (parser.previous.type == TOKEN_SEMICOLON)
			return;

		switch (parser.current.type)
		{
		case TOKEN_CLASS:
		case TOKEN_FUN:
		case TOKEN_VAR:
		case TOKEN_FOR:
		case TOKEN_IF:
		case TOKEN_WHILE:
		case TOKEN_PRINT:
		case TOKEN_RETURN:
			return;
		default:
			; // Do nothing
		}
	}
}

static void declaration()
{
	if (match(TOKEN_VAR))
	{
		varDeclaration();
	}
	else
	{
		statement();
	}

	if (parser.panicMode) synchronize();
}

static void	statement()
{
	if (match(TOKEN_PRINT))
	{
		printStatement();
	}
	else if (match(TOKEN_IF))
	{
		ifStatement();
	}
	else if (match(TOKEN_WHILE))
	{
		whileStatement();
	}
	else if (match(TOKEN_FOR))
	{
		forStatement();
	}
	else if (match(TOKEN_LEFT_BRACE))
	{
		beginScope();
		block();
		endScope();
	}
	else
	{
		expressionStatement();
	}
}

bool compile(const char* source, Chunk* chunk)
{
	initScanner(source);

	Compiler compiler;
	initCompiler(&compiler);

	compilingChunk = chunk;

	parser.hadError = false;
	parser.panicMode = false;

	advance();

	while (!match(TOKEN_EOF))
	{
		declaration();
	}

	endCompiler();
	return !parser.hadError;
}