# Phase 5 local evidence privacy repair

Recorded 2026-11-12 after Plan 05-06, before dispatching 05-07. The parent's tree hygiene check found a local checkout path in the stderr field of `05-06-task1-refusal-red.json`. It was present in five unpublished commit trees. No credentials or private test content were involved; the path presentation violated the repository privacy rule.

The live remote main ref matched the Phase 4 closeout at `1d9c9356f1a880dd949b4560fcee918d4073f14d`, below the affected linear range. A private full-ref Git bundle, original report and workspace fingerprints were saved outside the repository. Only the report's checkout-path prefix was normalized in each affected tree. Rebuilt commits preserve author, committer, timestamps, messages and all other tree entries; descendant parent links necessarily changed. A compare-and-swap update selected the repaired local tip. No published commit or remote ref changed, and no force push, reset, stash or checkout was used.

Before/after checks established that each changed tree differs solely at that evidence path. All production and test source bytes are identical. The staged diff, existing unstaged summary corrections, untracked listing and generated state-cache bytes were preserved. The JSON exit code, command, TAP output, expected/actual behavior, assessment and failure diagnostic remain unchanged apart from path presentation. Current administrative references were then refreshed using the map below.

## Local reference map

| Previous local commit | Sanitized local commit |
|---|---|
| `778a8a583fd32780a059cad1529816dfc6f4bfaf` | `d3a1b3208fb010a4a87afa8bcfffc2291b1b0076` |
| `11ced2179088ac3c0f9b47f4d7000de8c12f9c3f` | `d7e2cbe8d7da476ddf98c754f5eae5e9ede2c473` |
| `ac5527cdc401e9122acc5e9eb8f3334cd37fdfbe` | `f9946c2fbed7e04228a271d2694be871278578af` |
| `385be8ae82e33fa82991834ecad1c7e17605e6ad` | `0135ca3dfff5a50b5e9d2230ca3a8704e9deef3f` |
| `030c03d81ef02e02f1ba611e93cf6ac3020c9768` | `fdba002d11b26d6226413bf4b3ef6e7748942bb3` |

## Verification and prevention

The repaired range passed `sh scripts/hygiene.sh --history f9dc36c..HEAD`. Tree hygiene and diff checks are required before this audit commit. The final source qualification remains Plan 05-12; source tests are not repeated solely for normalized evidence and commit parent changes.

The executor missed the existing requirement to normalize generated evidence before committing. At every plan handoff, the parent now checks current-tree hygiene and the just-completed plan's history range before dispatching the next executor. Executors continue to run tree hygiene before committing generated evidence. This is a local workflow guard, not a duplicate CI job or a relaxed privacy rule.
