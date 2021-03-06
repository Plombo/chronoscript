/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2013 OpenBOR Team
 */

#ifndef PARSER_HPP
#define PARSER_HPP

#include "depends.h"
#include "ParserSet.h"
#include "List.hpp"
#include "SSABuilder.hpp"
#include "ExecBuilder.hpp"

class Parser
{
private:
    Lexer theLexer;                    //this parser's lexer
    ParserSet parserSet;               //this parser's parserSet
    Token theNextToken;                //The next token
    Token theNextNextToken;            //The token after the next one (used for overread)
    bool rewound;                      //If true, use theNextNextToken instead of lexing another
    int labelCount;                   //A counter to track the number of labels
    int errorCount;
    SSABuilder *bld;
    SSABuildUtil *bldUtil;
    SSABuilder *initBld;
    SSABuildUtil *initBldUtil;
    ExecBuilder *execBuilder;

public:
    void *memCtx; // TODO: make this a parameter and private

    Parser(pp_context *pcontext, ExecBuilder *builder, char *scriptText,
           int startingLineNumber, const char *path);
    void parseText();
    bool errorFound() { return !!errorCount; }

private:
    bool check(MY_TOKEN_TYPE theType);
    void match();
    void rewind(Token *token);
    void switchToInitFunction();
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
    void stmtList();
    
    void stmt();
    void exprStmt();
    void compStmt();
    
    void selectStmt();
    void switchBody(RValue *baseVal);
    void iterStmt();
    void optExprStmt();
    void jumpStmt();
    
    RValue *optExpr();
    inline RValue *expr() { return commaExpr(); }
    RValue *commaExpr();
    RValue *commaExpr2(RValue *lhs);
    OpCode assignmentOp();
    RValue *assignmentExpr();
    RValue *assignmentExpr2(RValue *lhs);
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
    RValue *postfixExpr2(RValue *src);
    void argExprList(FunctionCall *call);
    void argExprList2(FunctionCall *call);
    RValue *primaryExpr();
    RValue *object();
    RValue *list();
    RValue *constant();

    void handleError(PRODUCTION offender);
    void errorDefault(PRODUCTION offender, const char *offenderStr);
};

#define Parser_Error(pa, pr) pa->errorDefault(Productions::pr, #pr)

#endif

