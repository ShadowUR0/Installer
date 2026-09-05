# Modifications

This repository is a modified fork of Vencord/Installer.

Fork-specific changes maintained by ShadowUR0:

- Rebranded the application as Vencord Arabic Installer.
- Changed Vencord build downloads to ShadowUR0/Vencord.
- Changed installer self-updates to the rolling `latest` release in ShadowUR0/Installer.
- Removed fallback downloads from the official vencord.dev service.
- Isolated the local application-data directory as VencordArabic.
- Kept the classic installer structure while polishing spacing, controls, colors and rounded corners.
- Preserved custom Discord installation-path selection.
- Publishes Windows GUI/CLI, Linux x86_64/arm64 CLI, and a universal macOS DMG from GitHub Actions.
- Publishes SHA-256 checksums for every release asset.
- Linux and PowerShell bootstrap scripts download only Vencord Arabic installer assets.
- macOS uses a fork-specific bundle identifier and display name; public builds are ad-hoc signed until notarization credentials are configured.
- Periodically synchronizes the underlying installer code with upstream Vencord/Installer while retaining the fork-specific behavior above.

No fork-specific changes are submitted upstream automatically.

The project remains licensed under GPL-3.0.
