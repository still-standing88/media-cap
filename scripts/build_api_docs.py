#!/usr/bin/env python3
"""Build the Sphinx site with the C and C++ API and optional CLI reference."""
import argparse
from pathlib import Path
import os
import subprocess
import sys
import tempfile

from docs_tools import (
    DEFAULT_VENV, DOXYGEN_VERSION, ROOT, doxygen_version, environment_python, find_doxygen,
)


def quote(path):
    value = path.as_posix()
    if any(character in value for character in ('"', '\n', '\r')):
        raise ValueError("Doxygen paths cannot contain quotes or newlines")
    return f'"{value}"'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--doxygen", help="Doxygen executable or path (also accepts DOXYGEN)")
    parser.add_argument("--venv", type=Path, default=DEFAULT_VENV, help="Documentation virtual environment")
    parser.add_argument("--output", type=Path, default=ROOT / "build-api-docs", help="Generated output directory")
    parser.add_argument("--cli", type=Path, help="MediaCap executable for the generated command reference")
    args = parser.parse_args()

    executable = find_doxygen(args.doxygen)
    if not executable or doxygen_version(executable) < DOXYGEN_VERSION:
        parser.error("Doxygen 1.15 or newer is required. Run python scripts/setup_docs.py or pass --doxygen.")
    python = environment_python(args.venv.resolve())
    if not python.is_file():
        python = Path(sys.executable)
    available = subprocess.run(
        [str(python), "-c", "import breathe, furo, sphinx"], capture_output=True,
    )
    if available.returncode:
        parser.error("Documentation packages are missing. Run python scripts/setup_docs.py first.")

    output = args.output.resolve()
    doxygen_output = output / "doxygen"
    doxygen_output.mkdir(parents=True, exist_ok=True)
    warnings = doxygen_output / "warnings.log"
    with tempfile.TemporaryDirectory(prefix="mediacap-docs-") as temporary:
        config = Path(temporary) / "Doxyfile"
        config.write_text(
            f'@INCLUDE = {quote(ROOT / "docs/Doxyfile")}\n'
            f'OUTPUT_DIRECTORY = {quote(doxygen_output)}\n'
            f'WARN_LOGFILE = {quote(warnings)}\n', encoding="utf-8",
        )
        result = subprocess.run([executable, str(config)], cwd=ROOT)
    if result.returncode:
        if warnings.exists():
            print(warnings.read_text(encoding="utf-8", errors="replace"), file=sys.stderr)
        return result.returncode
    xml = doxygen_output / "xml"
    if not (xml / "index.xml").is_file():
        raise SystemExit("Doxygen completed without producing xml/index.xml")

    environment = {**os.environ, "MEDIACAP_DOXYGEN_XML": str(xml)}
    with tempfile.TemporaryDirectory(prefix="mediacap-cli-docs-") as temporary:
        if args.cli:
            reference = Path(temporary) / "command-reference.rst"
            result = subprocess.run([str(args.cli.resolve()), "docs", "--output", str(reference)], cwd=ROOT)
            if result.returncode:
                return result.returncode
            environment["MEDIACAP_CLI_REFERENCE"] = str(reference)
        result = subprocess.run([
            str(python), "-m", "sphinx", "-b", "html", "-W", "--keep-going", "-E", "-a",
            "-d", str(output / "doctrees"), str(ROOT / "docs"), str(output / "html"),
        ], cwd=ROOT, env=environment)
    if result.returncode:
        return result.returncode
    index = output / "html/index.html"
    if not index.is_file():
        raise SystemExit("Sphinx completed without producing html/index.html")
    print(f"Documentation: {index}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        raise SystemExit(str(error))
