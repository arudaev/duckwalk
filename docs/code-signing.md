# Code signing policy

[Docs home](README.md)

The Windows builds of Duckwalk's desktop app are signed so that Windows can tell they come from this project and haven't been changed.

**Free code signing provided by [SignPath.io](https://about.signpath.io/), certificate by [SignPath Foundation](https://signpath.org/).** This starts with the first signed release; until then the Windows builds are unsigned.

## What gets signed

Only what this repository builds: the desktop app (`desktop/app`) for Windows, built from a tagged commit by the Release workflow in GitHub Actions (`.github/workflows/release.yml`). Nothing built on a personal computer is signed, and no third-party program is signed under this certificate.

## Roles

| Role | Who |
|---|---|
| Committers and reviewers | [Alexander Rudaev](https://github.com/arudaev) |
| Approvers | [Alexander Rudaev](https://github.com/arudaev) |

Every change reaches `main` through a pull request. Every release is approved by hand before it is signed.

## Privacy

Duckwalk doesn't send any information to other networked systems. The desktop app talks only to the Flipper on your desk, over USB or Bluetooth, and sends it one number: how long ago you last used your own mouse or keyboard. Nothing is saved, and nothing leaves your computer.

## Account security

Everyone with access to this repository or to the SignPath project uses multi-factor authentication.

---

[Docs home](README.md)
