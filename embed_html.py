"""PlatformIO pre-build script: gzips data/editor.html → src/editor_html.h (PROGMEM)."""
import gzip
import os

Import("env")

src = os.path.join(env.subst("$PROJECT_DIR"), "data", "editor.html")
dst = os.path.join(env.subst("$PROJECT_DIR"), "src", "editor_html.h")

# Skip if source hasn't changed since last generation
if os.path.exists(dst) and os.path.getmtime(dst) >= os.path.getmtime(src):
    print("editor_html.h is up to date")
else:
    with open(src, "rb") as f:
        raw = f.read()
    compressed = gzip.compress(raw, compresslevel=9)

    with open(dst, "w") as f:
        f.write("#pragma once\n")
        f.write("#include <stddef.h>\n")
        f.write("#include <pgmspace.h>\n\n")
        f.write(f"// Auto-generated from data/editor.html ({len(raw)} bytes raw, {len(compressed)} bytes gzipped)\n")
        f.write(f"static const size_t EDITOR_HTML_GZ_LEN = {len(compressed)};\n")
        f.write("static const uint8_t EDITOR_HTML_GZ[] PROGMEM = {\n")
        for i in range(0, len(compressed), 16):
            chunk = compressed[i:i+16]
            f.write("    " + ", ".join(f"0x{b:02X}" for b in chunk) + ",\n")
        f.write("};\n")

    print(f"Generated {dst} ({len(compressed)} bytes gzipped)")
