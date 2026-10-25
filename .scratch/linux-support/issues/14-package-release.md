Status: claimed
Blocked by: 08, 11

# 14 Tarball, portability and the release leg

## Scope

May write:
- `NeuralAmpModeler/scripts/makedist-linux.sh`
- `NeuralAmpModeler/installer/linux/**` (`install.sh`, `.desktop`, icon)
- `.github/workflows/release-native.yml`: Linux leg
- `.github/workflows/ci.yml`: `linux-package` job

## Work

- Build `VoLum-<version>-linux-x86_64.tar.gz` per spec.md Package.
- **`install.sh`:** supports `--uninstall`. It never touches system paths without
  `--system`.
- **Portability matrix (CI containers):**
  - Ubuntu 22.04 and 24.04: validator plus a standalone launch smoke.
  - Fedora latest: the same, plus a `ldd` check that only the expected system
    libraries are needed.
- **`release-native.yml`:** add the Linux leg to the matrix, producing the
  tarball as a release asset. Prove it with a dry run (workflow_dispatch input or
  a branch-only trigger) that does not publish.

## Acceptance

- Green `linux-package` job and a green dry run of the release leg, with run ids.
- The tarball installs, launches, and uninstalls cleanly in each container.

## Carried from ticket 10

- **Runtime libraries:** the standalone needs GDK 3 at runtime (SWELL-GDK:
  libgdk-3, gdk_pixbuf, gobject, glib, cairo) plus libXi. The VST3 must not:
  check both NEEDED lists, so a GDK dependency creeping into the plug-in fails.
- `install.sh` names the missing package when the standalone's libraries are
  absent, and still installs the VST3. The container matrix runs one case
  without GTK 3 to show it.
- The spec's linked-libraries list gains the standalone's GDK 3 set.
