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
#include "List.h"
#include "ssa.h"
#include "ExecBuilder.h"

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
    int labelCount;                   //A counter to track the number of labels
    Token theFieldToken;               //A pointer to the field source token
    int paramCount;
    char currentPath[256];                 // current path info of the text
    bool errorFound;
    BOOL isImport;

    void *memCtx;
    SSABuilder *bld;
    SSABuildUtil *bldUtil;
    ExecBuilder *execBuilder;
    CList<SSABuilder> functions;

    Parser(pp_context *pcontext, ExecBuilder *builder, char *scriptText,
           int startingLineNumber, const char *path);
    ~Parser();
    void parseText();
    bool check(MY_TOKEN_TYPE theType);
    void match();
    void rewind(Token *token);
    void externalDecl();
    void externalDecl2(bool variableonly);
    RValue *initializer();
    
    void declSpec();
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

    void error(PRODUCTION offender, const char *offenderStr);
};

#define Parser_Error(pa, pr) pa->error(Productions::pr, #pr)

#endif
