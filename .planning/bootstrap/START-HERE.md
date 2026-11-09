# Lynxplosion bootstrap

Prepared 2026-11-09 as pre-GSD setup for a new Atari Lynx emulator. The target
was empty when inspected: no source, Git repository, GSD config or generated
project/requirements/roadmap/state artifacts existed.

## Read before kickoff

1. Root `AGENTS.md` for owner direction and durable engineering policy.
2. `.planning/bootstrap/PROJECT-SEED.md` for project kickoff input.
3. `.planning/bootstrap/EMULATOR-DNA.md` for transferable engineering lessons,
   Lynx research, test/rights boundaries and open design questions.
4. Current `.planning/config.json` for GSD workflow defaults.

## Initialize the project

In a fresh LLM session whose working directory is this project, run:

    $gsd-new-project --auto @.planning/bootstrap/PROJECT-SEED.md

Use the installed session's exact syntax if it differs. This is a new project,
not a new milestone: the directory has not yet been initialized with generated
GSD project records. The owner wants routine, reversible work to advance
autonomously. The config enables auto-advance; the project instructions keep
material scope, rights, authority, publication and physical qualification
decisions explicit.

GSD should create and validate the formal project, requirements, rolling
roadmap and state. Treat the seed and research as dated input: check current
source revisions, fixture terms, BIOS/boot requirements and any existing
project files before accepting a claim. Keep recommendations distinct from
owner decisions. Do not start phase planning or implementation in this
initialization step. Return the generated artifacts and next useful GSD
command.

## Open before GSD planning

The Atari Lynx model/CPU profile and lawful/reproducible boot path affect the
first automated guest test. Research these early. The seed recommends a
source-built diagnostic and deterministic headless path, but it does not
select a BIOS policy, cartridge format, first milestone, language, API, license
or publication route.

Do not add copyrighted BIOS or commercial game bytes to source control or
shared CI. Refresh-rate calibration and physical ComLynx tests require
hardware and are not ordinary headless CI cases.
