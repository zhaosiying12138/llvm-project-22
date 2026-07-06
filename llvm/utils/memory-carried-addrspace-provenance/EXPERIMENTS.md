# Memory-carried address-space provenance experiments

This directory contains local artifacts for the branch `recover-memory-carried-addrspace-provenance`.

## Reproducer

`godbolt_reproducer.cu` models the Godbolt/RFC CUDA pattern: a shared array, a private/local pointer array `digit_counters[4]`, a dynamic selector, a reload of a generic pointer from local memory, and a final `*p` dereference. The source uses inline PTX for thread id and barrier and macro definitions for CUDA attributes so it builds with `-nocudainc -nocudalib`.

## Experiment matrix

Complete IR/PTX artifacts are saved for both `-O0` and `-O3`:

- `artifacts/godbolt.original-clang21.O0.ll` / `.ptx`
- `artifacts/godbolt.original-clang21.O3.ll` / `.ptx`
- `artifacts/godbolt.patched-clang23.O0.ll` / `.ptx`
- `artifacts/godbolt.patched-clang23.O3.ll` / `.ptx`
- `artifacts/godbolt.before-infer.O0.ll` and `artifacts/godbolt.after-infer.O0.ll`
- `artifacts/godbolt.before-infer.O3.ll` and `artifacts/godbolt.after-infer.O3.ll`
- matching `before-infer` / `after-infer` PTX for O0 and O3

Because the complete CUDA frontend pipeline can reshape the source-level pattern, `artifacts/reduced-godbolt-pattern.ll` records the RFC/pass-level shape used for deterministic pass logs and PTX proof.

## Key conclusion

The reduced pass-level artifact demonstrates the intended transformation: a generic pointer reload remains a generic slot load, but InferAddressSpaces materializes a post-reload `addrspacecast` to `addrspace(3)`, letting the final data dereference become a shared-memory access in PTX. Full frontend O0/O3 outputs are retained for comparison against original clang and the patched clang pipeline.

## Logs

- `logs/tool-versions.md`: compiler/tool versions.
- `logs/memoryssa.full.log`: complete MemorySSA print output for the reduced artifact.
- `logs/aa-eval.full.log`: complete alias-analysis evaluator output or recorded fallback details.
- `logs/infer-address-spaces.debug.full.log`: `-debug-only=infer-address-spaces` output when available, otherwise pass-manager before/after fallback plus the exact failure.
- `logs/pass-prefixed-key-excerpts.log`: prefixed `[MemorySSA]`, `[AAEval]`, and `[InferAddressSpaces]` excerpts.

See `artifacts/ir-ptx-key-diff.md` for exact IR/PTX snippets.
