"""Create portable application and source archives, excluding the local toolchain."""
from pathlib import Path
import hashlib
import shutil
import zipfile

root=Path(__file__).resolve().parents[1]
out=root/"output"
out.mkdir(exist_ok=True)
binary=out/"SCAPMS-Windows.zip"
source=out/"SCAPMS-Source.zip"
shutil.copytree(root/"docs", root/"dist/SCAPMS-v3/docs", dirs_exist_ok=True)
with zipfile.ZipFile(binary,"w",zipfile.ZIP_DEFLATED) as archive:
    for path in sorted((root/"dist/SCAPMS-v3").rglob("*")):
        if path.is_file():
            archive.write(path,Path("SCAPMS")/path.relative_to(root/"dist/SCAPMS-v3"))
sources=[]
for directory in ["src","tests","scripts","docs","data","licenses","output/pdf","output/docx","output/screenshots"]:
    sources.extend(p for p in (root/directory).rglob("*") if p.is_file() and "__pycache__" not in p.parts and not p.name.startswith("~$"))
sources.extend(root/p for p in ["CMakeLists.txt","README.md","THIRD_PARTY_NOTICES.md","Launch-SCAPMS.cmd",".clang-format",".gitignore","output/test-results.txt","output/test-results.xml"])
with zipfile.ZipFile(source,"w",zipfile.ZIP_DEFLATED) as archive:
    for path in sorted(sources):
        archive.write(path,Path("SCAPMS-Source")/path.relative_to(root))
for path in [binary,source]:
    with zipfile.ZipFile(path) as archive:
        assert archive.testzip() is None
    print(path.name, path.stat().st_size, "bytes")
files=[binary,source,root/"output/pdf/SCAPMS-assessment.pdf",root/"output/docx/SCAPMS-assessment.docx",root/"output/pdf/SCAPMS-project-report.pdf",root/"output/docx/SCAPMS-project-report.docx"]
(out/"SHA256SUMS.txt").write_text("\n".join(hashlib.sha256(p.read_bytes()).hexdigest()+"  "+p.name for p in files)+"\n",encoding="utf-8")
