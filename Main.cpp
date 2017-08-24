// Compiles and runs a script from a file.
#include <stdio.h>
#include <errno.h>
#include "pp_parser.h"
#include "Parser.h"
#include "List.h"
#include "regalloc.h"
#include "liveness.h"
#include "ExecBuilder.h"
#include "Builtins.h"

#include "ImportCache.h"
#include "StrCache.h"
#include "ObjectHeap.h"

void doTest(const char *filename)
{
    Interpreter *interpreter = ImportCache_ImportFile(filename);
    if (interpreter && interpreter->functions.findByName("main"))
    {
        ScriptVariant retval;
        printf("\n\nRunning function 'main'...\n");
        interpreter->runFunction(interpreter->functions.retrieve(), NULL, &retval);
        char buf[256];
        if (retval.vt == VT_OBJECT)
        {
            printf("Returned value: ");
            ObjectHeap_Get(retval.objVal)->print();
            printf("\n");
            ObjectHeap_Unref(retval.objVal);
        }
        else
        {
            ScriptVariant_ToString(&retval, buf, sizeof(buf));
            printf("\nReturned value: %s\n", buf);
        }
        ScriptVariant_Clear(&retval);
    }
}

#if 0
bool testFile(const char *filename)
{
    char *scriptText;
    int fileSize;
    FILE *fp = fopen(filename, "rb");
    if(!fp) goto io_error;
    if (fseek(fp, 0, SEEK_END) < 0) goto io_error;
    fileSize = ftell(fp);
    scriptText = (char*)malloc(fileSize + 1);
    if (fseek(fp, 0, SEEK_SET) < 0) goto io_error;
    if (fread(scriptText, 1, fileSize, fp) != fileSize) goto io_error;
    scriptText[fileSize] = 0;
    
    doTest(scriptText, filename);
    
    free(scriptText);
    return true;

io_error:
    fprintf(stderr, "error reading file %s: %s\n", filename, strerror(errno));
    if (fp) fclose(fp);
    return false;
}
#endif

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf(stderr, argc < 2 ? "no file specified\n" : "too many arguments\n");
        fprintf(stderr, "usage: %s script.c\n", argv[0]);
        return 1;
    }
    
    doTest(argv[1]);
    // testFile(argv[1]);

    ImportCache_Clear();
    ObjectHeap_ClearAll();
    StrCache_Clear();
    return 0;
}

