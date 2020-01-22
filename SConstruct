env_options = {
    'CPPPATH': ['.', 'script'],
    'CCFLAGS': '-g -Wall -O2 -ffast-math',
    'CXXFLAGS': '-std=c++11 -fno-exceptions -fno-rtti',
}

if Platform().name == 'msys':
    # disable stack protection to improve speed (at the expense of security)
    env_options['CCFLAGS'] += ' -fno-stack-check -fno-stack-protector -mno-stack-arg-probe'
elif Platform().name == 'win32':
    env_options['tools'] = ['mingw']

env = Environment(**env_options)

# Link with the C compiler so that it doesn't pull in the standard C++ library
env.Replace(LINK = env['CC'])

c_sources = [
    'ralloc',
    'ScriptUtils',
    'script/pp_lexer',
]
cpp_sources = [
    'SSABuilder',
    'ScriptVariant',
    'Instruction',
    'Parser',
    'Main',
    'RegAlloc',
    'liveness',
    'RegAllocUtil',
    'ExecBuilder',
    'Interpreter',
    'Builtins',
    'StrCache',
    'ScriptObject',
    'ScriptList',
    'ObjectHeap',
    'ImportCache',
    'SymbolTable',
    'HashTable',
    'List',
    'FakeEngineTypes',
    'script/Lexer',
    'script/ParserSet',
    'script/pp_parser',
    'script/pp_expr',
    'fakestdc++',
]
objects = []

for name in c_sources:
    env.Object(name + '.o', name + '.c')
    objects.append(name + '.o')
for name in cpp_sources:
    env.Object(name + '.o', name + '.cpp')
    objects.append(name + '.o')

env.Program('runscript', objects)


