# Vencord Arabic Installer

Vencord Arabic Installer is an unofficial Arabic-focused fork of the GPL-licensed Vencord Installer.
It installs builds from [ShadowUR0/Vencord](https://github.com/ShadowUR0/Vencord) and is not affiliated with Discord or the official Vencord team.

## Usage

Download the latest Windows build from [GitHub Releases](https://github.com/ShadowUR0/Installer/releases).

The installer supports:

- Installing Vencord Arabic on supported Discord desktop installations.
- Reinstalling or repairing an existing installation.
- Uninstalling Vencord Arabic and restoring Discord.
- Managing optional OpenAsar installation.
- Selecting a custom Discord installation path when automatic detection is not enough.
- Automatic installer updates from this repository's releases.

## Building from source

### Prerequisites

Install Go and the platform build dependencies required by the upstream Vencord Installer. On Windows, the release workflow uses MSYS2 with MinGW, GCC and SDL2.

### Windows GUI

```sh
make GUI=1 VERSION=dev
```

The upstream-compatible Makefile writes the GUI executable to `build/VencordInstaller.exe`. Official Vencord Arabic releases rename the final Windows artifact to `VencordArabicInstaller.exe`.

### Other platforms

The upstream Makefile also supports CLI, Linux, Wayland and macOS build targets. Vencord Arabic's published installer release is currently focused on Windows.

## Fork modifications

See [MODIFICATIONS.md](MODIFICATIONS.md) for the fork-specific changes.

Release binaries are built from this repository by GitHub Actions and include SHA-256 checksums.
