"""Regenerate the embedded page after editing Disco_ESP/data/index.html."""
from pathlib import Path
root = Path(__file__).resolve().parents[1] / "Disco_ESP"
html = (root / "data/index.html").read_text(encoding="utf-8")
if ')PAGE"' in html:
    raise ValueError("HTML contains the reserved C++ raw string delimiter")
(root / "page.cpp").write_text('extern "C" { extern const char page_html[] = R"PAGE(' + html + ')PAGE"; }\n', encoding="utf-8")
print("page.cpp regenerated")
