# Phase 3 local history privacy repair

Recorded 2026-11-10 during plan 03-03 verification. The full gate found a
local checkout path in a generated RED-test JSON record from plan 03-02.
The current record had already been normalized; six earlier unpublished
commit trees still carried its historical copy.

The parent checked the live remote main ref against the local remote-tracking
base, confirmed the entire affected range was local and linear, normalized
only that evidence file in those historical trees, and rebuilt descendant
commit parent links. No remote ref was changed. No published commit was
rewritten. A compare-and-swap ref update selected the repaired local tip.

Before/after checks established that the final committed tree was identical
and that the unstaged diff, staged diff, untracked listing and generated state
cache bytes were unchanged. Existing uncommitted plan 03-03 implementation
work was preserved without a stash, reset or checkout. Administrative commit
references in current planning records were refreshed using this map; test
results, commands and observed behavior were not changed.

## Local reference map

| Previous local commit | Sanitized local commit |
|---|---|
| `d37170dcdee06f2a44e3fcf7d9fd02fe85374371` | `a1fd5839f85b348761668cb99c9e8c27e02b8555` |
| `573efc5a1fd42331ee64d278a18c293cd0bbcd61` | `06f29b8502f29f7e2f0bd0541988b79c612f342b` |
| `f373a11f9d68657cafe1e997fe8aa1add2861b52` | `602b53a095912cab84f1d0ab4a95c0da491132f5` |
| `cec423c9c2f0bb1badc490de538e9fbb40ef9f10` | `68902a57f6cfac6b5416f4da708743905beeaa51` |
| `c573323a057fd7bf8f276dd6a637aa774f6020be` | `21470465617474b6b56e13ba5927b431e2fa9946` |
| `a4faba07edb86f289488d7f0ee5b565742402d2b` | `a5e2ca6ba2479101cc00ef038a9e83dc9d2aedab` |
| `ab328f6156107fb93446f1d02dab84ce1b642b83` | `a923e4f15da12b746a13e27ce9b017b0a778c453` |

## Prevention

Generated TDD reports can contain checkout paths even when the invoked command
is relative. Keep raw private logs in temporary storage; normalize only path
presentation in repository evidence and run tree hygiene before committing
generated records. Preserve exit status, test identity, intended failure and
failing-direction provenance. Never weaken the privacy gate or rewrite a
published range to repair a local evidence artifact.
