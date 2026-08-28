"""Generate release notes from the git history.

Only commits a player can recognise as a change to the mod are included. The
rules, in order:

  * Conventional Commit types `feat`, `fix` and `perf` are kept. Everything
    else (`refactor`, `chore`, `docs`, `test`, `build`, `ci`, `style`) is
    internal and dropped.
  * A commit reverted inside the same range disappears, and so does the revert
    itself. Work that was tried and undone never reaches the notes.
  * `[skip changelog]` anywhere in the message drops that commit.
  * A `changelog: <text>` trailer replaces the subject with <text>. Use it when
    the commit subject is accurate but means nothing to a player.

Usage:
  changelog.py [--from <ref>] [--to <ref>] [--version <x.y.z>] [--write]

With no --from, the most recent tag is used. --write prepends the section to
CHANGELOG.md instead of printing it.
"""
import argparse
import os
import re
import subprocess
import sys

SECTIONS = [("feat", "Added"), ("fix", "Fixed"), ("perf", "Improved")]
KEPT_TYPES = {name for name, _ in SECTIONS}

RECORD = "\x1e"
FIELD = "\x1f"

SUBJECT = re.compile(r"^(?P<type>[a-z]+)(?P<scope>\([^)]*\))?(?P<bang>!)?: (?P<text>.+)$")
REVERT = re.compile(r'^Revert "(?P<subject>.+)"$')
OVERRIDE = re.compile(r"^changelog:\s*(?P<text>.+)$", re.MULTILINE)


def run(*args):
    result = subprocess.run(args, capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit(result.stderr.strip() or f"command failed: {' '.join(args)}")
    return result.stdout


def latest_tag():
    result = subprocess.run(["git", "describe", "--tags", "--abbrev=0"],
                            capture_output=True, text=True)
    return result.stdout.strip() if result.returncode == 0 else None


def read_commits(since, until):
    span = f"{since}..{until}" if since else until
    raw = run("git", "log", "--no-merges", f"--format=%H{FIELD}%s{FIELD}%b{RECORD}",
              span)
    commits = []
    for record in raw.split(RECORD):
        record = record.strip("\n")
        if not record:
            continue
        sha, subject, body = record.split(FIELD)
        commits.append({"sha": sha, "subject": subject, "body": body})
    return commits


def collect(commits):
    """Drop reverted work, then classify what remains."""
    reverted = set()
    for commit in commits:
        match = REVERT.match(commit["subject"])
        if match:
            reverted.add(match.group("subject"))
            reverted.add(commit["subject"])

    entries = {name: [] for name in KEPT_TYPES}
    breaking = []

    for commit in reversed(commits):
        subject = commit["subject"]
        if subject in reverted:
            continue
        if "[skip changelog]" in subject or "[skip changelog]" in commit["body"]:
            continue

        match = SUBJECT.match(subject)
        if not match or match.group("type") not in KEPT_TYPES:
            continue

        override = OVERRIDE.search(commit["body"])
        text = override.group("text").strip() if override else match.group("text")
        text = text[0].upper() + text[1:]

        if match.group("bang"):
            breaking.append(text)
        else:
            entries[match.group("type")].append(text)

    return entries, breaking


def render(version, entries, breaking):
    lines = [f"## {version}", ""]
    if breaking:
        lines.append("### Breaking")
        lines.append("")
        lines += [f"- {item}" for item in breaking]
        lines.append("")
    for name, heading in SECTIONS:
        if not entries[name]:
            continue
        lines.append(f"### {heading}")
        lines.append("")
        lines += [f"- {item}" for item in entries[name]]
        lines.append("")
    if len(lines) == 2:
        lines.append("No player facing changes.")
        lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--from", dest="since", default=None)
    parser.add_argument("--to", dest="until", default="HEAD")
    parser.add_argument("--version", default=None)
    parser.add_argument("--write", action="store_true")
    args = parser.parse_args()

    since = args.since or latest_tag()
    version = args.version or "Unreleased"

    entries, breaking = collect(read_commits(since, args.until))
    section = render(version, entries, breaking)

    if not args.write:
        print(section, end="")
        return 0

    path = "CHANGELOG.md"
    existing = ""
    if os.path.exists(path):
        existing = open(path, encoding="utf-8").read()
    header = "# Changelog\n\n"
    body = existing[len(header):] if existing.startswith(header) else existing
    open(path, "w", encoding="utf-8").write(header + section + "\n" + body.lstrip("\n"))
    print(f"{path} updated with section {version}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
