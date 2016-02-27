env = Environment(tools=['mingw'], CPPPATH=['.', 'script'], CCFLAGS='-g')

c_sources = [
    'List',
    'ScriptVariant',
    'Stack',
    'SymbolTable',
    'ralloc',
    'script/ParserSet',
    'script/Lexer',
    'script/pp_parser',
    'script/pp_lexer',
    'script/pp_expr'
]
cpp_sources = [
    'ssa',
    'Parser',
    'ParserTest',
    'regalloc',
    'liveness',
    'RegAllocUtil',
    'ExecBuilder',
    'Interpreter',
    'Builtins'
]
objects = []

for name in c_sources:
    env.Object(name + '.o', name + '.c')
    objects.append(name + '.o')
for name in cpp_sources:
    env.Object(name + '.o', name + '.cpp')
    objects.append(name + '.o')

env.Program('ssa', objects)


