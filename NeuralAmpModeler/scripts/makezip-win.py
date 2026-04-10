import glob
import os
import shutil
import sys
import zipfile

scriptpath = os.path.dirname(os.path.realpath(__file__))
projectpath = os.path.abspath(os.path.join(scriptpath, os.pardir))

IPLUG2_ROOT = "..\\..\\iPlug2"

sys.path.insert(0, os.path.join(scriptpath, IPLUG2_ROOT + "\\Scripts"))

from get_archive_name import get_archive_name
from parse_config import parse_config


def main():
    if len(sys.argv) != 3:
        print("Usage: make_zip.py demo[0/1] zip[0/1]")
        sys.exit(1)

    demo = int(sys.argv[1])
    zip_loose = int(sys.argv[2])
    config = parse_config(projectpath)
    plug = config["PLUG_NAME"]

    out_dir = projectpath + "\\build-win\\out"

    if os.path.exists(out_dir):
        shutil.rmtree(out_dir)

    os.makedirs(out_dir)

    files = []

    if not zip_loose:
        installer_name = f"{plug}-Demo-Setup.exe" if demo else f"{plug}-Setup.exe"
        installer = os.path.join(projectpath, "build-win", "installer", installer_name)
        files = [installer]
        extras = [
            os.path.join(projectpath, "installer", "changelog.txt"),
            os.path.join(projectpath, "installer", "known-issues.txt"),
            os.path.join(projectpath, "manual", f"{plug} manual.pdf"),
        ]
        for p in extras:
            if os.path.isfile(p):
                files.append(p)
        if not os.path.isfile(files[0]):
            print("ERROR: installer not found: " + files[0])
            sys.exit(1)
    else:
        vst3_inner = os.path.join(
            projectpath,
            "build-win",
            f"{plug}.vst3",
            "Contents",
            "x86_64-win",
            f"{plug}.vst3",
        )
        app_copy = os.path.join(projectpath, "build-win", f"{plug}_x64.exe")
        files = [vst3_inner, app_copy]

    zipname = get_archive_name(projectpath, "win", "demo" if demo == 1 else "full")

    zf = zipfile.ZipFile(
        projectpath + "\\build-win\\out\\" + zipname + ".zip", mode="w"
    )

    for f in files:
        print("adding " + f)
        if not os.path.isfile(f):
            print("ERROR: missing file for zip: " + f)
            sys.exit(1)
        zf.write(f, os.path.basename(f), zipfile.ZIP_DEFLATED)

    zf.close()
    print("wrote " + zipname)

    zf = zipfile.ZipFile(
        projectpath + "\\build-win\\out\\" + zipname + "-pdbs.zip", mode="w"
    )

    pdb_dir = os.path.join(projectpath, "build-win", "pdbs")
    pdb_files = sorted(glob.glob(os.path.join(pdb_dir, "*_x64.pdb")))
    if not pdb_files:
        print("ERROR: no *_x64.pdb files in " + pdb_dir)
        sys.exit(1)
    for f in pdb_files:
        print("adding " + f)
        zf.write(f, os.path.basename(f), zipfile.ZIP_DEFLATED)

    zf.close()
    print("wrote " + zipname)


if __name__ == "__main__":
    main()
