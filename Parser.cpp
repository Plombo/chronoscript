/*
 * OpenBOR - http://www.chronocrash.com
 * -----------------------------------------------------------------------
 * All rights reserved, see LICENSE in OpenBOR root for details.
 *
 * Copyright (c) 2004 - 2013 OpenBOR Team
 */

#include "Parser.hpp"
#include "Builtins.hpp"

#define errorWithMessage(pr, ...) {                  \
    pp_error(&(theLexer.preprocessor), __VA_ARGS__); \
    handleError(Productions::pr);                    \
}

#define checkAndMatchOrErrorReturn(token_type, pr, ret_expr)    \
    if (check(token_type)) match();                             \
    else { errorWithMessage(pr, "expected '%s'", tokenName(token_type)); return ret_expr; }

#define checkAndMatchOrError(token_type, pr) checkAndMatchOrErrorReturn(token_type, pr, )

static const char *tokenName(MY_TOKEN_TYPE type);
static bool isKeyword(MY_TOKEN_TYPE type);

Parser::Parser(pp_context *pcontext, ExecBuilder *builder, char *scriptText,
               int startingLineNumber, const char *path)
 : theLexer(pcontext, path, scriptText, {startingLineNumber, 1})
{
    memCtx = ralloc_context(NULL);

    labelCount = 0;
    theNextToken.theType = END_OF_TOKENS;
    rewound = false;
    bld = NULL;
    bldUtil = NULL;
    initBld = NULL;
    initBldUtil = NULL;
    execBuilder = builder;
    errorCount = 0;
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
    else if (CC_FAIL == theLexer.getNextToken(&theNextToken))
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

/* Sets current function to "@init", creating the "@init" function if it
   doesn't exist yet */
void Parser::switchToInitFunction()
{
    if (!initBld)
    {
        initBld = new(memCtx) SSABuilder(memCtx, "@init");
        initBldUtil = new(initBld) SSABuildUtil(initBld, &execBuilder->globals);
        BasicBlock *startBlock = initBldUtil->createBBAfter(NULL);
        initBld->sealBlock(startBlock);
        initBldUtil->setCurrentBlock(startBlock);
        initBldUtil->pushScope();
    }

    bld = initBld;
    bldUtil = initBldUtil;
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

    if (!check(TOKEN_IDENTIFIER))
    {
        if (isKeyword(token.theType))
        {
            errorWithMessage(external_decl,
                "'%s' is a keyword, so it can't be used as a variable or function name",
                token.theSource);
        }
        else
        {
            errorWithMessage(external_decl,
                "variable or function name (identifier) expected before '%s'",
                token.theSource);
        }
        return;
    }
    match();

    // global variable declaration
    //type a =
    if (parserSet.first(Productions::initializer, theNextToken.theType))
    {
        switchToInitFunction();

        // get the initializer; type a = expression
        RValue *initialValue = initializer();
        if (!initialValue->isConstant())
        {
            // TODO: also make sure @init function stays trivial, so no "global2 = global1 = 3"
            errorWithMessage(external_decl,
                "initial value of global variable must be a constant");
            return;
        }

        // end of declaration list
        if (check(TOKEN_SEMICOLON) || check(TOKEN_COMMA))
        {
            // declare the variable
            if (execBuilder->globals.globalVariableExists(token.theSource))
            {
                errorWithMessage(external_decl,
                    "there is already a global variable named '%s'", token.theSource);
                return;
            }
            else
            {
                execBuilder->globals.declareGlobalVariable(token.theSource,
                    initialValue->asConstant()->constValue);
            }

            if (check(TOKEN_COMMA))
            {
                // there's a comma instead of semicolon, so there should be another identifier
                match();
                externalDecl2(true);
            }
            else
            {
                // otherwise it's a semicolon, so this is the end of the declaration list
                match();
            }
        }
        else
        {
            match();
            printf("Semicolon or comma expected before '%s'\n", theNextToken.theSource);
            Parser_Error(this, external_decl );
        }
    }
    // semicolon, end expression.
    else if (check(TOKEN_SEMICOLON))
    {
        // declare the variable
        if (execBuilder->globals.globalVariableExists(token.theSource))
        {
            errorWithMessage(external_decl,
                "there is already a global variable named '%s'", token.theSource);
            return;
        }
        else
        {
            execBuilder->globals.declareGlobalVariable(token.theSource, {{.ptrVal = NULL}, .vt = VT_EMPTY});
        }
        match();
    }
    // still comma? there should be another identifier so declare the variable and go for the next
    else if (check(TOKEN_COMMA))
    {
        if (execBuilder->globals.globalVariableExists(token.theSource))
        {
            errorWithMessage(external_decl,
                "there is already a global variable named '%s'", token.theSource);
            return;
        }
        else
        {
            execBuilder->globals.declareGlobalVariable(token.theSource, {{.ptrVal = NULL}, .vt = VT_EMPTY});
        }
        match();
        externalDecl2(true);
    }

    // not a comma, semicolon, or initializer, so must be a function declaration
    else if (!variableonly && parserSet.first(Productions::funcDecl, theNextToken.theType))
    {
        bld = new(memCtx) SSABuilder(memCtx, token.theSource);
        bldUtil = new(bld) SSABuildUtil(bld, &execBuilder->globals);
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
    if (!check(TOKEN_IDENTIFIER))
    {
        if (isKeyword(token.theType))
        {
            errorWithMessage(decl,
                "'%s' is a keyword, so it can't be used as a variable name",
                token.theSource);
        }
        else
        {
            errorWithMessage(decl,
                "variable name (identifier) expected before '%s'",
                token.theSource);
        }
        return;
    }

    // it's an error if there's already a variable, global or local, with the same name
    if (execBuilder->globals.globalVariableExists(token.theSource))
    {
        errorWithMessage(decl, "there is already a global variable named '%s'", token.theSource);
        return;
    }
    else if (!bldUtil->declareVariable(token.theSource))
    {
        errorWithMessage(decl, "there is already a variable named '%s'", token.theSource);
        return;
    }

    match();

    // =
    if (parserSet.first(Productions::initializer, theNextToken.theType))
    {
        //Get the initializer;
        RValue *initialValue = initializer();
        if (check(TOKEN_SEMICOLON))
        {
            match();
            //Save the initializer
            if (initialValue)
            {
                bldUtil->writeVariable(token.theSource, initialValue);
            }
        }
        else if (check(TOKEN_COMMA))
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
            errorWithMessage(decl, "expected ';' or ',' before '%s'", theNextToken.theSource);
        }
    }
    // ,
    else if (check(TOKEN_COMMA))
    {
        match();
        decl2();
    }
    // ;
    else if (check(TOKEN_SEMICOLON))
    {
        match();
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
        checkAndMatchOrError(TOKEN_RPAREN, funcDecl1);
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
    return bldUtil->undef();
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
        else if (isKeyword(theNextToken.theType))
        {
            errorWithMessage(parm_decl,
                "'%s' is a keyword, so it can't be used as a parameter name",
                theNextToken.theSource);
        }
        else
        {
            errorWithMessage(parm_decl,
                "expected parameter name (identifier) before '%s'",
                theNextToken.theSource);
        }
    }
    else
    {
        Parser_Error(this, parm_decl);
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

/******************************************************************************
*  Statement evaluation -- These methods translate statements into
*  appropriate instructions.
******************************************************************************/
void Parser::stmtList()
{
    // Use iteration instead of recursion here to guard against stack overflows.
    while (true)
    {
        if (parserSet.first(Productions::stmt, theNextToken.theType))
        {
            stmt();
        }
        else if (parserSet.first(Productions::decl, theNextToken.theType))
        {
            decl();
        }
        else
        {
            break;
        }
    }

    if (parserSet.follow(Productions::stmt_list, theNextToken.theType)) {}
    else
    {
        Parser_Error(this, stmt_list);
    }
}

void Parser::stmt()
{
    if (parserSet.first(Productions::comp_stmt, theNextToken.theType))
    {
        compStmt();
    }
    else if (parserSet.first(Productions::expr_stmt, theNextToken.theType))
    {
        exprStmt();
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
    else if (check(TOKEN_SEMICOLON))
    {
        // empty statement
        match();
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
        checkAndMatchOrError(TOKEN_SEMICOLON, expr_stmt);
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
        stmtList();
        bldUtil->popScope();
        checkAndMatchOrError(TOKEN_RCURLY, comp_stmt);
    }
    else
    {
        Parser_Error(this, comp_stmt);
    }
}

void Parser::selectStmt()
{
    if (check(TOKEN_IF))
    {
        BasicBlock *afterIfBlock = bldUtil->createBBAfter(bldUtil->currentBlock);

        /* For long if->else if->else if patterns like:
               if (x == 0) { ... }
               else if (x == 1) { ... }
               else if (x == 2) { ... }
               ...
               else if (x == 99) { ... }
               else { ... }
           Use a loop to handle all of the "else if" without making 100 recursive
           calls to this function. We also create fewer basic blocks this way. */
        do
        {
            assert(check(TOKEN_IF));
            match(); // TOKEN_IF

            // condition
            checkAndMatchOrError(TOKEN_LPAREN, select_stmt);
            RValue *condition = expr();
            checkAndMatchOrError(TOKEN_RPAREN, select_stmt);
            Jump *skipIfJump = bldUtil->mkJump(OP_BRANCH_FALSE, NULL, condition);

            // body
            BasicBlock *conditionBlock = bldUtil->currentBlock,
                       *ifBlock = bldUtil->createBBAfter(conditionBlock);
            ifBlock->addPred(conditionBlock);
            bld->sealBlock(ifBlock);
            bldUtil->setCurrentBlock(ifBlock);
            stmt();

            if (check(TOKEN_ELSE)) // if-else
            {
                // if the condition was true, jump over the else body
                if (!bldUtil->currentBlock->endsWithJump())
                {
                    bldUtil->mkJump(OP_JMP, afterIfBlock, NULL);
                    afterIfBlock->addPred(bldUtil->currentBlock);
                }

                match(); // TOKEN_ELSE
                BasicBlock *elseBlock = bldUtil->createBBAfter(bldUtil->currentBlock);
                elseBlock->addPred(conditionBlock);
                skipIfJump->target = elseBlock;
                bld->sealBlock(elseBlock);
                bldUtil->setCurrentBlock(elseBlock);

                if (check(TOKEN_IF)) // "else if"
                {
                    continue;
                }
                else // "else" that is not an "else if"
                {
                    stmt();
                    if (!bldUtil->currentBlock->endsWithJump())
                        afterIfBlock->addPred(bldUtil->currentBlock);
                    break;
                }
            }
            else // standalone if
            {
                if (!bldUtil->currentBlock->endsWithJump())
                    afterIfBlock->addPred(bldUtil->currentBlock);
                afterIfBlock->addPred(conditionBlock);
                skipIfJump->target = afterIfBlock;
                break;
            }
        } while (true);

        bld->sealBlock(afterIfBlock);
        bldUtil->setCurrentBlock(afterIfBlock);
    }
    else if (check(TOKEN_SWITCH))
    {
        match();
        checkAndMatchOrError(TOKEN_LPAREN, select_stmt);
        RValue *baseVal = expr();
        checkAndMatchOrError(TOKEN_RPAREN, select_stmt);
        checkAndMatchOrError(TOKEN_LCURLY, select_stmt);

        //Parse body of switch, and consume the closing brace
        bldUtil->pushScope();
        switchBody(baseVal);
        bldUtil->popScope();
        checkAndMatchOrError(TOKEN_RCURLY, select_stmt);
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
               *defaultTarget = NULL,
               *lastCaseStartBlock = NULL;
    jumps->addPred(bldUtil->currentBlock);
    bld->sealBlock(jumps);
    body->addPred(jumps);
    bld->sealBlock(body);
    bldUtil->breakTargets.push(after);
    bldUtil->currentBlock = body;

    //Using a loop here instead of recursion goes against the idea of a
    //recursive descent parser, but it keeps us from having 200 stack frames.
    while (true)
    {
        if (parserSet.first(Productions::case_label, theNextToken.theType))
        {
            bool isNonTrivialFallthrough = false;
            body = bldUtil->currentBlock;

            if (body->isEmpty())
            {
                bool jumpsBlockIsPred = false;

                // add jumps block as a predecessor if it isn't already
                foreach_list(body->preds, BasicBlock*, iter)
                {
                    if (iter.value() == jumps)
                    {
                        jumpsBlockIsPred = true;
                    }
                    else
                    {
                        isNonTrivialFallthrough = true;
                    }
                }

                if (!jumpsBlockIsPred)
                {
                    body->addPred(jumps);
                }
            }
            else
            {
                BasicBlock *newBody = bldUtil->createBBAfter(bldUtil->currentBlock);
                newBody->addPred(jumps);
                if (!bldUtil->currentBlock->endsWithJump())
                {
                    newBody->addPred(bldUtil->currentBlock);
                    isNonTrivialFallthrough = true;
                }
                bld->sealBlock(newBody);
                body = newBody;
            }

            // Print a warning if it looks like the preceding case is missing a "break" by accident.
            if (isNonTrivialFallthrough)
            {
                // Silence the warning if the case label is preceded by a "fall through" comment, or if it is the
                // second or later of consecutive case labels (meaning the warning has already been either printed
                // or silenced).
                if (!theNextToken.fallthrough && body != lastCaseStartBlock)
                {
                    pp_warning(&(theLexer.preprocessor), "non-trivial case fallthrough (you can silence this with a "
                        "'break;' statement or a comment including the words \"fall\" and \"through\")");
                }
            }

            lastCaseStartBlock = body;

            if (check(TOKEN_CASE))
            {
                match();
                bldUtil->currentBlock = jumps;
                RValue *caseVal = constant();
                bldUtil->mkJump(OP_BRANCH_EQUAL, body, baseVal, caseVal);
                checkAndMatchOrError(TOKEN_COLON, switch_body);
            }
            else
            {
                assert(check(TOKEN_DEFAULT));
                match();
                checkAndMatchOrError(TOKEN_COLON, switch_body);
                defaultTarget = body;
            }
            bldUtil->currentBlock = body;
        }
        else if (parserSet.first(Productions::stmt_list, theNextToken.theType))
        {
            stmtList();
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

    // If there's no "default" case, create an empty basic block for it so SSABuilder can put phi moves there
    // instead of in the jumps block, where they would cause problems with register allocation.
    if (defaultTarget == NULL)
    {
        if (!bldUtil->currentBlock->isEmpty())
        {
            BasicBlock *newBody = bldUtil->createBBAfter(bldUtil->currentBlock);
            newBody->addPred(jumps);
            if (!bldUtil->currentBlock->endsWithJump())
                newBody->addPred(bldUtil->currentBlock);
            bld->sealBlock(newBody);
            defaultTarget = newBody;
            bldUtil->currentBlock = newBody;
        }
    }

    // implicit jump from end of switch body to after switch
    if (!bldUtil->currentBlock->endsWithJump())
        after->addPred(bldUtil->currentBlock);

    // set default target
    bldUtil->currentBlock = jumps;
    bldUtil->mkJump(OP_JMP, defaultTarget, NULL);
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
        checkAndMatchOrError(TOKEN_LPAREN, iter_stmt);
        RValue *condition = expr();
        checkAndMatchOrError(TOKEN_RPAREN, iter_stmt);
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
        checkAndMatchOrError(TOKEN_WHILE, iter_stmt);
        checkAndMatchOrError(TOKEN_LPAREN, iter_stmt);
        RValue *condition = expr();
        checkAndMatchOrError(TOKEN_RPAREN, iter_stmt);
        bldUtil->mkJump(OP_BRANCH_TRUE, bodyStart, condition);
        checkAndMatchOrError(TOKEN_SEMICOLON, iter_stmt);
        
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
        checkAndMatchOrError(TOKEN_LPAREN, iter_stmt);

        // create a scope for declarations in the loop header
        bldUtil->pushScope();

        //Add any initializer code
        optExprStmt();
        header->addPred(before);

        //Build the conditional statement
        bldUtil->currentBlock = header;
        bldUtil->pushLoop(loop);
        RValue *condition = optExpr();
        if (condition)
            bldUtil->mkJump(OP_BRANCH_FALSE, after, condition);
        checkAndMatchOrError(TOKEN_SEMICOLON, iter_stmt);
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
        checkAndMatchOrError(TOKEN_RPAREN, iter_stmt);

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
        bldUtil->popScope();
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
        if (bldUtil->breakTargets.isEmpty())
        {
            errorWithMessage(jump_stmt, "'break' outside of a loop or switch");
            return;
        }

        match();
        checkAndMatchOrError(TOKEN_SEMICOLON, jump_stmt);

        jumpTarget = bldUtil->breakTargets.top();
        bldUtil->mkJump(OP_JMP, jumpTarget, NULL);
        jumpTarget->addPred(bldUtil->currentBlock);
    }
    else if (check(TOKEN_CONTINUE))
    {
        if (bldUtil->continueTargets.isEmpty())
        {
            errorWithMessage(jump_stmt, "'continue' outside of a loop");
            return;
        }

        match();
        checkAndMatchOrError(TOKEN_SEMICOLON, jump_stmt);

        jumpTarget = bldUtil->continueTargets.top();
        bldUtil->mkJump(OP_JMP, jumpTarget, NULL);
        jumpTarget->addPred(bldUtil->currentBlock);
    }
    else if (check(TOKEN_RETURN))
    {
        match();
        RValue *value = optExpr();
        checkAndMatchOrError(TOKEN_SEMICOLON, jump_stmt);

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

RValue *Parser::commaExpr()
{
    if (parserSet.first(Productions::assignment_expr, theNextToken.theType))
    {
        RValue *lhs = assignmentExpr();
        return commaExpr2(lhs);
    }
    else
    {
        Parser_Error(this, comma_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::commaExpr2(RValue *lhs)
{
    if (check(TOKEN_COMMA))
    {
        match();
        RValue *rhs = assignmentExpr();
        return commaExpr2(rhs);
    }
    else if (parserSet.follow(Productions::comma_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        Parser_Error(this, comma_expr2);
        return bldUtil->undef();
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
        return OP_REM;
    }
    else
    {
        Parser_Error(this, assignment_op);
        return OP_NOOP;
    }
}

RValue *Parser::assignmentExpr()
{
    if (parserSet.first(Productions::cond_expr, theNextToken.theType))
    {
        RValue *lhs = condExpr();
        return assignmentExpr2(lhs);
    }
    else
    {
        Parser_Error(this, assignment_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::assignmentExpr2(RValue *lhs)
{
    if (parserSet.first(Productions::assignment_op, theNextToken.theType))
    {
        LValue *target = lhs->lvalue;
        if (!target)
        {
            printf("Error: trying to assign to something unassignable\n");
            Parser_Error(this, assignment_expr2);
            return lhs;
        }
        if (target->varName) printf("assign to %s\n", target->varName);

        OpCode op = assignmentOp();
        /* yes, assignmentExpr, not assignmentExpr2, because chained assignments
           are evaluated right to left, unlike the other operators */
        RValue *rhs = assignmentExpr(), *result;
        if (op == OP_NOOP) // regular assignment (=)
        {
            result = rhs;
        }
        else // operation and assignment (+=, >>=, etc.)
        {
            result = bldUtil->mkBinaryOp(op, lhs, rhs);
        }
        bldUtil->mkAssignment(target, result);
        return result;
    }
    else if (parserSet.follow(Productions::assignment_expr2, theNextToken.theType))
    {
        return lhs;
    }
    else
    {
        Parser_Error(this, assignment_expr2);
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
    snprintf(varName, sizeof(varName), "?:%i", labelCount++);
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

        checkAndMatchOrErrorReturn(TOKEN_COLON, cond_expr2, bldUtil->undef());

        // evaluate other expression
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
        snprintf(varName, sizeof(varName), "||%i", labelCount++);
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
        snprintf(varName, sizeof(varName), "&&%i", labelCount++);
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
        return OP_REM;
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
    if (parserSet.first(Productions::postfix_expr, theNextToken.theType))
    {
        return postfixExpr();
    }
    else if (check(TOKEN_INC_OP) || check(TOKEN_DEC_OP))
    {
        OpCode op = check(TOKEN_INC_OP) ? OP_INC : OP_DEC;
        match();
        RValue *src = postfixExpr();
        if (!src->lvalue)
        {
            errorWithMessage(unary_expr, "cannot increment or decrement an unassignable value");
            return bldUtil->undef();
        }
        RValue *prefixed = bldUtil->mkUnaryOp(op, src);
        bldUtil->mkAssignment(src->lvalue, prefixed);
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
        return bldUtil->mkUnaryOp(OP_NEG, unaryExpr());
    }
    else if (check(TOKEN_BOOLEAN_NOT))
    {
        match();
        return bldUtil->mkUnaryOp(OP_BOOL_NOT, unaryExpr());
    }
    else if (check(TOKEN_BITWISE_NOT))
    {
        match();
        return bldUtil->mkUnaryOp(OP_BIT_NOT, unaryExpr());
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

RValue *Parser::postfixExpr2(RValue *src)
{
    if (check(TOKEN_LPAREN)) // function call
    {
        if (!src->lvalue || !src->lvalue->varName)
        {
            errorWithMessage(postfix_expr2, "unexpected '('");
            return bldUtil->undef();
        }
        match();
        FunctionCall *call = bldUtil->startFunctionCall(src->lvalue->varName);
        argExprList(call);
        checkAndMatchOrErrorReturn(TOKEN_RPAREN, postfix_expr2, bldUtil->undef());
        bldUtil->insertFunctionCall(call);

        return postfixExpr2(call->value());
    }
    else if (check(TOKEN_FIELD))
    {
        match();
        if (!check(TOKEN_IDENTIFIER))
        {
            errorWithMessage(postfix_expr2, "expected identifier after '.'");
            return src;
        }
        RValue *fieldName = bldUtil->mkConstString(theNextToken.theSource);
        match();
        if (check(TOKEN_LPAREN)) // method call
        {
            match();

            const char *methodName = StrCache_Get(fieldName->asConstant()->constValue.strVal);
            int methodIndex = getMethodIndex(methodName);
            if (methodIndex == -1)
            {
                errorWithMessage(postfix_expr2, "there is no method called '%s'", methodName);
                return bldUtil->undef();
            }

            FunctionCall *call = bldUtil->startMethodCall(methodIndex, src);
            argExprList(call);
            checkAndMatchOrErrorReturn(TOKEN_RPAREN, postfix_expr2, bldUtil->undef());
            bldUtil->insertFunctionCall(call);
            return postfixExpr2(call->value());
        }
        else // get field value
        {
            return postfixExpr2(bldUtil->mkGet(src, fieldName));
        }
    }
    else if (check(TOKEN_LBRACKET))
    {
        match();
        RValue *fieldName = expr();
        checkAndMatchOrErrorReturn(TOKEN_RBRACKET, postfix_expr2, bldUtil->undef());
        return postfixExpr2(bldUtil->mkGet(src, fieldName));
    }
    else if (check(TOKEN_INC_OP) || check(TOKEN_DEC_OP))
    {
        if (!src->lvalue)
        {
            errorWithMessage(postfix_expr2, "cannot increment or decrement an unassignable value");
            return bldUtil->undef();
        }
        OpCode op = check(TOKEN_INC_OP) ? OP_INC : OP_DEC;
        match();
        RValue *postfixed = bldUtil->mkUnaryOp(op, src);
        bldUtil->mkAssignment(src->lvalue, postfixed);
        src->lvalue = NULL;
        // return the value from before the increment/decrement
        return postfixExpr2(src);
    }
    else if (parserSet.follow(Productions::postfix_expr2, theNextToken.theType))
    {
        return src;
    }
    else
    {
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
        match();
        return value;
    }
    else if (parserSet.first(Productions::object, theNextToken.theType))
    {
        return object();
    }
    else if (parserSet.first(Productions::list_initializer, theNextToken.theType))
    {
        return list();
    }
    else if (parserSet.first(Productions::constant, theNextToken.theType))
    {
        return constant();
    }
    else if (check(TOKEN_LPAREN))
    {
        match();
        RValue *value = expr();
        checkAndMatchOrErrorReturn(TOKEN_RPAREN, primary_expr, bldUtil->undef());
        return value;
    }
    else
    {
        Parser_Error(this, primary_expr);
        return bldUtil->undef();
    }
}

RValue *Parser::object()
{
    int32_t numMembers = 0;
    match(); // TOKEN_LCURLY

    RValue *object = bldUtil->mkObject();

    /* Let's implement these productions iteratively, not recursively, to avoid stack overflow:
        kv_list: kv_pair kv_list2 | EPSILON
        kv_list2: COMMA kv_pair kv_list2 | COMMA | EPSILON
        kv_pair: expr COLON expr
    */
    while (parserSet.first(Productions::expr, theNextToken.theType))
    {
        /* We could, in theory, use expr() instead of assignmentExpr() here. But it's
           hard to imagine a valid use for a comma operator in a key, and easier to
           imagine a comma being in one by mistake. So limit keys to assignment
           expressions for now. */
        RValue *key = assignmentExpr();
        checkAndMatchOrErrorReturn(TOKEN_COLON, kv_pair, bldUtil->undef());
        RValue *value = assignmentExpr();
        bldUtil->mkSet(object, key, value);
        ++numMembers;
        if (check(TOKEN_COMMA))
        {
            match();
        }
        else break;
    }

    // the initial number of members in the object is the parameter to the MKOBJECT instruction
    object->asTemporary()->expr->setSrc(0, bldUtil->mkConstInt(numMembers));

    checkAndMatchOrErrorReturn(TOKEN_RCURLY, object, bldUtil->undef());
    return object;
}

RValue *Parser::list()
{
    int32_t numElements = 0;
    match(); // TOKEN_LBRACKET

    RValue *list = bldUtil->mkList();

    // Let's implement the list production iteratively, not recursively, to avoid stack overflow
    while (parserSet.first(Productions::expr, theNextToken.theType))
    {
        RValue *value = assignmentExpr();
        bldUtil->mkSet(list, bldUtil->mkConstInt(numElements), value);
        ++numElements;
        if (check(TOKEN_COMMA))
        {
            match();
        }
        else break;
    }

    // the initial number of elements in the list is the parameter to the MKLIST instruction
    list->asTemporary()->expr->setSrc(0, bldUtil->mkConstInt(numElements));

    checkAndMatchOrErrorReturn(TOKEN_RBRACKET, list_initializer, bldUtil->undef());
    return list;
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
    else if (check(TOKEN_NULL))
    {
        value = bldUtil->mkNull();
        match();
        return value;
    }
    else
    {
        Parser_Error(this, constant);
        return bldUtil->undef();
    }
}

const char  *production_error_message(PRODUCTION offender)
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

void Parser::handleError(PRODUCTION offender)
{
    errorCount++;

    if (offender == Productions::error)
        return;

    //The script is obviously not valid, but it's good to try and find all the
    //errors at one time.  Therefore go into Panic Mode error recovery -- keep
    //grabbing tokens until we find one we can use
    do
    {
        while (theLexer.getNextToken(&theNextToken) != CC_OK);
        if (theNextToken.theType == TOKEN_EOF)
        {
            break;
        }
    }
    while (!parserSet.follow(offender, theNextToken.theType));
}

void Parser::errorDefault(PRODUCTION offender, const char *offenderStr)
{
    //Report the offending token to the error handler, along with the production
    //it offended in.
    if (offender != Productions::error)
        pp_error(&(theLexer.preprocessor), "%s '%s' (in production '%s')",
                 production_error_message(offender), theNextToken.theSource, offenderStr);

    handleError(offender);
}

static const char *tokenName(MY_TOKEN_TYPE type)
{
    switch (type)
    {
        case TOKEN_LPAREN:    return "(";
        case TOKEN_RPAREN:    return ")";
        case TOKEN_LCURLY:    return "{";
        case TOKEN_RCURLY:    return "}";
        case TOKEN_LBRACKET:  return "[";
        case TOKEN_RBRACKET:  return "]";
        case TOKEN_COLON:     return ":";
        case TOKEN_SEMICOLON: return ";";
        case TOKEN_WHILE:     return "while";
        case TOKEN_DEFAULT:   return "default";
        default:              return "(unknown token)";
    }
}

// return true if this is a keyword that looks like an identifier
static bool isKeyword(MY_TOKEN_TYPE type)
{
    switch (type)
    {
        case TOKEN_TYPEDEF:
        case TOKEN_EXTERN:
        case TOKEN_STATIC:
        case TOKEN_AUTO:
        case TOKEN_REGISTER:
        case TOKEN_CHAR:
        case TOKEN_SHORT:
        case TOKEN_INT:
        case TOKEN_LONG:
        case TOKEN_SIGNED:
        case TOKEN_UNSIGNED:
        case TOKEN_FLOAT:
        case TOKEN_DOUBLE:
        case TOKEN_CONST:
        case TOKEN_VOLATILE:
        case TOKEN_VOID:
        case TOKEN_STRUCT:
        case TOKEN_UNION:
        case TOKEN_ENUM:
        case TOKEN_CASE:
        case TOKEN_DEFAULT:
        case TOKEN_IF:
        case TOKEN_ELSE:
        case TOKEN_SWITCH:
        case TOKEN_WHILE:
        case TOKEN_DO:
        case TOKEN_FOR:
        case TOKEN_GOTO:
        case TOKEN_CONTINUE:
        case TOKEN_BREAK:
        case TOKEN_RETURN:
        case TOKEN_NULL:
            return true;
        default:
            return false;
    }
}

