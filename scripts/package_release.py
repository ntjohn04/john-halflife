"""Makes John Half-Life's release zip (docs/release.md): one zip for Windows and Linux, extracted into the
Half-Life folder.

It holds:
* johnhalflife's files, from each platform's staging install (cmake --install into a folder of your choosing; on
  Linux scripts/linux/build.sh): the game code, settings, menu files, the four game folders. From the Linux one only
  its libraries (.so), the rest being the same.
* The Unified SDK's files in release_unified_files.txt, from its release zip: its own work (configs, scripts, HUD
  sprites, its models), and the Opposing Force and Blue Shift models and sprites the game code names (they must be
  there for Half-Life to load a map without the expansions; the rest of theirs comes from the player's own games on
  the first start, client jhl_setup.cpp). With the vanilla NPC speeds put back in its models
  (restore_npc_speeds.py) and the hornet dart splats added to its decals.wad (make_hornet_decals.py).
Nothing else of Valve's: the first start copies and converts that from the player's games.

Usage:
  python package_release.py --windows <staging folder> --linux <staging folder> --unified <release zip> [--out dist]
      (each staging folder is the one that holds johnhalflife, johnhalflife_op4, ...)
  python package_release.py --windows <staging folder> --unified <release zip> --install <Half-Life folder>
      (testing: a Windows-only zip, extracted into the game as a player would; the development build's install goes
      to the staging folder, so this is how a build gets into the game)
  python package_release.py --make-list <Half-Life folder> <release zip>
      (writes release_unified_files.txt: run on a machine with Half-Life, Opposing Force and Blue Shift, when the
      Unified SDK's release changes)
"""

import argparse
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
LIST = os.path.join(HERE, "release_unified_files.txt")
SETUP_FILES = os.path.join(REPO, "config", "mod", "jhl_setup_files.txt")
FOLDERS = ("johnhalflife", "johnhalflife_op4", "johnhalflife_bs", "johnhalflife_uplink")

# The Unified SDK's release files never shipped: its tools and installer, developer files, its test maps (and the TFC
# textures only zoo_npcs uses), the Visual C++ installer, its readme, files johnhalflife has its own of (the icon is
# Half-Life's, from the first start; its default_config.cfg is its author's own config, the first start writes key
# bindings instead). Its node graphs of Valve's maps are shipped: without them the game builds each the first time its
# map loads, a hitch vanilla (which has its own) doesn't have; the first start keeps them newer than the maps it
# writes, or the game would take them for out of date.
DROP = [r"^tools/", r"^installer/", r"^redist/", r"^halflife-unified\.fgd$", r"^README\.txt$",
        r"^maps/(test_|zoo_|hlu_technology_demonstration)", r"^maps/graphs/(test_|zoo_|hlu_technology_demonstration)",
        r"^cfg/maps/(test_|zoo_)",
        r"^game\.(ico|tga)$", r"^default_config\.cfg$",
        r"^tfc2?\.wad$", r"^liblist\.gam$", r"^delta\.lst$", r"^cl_dlls/", r"^dlls/", r"^user\.scr", r"^settings\.scr",
        r"^resource/GameMenu\.res$"]


def release_files(release_zip):
    """The release's files by path in the mod folder (its zip has them under hlu\\, .install dropped)."""
    files = {}
    with zipfile.ZipFile(release_zip) as z:
        for info in z.infolist():
            if info.is_dir():
                continue
            name = info.filename.replace("\\", "/")
            if not name.lower().startswith("hlu/"):
                continue
            name = name[4:]
            if name.endswith(".install"):
                name = name[:-len(".install")]
            files[name] = info
    return files


def from_half_life():
    """The johnhalflife files the first start copies from Half-Life itself (every player has it): never shipped, or
    installing over a set-up folder would put the Unified SDK's back over Half-Life's (its shell.mdl is Opposing
    Force's) with nothing to set them right, the first start having run."""
    names = set()
    for line in open(SETUP_FILES, encoding="utf-8"):
        parts = line.rstrip("\n").split("\t")
        if len(parts) > 2 and parts[0] == "johnhalflife" and parts[2].lower().startswith("valve/"):
            names.add(parts[1].lower())
    return names


def md5(data):
    return hashlib.md5(data).hexdigest()


def make_list(half_life, release_zip):
    # Valve's files by size and hash.
    valve = {}
    for game in ("valve", "gearbox", "bshift"):
        for dp, _, fs in os.walk(os.path.join(half_life, game)):
            for f in fs:
                p = os.path.join(dp, f)
                valve.setdefault(os.path.getsize(p), []).append((game, p))
    hashes = {}

    def valve_copy(data):
        """The games a byte-for-byte copy of this is in."""
        found = []
        for game, p in valve.get(len(data), []):
            if p not in hashes:
                hashes[p] = md5(open(p, "rb").read())
            if hashes[p] == md5(data):
                found.append(game)
        return found

    # The game code's names, for the expansions' models and sprites it uses.
    code = []
    for dp, _, fs in os.walk(os.path.join(REPO, "src", "game")):
        for f in fs:
            if f.endswith((".cpp", ".h")):
                code.append(open(os.path.join(dp, f), encoding="latin-1").read().lower())
    code = "\n".join(code)

    ship, named = [], 0
    with zipfile.ZipFile(release_zip) as z:
        for name, info in sorted(release_files(release_zip).items()):
            if any(re.search(p, name, re.I) for p in DROP):
                continue
            games = valve_copy(z.read(info))
            if not games:
                ship.append(name)
            elif "valve" not in games and name.lower().startswith(("models/", "sprites/", "events/")) and os.path.basename(name).lower() in code:
                ship.append(name)
                named += 1

    with open(LIST, "w", encoding="utf-8", newline="\n") as out:
        out.write("// The Unified SDK release's files johnhalflife's release ships (package_release.py --make-list): its own\n"
                  "// work, and the Opposing Force and Blue Shift models, sprites and event scripts the game code names. Paths in the mod folder.\n")
        out.write("\n".join(ship) + "\n")
    print(f"package_release: {len(ship)} files in {LIST} ({named} of them the expansions' models and sprites)")


def version():
    text = open(os.path.join(REPO, "CMakeLists.txt"), encoding="utf-8").read()
    number = re.search(r"project\(\s*HalfLifeMod.*?^\s*VERSION\s+([0-9.]+)", text, re.S | re.M).group(1)
    release_type = re.search(r'set\(HalfLife_RELEASE_TYPE\s+"?([A-Za-z-]+)', text)
    return f"v{number}-" + (release_type.group(1).lower().replace("-", "") if release_type else "prealpha")


def package(windows, linux, release_zip, out, install=None):
    stage = tempfile.mkdtemp(prefix="jhl_release_")
    try:
        # 1. The Unified SDK's files, into johnhalflife.
        wanted = [l.strip() for l in open(LIST, encoding="utf-8") if l.strip() and not l.startswith("//")]
        valve_own = from_half_life()
        wanted = [n for n in wanted if n.lower() not in valve_own]
        files = release_files(release_zip)
        missing = [n for n in wanted if n not in files]
        if missing:
            sys.exit(f"package_release: not in {release_zip}: {missing[:5]}")
        with zipfile.ZipFile(release_zip) as z:
            for name in wanted:
                path = os.path.join(stage, "johnhalflife", *name.split("/"))
                os.makedirs(os.path.dirname(path), exist_ok=True)
                with open(path, "wb") as f:
                    f.write(z.read(files[name]))

        base = os.path.join(stage, "johnhalflife")
        python = [sys.executable, "-I"]
        subprocess.run(python + [os.path.join(HERE, "restore_npc_speeds.py"), base], check=True)
        subprocess.run(python + [os.path.join(HERE, "make_hornet_decals.py"), base], check=True)
        subprocess.run(python + [os.path.join(HERE, "make_half_hud.py"), "sprites", base], check=True)
        unified_decals = os.path.join(base, "decals_unified.wad")
        if os.path.exists(unified_decals):
            os.remove(unified_decals)

        # 2. johnhalflife's files, Windows' then Linux's libraries, over them.
        for folder in FOLDERS:
            source = os.path.join(windows, folder)
            if not os.path.isdir(source):
                sys.exit(f"package_release: {source} not found")
            for dp, _, fs in os.walk(source):
                for f in fs:
                    if f.endswith((".pdb", ".exp", ".lib", ".ilk")):
                        continue
                    rel = os.path.relpath(os.path.join(dp, f), source)
                    target = os.path.join(stage, folder, rel)
                    os.makedirs(os.path.dirname(target), exist_ok=True)
                    shutil.copyfile(os.path.join(dp, f), target)

            if not linux:
                continue
            source = os.path.join(linux, folder)
            if not os.path.isdir(source):
                sys.exit(f"package_release: {source} not found")
            for dp, _, fs in os.walk(source):
                for f in fs:
                    if ".so" not in f:
                        continue
                    rel = os.path.relpath(os.path.join(dp, f), source)
                    # The OpenAL library (libopenal-hlu.so.1.22.2, linked as .so.1) as a file under the name the client
                    # asks for: links don't zip, nor read reliably from Windows through \\wsl.localhost.
                    if f.startswith("libopenal-hlu.so"):
                        if not re.search(r"\.so\.\d+\.\d+\.\d+$", f):
                            continue
                        rel = os.path.join(os.path.dirname(rel), "libopenal-hlu.so.1")
                    target = os.path.join(stage, folder, rel)
                    os.makedirs(os.path.dirname(target), exist_ok=True)
                    shutil.copyfile(os.path.join(dp, f), target)

        # 3. The zip.
        os.makedirs(out, exist_ok=True)
        zip_path = os.path.join(out, f"johnhalflife-{version()}" + ("" if linux else "-windows-test") + ".zip")
        count = size = 0
        with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
            for dp, _, fs in sorted(os.walk(stage)):
                for f in sorted(fs):
                    p = os.path.join(dp, f)
                    z.write(p, os.path.relpath(p, stage).replace("\\", "/"))
                    count += 1
                    size += os.path.getsize(p)
        print(f"package_release: {zip_path}: {count} files, {size / 1e6:.1f} MB extracted, "
              f"{os.path.getsize(zip_path) / 1e6:.1f} MB zipped" + ("" if linux else " (no Linux build: for testing, not a release)"))

        # 4. Extracted into the game, as a player would.
        if install:
            with zipfile.ZipFile(zip_path) as z:
                z.extractall(install)
            print(f"package_release: extracted into {install}")
    finally:
        shutil.rmtree(stage, ignore_errors=True)


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "--make-list":
        make_list(sys.argv[2], sys.argv[3])
        return
    parser = argparse.ArgumentParser(description="Makes John Half-Life's release zip.")
    parser.add_argument("--windows", required=True, help="Windows staging install (holds johnhalflife, ...)")
    parser.add_argument("--linux", help="Linux staging install (holds johnhalflife, ...); without it, a Windows-only zip to test")
    parser.add_argument("--unified", required=True, help="the Unified SDK's release zip (HalfLifeUnified-V1.0.0.zip)")
    parser.add_argument("--out", default=os.path.join(REPO, "dist"), help="where to write the zip (dist)")
    parser.add_argument("--install", help="the Half-Life folder to extract it into, as a player would (to test)")
    args = parser.parse_args()
    package(args.windows, args.linux, args.unified, args.out, args.install)


if __name__ == "__main__":
    main()
