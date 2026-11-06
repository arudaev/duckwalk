# 0003 – A trusted install without paid accounts

[← Decision 0002](0002-one-desktop-app-in-rust.md) · [Docs home](../README.md)

Issue #127, 6 November 2026.

## Question

The desktop app runs on work computers. A download that Windows or macOS calls "unknown" or "damaged" looks suspicious there, and rightly so. The usual fixes cost money and accounts the project doesn't have: the Microsoft Store, a code signing certificate, Apple's Developer Program for notarization. How do we install without warnings, for free and in the open?

## Decision

| System | Route | Costs |
|---|---|---|
| Windows | Releases signed through the **SignPath Foundation** (free code signing for open-source projects, in the Release workflow), and a **winget** package. The app installs itself into the Start menu (#140) | No money. A SignPath account with MFA; the certificate names SignPath Foundation as publisher; a [code signing policy](../code-signing.md) |
| macOS | A **Homebrew tap**, [arudaev/homebrew-tap](https://github.com/arudaev/homebrew-tap), whose formula **builds the app from source** on the Mac | No money, no Apple account. Building takes a few minutes; Homebrew installs Rust for it |
| Linux | `cargo install` from this repository, or the release binary | No money. Rust and a few system libraries |
| Agents | The Duckwalk agent skill (#144) knows these installs | Nothing |

**Why a source formula on macOS.** Since September 2026 Homebrew casks have to pass Gatekeeper, which means notarization and Apple's paid Developer Program. A formula that builds on the Mac is different: the app never crosses the network as a download, so it carries no quarantine flag and Gatekeeper doesn't stop it. The tap's CI builds and tests it on a macOS runner every week.

**Why SignPath and winget together.** A winget package can be unsigned, but an unsigned app still meets SmartScreen's warning the first time. Signing gives Windows a known publisher, and winget gives a one-line install.

## Not chosen

- **Microsoft Store, Apple notarization, a bought certificate:** money and accounts the project doesn't have, for a small free tool.
- **Asking people to click through the warnings:** it works, and the site still explains it, but it teaches people to ignore warnings, and it looks wrong on a work computer.
- **An installer (MSI, pkg):** the app already installs itself on Windows; an installer adds admin prompts and something more to maintain.

## Status

- SignPath: applied on 6 November 2026; waiting for their review. Until a release is signed, the Windows build is unsigned and the site says so.
- Homebrew: the tap works with `--HEAD`; after v0.1 its formula points at the release's source.
- winget: after the first signed release.

---

[← Decision 0002](0002-one-desktop-app-in-rust.md) · [Docs home](../README.md)
