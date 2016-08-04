#ifndef SCRIPT_UTILS_H
#define SCRIPT_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

// prints a string with quotes added around it and characters like '\n' escaped
void printEscapedString(const char *string);

#ifdef __cplusplus
};
#endif

#endif // !defined SCRIPT_UTILS_H

