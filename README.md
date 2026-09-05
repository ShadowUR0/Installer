# Vencord Arabic Installer

Vencord Arabic Installer is an unofficial Arabic-focused fork of the GPL-licensed Vencord Installer.
It installs builds from [ShadowUR0/Vencord](https://github.com/ShadowUR0/Vencord) and is not affiliated with Discord or the official Vencord team.

## Downloads

The rolling `latest` release publishes installers for the supported desktop platforms:

- **Windows x86_64:** `VencordArabicInstaller.exe` (GUI)
- **Windows x86_64 CLI:** `VencordArabicInstallerCli.exe`
- **Linux x86_64:** `VencordArabicInstallerCli-linux-x86_64`
- **Linux arm64:** `VencordArabicInstallerCli-linux-arm64`
- **macOS Intel + Apple Silicon:** `VencordArabicInstaller-macos-universal.dmg`

All release assets are accompanied by `SHA256SUMS.txt`.

### Linux quick install

```sh
sh -c "$(curl -fsSL https://raw.githubusercontent.com/ShadowUR0/Installer/main/install.sh)"
```

The script detects x86_64 vs arm64 automatically and downloads the matching Vencord Arabic installer.

### Windows CLI quick install

```powershell
irm https://raw.githubusercontent.com/ShadowUR0/Installer/main/install.ps1 | iex
```

### macOS note

The macOS DMG is built as a universal binary for Intel and Apple Silicon. It is currently ad-hoc signed rather than Apple-notarized, so macOS may require using **Open** from Finder's context menu or approving the app in Privacy & Security.

## Installer features

- Installing Vencord Arabic on supported Discord desktop installations.
- Reinstalling or repairing an existing installation.
- Uninstalling Vencord Arabic and restoring Discord.
- Managing optional OpenAsar installation.
- Selecting a custom Discord installation path when automatic detection is not enough.
- Automatic installer updates on supported executable builds.

## Building from source

### Prerequisites

Install Go and the platform build dependencies required by the upstream Vencord Installer.

### Windows GUI

```sh
make GUI=1 VERSION=dev
```

### Linux CLI

```sh
make PLATFORM=linux ARCH=amd64 VERSION=dev
```

Use `ARCH=arm64` for Linux arm64.

### macOS universal GUI + DMG

```sh
make ARCH=universal GUI=1 DMG=1 IDENTITY="-" VERSION=dev
```

A real Developer ID identity can be supplied instead of `-` when signing/notarization credentials are available.

## Fork modifications

See [MODIFICATIONS.md](MODIFICATIONS.md) for the fork-specific changes.

Release binaries are built from this repository by GitHub Actions.
