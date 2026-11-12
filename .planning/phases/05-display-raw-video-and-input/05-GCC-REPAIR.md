# GCC compatibility repair

The Linux canonical gate at `1700e10887a64f927631f094c97c1117c8dbf025` passed lint and stopped during GCC13.3 compilation before CTest: one function parameter shadowed the global `symbols` declaration and two nested aggregate initializers triggered missing-braces warnings under `-Werror`. That source passed the preceding complete macOS gate; no passing Linux result is claimed.

Commit `9e14687` only renames the local path parameter (both declaration/use) and uses standard C17 `{0}` aggregate initialization for the existing frame/packet arrays. No data flow, zero-initialization behavior, branch, buffer size, policy or assertion changes. GCC13.3 syntax checks with all project warning/error flags pass for production consumer, test consumer and determinism tests using the current read-only source mount. macOS affected targets rebuilt and five focused tests passed: consumer.frame_replay, consumer.replay_controls, consumer.replay_restore, det.video_paths and det.video_sensitivity (9.67 seconds total).

Full55-path fingerprint: `a0d5463d3b3e3a87973b076db23187281f9c965fcfe2e721da28ffeb6c543133`; only these two hashes differ:

| Path | SHA-256 |
|---|---|
| `consumer/main.c` | `5a74ae0262c8d2548b28333564079ef4944d2202016f835cce0a1a002dedb388` |
| `tests/det/test_det.c` | `fbd7802db04a8157f9463f08e69a02a6dc11dbb5b0de4223427cb2dfef0f1b4d` |

Independent mechanical code delta review and final-source environment qualification remain pending. Prior security verdicts remain applicable to identical control/data-flow behavior; no new security audit or risk acceptance is represented.

The independent two-path code delta review returned clean at `9e1468717c19c827e2d822f51667e45464274788`, confirming identical path flow and C17 zero initialization. The preserved independent Ubuntu clone was then fast-forwarded to that commit; complete configure/build passed under GCC13.3 with project warning/error flags. This is focused build evidence, not a complete Linux canonical gate.
