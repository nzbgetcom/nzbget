# Project history: from nzbget/nzbget to nzbgetcom/nzbget

This page documents how this repository relates to the original NZBGet repository by Andrey Prygunkov ([hugbug](https://github.com/hugbug)), and answers a question that comes up regularly:

> Why is nzbgetcom/nzbget not a GitHub fork of nzbget/nzbget? Was the code just re-uploaded?

**Short answer:** the code was not re-uploaded. The complete original Git history is contained in this repository, commit for commit and with identical commit hashes. Only the GitHub "forked from" relationship is missing. That relationship is GitHub metadata, not Git history.

Every statement below can be checked with the commands in [Verifying it yourself](#verifying-it-yourself).

## Timeline

| Date | Event |
|---|---|
| 2007-11-28 | First commit in the history (`f2338cf`, Andrey Prygunkov). |
| 2021-06-03 | Last original release, **v21.1** (`b0d35f9`). |
| 2021-10-01 | Last original commit, `5e26d52` "#784: removed expired root certificate". The version string reads `21.2-testing` (tag `v21.2-r2333`). |
| 2022-11-18 | nzbget/nzbget is archived by its owner and becomes read-only. |
| 2022–2023 | Community forks keep the project alive. Among them is nzbget-ng/nzbget, whose maintainer later recommended treating nzbgetcom/nzbget as the official repository. |
| 2023-08-22 | First nzbgetcom commit on top of hugbug's history (`f982d89`, "don't use FIPS_mode_set with OpenSSL 3"). |
| 2023 → | nzbgetcom releases v22.0 onwards (latest stable at the time of writing: v26.3). |

## What the Git history shows

These are the measured facts, as of `develop` at `eb68e5e` (2026-10-30):

- **The original tip is an ancestor of this repository.** `git merge-base` between nzbget/nzbget `develop` and nzbgetcom/nzbget `develop` is exactly `5e26d52`, the last commit of the original repository.
- **Nothing from the original is missing.** nzbget/nzbget `develop` has 0 commits that are not in nzbgetcom/nzbget. All 2,333 original commits are present with the same SHA-1 hashes, the same authors and the same dates.
- **The original release tags were carried over unchanged.** For example, `v21.0`, `v21.1` and `v21.2-r2333` point to the same commit hashes in both repositories.
- **nzbgetcom/nzbget adds about 500 commits on top.**

Identical hashes are the strongest possible evidence of continuity. A commit hash covers the content, the author, the date and the entire chain of parent commits. A re-upload or a squashed import would produce different hashes.

## So why is it not marked as a fork on GitHub?

On GitHub, nzbgetcom/nzbget is a standalone repository. Its page shows no "forked from" label, and the API reports `fork: false`. The README nevertheless describes it as a fork of hugbug's project.

The repository was created by pushing the existing Git history into a new repository, rather than by pressing GitHub's "Fork" button. The project maintainers have not publicly given a specific reason that we could find, so the following are the **practical reasons commonly behind this choice**, not an official statement:

- **The upstream was dead.** nzbget/nzbget is archived. A GitHub fork's main benefit is the ability to open pull requests back to its parent, and that was no longer possible.
- **GitHub forks are second-class in some respects.**
  - Forks are excluded from code search unless they have more stars than their parent.
  - Issues are disabled by default on forks.
  - New pull requests default to targeting the parent repository.
  - A fork stays attached to its parent's fork network, and detaching it has historically required contacting GitHub Support.

  None of this suits a project that intends to be the main home of the software.
- **It needs to be its own primary repository**, with its own issue tracker, releases, Docker images and package-manager references, so that users and downstream packagers point to it rather than to an archived parent.

Things that live in GitHub rather than in Git, such as issues, pull requests, release pages and stars, cannot be pushed between repositories. They remain in the archived nzbget/nzbget repository. Only the code history moved.

## What changed since the original

The same comparison, at a high level (`nzbget/nzbget develop` vs `nzbgetcom/nzbget develop`: 724 files changed, about +102k / −57k lines):

- **Build:** autotools, the MSVC project and Travis CI were replaced by CMake presets, cross-toolchains and GitHub Actions.
- **TLS:** the build is now OpenSSL only, since GnuTLS support was removed in #554. TLS 1.3 is preferred.
- **New features:** these include SystemHealth diagnostics, server speed tests, per-server volume and article statistics, deobfuscation, importing NZBs from archives, hardlinking, a feed API, and more.
- **Libraries:** Par2 was replaced by par2-turbo, and yEnc decoding uses rapidyenc.
- **Download engine:** this still follows the original design. Each article is downloaded by its own thread, one article per NNTP connection at a time (`ServerPool`, `ArticleDownloader`, `QueueCoordinator`). The main change is that the queue coordinator now selects the article before the connection, to support a desired server per NZB.

## Verifying it yourself

```sh
git clone https://github.com/nzbgetcom/nzbget.git
cd nzbget
git remote add original https://github.com/nzbget/nzbget.git
git fetch original

# The original's last commit is the common ancestor:
git merge-base original/develop origin/develop        # -> 5e26d52...

# Commits in the original but not here (expect 0) / added here:
git rev-list --count origin/develop..original/develop  # -> 0
git rev-list --count original/develop..origin/develop

# Same tags, same hashes:
git ls-remote --tags origin   'v21.1' 'v21.2-r2333'
git ls-remote --tags original 'v21.1' 'v21.2-r2333'

# Size of the divergence:
git diff --shortstat original/develop origin/develop
```

## References

- Original repository (archived): <https://github.com/nzbget/nzbget>
- nzbget-ng notice recommending nzbgetcom/nzbget as the official repository: <https://github.com/nzbget-ng/nzbget>
- This repository: <https://github.com/nzbgetcom/nzbget>
