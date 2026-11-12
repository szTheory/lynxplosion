# Qualification portability and control restoration

Commit `735cfa1` changes four shell helpers after the Ubuntu qualification preflight stopped on 15 SC2015 diagnostics. ShellCheck0.9 rejects compound test/fallback forms accepted by macOS0.11. The scripts now use explicit `if` refusals with the negated equivalent tests; option bounds, identity comparisons, ownership, path refusals and source oracle values remain unchanged. Both ShellCheck versions accept all four repaired scripts. No warning is disabled.

The first focused page-mode run passed its source/tool/path refusal controls, then the restored positive tool process exited137. On the exact retained private fixture, both the original and copied executable verified as valid on disk; the original restored inode was killed by signal9 while a fresh-inode byte-identical copy executed `ca65 --version` successfully. Targeted OS logs had no matching termination entry, so the kernel reason is not asserted. The observed differential supports restoring the private mutated executable through a fresh file and atomic rename. The control now verifies that fresh file's bytes match the saved original before replacement; no code-signing or validation bypass was introduced.

After the restoration change, `suite.page_mode.controls`, `hygiene.manifests`, and `hygiene.manifests.self_test` passed. Before the tool termination, both page observation cases retained their exact declared skips and `api.video_page_mode` passed. The failed control run's owner marker was verified; all6405 node types, regular-file hashes and symlink text were inventoried and verified unchanged after atomic move to private quarantine. The six protected legacy fixture/oracle/build identities remain unchanged.

Current55-path fingerprint: `5de70b75e913919755c198ceef2a989ebd8c636908f37986fedbe17ebbd11dc8`. Four changed source identities:

| Path | SHA-256 |
|---|---|
| `scripts/build-page-mode.sh` | `46f2e06663f7bcb5165b12c692b1e0740669d52ad8e694437d818ebee337f100` |
| `scripts/generate-page-mode-oracle.sh` | `dad3807b9a36cacd7bfb9b828064f01967ceda43ed73ca2fb9c86b8acd25b569` |
| `tests/gates/manifests_controls.sh` | `da5eaea803c4eeffe05e3c645633ce478bf672eec69f05fe59b8914c49d29021` |
| `tests/gates/page-mode-script-controls.sh` | `6a2e366d302f424d203ec2e1611106f4be6e022f4632e8ac2dc97940599bb14c` |

The preceding macOS canonical gate passed at `fddbbb4`; Linux at that source stopped before CTest. Final broad qualification must run again on this repaired source after independent affected-scope review. No Linux gate pass, push, hosted CI or phase completion is claimed.
