# Vencord Arabic Installer

Vencord Arabic Installer is an unofficial Arabic-focused fork of the GPL-licensed Vencord Installer.
It installs builds from [ShadowUR0/Vencord](https://github.com/ShadowUR0/Vencord) and is not affiliated with Discord or the official Vencord team.

## Usage

Download the latest Windows build from [GitHub Releases](https://github.com/ShadowUR0/Installer/releases).

The installer can install, repair/reinstall, uninstall Vencord Arabic, and optionally manage OpenAsar for supported Discord desktop installations.

## Code signing policy

See the project [Code signing policy](CODE_SIGNING_POLICY.md).

## Building from source

### Prerequisites

Install the [Go programming language](https://go.dev/doc/install) and GCC, the GNU Compiler Collection (MinGW on Windows).

<details>
<summary>Additionally, if you're using Linux, install these dependencies:</summary>

#### Base dependencies
```sh
apt install -y pkg-config libsdl2-dev libglx-dev libgl1-mesa-dev
dnf install pkg-config libGL-devel libXxf86vm-devel
```

#### X11 dependencies
```sh
apt install -y xorg-dev
dnf install libXcursor-devel libXi-devel libXinerama-devel libXrandr-devel
```

#### Wayland dependencies
```sh
apt install -y libwayland-dev libxkbcommon-dev wayland-protocols extra-cmake-modules
dnf install wayland-devel libxkbcommon-devel wayland-protocols-devel extra-cmake-modules
```

</details>

### Build

Install dependencies:

```sh
go mod tidy
```

Windows / macOS / Linux X11 GUI:

```sh
go build
```

Linux Wayland:

```sh
go build --tags wayland
```

CLI:

```sh
go build --tags cli
```

Release builds are produced by the workflows in `.github/workflows` so the binary can be traced back to the repository source and build configuration.
