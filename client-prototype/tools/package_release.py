"""Create and check a release that extracts directly into the game directory."""
from pathlib import Path
import json
import re
import shutil
import subprocess
from zipfile import ZipFile, ZIP_DEFLATED

ROOT = Path(__file__).resolve().parents[1]
ADDON = ROOT / "addon/NozdorAction"


def main():
    toc = (ADDON / "NozdorAction.toc").read_text(encoding="utf-8")
    version = re.search(r"^## Version: ([0-9.]+)$", toc, re.M).group(1)
    literal = r'"(?:[^"\\]|\\.)*"'
    translations = dict((json.loads(key), json.loads(value)) for key, value in re.findall(
        r'^    \[(' + literal + r')\] = (' + literal + r'),$',
        (ADDON / "Localization.lua").read_text(encoding="utf-8"), re.M))
    used = {json.loads(key) for key in re.findall(r'L\[(' + literal + r')\]',
                                                (ADDON / "NozdorAction.lua").read_text(encoding="utf-8"))}
    assert used <= translations.keys(), used - translations.keys()
    for key, value in translations.items():
        assert re.findall(r"%[sd]", key) == re.findall(r"%[sd]", value), key
    files = {
        "client-prototype/bin/NozdorAction.dll": ROOT / "bin/NozdorAction.dll",
        "client-prototype/bin/NozdorActionLauncher.exe": ROOT / "bin/NozdorActionLauncher.exe",
        "Start-NozdorAction.cmd": ROOT / "distribution/Start-NozdorAction.cmd",
        "NozdorAction-Install.ru.txt": ROOT / "distribution/NozdorAction-Install.ru.txt",
        "NozdorAction-README.ru.md": ROOT / "README.ru.md",
        "client-prototype/VERIFICATION.md": ROOT / "VERIFICATION.md",
        "client-prototype/compat/README.ru.md": ROOT / "compat/README.ru.md",
        "client-prototype/compat/RefinedBlizzPlates-1.12.0-class-colors.patch": ROOT / "compat/RefinedBlizzPlates-1.12.0-class-colors.patch",
    }
    for name in ("NozdorAction.toc", "Localization.lua", "NozdorAction.lua", "Bindings.xml", "Reticles.tga"):
        files["Interface/AddOns/NozdorAction/" + name] = ADDON / name
    for source in files.values():
        if not source.is_file():
            raise SystemExit(f"Missing release file: {source}")
    archive = ROOT / f"NozdorAction-{version}.zip"
    staging = ROOT / "build" / f"package-check-{version}"
    staging.mkdir(parents=True, exist_ok=False)
    with ZipFile(archive, "x", compression=ZIP_DEFLATED, compresslevel=9) as output:
        for name, source in files.items():
            output.write(source, name)
    with ZipFile(archive) as output:
        assert output.testzip() is None and set(output.namelist()) == set(files)
        assert "WoW.exe" not in output.namelist()
        output.extractall(staging)
    for name, source in files.items():
        assert (staging / name).read_bytes() == source.read_bytes(), name
    installed = staging / "Interface/AddOns/NozdorAction"
    for name in ("Localization.lua", "NozdorAction.lua", "Bindings.xml"):
        assert name in toc and (installed / name).is_file()
    shutil.copyfile(ROOT.parent / "WoW.exe", staging / "WoW.exe")
    result = subprocess.run([str(staging / "client-prototype/bin/NozdorActionLauncher.exe"), "--check"],
                            cwd=ROOT, capture_output=True)
    assert result.returncode == 0, result.stderr
    command = f'cmd.exe /d /s /c ""{staging / "Start-NozdorAction.cmd"}" --check"'
    result = subprocess.run(command, cwd=ROOT, capture_output=True)
    assert result.returncode == 0, result.stderr
    print(f"Verified {len(files)} release files, {len(translations)} translations, direct addon layout and startup --check")
    print(archive)


if __name__ == "__main__":
    main()
