env_options = {
    'CPPPATH': ['.', 'script'],
    'CCFLAGS': '-g -Wall -O2 -ffast-math',
    'CXXFLAGS': '-std=c++11 -fno-exceptions -fno-rtti',
}

if Platform().name == 'win32':
    env_options['tools'] = ['mingw']

env = Environment(**env_options)

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
    'List',
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


