"""Compatibility entrypoint for the editable Word submission builder.
First render scripts/draw_uml.py output and capture current application screenshots.
See scripts/create_submission.py and scripts/export_docx.ps1.
"""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name("create_submission.py")), run_name="__main__")
