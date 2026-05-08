# YSX Third-Party Provenance

## riscv-opcodes

- Source URL: https://github.com/riscv/riscv-opcodes
- Local path: `third_party/riscv-opcodes`
- Snapshot command: `git clone --depth 1 https://github.com/riscv/riscv-opcodes /tmp/ysx-third-party.XXXXXX/riscv-opcodes`
- Copy command: `rsync -a --delete --exclude='.git' /tmp/ysx-third-party.XXXXXX/riscv-opcodes/ third_party/riscv-opcodes/`
- Upstream commit: `ef103b65c682e7cb705cff67898c515f5c63175c`
- Policy: this directory is an unmodified upstream snapshot. Do not edit files in place; refresh from upstream and update this provenance file instead.
- Snapshot note: upstream `.git` metadata was removed from the local copy.

## riscv-isa-manual

- Source URL: https://github.com/riscv/riscv-isa-manual
- Local path: `third_party/riscv-isa-manual`
- Snapshot command: `git clone --depth 1 https://github.com/riscv/riscv-isa-manual /tmp/ysx-third-party.XXXXXX/riscv-isa-manual`
- Copy command: `rsync -a --delete --exclude='.git' /tmp/ysx-third-party.XXXXXX/riscv-isa-manual/ third_party/riscv-isa-manual/`
- Upstream commit: `2d034e16e3edeaa631aeb863adf8ef3a0b743aad`
- Policy: this directory is an unmodified upstream snapshot. Do not edit files in place; refresh from upstream and update this provenance file instead.
- Snapshot note: upstream `.git` metadata was removed from the local copy.

## ysx-opcodes

- Source URL: project-local YSX custom opcode overlay; no external upstream.
- Local path: `third_party/ysx-opcodes`
- Snapshot command: project-local creation under `third_party/ysx-opcodes/extensions/rv_xtinyv`
- Upstream commit: not applicable.
- Policy: this directory contains YSX-owned opcode definitions that layer on top of the upstream opcode snapshot.
