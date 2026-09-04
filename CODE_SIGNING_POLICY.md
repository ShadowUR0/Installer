# Code signing policy

## SignPath Foundation

Free code signing provided by SignPath.io, certificate by SignPath Foundation.

Status: **application pending SignPath Foundation approval**. Until approval and activation are complete, release binaries may remain unsigned and Windows SmartScreen may show an unknown-publisher warning.

Repository: https://github.com/ShadowUR0/Installer

The release artifacts intended for signing are built from this repository by GitHub Actions on GitHub-hosted runners. Once SignPath access is activated, release signing will use SignPath origin verification and the project signing policy, with manual approval for release signing as required by SignPath Foundation.

## Team roles

- Committer and reviewer: [ShadowUR0](https://github.com/ShadowUR0)
- Signing approver: [ShadowUR0](https://github.com/ShadowUR0)

Repository and SignPath accounts used for these roles are expected to use multi-factor authentication.

## Privacy and network behavior

Vencord Arabic Installer does not include telemetry or analytics and does not upload Discord messages, account tokens, local Discord files, or other user content.

The installer uses network access for its documented functions:

- It contacts GitHub APIs and GitHub Releases to check the installer/Vencord Arabic versions and download Vencord Arabic release files.
- Self-update checks use the ShadowUR0/Installer GitHub releases.
- If the user explicitly chooses to install OpenAsar, the installer downloads OpenAsar from the GooseMod/OpenAsar GitHub release URL.

Normal connection metadata such as the user's IP address and HTTP request metadata may therefore be visible to GitHub according to GitHub's own privacy practices.

## System changes and removal

The installer modifies the selected Discord desktop installation only as part of explicit install/repair/uninstall actions. It provides an Uninstall action for reverting the Vencord patch. Optional OpenAsar installation can also be removed from the installer.
