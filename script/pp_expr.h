#ifndef PP_EXPR_H
#define PP_EXPR_H

#include <stdbool.h>
#include "pp_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

int pp_expr_eval_expression(pp_parser *parser);

#ifdef __cplusplus
};
#endif

#endif

