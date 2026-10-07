# Changelog

## 1.2.0

- Add tray-only mode: fresh launches avoid the taskbar accessibility worker; mode changes pause/resume it.
- Add tracking details with session start, monitored adapter names and daily-reset state.
- Add settings export/restore, consistent SQLite history backups, storage retry, and recovery guidance.
- Preserve existing backups if snapshot creation or replacement fails.
- Add unsaved-settings feedback, discard confirmation and inline validation.
- Respect Windows focus visibility: cleaner mouse interaction, retained keyboard focus indicators.
- Add version-tagged draft-release automation and an opt-in, read-only resource measurement script.
- Extend backup, import and settings persistence regression coverage.

## 1.1.1

- Scope placement events to Explorer and reduce fallback scanning.
- Reuse widget rendering buffers/fonts and avoid unchanged tray updates.
- Simplify decorative borders and publish GPL source, README and build tooling.

## 1.1.0

- Add compact transparent widget, appearance settings, daily/cycle resets and Windows startup.
- Add exact integer byte allocation, accounting regression tests and Windows installer.
