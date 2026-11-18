"""Sphinx configuration for the MediaCap documentation."""
import os
from pathlib import Path

project = "MediaCap"
author = "MediaCap contributors"
extensions = ["breathe"]
source_suffix = {".rst": "restructuredtext"}
root_doc = "index"

html_theme = "furo"
html_title = "MediaCap documentation"
html_show_sourcelink = False
pygments_style = "friendly"
pygments_dark_style = "monokai"

root = Path(__file__).resolve().parent.parent
breathe_projects = {
    "MediaCap": os.environ.get("MEDIACAP_DOXYGEN_XML", str(root / "build-api-docs/doxygen/xml")),
}
breathe_default_project = "MediaCap"
breathe_domain_by_file_pattern = {
    "*/MediaCap_c.h": "c",
    "*/MediaCap_audio.h": "c",
    "*/MediaCap_types.h": "c",
    "*/MediaCap_export.h": "c",
    "*.hpp": "cpp",
}
breathe_show_define_initializer = True
breathe_show_enumvalue_initializer = True


def append_cli_reference(app, docname, source):
    reference = os.environ.get("MEDIACAP_CLI_REFERENCE")
    if docname == "cli" and reference:
        app.env.note_dependency(reference)
        source[0] += "\n\n" + Path(reference).read_text(encoding="utf-8")


def setup(app):
    app.connect("source-read", append_cli_reference)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
