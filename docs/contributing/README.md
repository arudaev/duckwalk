# Contributor guide

## Issues

Every piece of work starts as an issue with exactly one `type:` label.

| Type | Size | Title |
|---|---|---|
| `type:epic` | A milestone outcome; owns native sub-issues | `[M1] v0.1: the Flipper app over USB` |
| `type:story` | One user-visible behaviour, one PR | `[M1-S2] Walk mode: Harold walks the cursor on random paths` |
| `type:task` | Technical or delivery work, one PR | `[M0-S1] Contributor conventions and text-hygiene check` |
| `type:spike` | Time-boxed research ending in a written decision | `[M2-S1] Spike: one BLE profile with HID and a serial service` |
| `type:bug` | Something is broken | `Bug: …` |

Children are **native sub-issues** of their epic, and order between issues is recorded with the native **blocked by** relationship, not with checklists. `area:` labels say which part of the project an issue touches. `needs-triage`, `needs-design` and `needs-decision` say why an issue is waiting.

Milestones: **M0** repository, brand and CI · **M1** v0.1, the Flipper app over USB · **M2** Bluetooth mouse · **M3** desktop app · **M4** Harold's extra jobs.

Every issue and pull request lands on the [project board](project-board.md), which explains its views, fields and automation.

## Branches and commits

- Branch from `main`: `feat/`, `fix/`, `docs/`, `chore/`, `ci/` or `art/` plus a short slug.
- Commit messages and PR titles follow Conventional Commits. `art:` is for sprites, icons and other drawings.
- Pull requests merge with a **merge commit**; squash and rebase merges are off.

## Stacked pull requests

When a change needs several PRs, stack them:

1. PR 1 is based on `main`; PR 2 is based on PR 1's branch; and so on.
2. Titles start with the position: `[1/3] chore: …`, `[2/3] art: …`, `[3/3] feat(app): …`.
3. Every PR body lists the whole stack.
4. **Merge order:** retarget the next PR to `main` (`gh pr edit <n> --base main`), wait until it is mergeable, merge, repeat. Delete branches only at the end: deleting a base branch closes the PR above it instead of retargeting.

## Reviews and checks

Two workflows run on every PR:

- **PR conventions:** Conventional Commits title, at most 72 characters, and a linked issue (`Closes #n` or `Refs #n`; label `no-issue` for trivial fixes).
- **Text hygiene:** `scripts/check_text.py` on the changed files. It fails on invisible or format characters, unusual spaces, em dashes, and stock phrases in Markdown. Run it locally before pushing.

## Brand

- **Harold** is a yellow rubber duck with an orange beak, from the game VOYAGER // 56K. On the Flipper he is 1-bit pixel art drawn from the original 16 px sprite; keep his silhouette and proportions.
- **Colour:** the Flipper's orange (`#FF8200`) as the one flat colour field, dark ink, white space. No gradients, no generated imagery.
- **Voice:** calm and direct. Say what the thing does and who it helps; say plainly what is finished and what is not. No hype, no emoji decoration. En dashes (–), never em dashes.
