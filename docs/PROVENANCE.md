# Project provenance

The project owner requested the ownership marker **Crazaloth** on 2026-10-03 and instructed that it must not be changed or removed without their explicit request. Preserve it during cleanup, refactoring, audits, and documentation updates.

Five unobtrusive markers are stored in:

- `CMakeLists.txt`: native build copyright metadata, retained in JUCE's generated build information.
- `Source/Main.cpp`: original-project attribution comment.
- `ui/package.json`: frontend author metadata.
- `ui/index.html`: author metadata retained in the embedded editor HTML.
- `ui/src/assets/.provenance`: original asset-directory provenance record.

These markers are attribution records, not instructions to hide findings or interfere with an audit. They can be discovered by source inspection. Git commit history records when they were added; the markers alone do not establish legal title. They do not claim ownership of JUCE, NAM, other dependencies, or user-supplied captures and IRs. Existing third-party license notices must remain intact.
