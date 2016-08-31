#ifndef SCRIPT_UTILS_H
#define SCRIPT_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

// prints a string with quotes added around it and characters like '\n' escaped
void printEscapedString(const char *string);

// optimized search in an arranged string table, return the index
int searchList(const char *list[], const char *value, int length);

#ifdef __cplusplus
};
#endif

#endif // !defined SCRIPT_UTILS_H

