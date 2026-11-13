# Repository policy followups (2026-11-13)

Owner direction: follow the recommended repository hardening and commit checked
changes on main. Both policies were read before mutation and verified after.
No collaborator message, release, visibility or distribution change was made.

- Actions remain enabled with the existing allowed-actions policy. The
  `sha_pinning_required` field changed false→true; the sole current action was
  already pinned to a full commit. See[GitHub's Actions policy API](https://docs.github.com/en/rest/actions/permissions).
- [Main CI ruleset](https://github.com/szTheory/lynxplosion/rules/27214244) is active
  for `refs/heads/main`. It requires context `check` from GitHub Actions
  integration15368, with strict up-to-date checking. Effective branch-rules API
  confirms it applies to main.
- The verified repository owner, szTheory, has the sole `always` bypass actor.
  This preserves the explicitly authorized checked-main workflow and records
  bypass use; it is not an exempt/unlogged bypass or a grant to all admins.
  Other actors remain subject to the check. See[GitHub's ruleset API](https://docs.github.com/en/rest/repos/rules).

The owner bypass means this policy does not claim that hosted CI must finish
before an owner push. Project policy still requires checked changes, normal hooks,
independent review where configured and inspection of CI at the exact pushed
commit. No protection was removed; there was no prior ruleset or legacy main
protection. No PR, branch or external review requirement was added.

Private before/request/after/effective-rule evidence is in
`/private/tmp/lynx-repo-settings-e_hoqc3m/`. The final metadata push and its hosted
CI provide a practical check of the preserved owner workflow and SHA policy.
