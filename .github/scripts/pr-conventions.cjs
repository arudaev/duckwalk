// Pull-request conventions: Conventional Commits title, a linked issue, the
// PR template's sections in the body, and assignment of the PR author to the
// PR and the issues it completes.
// Runs from the default branch under pull_request_target; never executes PR code.

const TYPES = ['feat', 'fix', 'docs', 'refactor', 'test', 'chore', 'ci', 'perf', 'build', 'revert', 'art'];
// Stacked PRs may start with their position: `[2/3] feat(app): ...`.
const TITLE = new RegExp(`^(\\[\\d+/\\d+\\] )?(${TYPES.join('|')})(\\([a-z0-9][a-z0-9/-]*\\))?!?: \\S.*$`);
const MAX_TITLE = 72;
const BOTS = /\[bot\]$/;

function checkTitle(title) {
  const problems = [];
  if (!TITLE.test(title || '')) {
    problems.push(`Title must look like \`type(scope): summary\` (optionally prefixed with \`[n/N] \` in a stack) with type one of ${TYPES.join(', ')}.`);
  }
  if ((title || '').length > MAX_TITLE) problems.push(`Title is ${title.length} characters; keep it at most ${MAX_TITLE}.`);
  if (/\.$/.test(title || '')) problems.push('Title must not end with a period.');
  return problems;
}

// The sections of .github/PULL_REQUEST_TEMPLATE.md every body keeps; a
// stacked PR (title `[n/N] ...`) also lists its stack.
const SECTIONS = ['What and why', 'Issue', 'Verification'];

function checkBody(title, body) {
  const text = (body || '').replace(/```[\s\S]*?```/g, '').replace(/<!--[\s\S]*?-->/g, '');
  const has = (name) => new RegExp(`^##\\s+${name}\\s*$`, 'mi').test(text);
  const wanted = /^\[\d+\/\d+\] /.test(title || '') ? [...SECTIONS, 'Stack'] : SECTIONS;
  const missing = wanted.filter((name) => !has(name));
  return missing.length
    ? [`Fill in the PR template (.github/PULL_REQUEST_TEMPLATE.md); missing ${missing.map((n) => `\`## ${n}\``).join(', ')}.`]
    : [];
}

// Returns { closes: [n], refs: [n] } for same-repository references outside code blocks.
function issueReferences(body, owner, repo) {
  const text = (body || '').replace(/```[\s\S]*?```/g, '').replace(/<!--[\s\S]*?-->/g, '');
  const escaped = `${owner}/${repo}`.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const target = `(?:#(\\d+)|https://github\\.com/${escaped}/issues/(\\d+))\\b`;
  const collect = (keywords) => [...text.matchAll(new RegExp(`\\b(?:${keywords})\\s*:?\\s+${target}`, 'gi'))]
    .map((m) => Number(m[1] || m[2]))
    .filter((n, i, all) => all.indexOf(n) === i);
  const closes = collect('close[sd]?|fix(?:e[sd])?|resolve[sd]?|issue');
  const refs = collect('refs?|part of|relates to').filter((n) => !closes.includes(n));
  return { closes, refs };
}

function evaluate(pr, owner, repo) {
  const problems = checkTitle(pr.title);
  const labels = (pr.labels || []).map((l) => l.name);
  const exempt = BOTS.test(pr.user?.login || '') || labels.includes('no-issue');
  const refs = issueReferences(pr.body, owner, repo);
  if (!exempt && refs.closes.length + refs.refs.length === 0) {
    problems.push('Link an issue in the description: `Closes #n` for the issue this PR completes, or `Refs #n`. Add the `no-issue` label only for trivial fixes.');
  }
  if (!exempt) problems.push(...checkBody(pr.title, pr.body));
  return { problems, refs, exempt };
}

async function run({ github, context, core }) {
  const { owner, repo } = context.repo;
  const { data: pr } = await github.rest.pulls.get({ owner, repo, pull_number: context.payload.pull_request.number });
  const { problems, refs } = evaluate(pr, owner, repo);
  const author = pr.user.login;

  if (!BOTS.test(author) && pr.state === 'open') {
    if ((pr.assignees || []).length === 0) {
      await github.rest.issues.addAssignees({ owner, repo, issue_number: pr.number, assignees: [author] });
      core.info(`Assigned PR #${pr.number} to ${author}.`);
    }
    for (const number of refs.closes) {
      const { data: issue } = await github.rest.issues.get({ owner, repo, issue_number: number });
      if (issue.pull_request || issue.state !== 'open' || (issue.assignees || []).length > 0) continue;
      try {
        await github.rest.issues.addAssignees({ owner, repo, issue_number: number, assignees: [author] });
        core.info(`Assigned issue #${number} to ${author}.`);
      } catch (error) {
        core.warning(`Could not assign #${number} to ${author}: ${error.message}`);
      }
    }
  }

  await core.summary
    .addHeading('PR conventions', 3)
    .addList(problems.length ? problems : ['Title, issue link and description look good.'])
    .write();
  if (problems.length) core.setFailed(problems.join('\n'));
}

module.exports = run;
module.exports.checkTitle = checkTitle;
module.exports.issueReferences = issueReferences;
module.exports.checkBody = checkBody;
module.exports.evaluate = evaluate;
