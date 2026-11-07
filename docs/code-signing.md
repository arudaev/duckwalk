# Code signing

[Docs home](README.md)

The Windows builds of Duckwalk's desktop app are **not signed** yet, so the first time you run one, Windows SmartScreen may call it an unknown app: More info, then Run anyway. Signing is the plan, but it has to be free ([decision 0003](decisions/0003-trusted-install-for-free.md)).

The project applied to the SignPath Foundation, which signs open-source projects for free. On 7 November 2026 they declined for now: the Foundation looks for signs that a project is already widely used (stars, forks, contributors, articles), and Duckwalk is new. They welcome a new application once it has more of those, and the project will apply again after v0.1.

## What would be signed

Only what this repository builds: the desktop app (`desktop/app`) for Windows, built from a tagged commit by the Release workflow in GitHub Actions (`.github/workflows/release.yml`). Nothing built on a personal computer, and no third-party program.

## Until then

- Download only from [the releases page](https://github.com/arudaev/duckwalk/releases). Every release is built in the open by GitHub Actions from a tagged commit.
- Or, with Rust installed, build it yourself from source: `cargo install --git https://github.com/arudaev/duckwalk duckwalk`. A build made on your own computer doesn't trigger SmartScreen.

## Roles

| Role | Who |
|---|---|
| Committers and reviewers | [Alexander Rudaev](https://github.com/arudaev) |
| Approvers | [Alexander Rudaev](https://github.com/arudaev) |

Every change reaches `main` through a pull request. Every release is approved by hand.

## Privacy

Duckwalk doesn't send any information to other networked systems. The desktop app talks only to the Flipper on your desk, over USB or Bluetooth, and sends it one number: how long ago you last used your own mouse or keyboard. Nothing is saved, and nothing leaves your computer.

---

[Docs home](README.md)
