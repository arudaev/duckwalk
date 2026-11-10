# Agent guide

Read by Codex, Claude Code (through `CLAUDE.md`) and other coding agents. Humans: see [CONTRIBUTING.md](CONTRIBUTING.md).

## What this is

Duckwalk is a Flipper Zero app (C, built with `ufbt`) plus, later, an optional desktop app. Harold, a rubber debugging duck, keeps the computer awake by walking the cursor and works as a remote. Licence: GPL-3.0.

## Rules

- Follow [CONTRIBUTING.md](CONTRIBUTING.md): one issue per PR, Conventional Commits titles, stacked PRs for multi-part work, merge commits.
- Write every PR body from [the PR template](.github/PULL_REQUEST_TEMPLATE.md), even with `gh pr create --body-file`, which skips it: `## What and why`, `## Issue`, `## Stack` (stacked PRs only), `## Verification` as a checklist. #149 is the model; don't copy the shape of the latest PR. Add one `type:` label, the `area:` labels, the milestone and the assignee.
- Agents open pull requests ready for review. Agents never merge, never enable auto-merge, never force-push shared branches and never publish releases or catalog submissions.
- Official firmware only. Never add region-lock workarounds, radio transmission outside legal bands, or anything that bypasses the Flipper's intentional limits.
- Before committing prose, run `python scripts/check_text.py` and fix every problem. Write en dashes (–), never em dashes. Do not paste generated marketing language.
- No AI co-author trailers or "generated with" lines in commits or PRs.
- Keep secrets, personal data and private research out of this public repository.

## Commands

```bash
cd app && ufbt               # build dist/duckwalk.fap
cd app && ufbt launch        # run on a connected Flipper (qFlipper closed)
python scripts/check_text.py # text hygiene
```
