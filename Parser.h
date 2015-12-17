/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2013 OpenBOR Team
 */

#ifndef PARSER_H
#define PARSER_H

#include "depends.h"
#include "ParserSet.h"
#include "Stack.h"
#include "ssa.h"

class Parser
{
public:
    //Private data members
    Lexer theLexer;                    //this parser's lexer
    ParserSet theParserSet;            //this parser's parserSet
    Token theNextToken;                //The next token
    Token theNextNextToken;            //The token after the next one (used for overread)
    bool rewound;                      //If true, use theNextNextToken instead of lexing another
    List  *pIList;                      //A pointer to the instruction list
    LONG LabelCount;                   //A counter to track the number of labels
    Stack LabelStack;                  //A stack of labels for use in jumps
    CHAR theRetLabel[MAX_STR_LEN + 1];  //A label which holds the target of returns
    Token theFieldToken;               //A pointer to the field source token
    int paramCount;
    char currentPath[256];                 // current path info of the text
    BOOL errorFound;
    BOOL isImport;
    void *memCtx;
    SSABuilder *bld;
    SSABuildUtil *bldUtil;
    
    Parser();
    ~Parser();
    void parseText(pp_context *pcontext, List *pIList, LPSTR scriptText,
                      ULONG startingLineNumber, LPCSTR path);
    bool check(MY_TOKEN_TYPE theType);
    void match();
    void rewind(Token *token);
    void externalDecl2(BOOL variableonly);
    RValue *initializer();
    
    // void declSpec();
    void decl();
    void decl2();
    void funcDeclare();
    void funcDeclare1();
    void parmDecl();
    void paramList();
    void paramList2();
    void declList();
    void declList2();
    void stmtList();
    void stmtList2();
    
    void stmt();
    void exprStmt();
    void compStmt();
    void compStmt2();
    void compStmt3();
    
    void selectStmt();
    void switchBody(RValue *baseVal);
    void iterStmt();
    void optExprStmt();
    void jumpStmt();
    
    RValue *optExpr();
    inline RValue *expr() { return assignmentExpr(); }
    OpCode assignmentOp();
    RValue *assignmentExpr();
    RValue *condExpr();
    RValue *condExpr2(RValue *lhs);
    RValue *logOrExpr();
    RValue *logOrExpr2(RValue *lhs);
    RValue *logAndExpr();
    RValue *logAndExpr2(RValue *lhs);
    RValue *bitOrExpr();
    RValue *bitOrExpr2(RValue *lhs);
    RValue *xorExpr();
    RValue *xorExpr2(RValue *lhs);
    RValue *bitAndExpr();
    RValue *bitAndExpr2(RValue *lhs);
    OpCode equalOp();
    RValue *equalExpr();
    RValue *equalExpr2(RValue *lhs);
    OpCode relOp();
    RValue *relExpr();
    RValue *relExpr2(RValue *lhs);
    OpCode shiftOp();
    RValue *shiftExpr();
    RValue *shiftExpr2(RValue *lhs);
    OpCode addOp();
    RValue *addExpr();
    RValue *addExpr2(RValue *lhs);
    OpCode multOp();
    RValue *multExpr();
    RValue *multExpr2(RValue *lhs);
    RValue *unaryExpr();
    RValue *postfixExpr();
    RValue *postfixExpr2(RValue *lhs);
    void argExprList(FunctionCall *call);
    void argExprList2(FunctionCall *call);
    RValue *primaryExpr();
    RValue *constant();
};

void Parser_Init(Parser *pparser);
void Parser_Clear(Parser *pparser);
void Parser_ParseText(Parser *pparser, pp_context *pcontext, List *pIList, LPSTR scriptText,
                      ULONG startingLineNumber, LPCSTR path );
// void Parser_AddInstructionViaToken(Parser *pparser, OpCode pCode, Token *pToken, Label label );
// void Parser_AddInstructionViaLabel(Parser *pparser, OpCode pCode, Label instrLabel, Label listLabel );
BOOL Parser_Check(Parser *pparser, MY_TOKEN_TYPE theType );
void Parser_Match( Parser *pparser);
// Label Parser_CreateLabel( Parser *pparser );
void Parser_Start(Parser *pparser );
void Parser_External_decl(Parser *pparser );
void Parser_External_decl2(Parser *pparser, BOOL variableonly);
void Parser_Decl_spec(Parser *pparser );
void Parser_Decl(Parser *pparser );
void Parser_Decl2(Parser *pparser );
void Parser_FuncDecl(Parser *pparser );
void Parser_FuncDecl1(Parser *pparser );
void Parser_Initializer(Parser *pparser );
void Parser_Parm_decl(Parser *pparser );
void Parser_Param_list(Parser *pparser );
void Parser_Param_list2(Parser *pparser );
void Parser_Decl_list(Parser *pparser );
void Parser_Decl_list2(Parser *pparser );
void Parser_Stmt_list(Parser *pparser );
void Parser_Stmt_list2(Parser *pparser );
void Parser_Stmt( Parser *pparser);
void Parser_Expr_stmt(Parser *pparser );
//void Parser_Comp_stmt_Label(Parser *pparser, Label theLabel );
void Parser_Comp_stmt(Parser *pparser );
void Parser_Comp_stmt2(Parser *pparser );
void Parser_Comp_stmt3(Parser *pparser );
void Parser_Select_stmt(Parser *pparser );
void Parser_Opt_else(Parser *pparser );
void Parser_Switch_body(Parser *pparser, List *pCases );
void Parser_Case_label(Parser *pparser, List *pCases );
void Parser_Iter_stmt(Parser *pparser );
void Parser_Opt_expr_stmt(Parser *pparser );
List *Parser_Defer_expr_stmt(Parser *pparser );
void Parser_Jump_stmt(Parser *pparser );
void Parser_Opt_expr(Parser *pparser );
void Parser_Expr(Parser *pparser);
OpCode Parser_Assignment_op(Parser *pparser );
void Parser_Assignment_expr(Parser *pparser );
void Parser_Assignment_expr2(Parser *pparser );
void Parser_Cond_expr(Parser *pparser );
void Parser_Cond_expr2(Parser *pparser );
void Parser_Log_or_expr(Parser *pparser );
void Parser_Log_or_expr2(Parser *pparser );
void Parser_Log_and_expr(Parser *pparser );
void Parser_Log_and_expr2(Parser *pparser );
void Parser_Bit_or_expr(Parser *pparser );
void Parser_Bit_or_expr2(Parser *pparser );
void Parser_Xor_expr(Parser *pparser );
void Parser_Xor_expr2(Parser *pparser );
void Parser_Bit_and_expr(Parser *pparser );
void Parser_Bit_and_expr2(Parser *pparser );
OpCode Parser_Eq_op(Parser *pparser );
void Parser_Equal_expr(Parser *pparser );
void Parser_Equal_expr2(Parser *pparser );
OpCode Parser_Rel_op(Parser *pparser );
void Parser_Rel_expr(Parser *pparser );
void Parser_Rel_expr2(Parser *pparser );
OpCode Parser_Shift_op(Parser *pparser );
void Parser_Shift_expr(Parser *pparser );
void Parser_Shift_expr2(Parser *pparser );
OpCode Parser_Add_op(Parser *pparser );
void Parser_Add_expr(Parser *pparser );
void Parser_Add_expr2(Parser *pparser );
OpCode Parser_Mult_op(Parser *pparser );
void Parser_Mult_expr(Parser *pparser );
void Parser_Mult_expr2(Parser *pparser );
void Parser_Unary_expr(Parser *pparser );
void Parser_Postfix_expr(Parser *pparser );
void Parser_Postfix_expr2(Parser *pparser );
void Parser_Arg_expr_list(Parser *pparser );
void Parser_Arg_expr_list2(Parser *pparser, int argCount, int range);
void Parser_Primary_expr(Parser *pparser );
void Parser_Constant(Parser *pparser );
#define Parser_Error(pa, pr) Parser_Error2(pa, Productions::pr, #pr)
void Parser_Error2(Parser *pparser, PRODUCTION offender, const char *offenderStr );

#endif
