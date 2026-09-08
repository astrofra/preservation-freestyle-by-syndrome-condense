# Freestyle project conversation archive

This snapshot covers all three local Codex sessions found for this project's
working directory, from September 6 to September 8, 2026: archive extraction,
repository text translation, native restoration, the WebGL port and direct XM
playback. It contains 52 user/assistant messages, plus the recorded tool activity.

The cutoff is the user's export request at **2026-09-08 16:48:22.516 UTC**
(18:48:22.516 in Europe/Paris). The work performed to create this export and its
delivery response are outside the snapshot, so the archive does not recursively
include itself. This describes the locally available project history; it makes
no claim about sessions on other devices or deleted logs.

## Files

| File | Contents |
| --- | --- |
| [project.original.jsonl](project.original.jsonl) | Combined source-language event log, with explicit internal omissions. |
| [project.en.jsonl](project.en.jsonl) | The same records, with all user/assistant dialogue translated into English. |
| [conversation.original.md](conversation.original.md) | Readable original dialogue, without duplicated UI events or tool traces. |
| [conversation.en.md](conversation.en.md) | Readable English translation of that same complete dialogue. |
| [manifest.json](manifest.json) | Source session IDs, cutoff, source and output hashes, counts and exact omission locations. |
| [translations.en.json](translations.en.json) | Translation text keyed by session ID and original source line. |
| [export.py](export.py) | Reproducible export and verification script using only Python's standard library. |

## Fidelity and scope

The original dialogue is extracted from the actual local JSONL records, not
reconstructed from summaries. All 1,007 source records retain their order and
one-to-one line correspondence within the combined file. The manifest maps each
session's line numbers to the combined file. Unmodified original records retain
their exact bytes. All 147 tool calls and 147 tool results are retained, including
recorded errors, commands, patches and image payloads.

This is **not a byte-for-byte, unfiltered backup of Codex's internal runtime log**.
Private reasoning, system/developer instructions and internal turn/compaction
context are omitted. Each omitted record is replaced by an explicit
export_omission object at the same position. Session metadata is restricted to
provenance fields and marked as filtered. The manifest lists every affected line.
No user dialogue or public assistant response is omitted. App-supplied user
context remains in the JSONL, but is not counted as a message in the readable
conversation.

The English translation preserves the requests, qualifications, uncertainties,
progress reports, failures, successes and historical claims without summarizing
or retrospectively correcting them. For example, the earlier WAV delivery and
its measurements remain in place before the later direct-XM delivery. “ISO” is
retained in the user's WebGL request; the next assistant message explicitly
records its interpretation as parity with the native version.

Only conversational prose is translated, including its duplicate UI events and
task-completion messages. Commands, code, source patches, tool output, hashes,
paths, links, timestamps and IDs are preserved. French text appearing inside
source files or tool output therefore remains as recorded. Already-English
context stays in English. Decimal punctuation and French MB/KB unit names are
localized without changing values. Original links are preserved even when they
refer to this workstation.

The English Markdown is a reading aid; use the JSONL for the recorded execution
trace. No external translation service was used. Original local session files
were opened read-only and were not modified by the exporter.

## Verification and regeneration

Run this from the repository root to validate the delivered hashes, JSONL
structure, message counts, technical literals and unchanged tool records:

    python codex_log/export.py --verify

To regenerate the same snapshot on the original workstation:

    python codex_log/export.py --codex-root C:\Users\fra\.codex

Regeneration uses the frozen source-line boundaries listed in the script and
checks source-prefix hashes against the existing manifest. It does not append
later messages or discover new sessions automatically.
