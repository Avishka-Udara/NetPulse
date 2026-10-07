# Release acceptance checks

Use a separate `--data-dir` profile. Never reset or replace a user's real database during testing.

## Taskbar and lifecycle

- Open/close the widget menu and Explorer taskbar context menu repeatedly, including while traffic flows. Verify monitoring continues and placement recovers.
- Fill the taskbar, change app order, enable auto-hide, and change scaling/monitors. The widget must move or fall back to the tray without losing accounting samples.
- Restart Explorer in a disposable test account. Confirm tray recreation, event-hook reconnection, and no duplicate widget.
- Switch to tray-only mode, save, restart, and confirm no widget or placement scanner starts. Switch back and confirm placement resumes.
- Test suspend/resume, network disconnect/reconnect, midnight, monthly reset, and normal exit/restart.

## Settings and recovery

- Mouse interaction should not show dotted focus boxes. Tab/Shift+Tab and keyboard navigation must show focus and reach every control, including `?`.
- Validate invalid typed time, allowance, offset, width and font size. Check unsaved feedback and discard confirmation.
- Export settings, change the plan, restore the backup, and verify Windows startup and history stay unchanged. Invalid imports must not mutate active settings.
- Back up history during traffic; reopen a copy and run SQLite integrity_check. Never replace a live database. Recovery requires exiting NetPulse, preserving the original folder (including WAL/SHM), and restoring a known-good backup.
- Test insufficient space/read-only destinations: report failure and preserve the existing backup.

## Performance

Use `tools/measure-resources.ps1 -ProcessId <id> -Minutes 60 -OutputPath <file.csv>` for idle widget mode, tray-only mode, active traffic, and settings interaction. Compare median/p95 CPU and private/working memory, plus handle/thread trends. Record OS/build, scaling, mode, sampling duration, and whether UI automation was running. Task Manager's memory figure and working set are different metrics; neither one short sample nor a single machine establishes a universal limit.

## Distribution

Run native/SQL/startup tests and check clean install, upgrade, uninstall, data preservation, matching source archives and checksums. A matching version tag creates a draft release; review it and the outstanding limits before publishing. Signing requires the maintainer's certificate and is not supplied by this repository.
