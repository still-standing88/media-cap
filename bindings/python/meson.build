python_module = import('python')
binding_interpreter = get_option('python_executable')
binding_python = python_module.find_installation(binding_interpreter, pure: false)
binding_python_dep = binding_python.dependency()
binding_include = run_command(binding_python, '-c', 'import pybind11; print(pybind11.get_include())', check: true).stdout().strip()
binding_sources = []
foreach source : fs.read('sources.txt').strip().split()
  binding_sources += files(source)
endforeach
binding_package_files = fs.read('package_files.txt').strip().split()
subdir('mediacap')
