# Project board

[Duckwalk](https://github.com/users/arudaev/projects/9) tracks the work in this repository. Issues describe what will be delivered; pull requests are the review queue. Epics and blocked issues stay off the daily board.

## Views

| View | Layout | Filter | Purpose |
|---|---|---|---|
| Open work | Board, Status columns; Priority ascending | `is:issue is:open -label:"type:epic" -status:Blocked` | Stories, tasks, spikes and bugs being worked on |
| Art review | Table | `is:issue label:"area:brand","needs-design"` | Sprites, icons, banner and screen layouts, with their Design review state |
| Milestones | Table | `is:issue label:"type:epic"` | One epic per milestone and its sub-issue progress |
| Waiting | Table, grouped by Milestone | `is:issue is:open status:Blocked` | Issues waiting on another issue or a decision |
| Pull requests | Table, grouped by Status | `is:pr is:open` | Review queue |
| Completed | Table, newest Updated first | `is:closed` | Closed issues and merged or closed PRs |

Completed also shows PRs that were closed without merging and issues closed as not planned: check the native state before treating something as shipped.

## Fields

| Field | Meaning |
|---|---|
| Status | Backlog · Ready · In progress · In review · Blocked · Done |
| Priority | P0 blocks the milestone · P1 needed for it · P2 nice to have |
| Size | XS under 2 h · S half a day · M 1–2 days · L 3–5 days · XL split it |
| Design review | Not started · Changes needed · Approved; for art and screen layouts only |
| Start date | First time the item went In progress; set once |
| Target date | Only a real deadline, set by the maintainer |
| Completed date | Issue closure or PR merge date |
| Milestone, Labels, Assignees, Parent issue, Sub-issues progress, Linked pull requests, Reviewers | Native GitHub data |

New issues need Priority and Size before they move to Ready. Epics get no size. Dates are never estimated after the fact.

## Automation

The board uses GitHub's built-in project workflows:

| Trigger | Result |
|---|---|
| Issue or PR opened in this repository | Added to the board |
| Sub-issue added | Added to the board |
| Item added | Backlog |
| PR linked to an issue | In progress |
| Changes requested / PR approved | In progress / In review |
| Issue closed / PR merged | Done |
| Issue reopened | Ready |
| Card moved to Done | Does not close the issue (auto-close is off) |
| Completed work ages | Stays visible (auto-archive is off) |

Ready, Blocked, Priority, Size, Design review and accepting an epic are the maintainer's decisions. The board never closes issues or merges pull requests.
