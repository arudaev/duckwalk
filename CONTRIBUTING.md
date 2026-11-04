# 03 · Contributing to Duckwalk

[← PC helper](desktop/helper/README.md) · [Docs home](docs/README.md) · [Next: Architecture →](docs/architecture.md)

Duckwalk is an open-source (GPL-3.0) Flipper Zero app: Harold, a rubber debugging duck, keeps your computer awake and works as its remote. Code, art, testing on different computers and documentation are all welcome.

This page is the short version. The details live in [`docs/contributing/`](docs/contributing/README.md).

## Five-minute setup

You need Python 3.8+ and a Flipper Zero on the official firmware.

```bash
git clone https://github.com/arudaev/duckwalk.git
cd duckwalk
python -m pip install --upgrade ufbt
cd app
ufbt            # builds dist/duckwalk.fap against the release SDK
ufbt launch     # uploads it and starts it on a connected Flipper (close qFlipper first)
```

No Flipper at hand? The walk planner and the settings file have host tests that run with any C compiler and CMake:

```bash
cmake -S tests -B build/tests
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure
```

To see every screen without a Flipper, render them with the [screen simulator](sim/README.md); CI does the same on every pull request.

Before every push: `ufbt` builds without warnings, the host tests pass and `python scripts/check_text.py` reports no problems.

## How work flows

1. **Pick an issue** labelled `type:story`, `type:task` or `type:spike` with no assignee. Epics (`[M1]`) are not taken directly; take one of their sub-issues. Comment to say you are on it.
2. **Branch from `main`:** `feat/<issue>-<slug>`, `fix/…`, `docs/…`, `chore/…` or `art/…`.
3. **Open a draft PR early** with `Closes #<issue>` in the description.
4. **Mark it ready for review** when the acceptance criteria are met and the checks pass. Pull requests are merged with a merge commit, so your commits land on `main` as you wrote them.

Work that needs several PRs is a **stack**: each PR is based on the one below it and titled `[2/3] feat(app): …`. See [stacked pull requests](docs/contributing/README.md#stacked-pull-requests).

## Rules that keep `main` releasable

| Rule | Why |
|---|---|
| PR titles follow [Conventional Commits](https://www.conventionalcommits.org/) (`feat(app): add walk mode`), at most 72 characters; types `feat fix docs refactor test chore ci perf build revert art` | The title becomes the merge commit on `main`; CI checks it |
| Every PR links an issue (`Closes #n` or `Refs #n`) | Traceability; CI checks it |
| Official firmware only. Nothing that bypasses the Flipper's intentional limits | The Apps Catalog rejects it, and so do we |
| Prose passes `scripts/check_text.py`: en dashes (–) not em dashes, no invisible characters, no stock phrasing | Public text should read like a person wrote it; CI checks it |
| Harold's look follows [the brand notes](docs/contributing/README.md#brand) | One recognisable duck |

Commits are authored by the person who wrote them. Do not add AI co-author trailers or "generated with" lines to commits or PRs.

## More

- [Docs hub](docs/README.md): every page in one place, with the [architecture](docs/architecture.md) and the [developer guide](docs/development.md) (debugging, simulator, Bluetooth)
- [Contributor guide](docs/contributing/README.md): issues, stacks, reviews, brand
- [Code of conduct](CODE_OF_CONDUCT.md) · [Security policy](SECURITY.md)
- Agents: [AGENTS.md](AGENTS.md)

---

[← PC helper](desktop/helper/README.md) · [Docs home](docs/README.md) · [Next: Architecture →](docs/architecture.md)
