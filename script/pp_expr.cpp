#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "pp_parser.h"

/**
 * Parser and evaluator for the infix expressions that are used in
 * the #if and #elif preprocessor directives.
 */
class pp_expr {
private:
    pp_parser *parser;
    pp_token *token;

public:
    inline pp_expr(pp_parser *parser) : parser(parser) { match(); }
    inline CCResult eval(int *result) { return cond_expr(result); }

private:
    void match();
    bool check(PP_TOKEN_TYPE type);
    bool check_binary_op(int precedenceLevel);
    bool check_unary_op();
    int apply_binary_op(PP_TOKEN_TYPE op, int lhs, int rhs);
    CCResult expect_fail(const char *expected);

    CCResult cond_expr(int *result);
    CCResult cond_expr2(int *result);
    CCResult binary_expr(int *result, int precedenceLevel);
    CCResult binary_expr2(int *result, int precedenceLevel);
    CCResult unary_expr(int *result);
    CCResult primary_expr(int *result);
};

/**
 * @return the precedence from 1 (lowest) to 10 (highest) of a binary operator,
 *         or 0 if the operation is not binary
 */
static int binary_op_precedence(PP_TOKEN_TYPE op)
{
    switch(op)
    {
    case PP_TOKEN_MUL:
    case PP_TOKEN_DIV:
    case PP_TOKEN_MOD:
        return 10;
    case PP_TOKEN_ADD:
    case PP_TOKEN_SUB:
        return 9;
    case PP_TOKEN_LEFT_OP:
    case PP_TOKEN_RIGHT_OP:
        return 8;
    case PP_TOKEN_LT:
    case PP_TOKEN_GT:
    case PP_TOKEN_LE_OP:
    case PP_TOKEN_GE_OP:
        return 7;
    case PP_TOKEN_EQ_OP:
    case PP_TOKEN_NE_OP:
        return 6;
    case PP_TOKEN_BITWISE_AND:
        return 5;
    case PP_TOKEN_XOR:
        return 4;
    case PP_TOKEN_BITWISE_OR:
        return 3;
    case PP_TOKEN_AND_OP:
        return 2;
    case PP_TOKEN_OR_OP:
        return 1;
    default:
        return 0;
    }
}

int pp_expr::apply_binary_op(PP_TOKEN_TYPE op, int lhs, int rhs)
{
    switch(op)
    {
        case PP_TOKEN_MUL: return lhs * rhs;
        case PP_TOKEN_DIV: return lhs / rhs;
        case PP_TOKEN_MOD: return lhs % rhs;
        case PP_TOKEN_ADD: return lhs + rhs;
        case PP_TOKEN_SUB: return lhs - rhs;
        case PP_TOKEN_LEFT_OP: return lhs << rhs;
        case PP_TOKEN_RIGHT_OP: return ((unsigned int)lhs) >> rhs;
        case PP_TOKEN_LT: return lhs < rhs;
        case PP_TOKEN_GT: return lhs > rhs;
        case PP_TOKEN_LE_OP: return lhs <= rhs;
        case PP_TOKEN_GE_OP: return lhs >= rhs;
        case PP_TOKEN_EQ_OP: return lhs == rhs;
        case PP_TOKEN_NE_OP: return lhs != rhs;
        case PP_TOKEN_BITWISE_AND: return lhs & rhs;
        case PP_TOKEN_XOR: return lhs ^ rhs;
        case PP_TOKEN_BITWISE_OR: return lhs | rhs;
        case PP_TOKEN_AND_OP: return lhs && rhs;
        case PP_TOKEN_OR_OP: return lhs || rhs;
        default:
            assert(!"invalid token type for binary operator");
            return 0;
    }
    
}

static int applyUnaryOp(PP_TOKEN_TYPE op, int src)
{
    switch(op)
    {
        case PP_TOKEN_ADD: return src;
        case PP_TOKEN_SUB: return -src;
        case PP_TOKEN_BOOLEAN_NOT: return !src;
        case PP_TOKEN_BITWISE_NOT: return ~src;
        default:
            assert(!"invalid token type for unary operator");
            return 0;
    }
}

void pp_expr::match()
{
    do {
        token = parser->emitToken();
    } while(token->theType == PP_TOKEN_WHITESPACE);
}

bool pp_expr::check(PP_TOKEN_TYPE type)
{
    return (token->theType == type);
}

bool pp_expr::check_binary_op(int precedenceLevel)
{
    return (binary_op_precedence(token->theType) == precedenceLevel);
}

bool pp_expr::check_unary_op()
{
    return (token->theType == PP_TOKEN_ADD ||
            token->theType == PP_TOKEN_SUB ||
            token->theType == PP_TOKEN_BOOLEAN_NOT ||
            token->theType == PP_TOKEN_BITWISE_NOT);
}

CCResult pp_expr::cond_expr(int *result)
{
    if (FAILED(binary_expr(result, 1)))
        return CC_FAIL;
    if (check(PP_TOKEN_CONDITIONAL))
        return cond_expr2(result);
    return CC_OK;
}

CCResult pp_expr::cond_expr2(int *result)
{
    int resultTrue, resultFalse;
    match(); // PP_TOKEN_CONDITIONAL
    if (FAILED(cond_expr(&resultTrue)))
        return CC_FAIL;
    if (!check(PP_TOKEN_COLON))
        return expect_fail("':'");
    match();
    if (FAILED(cond_expr(&resultFalse)))
        return CC_FAIL;
    *result = *result ? resultTrue : resultFalse;
    return CC_OK;
}

CCResult pp_expr::binary_expr(int *result, int precedenceLevel)
{
    if (precedenceLevel == 10)
    {
        if (FAILED(unary_expr(result)))
            return CC_FAIL;
    }
    else if (FAILED(binary_expr(result, precedenceLevel+1)))
        return CC_FAIL;
    return binary_expr2(result, precedenceLevel);
}

CCResult pp_expr::binary_expr2(int *result, int precedenceLevel)
{
    int rhs;
    if (!check_binary_op(precedenceLevel)) return CC_OK;
    PP_TOKEN_TYPE op = token->theType;
    match();
    if (precedenceLevel == 10)
    {
        if (FAILED(unary_expr(&rhs)))
            return CC_FAIL;
    }
    else if (FAILED(binary_expr(&rhs, precedenceLevel+1)))
        return CC_FAIL;
    *result = apply_binary_op(op, *result, rhs);
    return binary_expr2(result, precedenceLevel);
}

CCResult pp_expr::unary_expr(int *result)
{
    if (check_unary_op())
    {
        PP_TOKEN_TYPE op = token->theType;
        match();
        if (FAILED(unary_expr(result))) return CC_FAIL;
        *result = applyUnaryOp(op, *result);
        return CC_OK;
    }
    else return primary_expr(result);
}

CCResult pp_expr::primary_expr(int *result)
{
    if (check(PP_TOKEN_IDENTIFIER) && !strcmp(parser->token.theSource, "defined"))
    {
        parser->lexToken(true);
        if (parser->token.theType == PP_TOKEN_LPAREN)
        {
            parser->lexToken(true);
            if (parser->token.theType != PP_TOKEN_IDENTIFIER) return expect_fail("<identifier>");
            *result = parser->isDefined(token->theSource);
            parser->lexToken(true);
            if (parser->token.theType != PP_TOKEN_RPAREN) return expect_fail("')'");
            match();
        }
        else
        {
            if (parser->token.theType != PP_TOKEN_IDENTIFIER) return expect_fail("<identifier>");
            *result = parser->isDefined(token->theSource);
            match();
        }
        return CC_OK;
    }
    else if (check(PP_TOKEN_LPAREN))
    {
        match();
        if (FAILED(cond_expr(result))) return CC_FAIL;
        if (!check(PP_TOKEN_RPAREN)) return expect_fail("')'");
        match();
        return CC_OK;
    }
    else if (check(PP_TOKEN_IDENTIFIER))
    {
        // undefined identifiers evaluate to 0
        match();
        *result = 0;
        return CC_OK;
    }
    else if (check(PP_TOKEN_INTCONSTANT))
    {
        if(token->theSource[0] == '0') // octal
            *result = strtol(token->theSource, NULL, 8);
        else
            *result = strtol(token->theSource, NULL, 10);
        match();
        return CC_OK;
    }
    else if (check(PP_TOKEN_HEXCONSTANT))
    {
        assert(!strncmp(token->theSource, "0x", 2) ||
               !strncmp(token->theSource, "0X", 2));
        *result = strtol(token->theSource + 2, NULL, 16);
        match();
        return CC_OK;
    }
    else
    {
        pp_error(parser, "unexpected token '%s'", token->theSource);
        return CC_FAIL;
    }
}

CCResult pp_expr::expect_fail(const char *expected)
{
    pp_error(parser, "expected %s", expected);
    return CC_FAIL;
}

extern "C" int pp_expr_eval_expression(pp_parser *parser)
{
    pp_expr expr(parser);
    int result;
    if (FAILED(expr.eval(&result)))
        return 0;
    return result;
}

