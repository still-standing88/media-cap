from pathlib import Path


def read_list(path):
    return Path(path).read_text(encoding='utf-8').split()


def objects(env, sources, output, shared=False):
    builder = env.SharedObject if shared else env.Object
    return [builder(target=output + path + env['OBJSUFFIX'], source=path) for path in sources]


def build_library(env, output, sources, avx2_source, avx2_flag, flex_archive):
    compiled = objects(env, sources, output, env['shared'])
    if env['PLATFORM'] == 'darwin':
        objc = env.Clone()
        objc.Append(CXXFLAGS=['-fobjc-arc'])
        compiled.extend(objects(objc, ['src/third_party/CameraCapture/src/ccap_imp_apple.mm'],
                                output, env['shared']))
    if avx2_source:
        avx2 = env.Clone()
        avx2.Append(CXXFLAGS=[avx2_flag])
        compiled.extend(objects(avx2, [avx2_source], output, env['shared']))
    builder = env.SharedLibrary if env['shared'] else env.StaticLibrary
    if env['shared'] and env['PLATFORM'] == 'darwin':
        env.Append(SHLINKFLAGS=['-Wl,-install_name,@rpath/libMediaCap.dylib'])
    if env['shared'] and env['PLATFORM'] == 'posix':
        env.Append(SHLINKFLAGS=['-Wl,--exclude-libs,ALL'])
    library = builder(output + 'MediaCap', compiled)
    if flex_archive:
        env.Depends(library, flex_archive)
    return library


def build_consumers(env, output, library, is_windows):
    consumer = env.Clone()
    consumer.Replace(CPPDEFINES=[item for item in env['CPPDEFINES'] if item != 'MEDIACAP_EXPORT'])
    library_link = next(node for node in library if str(node).endswith('.lib')) if is_windows else library[0]
    consumer.Prepend(LIBS=[library_link])
    if env['shared'] and not is_windows:
        consumer.Append(RPATH=[env.Dir(output).get_abspath()])

    examples = []
    if env['examples']:
        for name in read_list('examples/targets.txt'):
            examples.extend(consumer.Program(output + name,
                objects(consumer, [f'examples/{name}.cpp'], output)))

    cli = None
    cli_tests = []
    if env['cli']:
        sources = ['cli/' + name for name in read_list('cli/sources.txt')]
        cli_env = consumer.Clone()
        cli_include = ['cli/third_party/CLI11', 'cli/third_party/nlohmann']
        cli_env.Append(CPPPATH=cli_include)
        if is_windows and env['debug']:
            cli_env.Append(LINKFLAGS=['/PDB:' + output + 'MediaCap_cli.pdb'])
        elif env['shared']:
            if env['PLATFORM'] == 'darwin':
                cli_env.Append(LINKFLAGS=['-Wl,-rpath,@loader_path/../lib'])
            elif env['PLATFORM'] == 'posix':
                cli_env.Append(LINKFLAGS=[r'-Wl,-rpath,\$$ORIGIN/../lib'])
        cli = cli_env.Program(output + 'MediaCap', objects(cli_env, sources, output))
        if env['tests']:
            fixture_env = env.Clone()
            fixture_env.Replace(CPPDEFINES=['MEDIACAP_STATIC', 'NOMINMAX'])
            fixture_env.Append(CPPPATH=cli_include)
            fixture_sources = sources + ['tests/integration/cli/fake_api.cpp']
            fixture = fixture_env.Program(output + 'MediaCap_cli_fixture',
                objects(fixture_env, fixture_sources, output + 'fixture/'))
            cli_tests = [
                ('tests/integration/cli/test_cli.py', cli[0]),
                ('tests/integration/cli/test_discovery.py', fixture[0]),
                ('tests/integration/cli/test_capture.py', fixture[0]),
            ]
    return consumer, examples, cli, cli_tests
