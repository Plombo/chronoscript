if Platform().name == 'win32':
    env = Environment(tools=['mingw'], CPPPATH=['.', 'script'], CCFLAGS='-g -Wall')
else:
    env = Environment(CPPPATH=['.', 'script'], CCFLAGS='-g -Wall -O2', CXXFLAGS='-std=c++11')

c_sources = [
    'ralloc',
    'ScriptUtils',
    'script/pp_lexer',
]
cpp_sources = [
    'ssa',
    'ScriptVariant',
    'Instruction',
    'Parser',
    'Main',
    'regalloc',
    'liveness',
    'RegAllocUtil',
    'ExecBuilder',
    'Interpreter',
    'Builtins',
    'StrCache',
    'ScriptObject',
    'ImportCache',
    'SymbolTable',
    'HashTable',
    'script/Lexer',
    'script/ParserSet',
    'script/pp_parser',
    'script/pp_expr',
]
objects = []

for name in c_sources:
    env.Object(name + '.o', name + '.c')
    objects.append(name + '.o')
for name in cpp_sources:
    env.Object(name + '.o', name + '.cpp')
    objects.append(name + '.o')

env.Program('runscript', objects)


