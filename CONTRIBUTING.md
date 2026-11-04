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

1. **Start from an issue.** Pick one labelled `type:story`, `type:task` or `type:spike` with no assignee and comment to say you are on it. Epics (`[M1]`) are not taken directly; take one of their sub-issues. No issue for what you want to do? Open one first, so we can agree on it before you write the code.
2. **Fork the repository** (a star is appreciated too) and branch from `main` in your fork: `feat/<issue>-<slug>`, `fix/…`, `docs/…`, `chore/…` or `art/…`.
3. **Open a draft PR early** with `Closes #<issue>` in the description, so the issue shows up under **Development** in the PR's sidebar.
4. **Make it ready for review** when the acceptance criteria are met and the checks pass. Describe what changed, why, and how you tested it, with screenshots for anything you can see. The PR conventions check labels the PR by the files it touches and assigns it to you. Request a review from [arudaev](https://github.com/arudaev), or mention me in the PR if GitHub doesn't let you.
5. **Then give me a little time.** If you haven't heard from me within 72 hours, send me an email; the address is on [my GitHub profile](https://github.com/arudaev).

Pull requests are merged with a merge commit, so your commits land on `main` as you wrote them.

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

## Working with AI coding agents

I'm not on team AI or team no-AI. I care about the result. If an AI coding agent helps you make Duckwalk better than you could on your own, use it. Whatever you used, the pull request has to meet the same bar:

- It works, and you tested it, on a Flipper if it touches the app.
- It follows the conventions on this page and in the [contributor guide](docs/contributing/README.md).
- It is documented: comments where the code needs them, and the docs updated when behaviour changes.
- The PR is complete: a clear description, the issue linked, and a review requested.
- It has no AI watermarks, no generated filler, and nothing you wouldn't put your own name to.

What I don't want is a quick contribution that was easy to generate and never tested. Read what you submit, run it, and treat it as your own work, because it is. If you hand the work to an agent, point it at [AGENTS.md](AGENTS.md) first.

## More

- [Docs hub](docs/README.md): every page in one place, with the [architecture](docs/architecture.md) and the [developer guide](docs/development.md) (debugging, simulator, Bluetooth)
- [Contributor guide](docs/contributing/README.md): issues, stacks, reviews, brand
- [Code of conduct](CODE_OF_CONDUCT.md) · [Security policy](SECURITY.md)
- Agents: [AGENTS.md](AGENTS.md)

---

[← PC helper](desktop/helper/README.md) · [Docs home](docs/README.md) · [Next: Architecture →](docs/architecture.md)
