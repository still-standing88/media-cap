#!/usr/bin/env python3
"""Check Windows PE imports using the developer toolchain; never needed by users."""
import argparse
import json
from pathlib import Path
import re
import subprocess

def main():
    p = argparse.ArgumentParser()
    p.add_argument('binary')
    p.add_argument('--output')
    p.add_argument('--allow-local', action='append', default=[], help='Explicit application DLL alongside the executable (checked separately)')
    a = p.parse_args()
    binary = Path(a.binary).resolve()
    text = subprocess.check_output(['dumpbin', '/nologo', '/dependents', str(binary)], text=True)
    imports = sorted(set(re.findall(r'^\s+([\w.-]+\.dll)\s*$', text, re.MULTILINE | re.IGNORECASE)))
    if not imports:
        raise SystemExit('No PE imports found; cannot validate runtime dependencies')
    forbidden = [name for name in imports if re.match(
        r'(?:msvcp\d|msvcr\d|vcruntime\d|ucrtbased|ucrtbase|api-ms-win-crt-|avcodec|avformat|avutil|swscale|swresample|libstdc\+\+|libgcc)', name, re.I)]
    # API-set contracts are provided by Windows; ordinary imports must be OS DLLs.
    import os
    system = Path(os.environ['SystemRoot']) / 'System32'
    allowed = {name.lower() for name in a.allow_local if (binary.parent / name).is_file()}
    non_system = [name for name in imports if name.lower() not in allowed and
                  not name.lower().startswith(('api-ms-win-', 'ext-ms-')) and not (system / name).is_file()]
    record = {'binary': binary.name, 'imports': imports, 'forbidden_runtime_imports': forbidden,
              'missing_system_imports': non_system, 'passed': not forbidden and not non_system}
    result = json.dumps(record, indent=2)
    if a.output: Path(a.output).write_text(result+'\n', encoding='utf-8')
    print(result)
    if not record['passed']: raise SystemExit(1)

if __name__ == '__main__':
    main()
