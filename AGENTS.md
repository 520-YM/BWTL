# BWTL isolation requirement

The user explicitly required complete isolation from their earlier Codex–Creo integration on 2026-11-05.

- Maintain BWTL as an independent product, with separate source, deployment, startup/configuration, data, logs, backups, and Creo runtime session.
- Do not edit, reuse, merge, or route BWTL work through the legacy Codex–Creo bridge or its MCP tools. Use BWTL's own native interface and diagnostics for BWTL validation.
- Do not modify the legacy bridge's source, MCP settings, registrations, or data as a side effect of BWTL work.
- Preserve BWTL's existing chat history and memory. Keep model capabilities tied to BWTL's actual compiled implementation.
- Complete runtime isolation requires independent startup configuration and no co-loading of BWTL and the old bridge in one Creo process. Do not claim this is achieved based only on separate folders or command names.
- Historical audit before the foundation update (2026-11-05): the user's home config.pro registered both CreoSafeResident and BWTL. The foundation update removed only BWTL's owned block, preserving the legacy registration, and migrated BWTL to the isolated profile recorded in `%LOCALAPPDATA%\BWTL\isolation.json`.
- Foundation activation audit (2026-11-05): after the user confirmed no unsaved models, mixed session PID 58892 was normally closed. Production PID 61812 started through the independent launcher; loaded BWTL 0.21.3-local.foundation.1, no legacy bridge module, and preserved chat/memory/context hashes. Inspect current `foundation-update-status.json` and process state for later sessions. The launcher refuses concurrent BWTL access to the same data directory.

# Local update workflow

- Do not operate the user's mouse except as a last resort (explicit preference, 2026-11-05). Prefer files, CLI and BWTL's own native interfaces. Explain unavoidable UI steps first and minimize disruption.

- User preference recorded on 2026-11-05: future BWTL updates are developed, installed, and verified on this computer by default.
- User preference recorded on 2026-11-06: handle plugin development and updates in Codex. The BWTL chat window is for Creo tasks and has no development or upgrade controls.
- Source checkout: `C:\Users\Administrator\Documents\BWTL`. Managed installation: `%LOCALAPPDATA%\BWTL`. Inspect the current `active.json`, source changes, and active Creo work before an update.
- Back up affected configuration/data, preserve all existing chats and memory, retain the previous version for rollback, and document the change and how to verify/roll it back.
- Build and run checks appropriate to the change; verify through BWTL's own interface and diagnostics. Respect the isolation requirements above, and never use the old Codex–Creo bridge as a substitute for BWTL functionality or verification.
- Default scope is local installation. Pushes and remote releases require an explicit request. Account for unsaved models before restarting Creo.

# Feature source organization

- Keep the main implementation of each BWTL feature or closely related feature group in its domain subfolder under `src/功能`. Group features by shared behavior and keep unrelated features in separate files.
- Keep shared menu entries, command validation, and dispatch in their existing `src` integration files. Record the feature or feature group and its implementation path in `docs/feature-files.json` when adding or moving it.

# Public release packaging

- Keep credentials, diagnostic logs, company files and machine-specific settings out of new public artifacts. Publish a BWTL chat export only when explicitly requested, in a dedicated archive outside the installer.
