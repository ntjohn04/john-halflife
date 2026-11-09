"""Builds John Half-Life's own models, assets/models/<name>/<name>.qc, into config/mod/models (installed over the Unified
SDK's): each with the Unified SDK's studiomdl and its shared model sources (meshes, animations, textures), from a
checkout of its assets repository (https://github.com/twhl-community/halflife-unified-sdk-assets). Each is built in a
temporary folder beside its shared sources, removed afterwards, then given its NPCs' vanilla speeds
(restore_npc_speeds.py; the release packager restores the Unified SDK's models before ours go over them).

The built models are committed, so neither the build nor the packager needs the assets repository.

Usage:
  python build_models.py <halflife-unified-sdk-assets folder> [name ...]
"""

import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
SOURCES = os.path.join(REPO, "assets", "models")
OUT = os.path.join(REPO, "config", "mod", "models")


def build(assets, name):
    source = os.path.join(SOURCES, name)
    work = os.path.join(assets, "modelsrc", "models", f"_jhl_{name}")
    if os.path.exists(work):
        shutil.rmtree(work)
    shutil.copytree(source, work)
    try:
        result = subprocess.run([os.path.join(assets, "tools", "studiomdl.exe"), f"{name}.qc"], cwd=work,
                                capture_output=True, text=True)
        built = os.path.join(work, f"{name}.mdl")
        if result.returncode != 0 or not os.path.exists(built):
            sys.exit(f"build_models: {name} failed:\n{result.stdout}\n{result.stderr}")
        warnings = [l for l in result.stdout.splitlines() if "warning" in l.lower() or "error" in l.lower()]
        for line in warnings:
            print(f"build_models: {name}: {line.strip()}")
        os.makedirs(OUT, exist_ok=True)
        shutil.copyfile(built, os.path.join(OUT, f"{name}.mdl"))
        print(f"build_models: {name}.mdl, {os.path.getsize(built)} bytes")
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    assets = sys.argv[1]
    names = sys.argv[2:] or sorted(n for n in os.listdir(SOURCES) if os.path.isfile(os.path.join(SOURCES, n, f"{n}.qc")))
    for name in names:
        build(assets, name)
    subprocess.run([sys.executable, "-I", os.path.join(HERE, "restore_npc_speeds.py"), os.path.join(REPO, "config", "mod")], check=True)


if __name__ == "__main__":
    main()
