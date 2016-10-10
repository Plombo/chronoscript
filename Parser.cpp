/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2013 OpenBOR Team
 */

#include "Parser.h"

Parser::Parser(pp_context *pcontext, ExecBuilder *builder, char *scriptText,
               int startingLineNumber, const char *path)
 : theLexer(pcontext, path, scriptText, {startingLineNumber, 1})
{
    memCtx = ralloc_context(NULL);

    labelCount = 0;
    theNextToken.theType = END_OF_TOKENS;
    rewound = false;
    bldUtil = NULL;
    execBuilder = builder;
    errorFound = false;
}

/******************************************************************************
*  ParseText -- This method begins the recursive-descent process for each global
*  variable and function definition.
*  Parameters: pcontext -- Preprocessor context for the script to be parsed.
*              pIList -- pointer to a TList<CInstruction> which takes the
*                        stream of CInstructions representing the parsed text
*              scriptText -- string which contains the script to be parsed.
*              startingLineNumber -- Line on which this script starts.  The
*                        lexer needs this information to maintain accurate
*                        line counts.
*  Returns:
******************************************************************************/
void Parser::parseText()
{
    //Get the first token from the CLexer.
    theLexer.getNextToken(&theNextToken);

    //Parse the script text until you reach the end of the file, or until
    //an error occurs.
    while(theNextToken.theType != TOKEN_EOF)
    {
        externalDecl();
    }
}

/******************************************************************************
*  Check -- This method compares the type of the current token with a specified
*  token type.
*  Parameters: theType -- MY_TOKEN_TYPE specifying the type to compare
*  Returns: true if the current token type matches theType
*           false otherwise.
******************************************************************************/
bool Parser::check(MY_TOKEN_TYPE theType)
{
    //compare the token types
    return (theNextToken.theType == theType);
}

/*****************************************************************************
*  Match -- This method consumes the current token and retrieves the next one.
*  Parameters:
*  Returns:
******************************************************************************/
void Parser::match()
{
    if (rewound) // the next token has already been lexed
    {
        memcpy(&theNextToken, &theNextNextToken, sizeof(Token));
        rewound = false;
    }
    else if (FAILED(theLexer.getNextToken(&theNextToken)))
    {
        Parser_Error(this, error);
    }
}

/*****************************************************************************
*  Rewind -- This method sets the current token to the given token. The
*  previous current token will be treated as the next token in the stream.
*  Parameters: token -- a pointer to the token to set as current
*  Returns:
******************************************************************************/
void Parser::rewind(Token *token)
{
    assert(!rewound); // can't rewind by more than one level
    memcpy(&theNextNextToken, &theNextToken, sizeof(Token));
    memcpy(&theNextToken, token, sizeof(Token));
    rewound = true;
}

/******************************************************************************
*  Productions -- These methods recursively parse the token stream into an
*  Abstract Syntax Tree.
******************************************************************************/


/******************************************************************************
*  Declaration evaluation -- These methods translate declarations into
*  appropriate instructions.
******************************************************************************/
void Parser::externalDecl()
{
    if (parserSet.first(Productions::decl_spec, theNextToken.theType))
    {
        declSpec();
        externalDecl2(false); // go for the declaration
    }

    else
    {
        Parser_Error(this, external_decl);
    }
}

// this function is used by Parser::externalDecl, because there can be multiple identifiers share only one type token
// variable only means the function only accept variables, not function declaration, e.g.
// int a, b=1, c;
void Parser::externalDecl2(bool variableonly)
{
    Token token = theNextToken;
    //ignore the type of this declaration
    if(!check(TOKEN_IDENTIFIER))
    {
        printf("Identifier expected before '%s'.\n", token.theSource);
        Parser_Error(this, external_decl);
    }
    match();

    // global variable declaration
#if 0 // global variables with initializer not supported yet
    //type a =
    if (parserSet.first(Productions::initializer, theNextToken.theType))
    {
        //switch to immediate mode and allocate a variable.
        execBuilder->globals.declareGlobalVariable(token.theSource);
        // Parser_AddInstructionViaToken(pparser, IMMEDIATE, (Token *)NULL, NULL );
        // Parser_AddInstructionViaToken(pparser, DATA, &token, NULL );

        //Get the initializer; type a = expression
        RValue *initialValue = initializer();
        //type a = expresson;
        if(check(TOKEN_SEMICOLON))
        {
            match();

            //Save the initializer
            Parser_AddInstructionViaToken(pparser, SAVE, &token, NULL );

            //Switch back to deferred mode
            Parser_AddInstructionViaToken(pparser, DEFERRED, (Token *)NULL, NULL );
        }
        //there's a comma instead of semicolon, so there should be another identifier
        else if(check(TOKEN_COMMA))
        {
            match();
            //Save the initializer
            Parser_AddInstructionViaToken(pparser, SAVE, &token, NULL );
            externalDecl2(TRUE);
        }
        else
        {
            match();
            printf("Semicolon or comma expected before '%s'\n", theNextToken.theSource);
            Parser_Error(this, external_decl );
        }
    }
    else
#endif
    // semicolon, end expression.
    if (check(TOKEN_SEMICOLON))
    {
        match();
        // declare the variable
        execBuilder->globals.declareGlobalVariable(token.theSource);
    }
    // still comma? there should be another identifier so declare the variable and go for the next
    else if (check(TOKEN_COMMA))
    {
        match();
        execBuilder->globals.declareGlobalVariable(token.theSource);
        externalDecl2(true);
    }

    // not a comma, semicolon, or initializer, so must be a function declaration
    else if (!variableonly && parserSet.first(Productions::funcDecl, theNextToken.theType))
    {
        bld = new(memCtx) SSABuilder(memCtx, token.theSource);
        bldUtil = new SSABuildUtil(bld, &execBuilder->globals);
        BasicBlock *startBlock = bldUtil->createBBAfter(NULL);
        bld->sealBlock(startBlock);
        bldUtil->setCurrentBlock(startBlock);
        bldUtil->pushScope();
        funcDeclare();
        compStmt();
        bldUtil->popScope();
        if (!bldUtil->currentBlock->endsWithJump())
            bldUtil->mkReturn(NULL);
        delete bldUtil;
        execBuilder->ssaFunctions.insertAfter(bld, bld->functionName);
    }
    else
    {
        Parser_Error(this, external_decl);
    }
}

void Parser::declSpec()
{
    if (check(TOKEN_CONST))
    {
        match();
    }

    if (check(TOKEN_SIGNED))
    {
        match();
    }
    else if (check(TOKEN_UNSIGNED))
    {
        match();
    }
    // It's OK though not all below are valid types for our language
    if (check(TOKEN_VOID))
    {
        match();
    }
    else if (check(TOKEN_CHAR))
    {
        match();
    }
    else if (check(TOKEN_SHORT))
    {
        match();
    }
    else if (check(TOKEN_INT))
    {
        match();
    }
    else if (check(TOKEN_LONG))
    {
        match();
    }
    else if (check(TOKEN_FLOAT))
    {
        match();
    }
    else if (check(TOKEN_DOUBLE))
    {
        match();
    }
    else
    {
        Parser_Error(this, decl_spec);
    }
}

// internal decleraton for variables.
void Parser::decl()
{
    if (parserSet.first(Productions::decl_spec, theNextToken.theType))
    {
        //ignore the type of this declaration
        declSpec();
        decl2();
    }
    else
    {
        Parser_Error(this, decl );
    }
}

// this function is used by Parser_Decl for multiple declaration separated by commas
void Parser::decl2()
{
    Token token = theNextToken;
    if(!check(TOKEN_IDENTIFIER))
    {
        printf("Identifier expected before '%s'.\n", token.theSource);
        Parser_Error(this, decl );
    }
    match();

    // =
    if (parserSet.first(Productions::initializer, theNextToken.theType))
    {
        // Parser_AddInstructionViaToken(pparser, DATA, &token, NULL );
        bldUtil->declareVariable(token.theSource);

        //Get the initializer;
        RValue *initialValue = initializer();
        if(check(TOKEN_SEMICOLON ))
        {
            match();
            //Save the initializer
            if (initialValue)
            {
                bldUtil->writeVariable(token.theSource, initialValue);
            }
        }
        else if(check(TOKEN_COMMA ))
        {
            match();
            if (initialValue)
            {
                bldUtil->writeVariable(token.theSource, initialValue);
            }
            decl2();
        }
        else
        {
            match();
            printf("Semicolon or comma expected before '%s'\n", theNextToken.theSource);
            Parser_Error(this, decl );
        }
    }
    // ,
    else if (check(TOKEN_COMMA))
    {
        match();
        bldUtil->declareVariable(token.theSource);
        decl2();
    }
    // ;
    else if (check(TOKEN_SEMICOLON))
    {
        match();
        bldUtil->declareVariable(token.theSource);
    }
    else
    {
        Parser_Error(this, decl);
    }
}

void Parser::funcDeclare()
{
    if (check(TOKEN_LPAREN))
    {
        match();
        funcDeclare1();
    }
    else if (parserSet.follow(Productions::funcDecl, theNextToken.theType)) {}
    else
    {
        Parser_Error(this, funcDecl);
    }
}

void Parser::funcDeclare1()
{
    if (parserSet.first(Productions::param_list, theNextToken.theType))
    {
        paramList();
        check(TOKEN_RPAREN);
        match();
    }
    else if (check(TOKEN_RPAREN))
    {
        match();
    }
    else
    {
        Parser_Error(this, funcDecl1 );
    }
}

RValue *Parser::initializer()
{
    if (check(TOKEN_ASSIGN))
    {
        match();
        return assignmentExpr();
    }
    else if (parserSet.follow(Productions::initializer, theNextToken.theType)) {}
    else
    {
        Parser_Error(this, initializer);
    }
    return NULL;
}

void Parser::parmDecl()
{
    if (parserSet.first(Productions::decl_spec, theNextToken.theType))
    {
        declSpec();

        if (check(TOKEN_IDENTIFIER))
        {
            bldUtil->addParam(theNextToken.theSource);
            match();
        }
    }
    else
    {
        Parser_Error(this, parm_decl );
    }
}

void Parser::paramList()
{
    if (parserSet.first(Productions::parm_decl, theNextToken.theType))
    {
        parmDecl();
        paramList2();
    }
    else
    {
        Parser_Error(this, param_list );
    }
}

void Parser::paramList2()
{
    if (check(TOKEN_COMMA))
    {
        match();
        parmDecl();
        paramList2();
    }
    else if (parserSet.follow(Productions::param_list2, theNextToken.theType)) {}
    else
    {
        Parser_Error(this, param_list2 );
    }
}

void Parser::declList()
{
    if (parserSet.first(Productions::decl, theNextToken.theType))
    {
        decl();
        declList2();
    }
    else
    {
        Parser_Error(this, decl_list );
    }
}

void Parser::declList2()
{
    if (parserSet.first(Productions::decl, theNextToken.theType))
    {
        decl();
        declList2();
    }
    if (parserSet.first(Productions::stmt, theNextToken.theType))
    {
        stmt();
        stmtList2();
    }
    else if (parserSet.follow(Productions::decl_list2, theNextToken.theType)) {}
    else
    {
        Parser_Error(this, decl_list2 );
    }
}

/******************************************************************************
*  Statement evaluation -- These methods translate statements into
*  appropriate instructions.
******************************************************************************/
void Parser::stmtList()
{
    if (parserSet.first(Productions::stmt, theNextToken.theType))
    {
        stmt();
        stmtList2();
    }
    else
    {
        Parser_Error(this, stmt_list );
    }
}

void Parser::stmtList2()
{
    if (parserSet.first(Productions::stmt, theNextToken.theType))
    {
        stmt();
        stmtList2();
    }
    else if (parserSet.first(Productions::decl, theNextToken.theType))
    {
        decl();
        declList2();
    }
    else if (parserSet.follow(Productions::stmt_list2, theNextToken.theType)) {}
    else
    {
        Parser_Error(this, stmt_list2);
    }
}

void Parser::stmt()
{
    if (parserSet.first(Productions::expr_stmt, theNextToken.theType))
    {
        exprStmt();
    }
    else if (parserSet.first(Productions::comp_stmt, theNextToken.theType))
    {
        compStmt();
    }
    else if (parserSet.first(Productions::select_stmt, theNextToken.theType))
    {
        selectStmt();
    }
    else if (parserSet.first(Productions::iter_stmt, theNextToken.theType))
    {
        iterStmt();
    }
    else if (parserSet.first(Productions::jump_stmt, theNextToken.theType))
    {
        jumpStmt();
    }
    else
    {
        Parser_Error(this, stmt);
    }
}

void Parser::exprStmt()
{
    if (parserSet.first(Productions::expr, theNextToken.theType))
    {
        expr();
        check(TOKEN_SEMICOLON);
        match();
    }
    else
    {
        Parser_Error(this, expr_stmt);
    }
}

void Parser::compStmt()
{
    if (check(TOKEN_LCURLY))
    {
        match();
        bldUtil->pushScope();
        compStmt2();
        compStmt3();
        bldUtil->popScope();
        check(TOKEN_RCURLY);
        match();
    }
    else
    {
        Parser_Error(this, comp_stmt);
    }
}

void Parser::compStmt2()
{
    if (parserSet.first(Productions::decl_list, theNextToken.theType))
    {
        declList();
    }
    else if (parserSet.follow(Productions::comp_stmt2, theNextToken.theType)) {}
    else
    {
        Parser_Error(this, comp_stmt2);
    }
}

void Parser::compStmt3()
{
    if (parserSet.first(Productions::stmt_list, theNextToken.theType))
    {
        stmtList();
    }
    else if (parserSet.follow(Productions::comp_stmt3, theNextToken.theType)) {}
    else
    {
        Parser_Error(this, comp_stmt3);
    }
}

void Parser::selectStmt()
{
    if (check(TOKEN_IF))
    {
        // condition
        match();
        check(TOKEN_LPAREN);
        match();
        RValue *condition = expr();
        check(TOKEN_RPAREN);
        match();

        // body
        BasicBlock *startBlock = bldUtil->currentBlock,
                   *ifBlock = bldUtil->createBBAfter(bldUtil->currentBlock),
                   *endIfBlock,
                   *afterIfBlock;
        ifBlock->addPred(startBlock);
        bld->sealBlock(ifBlock);
        Jump *skipIfJump = bldUtil->mkJump(OP_BRANCH_FALSE, NULL, condition);
        
        bldUtil->setCurrentBlock(ifBlock);
        stmt();
        endIfBlock = bldUtil->currentBlock;
        
        if (check(TOKEN_ELSE)) // if-else
        {
            match();
            BasicBlock *elseBlock = bldUtil->createBBAfter(endIfBlock); // comes before afterIfBlock
            elseBlock->addPred(startBlock);
            bld->sealBlock(elseBlock);
            skipIfJump->target = elseBlock;
            bldUtil->setCurrentBlock(elseBlock);
            stmt();
            BasicBlock *endElseBlock = bldUtil->currentBlock;
            afterIfBlock = bldUtil->createBBAfter(endElseBlock);
            if (!endElseBlock->endsWithJump())
                afterIfBlock->addPred(endElseBlock);
            bldUtil->setCurrentBlock(endIfBlock);
            if (!endIfBlock->endsWithJump())
            {
                bldUtil->mkJump(OP_JMP, afterIfBlock, NULL);
                afterIfBlock->addPred(endIfBlock);
            }
        }
        else // standalone if
        {
            afterIfBlock = bldUtil->createBBAfter(endIfBlock);
            if (!endIfBlock->endsWithJump())
                afterIfBlock->addPred(endIfBlock);
            afterIfBlock->addPred(startBlock);
            skipIfJump->target = afterIfBlock;
        }
        // common to both if-else and standalone if
        bld->sealBlock(afterIfBlock);
        bldUtil->setCurrentBlock(afterIfBlock);
    }
    else if (check(TOKEN_SWITCH))
    {
        match();
        check(TOKEN_LPAREN);
        match();
        RValue *baseVal = expr();
        check(TOKEN_RPAREN);
        match();
        check(TOKEN_LCURLY);
        match();

        //Parse body of switch, and consume the closing brace
        bldUtil->pushScope();
        switchBody(baseVal);
        bldUtil->popScope();
        check(TOKEN_RCURLY);
        match();
    }
    else
    {
        Parser_Error(this, select_stmt);
    }
}

void Parser::switchBody(RValue *baseVal)
{
    BasicBlock *jumps = bldUtil->createBBAfter(bldUtil->currentBlock),
               *body = bldUtil->createBBAfter(jumps),
               *after = bldUtil->createBBAfter(body),
               *defaultTarget = after;
    jumps->addPred(bldUtil->currentBlock);
    bld->sealBlock(jumps);
    body->addPred(jumps);
    bld->sealBlock(body);
    bldUtil->breakTargets.push(after);

    //Using a loop here instead of recursion goes against the idea of a
    //recursive descent parser, but it keeps us from having 200 stack frames.
    while(1)
    {
        bldUtil->currentBlock = body;
        if (parserSet.first(Productions::case_label, theNextToken.theType))
        {
            if (!body->isEmpty())
            {
                BasicBlock *newBody = bldUtil->createBBAfter(body);
                newBody->addPred(jumps);
                if (!body->endsWithJump())
                    newBody->addPred(body);
                bld->sealBlock(newBody);
                body = newBody;
            }
            if (check(TOKEN_CASE))
            {
                match();
                bldUtil->currentBlock = jumps;
                RValue *caseVal = constant();
                bldUtil->mkJump(OP_BRANCH_EQUAL, body, baseVal, caseVal);
                check(TOKEN_COLON);
                match();
            }
            else
            {
                check(TOKEN_DEFAULT);
                match();
                check(TOKEN_COLON);
                match();
                defaultTarget = body;
            }
        }
        else if (parserSet.first(Productions::stmt, theNextToken.theType))
        {
            stmt();
            stmtList2();
        }
        else if (parserSet.first(Productions::decl, theNextToken.theType))
        {
            decl();
            declList2();
        }
        else if (parserSet.follow(Productions::switch_body, theNextToken.theType))
        {
            break;
        }
        else
        {
            Parser_Error(this, switch_body);
            break;
        }
    }
    
    // implicit jump from end of switch body to after switch
    if (!body->endsWithJump())
        after->addPred(body);

    // set default target
    bldUtil->currentBlock = jumps;
    bldUtil->mkJump(OP_JMP, defaultTarget, NULL);
    if (defaultTarget == after)
        after->addPred(jumps);
    bld->sealBlock(after);
    bldUtil->breakTargets.pop();
    bldUtil->currentBlock = after;
}

void Parser::iterStmt()
{
    BasicBlock *before, *header, *bodyStart, *bodyEnd, *footer, *after;
    Loop *loop;
    before = bldUtil->currentBlock;

    if (check(TOKEN_WHILE))
    {
        header = bldUtil->createBBAfter(before);
        loop = new(memCtx) Loop(header);
        header->addPred(before);
        // can't seal header yet
        bodyStart = bldUtil->createBBAfter(header, loop);
        after = bldUtil->createBBAfter(bodyStart, NULL);
        bldUtil->pushLoop(loop);

        // loop header (condition)
        bldUtil->setCurrentBlock(header);
        match();
        check(TOKEN_LPAREN);
        match();
        RValue *condition = expr();
        check(TOKEN_RPAREN);
        match();
        bldUtil->mkJump(OP_BRANCH_FALSE, after, condition);

        // note that bldUtil->currentBlock might not be the same as header
        bodyStart->addPred(bldUtil->currentBlock);
        bld->sealBlock(bodyStart);
        after->addPred(bldUtil->currentBlock);
        
        // push after to break stack and header to continue stack
        bldUtil->breakTargets.push(after);
        bldUtil->continueTargets.push(header);
        
        // loop body
        bldUtil->setCurrentBlock(bodyStart);
        stmt(); // fill the loop body
        bodyEnd = bldUtil->currentBlock;
        if (!bodyEnd->endsWithJump())
        {
            bldUtil->mkJump(OP_JMP, header, NULL);
            header->addPred(bodyEnd);
        }
        // now we can finally seal these two blocks
        bld->sealBlock(header);
        bld->sealBlock(after);
        bldUtil->breakTargets.pop();
        bldUtil->continueTargets.pop();
        bldUtil->currentBlock = after;
        bldUtil->popLoop();
    }
    else if (check(TOKEN_DO))
    {
        match();
        bodyStart = bldUtil->createBBAfter(before);
        loop = new(memCtx) Loop(bodyStart);
        bodyStart->addPred(before);
        footer = bldUtil->createBBAfter(bodyStart, loop);
        // can't seal footer yet (continue target, plus end of body)
        after = bldUtil->createBBAfter(footer, NULL);
        // can't seal after yet (break target)
        
        // push after to break stack and footer to continue stack
        bldUtil->breakTargets.push(after);
        bldUtil->continueTargets.push(footer);

        // loop body
        bldUtil->pushLoop(loop);
        bldUtil->currentBlock = bodyStart;
        stmt();
        bodyEnd = bldUtil->currentBlock;
        if (!bodyEnd->endsWithJump())
        {
            footer->addPred(bodyEnd);
        }

        // condition
        bldUtil->currentBlock = footer;
        check(TOKEN_WHILE);
        match();
        check(TOKEN_LPAREN);
        match();
        RValue *condition = expr();
        check(TOKEN_RPAREN);
        match();
        bldUtil->mkJump(OP_BRANCH_TRUE, bodyStart, condition);
        check(TOKEN_SEMICOLON);
        match();
        
        // now we can finally seal these blocks
        after->addPred(bldUtil->currentBlock);
        bodyStart->addPred(bldUtil->currentBlock);
        bld->sealBlock(bodyStart);
        bld->sealBlock(footer);
        bld->sealBlock(after);

        bldUtil->breakTargets.pop();
        bldUtil->continueTargets.pop();
        bldUtil->currentBlock = after;
        bldUtil->popLoop();
    }
    else if (check(TOKEN_FOR))
    {
        header = bldUtil->createBBAfter(before);
        loop = new(memCtx) Loop(header);
        bodyStart = bldUtil->createBBAfter(header, loop);
        footer = bldUtil->createBBAfter(bodyStart, loop);
        after = bldUtil->createBBAfter(footer, NULL);
        
        match();
        check(TOKEN_LPAREN);
        match();

        //Add any initializer code
        optExprStmt();
        header->addPred(before);

        //Build the conditional statement
        bldUtil->currentBlock = header;
        bldUtil->pushLoop(loop);
        RValue *condition = optExpr();
        if (condition)
            bldUtil->mkJump(OP_BRANCH_FALSE, after, condition);
        check(TOKEN_SEMICOLON);
        match();
        bodyStart->addPred(bldUtil->currentBlock);
        bld->sealBlock(bodyStart);
        after->addPred(bldUtil->currentBlock);

        // The last parameter goes in the footer
        bldUtil->currentBlock = footer;
        if (parserSet.first(Productions::expr, theNextToken.theType))
        {
            expr();
        }
        else if (parserSet.follow(Productions::defer_expr_stmt, theNextToken.theType)) {}
        else
        {
            Parser_Error(this, defer_expr_stmt);
        }
        bldUtil->mkJump(OP_JMP, header, NULL);
        header->addPred(bldUtil->currentBlock);
        bld->sealBlock(header);
        check(TOKEN_RPAREN);
        match();

        // loop body
        bldUtil->currentBlock = bodyStart;
        bldUtil->breakTargets.push(after);
        bldUtil->continueTargets.push(footer);
        stmt();
        bodyEnd = bldUtil->currentBlock;
        bldUtil->breakTargets.pop();
        bldUtil->continueTargets.pop();
        if (!bodyEnd->endsWithJump())
        {
            footer->addPred(bodyEnd);
        }
        
        // now we can seal the rest of the blocks
        bld->sealBlock(footer);
        bld->sealBlock(after);
        bldUtil->currentBlock = after;
        bldUtil->popLoop();
    }
    else
    {
        Parser_Error(this, iter_stmt);
    }
}

void Parser::optExprStmt() // used in for loop
{
    if (parserSet.first(Productions::expr_stmt, theNextToken.theType))
    {
        exprStmt();
    }
    else if (parserSet.first(Productions::decl, theNextToken.theType))
    {
        decl();
    }
    else if (check(TOKEN_SEMICOLON))
    {
        match();
    }
    else if (parserSet.follow(Productions::opt_expr_stmt, theNextToken.theType)) {}
    else
    {
        Parser_Error(this, opt_expr_stmt);
    }
}

void Parser::jumpStmt()
{
    BasicBlock *jumpTarget;
    if (check(TOKEN_BREAK))
    {
        match();
        check(TOKEN_SEMICOLON);
        match();

        jumpTarget = bldUtil->breakTargets.top();
        bldUtil->mkJump(OP_JMP, jumpTarget, NULL);
        jumpTarget->addPred(bldUtil->currentBlock);
    }
    else if (check(TOKEN_CONTINUE))
    {
        match();
        check(TOKEN_SEMICOLON);
        match();

        jumpTarget = bldUtil->continueTargets.top();
        bldUtil->mkJump(OP_JMP, jumpTarget, NULL);
        jumpTarget->addPred(bldUtil->currentBlock);
    }
    else if (check(TOKEN_RETURN))
    {
        match();
        RValue *value = optExpr();
        check(TOKEN_SEMICOLON);
        match();

        bldUtil->mkReturn(value == NULL ? bldUtil->mkNull() : value);
    }
    else
    {
        Parser_Error(this, jump_stmt);
        return;
    }
}

/******************************************************************************
*  Expression Evaluation -- These methods translate expressions into
*  appropriate instructions.
******************************************************************************/
RValue *Parser::optExpr()
{
    if (parserSet.first(Productions::expr, theNextToken.theType))
    {
        return expr();
    }
    else if (parserSet.follow(Productions::opt_expr, theNextToken.theType))
    {
        return NULL;
    }
    else
    {
        Parser_Error(this, opt_expr);
        return NULL;
    }
}

OpCode Parser::assignmentOp()
{
    if (check(TOKEN_ASSIGN))
    {
        match();
        return OP_NOOP;
    }
    else if (check(TOKEN_MUL_ASSIGN))
    {
        match();
        return OP_MUL;
    }
    else if (check(TOKEN_DIV_ASSIGN))
    {
        match();
        return OP_DIV;
    }
    else if (check(TOKEN_ADD_ASSIGN))
    {
        match();
        return OP_ADD;
    }
    else if (check(TOKEN_SUB_ASSIGN))
    {
        match();
        return OP_SUB;
    }
    else if (check(TOKEN_MOD_ASSIGN))
    {
        match();
        return OP_MOD;
    }
    else
    {
        Parser_Error(this, assignment_op);
        return OP_NOOP;
    }
}

RValue *Parser::assignmentExpr()
{
    if (theNextToken.theType == TOKEN_IDENTIFIER) // maybe the start of an assignment?
    {
        Token identifier = theNextToken;
        match();
        if (parserSet.first(Productions::assignment_op, theNextToken.theType))
        {
            // this is actually an assignment
            OpCode op = assignmentOp();
            RValue *rhs = assignmentExpr(), *result;
            if (op == OP_NOOP) // regular assignment (=)
            {
                result = rhs;
            }
            else // operation and assignment (+=, >>=, etc.)
            {
                RValue *lhs = bldUtil->readVariable(identifier.theSource);
                result = bldUtil->mkBinaryOp(op, lhs, rhs);
            }
            bldUtil->writeVariable(identifier.theSource, result);
            return result;
        }
        else
        {
            // the identifier we matched is not the LHS of an assignment, so rewind
            rewind(&identifier);
            // now fall through to the logic below
        }
    }
    if (parserSet.first(Productions::cond_expr, theNextToken.theType)) //= /= += Operator, or a comma and the like (the reputation of a variable)
    {
        return condExpr();
    }
    else
    {
        Parser_Error(this, assignment_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::condExpr()
{
    if (parserSet.first(Productions::log_or_expr, theNextToken.theType))
    {
        RValue *lhs = logOrExpr();
        return condExpr2(lhs);
    }
    else
    {
        Parser_Error(this, cond_expr);
    }
    return bldUtil->undef();
}

RValue *Parser::condExpr2(RValue *lhs)
{
    char varName[64];
    sprintf(varName, "?:%i", labelCount++);
    if (check(TOKEN_CONDITIONAL))
    {
        match();
        bldUtil->declareVariable(varName);
        Jump *branch = bldUtil->mkJump(OP_BRANCH_FALSE, NULL, lhs);

        // evaluate if-true expression
        BasicBlock *firstBlock = bldUtil->currentBlock,
                   *trueBlock = bldUtil->createBBAfter(firstBlock);
        trueBlock->addPred(bldUtil->currentBlock);
        bld->sealBlock(trueBlock);
        bldUtil->currentBlock = trueBlock;
        RValue *trueVal = expr();
        bldUtil->writeVariable(varName, trueVal);
        Jump *jumpAfterTrue = bldUtil->mkJump(OP_JMP, NULL, NULL);
        if (check(TOKEN_COLON))
        {
            match();
            BasicBlock *lastTrueBlock = bldUtil->currentBlock,
                       *falseBlock = bldUtil->createBBAfter(lastTrueBlock);
            falseBlock->addPred(firstBlock);
            bld->sealBlock(falseBlock);
            branch->target = falseBlock;
            bldUtil->currentBlock = falseBlock;
            RValue *falseVal = condExpr();
            bldUtil->writeVariable(varName, falseVal);
            // assert(!(bldUtil->currentBlock->endsWithJump());

            BasicBlock *afterBlock = bldUtil->createBBAfter(bldUtil->currentBlock);
            jumpAfterTrue->target = afterBlock;
            afterBlock->addPred(lastTrueBlock);
            afterBlock->addPred(bldUtil->currentBlock);
            bld->sealBlock(afterBlock);
            bldUtil->currentBlock = afterBlock;
            return bldUtil->readVariable(varName);
        }
        else
        {
            Parser_Error(this, cond_expr2);
            return bldUtil->undef();
        }
    }
    else if (parserSet.follow(Productions::cond_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        Parser_Error(this, cond_expr2);
        return bldUtil->undef();
    }
}

// void Parser_Log_or_expr(Parser *pparser )
RValue *Parser::logOrExpr()
{
    if (parserSet.first(Productions::log_and_expr, theNextToken.theType))
    {
        RValue *lhs = logAndExpr();
        return logOrExpr2(lhs);
    }
    else
    {
        Parser_Error(this, log_or_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::logOrExpr2(RValue *lhs)
{
    if (check(TOKEN_OR_OP))
    {
        match();
        char varName[64];
        sprintf(varName, "||%i", labelCount++);
        bldUtil->declareVariable(varName);
        bldUtil->writeVariable(varName, lhs);
        Jump *jumpFromFirst = bldUtil->mkJump(OP_BRANCH_TRUE, NULL, lhs);
        BasicBlock *firstBlock = bldUtil->currentBlock,
                   *secondBlock = bldUtil->createBBAfter(firstBlock);
        secondBlock->addPred(firstBlock);
        bld->sealBlock(secondBlock);
        bldUtil->currentBlock = secondBlock;
        RValue *rhs = logAndExpr();
        bldUtil->writeVariable(varName, rhs);
        BasicBlock *afterBlock = bldUtil->createBBAfter(bldUtil->currentBlock);
        jumpFromFirst->target = afterBlock;
        afterBlock->addPred(firstBlock);
        afterBlock->addPred(bldUtil->currentBlock);
        bld->sealBlock(afterBlock);
        bldUtil->currentBlock = afterBlock;
        RValue *result = bldUtil->readVariable(varName);
        result = bldUtil->mkUnaryOp(OP_BOOL, result);
        return logOrExpr2(result);
    }
    else if (parserSet.follow(Productions::log_or_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        Parser_Error(this, log_or_expr2);
        return bldUtil->undef();
    }
}

RValue *Parser::logAndExpr()
{
    if (parserSet.first(Productions::bit_or_expr, theNextToken.theType))
    {
        RValue *lhs = bitOrExpr();
        return logAndExpr2(lhs);
    }
    else
    {
        Parser_Error(this, log_and_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::logAndExpr2(RValue *lhs)
{
    if (check(TOKEN_AND_OP))
    {
        match();
        char varName[64];
        sprintf(varName, "&&%i", labelCount++);
        bldUtil->declareVariable(varName);
        bldUtil->writeVariable(varName, lhs);
        Jump *jumpFromFirst = bldUtil->mkJump(OP_BRANCH_FALSE, NULL, lhs);
        BasicBlock *firstBlock = bldUtil->currentBlock,
                   *secondBlock = bldUtil->createBBAfter(firstBlock);
        secondBlock->addPred(firstBlock);
        bld->sealBlock(secondBlock);
        bldUtil->currentBlock = secondBlock;
        RValue *rhs = bitOrExpr();
        bldUtil->writeVariable(varName, rhs);
        BasicBlock *afterBlock = bldUtil->createBBAfter(bldUtil->currentBlock);
        jumpFromFirst->target = afterBlock;
        afterBlock->addPred(firstBlock);
        afterBlock->addPred(bldUtil->currentBlock);
        bld->sealBlock(afterBlock);
        bldUtil->currentBlock = afterBlock;
        RValue *result = bldUtil->readVariable(varName);
        result = bldUtil->mkUnaryOp(OP_BOOL, result);
        return logAndExpr2(result);
    }
    else if (parserSet.follow(Productions::log_and_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        Parser_Error(this, log_and_expr2);
        return bldUtil->undef();
    }
}

RValue *Parser::bitOrExpr()
{
    if (parserSet.first(Productions::xor_expr, theNextToken.theType))
    {
        RValue *lhs = xorExpr();
        return bitOrExpr2(lhs);
    }
    else
    {
        Parser_Error(this, bit_or_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::bitOrExpr2(RValue *lhs)
{
    if (check(TOKEN_BITWISE_OR))
    {
        match();
        RValue *rhs = xorExpr();
        RValue *result = bldUtil->mkBinaryOp(OP_BIT_OR, lhs, rhs);
        return bitOrExpr2(result);
    }
    else if (parserSet.follow(Productions::bit_or_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        Parser_Error(this, bit_or_expr2);
        return bldUtil->undef();
    }
}

RValue *Parser::xorExpr()
{
    if (parserSet.first(Productions::bit_and_expr, theNextToken.theType))
    {
        RValue *lhs = bitAndExpr();
        return xorExpr2(lhs);
    }
    else
    {
        Parser_Error(this, xor_expr);
        return bldUtil->undef();
    }
}

// void Parser_Xor_expr2(Parser *pparser )
RValue *Parser::xorExpr2(RValue *lhs)
{
    if (check(TOKEN_XOR))
    {
        match();
        RValue *rhs = bitAndExpr();
        RValue *result = bldUtil->mkBinaryOp(OP_XOR, lhs, rhs);
        return xorExpr2(result);
    }
    else if (parserSet.follow(Productions::xor_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        Parser_Error(this, xor_expr2);
        return bldUtil->undef();
    }
}

RValue *Parser::bitAndExpr()
{
    if (parserSet.first(Productions::equal_expr, theNextToken.theType))
    {
        RValue *lhs = equalExpr();
        return bitAndExpr2(lhs);
    }
    else
    {
        Parser_Error(this, bit_and_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::bitAndExpr2(RValue *lhs)
{
    if (check(TOKEN_BITWISE_AND))
    {
        match();
        RValue *rhs = equalExpr();
        RValue *result = bldUtil->mkBinaryOp(OP_BIT_AND, lhs, rhs);
        return bitAndExpr2(result);
    }
    else if (parserSet.follow(Productions::bit_and_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        Parser_Error(this, bit_and_expr2);
        return bldUtil->undef();
    }
}

OpCode Parser::equalOp()
{
    if (check(TOKEN_EQ_OP))
    {
        match();
        return OP_EQ;
    }
    else if (check(TOKEN_NE_OP))
    {
        match();
        return OP_NE;
    }
    else
    {
        Parser_Error(this, eq_op);
        return OP_NOOP;
    }
}

RValue *Parser::equalExpr()
{
    if (parserSet.first(Productions::rel_expr, theNextToken.theType))
    {
        RValue *lhs = relExpr();
        return equalExpr2(lhs);
    }
    else
    {
        Parser_Error(this, equal_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::equalExpr2(RValue *lhs)
{
    OpCode code;
    if (parserSet.first(Productions::eq_op, theNextToken.theType))
    {
        code = equalOp();
        RValue *rhs = relExpr();
        RValue *result = bldUtil->mkBinaryOp(code, lhs, rhs);
        return equalExpr2(result);
    }
    else if (parserSet.follow(Productions::equal_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        Parser_Error(this, equal_expr2);
        return bldUtil->undef();
    }
}

OpCode Parser::relOp()
{
    if (check(TOKEN_GE_OP))
    {
        match();
        return OP_GE;
    }
    else if (check(TOKEN_LE_OP))
    {
        match();
        return OP_LE;
    }
    else if (check(TOKEN_LT))
    {
        match();
        return OP_LT;
    }
    else if (check(TOKEN_GT))
    {
        match();
        return OP_GT;
    }
    else
    {
        Parser_Error(this, rel_op);
        return OP_ERR;
    }
}

RValue *Parser::relExpr()
{
    if (parserSet.first(Productions::shift_expr, theNextToken.theType))
    {
        RValue *lhs = shiftExpr();
        return relExpr2(lhs);
    }
    else
    {
        Parser_Error(this, rel_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::relExpr2(RValue *lhs)
{
    OpCode code;
    if (parserSet.first(Productions::rel_op, theNextToken.theType))
    {
        code = relOp();
        RValue *rhs = shiftExpr();
        RValue *result = bldUtil->mkBinaryOp(code, lhs, rhs);
        return relExpr2(result);
    }
    else if (parserSet.follow(Productions::rel_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        Parser_Error(this, rel_expr2);
        return bldUtil->undef();
    }
}

OpCode Parser::shiftOp()
{
    if (check(TOKEN_LEFT_OP))
    {
        match();
        return OP_SHL;
    }
    else if (check(TOKEN_RIGHT_OP))
    {
        match();
        return OP_SHR;
    }
    else
    {
        Parser_Error(this, shift_op);
        return OP_ERR;
    }
}

RValue *Parser::shiftExpr()
{
    if (parserSet.first(Productions::add_expr, theNextToken.theType))
    {
        RValue *lhs = addExpr();
        return shiftExpr2(lhs);
    }
    else
    {
        Parser_Error(this, shift_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::shiftExpr2(RValue *lhs)
{
    OpCode code;
    if (parserSet.first(Productions::shift_op, theNextToken.theType))
    {
        code = shiftOp();
        RValue *rhs = addExpr();
        RValue *result = bldUtil->mkBinaryOp(code, lhs, rhs);
        return shiftExpr2(result);
    }
    else if (parserSet.follow(Productions::shift_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        Parser_Error(this, shift_expr2);
        return bldUtil->undef();
    }
}

OpCode Parser::addOp()
{
    if (check(TOKEN_ADD))
    {
        match();
        return OP_ADD;
    }
    else if (check(TOKEN_SUB))
    {
        match();
        return OP_SUB;
    }
    else
    {
        Parser_Error(this, add_op);
        return OP_ERR;
    }
}

RValue *Parser::addExpr()
{
    if (parserSet.first(Productions::mult_expr, theNextToken.theType))
    {
        RValue *lhs = multExpr();
        return addExpr2(lhs);
    }
    else
    {
        Parser_Error(this, add_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::addExpr2(RValue *lhs)
{
    OpCode code;
    if (parserSet.first(Productions::add_op, theNextToken.theType))
    {
        code = addOp();
        RValue *rhs = multExpr();
        RValue *result = bldUtil->mkBinaryOp(code, lhs, rhs);
        return addExpr2(result);
    }
    else if (parserSet.follow(Productions::add_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        Parser_Error(this, add_expr2);
        return bldUtil->undef();
    }
}

OpCode Parser::multOp()
{
    if (check(TOKEN_MUL))
    {
        match();
        return OP_MUL;
    }
    else if (check(TOKEN_DIV))
    {
        match();
        return OP_DIV;
    }
    else if (check(TOKEN_MOD))
    {
        match();
        return OP_MOD;
    }
    else
    {
        Parser_Error(this, mult_op);
        return OP_ERR;
    }
}

RValue *Parser::multExpr()
{
    if (parserSet.first(Productions::unary_expr, theNextToken.theType))
    {
        RValue *lhs = unaryExpr();
        return multExpr2(lhs);
    }
    else
    {
        Parser_Error(this, mult_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::multExpr2(RValue *lhs)
{
    OpCode code;
    if (parserSet.first(Productions::mult_op, theNextToken.theType))
    {
        code = multOp();
        RValue *rhs = unaryExpr();
        RValue *result = bldUtil->mkBinaryOp(code, lhs, rhs);
        return multExpr2(result);
    }
    else if (parserSet.follow(Productions::mult_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        Parser_Error(this, mult_expr2);
        return bldUtil->undef();
    }
}

RValue *Parser::unaryExpr()
{
    RValue *src;

    if (parserSet.first(Productions::postfix_expr, theNextToken.theType))
    {
        return postfixExpr();
    }
    else if (check(TOKEN_INC_OP) || check(TOKEN_DEC_OP))
    {
        OpCode op = check(TOKEN_INC_OP) ? OP_ADD : OP_SUB;
        match();
        src = unaryExpr();
        if (src->lvalue == NULL)
        {
            printf("Error: trying to increment or decrement a non-lvalue\n");
            // FIXME actually go through the error path
            return bldUtil->undef();
        }
        RValue *prefixed = bldUtil->mkBinaryOp(op, src, bldUtil->mkConstInt(1));
        bldUtil->writeVariable(src->lvalue, prefixed);
        src->lvalue = NULL;
        return prefixed;
    }
    else if (check(TOKEN_ADD))
    {
        match();
        return unaryExpr();
    }
    else if (check(TOKEN_SUB))
    {
        match();
        src = unaryExpr();
        return bldUtil->mkUnaryOp(OP_NEG, src);
    }
    else if (check(TOKEN_BOOLEAN_NOT))
    {
        match();
        src = unaryExpr();
        return bldUtil->mkUnaryOp(OP_BOOL_NOT, src);
    }
    else if (check(TOKEN_BITWISE_NOT))
    {
        match();
        src = unaryExpr();
        return bldUtil->mkUnaryOp(OP_BIT_NOT, src);
    }
    else
    {
        Parser_Error(this, unary_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::postfixExpr()
{
    if (parserSet.first(Productions::primary_expr, theNextToken.theType))
    {
        RValue *lhs = primaryExpr();
        return postfixExpr2(lhs);
    }
    else
    {
        Parser_Error(this, postfix_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::postfixExpr2(RValue *lhs)
{
    if (check(TOKEN_LPAREN)) // function call
    {
        if (lhs->lvalue == NULL)
        {
            printf("Error: Unexpected '('\n");
            // FIXME actually go through the error path
            return bldUtil->undef();
        }
        match();
        FunctionCall *call = bldUtil->startFunctionCall(lhs->lvalue);
        lhs->lvalue = NULL;
        argExprList(call);
        check(TOKEN_RPAREN);
        match();
        bldUtil->insertFunctionCall(call);

        return postfixExpr2(call->value());
    }
#if 0 // Structure field reference? WTF? We don't have structures!
    else if (check(TOKEN_FIELD))
    {
        //cache the token of the field source for assignment expressions.
        pInstruction = (Instruction *)List_Retrieve(pparser->pIList);
        pparser->theFieldToken = *(pInstruction->theToken);

        Parser_AddInstructionViaToken(pparser, FIELD, &(pparser->theNextToken), NULL );
        match();
        check(TOKEN_IDENTIFIER);
        Parser_AddInstructionViaToken(pparser, LOAD, &(pparser->theNextToken), NULL );
        match();
        postfixExpr2();
    }
#endif
    else if (check(TOKEN_INC_OP) || check(TOKEN_DEC_OP))
    {
        if (lhs->lvalue == NULL)
        {
            printf("Error: trying to increment or decrement a non-lvalue\n");
            // FIXME actually go through the error path
            return bldUtil->undef();
        }
        OpCode op = check(TOKEN_INC_OP) ? OP_ADD : OP_SUB;
        match();
        RValue *postfixed = bldUtil->mkBinaryOp(op, lhs, bldUtil->mkConstInt(1));
        bldUtil->writeVariable(lhs->lvalue, postfixed);
        lhs->lvalue = NULL;
        return postfixExpr2(lhs);
    }
    else if (parserSet.follow(Productions::postfix_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        fprintf(stderr, "error token '%s'\n", this->theNextToken.theSource);
        Parser_Error(this, postfix_expr2);
        return bldUtil->undef();
    }
}

void Parser::argExprList(FunctionCall *call)
{
    if (parserSet.first(Productions::assignment_expr, theNextToken.theType))
    {
        RValue *arg = assignmentExpr();
        call->appendOperand(arg);
        argExprList2(call);
    }
    else if (parserSet.follow(Productions::arg_expr_list, theNextToken.theType))
    {
        //No arguments, so push a zero argument count onto the stack
        // Parser_AddInstructionViaLabel(pparser, CONSTINT, "0", NULL );
    }
    else
    {
        Parser_Error(this, arg_expr_list);
    }
}

void Parser::argExprList2(FunctionCall *call)
{
    if (check(TOKEN_COMMA))
    {
        match();
        RValue *arg = assignmentExpr();
        call->appendOperand(arg);
        argExprList2(call);
    }
    else if (parserSet.follow(Productions::arg_expr_list2, theNextToken.theType))
    {
    }
    else
    {
        Parser_Error(this, arg_expr_list2);
    }
}

RValue *Parser::primaryExpr()
{
    if (check(TOKEN_IDENTIFIER))
    {
        RValue *value = bldUtil->readVariable(theNextToken.theSource);
        value->lvalue = ralloc_strdup(value, theNextToken.theSource);
        match();
        return value;
    }
    else if (parserSet.first(Productions::constant, theNextToken.theType))
    {
        return constant();
    }
    else if (check(TOKEN_LPAREN))
    {
        match();
        RValue *value = expr();
        check(TOKEN_RPAREN);
        match();
        return value;
    }
    else
    {
        Parser_Error(this, primary_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::constant()
{
    Constant *value;
    if (check(TOKEN_INTCONSTANT))
    {
        value = bldUtil->mkConstInt(strtol(theNextToken.theSource, NULL, 10));
        match();
        return value;
    }
    else if (check(TOKEN_HEXCONSTANT))
    {
        value = bldUtil->mkConstInt(strtol(theNextToken.theSource, NULL, 16));
        match();
        return value;
    }
    else if (check(TOKEN_FLOATCONSTANT))
    {
        value = bldUtil->mkConstFloat(atof(theNextToken.theSource));
        match();
        return value;
    }
    else if (check(TOKEN_STRING_LITERAL))
    {
        value = bldUtil->mkConstString(theNextToken.theSource);
        match();
        return value;
    }
    else
    {
        Parser_Error(this, constant);
        return bldUtil->undef();
    }
}

const char  *_production_error_message(Parser *pparser, PRODUCTION offender)
{
    switch(offender)
    {
    case Productions::constant:
        return "Invalid constant format";
    case Productions::start:
        return "Invalid start of declaration";
    case Productions::case_label:
        return "Case label must be an integer or string constant";
    case Productions::postfix_expr2:
        return "Invalid function call or expression";
    case Productions::funcDecl1:
        return "Parameters or ')' expected after function declaration";
    case Productions::external_decl:
        return "Invalid external declaration";
    case Productions::decl_spec:
        return "Invalid identifier";
    case Productions::decl:
        return "Invalid declaration(expected comma, semicolon or initializer?)";
    default:
        return "Unknown error";
    }
}


void Parser::error(PRODUCTION offender, const char *offenderStr)
{
    //Report the offending token to the error handler, along with the production
    //it offended in.
    if (offender != Productions::error)
        pp_error(&(theLexer.preprocessor), "%s '%s' (in production '%s')",
                 _production_error_message(this, offender), theNextToken.theSource, offenderStr);

    errorFound = true;

    if (offender == Productions::error)
        return;

    //The script is obviously not valid, but it's good to try and find all the
    //errors at one time.  Therefore go into Panic Mode error recovery -- keep
    //grabbing tokens until we find one we can use
    do
    {
        while (!SUCCEEDED(theLexer.getNextToken(&theNextToken)));
        if (theNextToken.theType == TOKEN_EOF)
        {
            break;
        }
    }
    while (!parserSet.follow(offender, theNextToken.theType));
}
