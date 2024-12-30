# Contributing to Torr
Contributions are highly welcomed, as Torr strives to become a feature complete BitTorrent implementation in the future.

Make sure that changes are in line with the project direction, utilize the roadmap found in README.md or open an issue if unsure.

**Recommendation: For your first couple of PRs, start with something small to get familiar with the project and its development processes.**

## Guidelines
* Utilize modern C++ paradigms
* C is a welcomed language to interface with syscalls, see ``network/socket/cassync.h``
* Always use snake_case, except for macros
* Use std::expected error handling when exposing API functionality
* Commit messages should begin with a category ex: "network/socket: make tcp write asynchronous"

## Example Git workflow
The recommended way to work on Torr is by cloning the main repository locally, then forking it on GitHub and adding your repository as a Git remote:
```sh
git remote add myfork git@github.com:MyUsername/Torr.git
```

You can then create a new branch and start making changes to the code:
```sh
git checkout -b mybranch
```

And finally push the commits to your fork:
```sh
# ONLY run this the first time you push
git push --set-upstream myfork mybranch
# This will work for further pushes
git push
```

If you wish to sync your branch with master, or locally resolve merge conflicts, use:
```sh
# On mybranch
git fetch origin
git rebase master
```

## Communication

GitHub is the main platform for communication @ DevGev/Torr.
