# Agent guide — esp-zerocode-blocks

This repo is the firmware vocabulary of ZeroCode AI: code blocks, product
templates, frameworks, and the generator engine. **Read `CLAUDE.md`** for the
block contract and repo layout — it is the authoritative authoring guide for
any AI assistant working here.

Guided contribution flows (Claude Code loads these as skills; other tools:
read them as instructions):

- `.claude/skills/new-block/SKILL.md` — add a driver / behavior / device type
- `.claude/skills/new-product-template/SKILL.md` — add a ready-made product
- `.claude/skills/new-framework/SKILL.md` — add a framework (audio, display, a protocol stack)

Always verify before proposing a PR: `python3 scripts/check.py` (instant),
then `scripts/test-product.sh <id> --build` (real ESP-IDF build).

## The three declarations that are easy to skip and expensive to omit

All three are checked (`scripts/check.py` + the engine validator), all three
fail SILENTLY on real hardware when they are missing, and none of them is
visible in a compiler error. Read the named CLAUDE.md section before authoring
a block that touches shared hardware:

| declare | when | CLAUDE.md section |
|---|---|---|
| `requires_bus:` | your block does I2C/SPI transactions but does not configure the bus | Shared buses |
| `provides_bus:` | your block configures a bus other blocks will talk on | Shared buses |
| `bmgr:` | your block's pins should be visible to esp-board-manager's IO-conflict check when the product runs on a board | Boards (`board.yaml` + `bmgr:`) |

A product.yml **never names a board**: the catalog says what a product does,
and a separate `board.yaml` (schema `zc-board/1`) says what hardware one
user's product runs on. Two sample products carry one so CI keeps building the
board path; no other product may.

Two traps worth knowing before you hit them: a bmgr `periph_gpio` cannot drive
an **active-low** output safely (it configures the pin before writing the idle
level — a pulse to the load on every boot), and a YAML value that STARTS with
`{{` must be quoted (`pin: '{{cfg.gpio}}'`) or it parses as a flow mapping.

## The repo family

Four repos, each fetched at runtime except the platform itself:

| repo | owns | goes live by |
|---|---|---|
| **esp-zerocode-blocks** (here) | firmware vocabulary — blocks, templates, frameworks, the engine | `scripts/publish-catalog.sh` |
| esp-virtual-parts | diagram vocabulary — parts + boards | `scripts/publish.sh` |
| esp-zerocode-agents | agent vocabulary — prompts, tool lists, models | `scripts/publish.sh` |
| esp-zerocode-ai | the platform — studio, runner, tools, providers | image build + deploy |
