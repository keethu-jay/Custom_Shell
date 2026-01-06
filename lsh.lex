%{

#include <stdio.h>
#include "lsh_ast.h"
#include "lsh.yacc.generated_h"

%}

%option reentrant
%option bison-bridge
%option bison-locations
%option yylineno

%option header-file="lsh.lex.generated_h"

%%

[ \t]+		{ ; }
\|		{ return PIPE; }
\;		{ return SEMICOLON; }
\n		{ return NEW_LINE; }
\&		{ return AMPERSAND; }

for		{ return FOR; }
in		{ return IN; }
do		{ return DO; }
pdo		{ return PDO; }
done		{ return DONE; }
if		{ return IF; }
then		{ return THEN; }
elif		{ return ELIF; }
else		{ return ELSE; }
fi		{ return FI; }

[$][a-zA-Z_][a-zA-Z0-9_]*	{ yylval->strval = strdup(yytext+1); return VAR; }
[a-zA-Z0-9_\-\.^$/*]+		{ yylval->strval = strdup(yytext); return WORD; }
[a-zA-Z_][a-zA-Z0-9_]*=		{ yylval->strval = strdup(yytext); return VAR_ASSIGN; }
\'[^']*\'			{ yylval->strval = strdup(yytext+1); {int sl = strlen(yylval->strval); if (sl > 0) yylval->strval[sl - 1] = 0; } return WORD; }

.		{ fprintf(stderr, "bad input character '%s' at line %d\n", yytext, yylineno); return YYEOF; }


%%
