# Validation status



## Compact widget and counter audit



Current executable: **816,640 bytes**. Widget width reduced from 286 to 184 logical pixels (35.7%), retaining both live rates and daily direction totals. The settings-window layout is unchanged.



Corrections: automatic selection now requires a physical interface with a default route and excludes filter/endpoint interfaces. A dual-stack adapter is still counted once. Transient counter-query failures retain the last successful baseline; independent counter resets preserve the unaffected direction. Final samples are collected before suspend/shutdown. Local-day bounds refresh when the system time/settings change.



Validation completed:



- `build.bat`: all three native suites pass without compiler warnings.

- Counter tests: stable interface identity despite row reordering, no double counting on repeated polls, independent counter resets, new/removed interfaces.

- Exact-byte disk test: **134,217,728 download bytes and 16,777,216 upload bytes** (128 MiB / 16 MiB) remained exact after flush and database reopen. Yesterday/next-day records and manual quota adjustments were isolated correctly.

- `build/test_network.exe --live`: measured **837,949 download bytes / 150,569 upload bytes** during a live interval. Both were within the bounds from independent `GetIfEntry2` reads before/after the collector, accounting for time between API calls.



These tests validate adapter accounting, not agreement with an ISP. Physical-adapter counters include local traffic and overhead. The reported ISP discrepancy has not been reproduced against provider records, and existing aggregate history cannot be retroactively corrected into internet-only data. No historical records were changed. The compact widget has been compiled; the preceding visual checks below refer to the wider design.



## Minimalist UI and daily totals update



The preceding executable was **815,616 bytes**. Both native suites passed again after adding daily direction totals and redesigning the interface. New database assertions verify combined peak/off-peak download and upload separately, half-open daily boundaries, pending samples, and independence from quota adjustments.



The running Overview, Data plan, and Taskbar pages were inspected with Windows Computer Use. Controls remain native and keyboard accessible; custom-drawn buttons provide focus outlines. Paired labels were reordered to precede their associated controls. Settings pages no longer repaint their form every second. Daily totals are loaded from the ledger at startup and day boundaries, then updated incrementally.



The older end-to-end smoke report below covers the preceding UI. Its automation has been updated for the new navigation but has not been rerun after the redesign. Memory/CPU measurements below also describe that preceding build, not a new performance claim.



## Verified on 2026-10-07



Built with LLVM-MinGW 20260922 (x64 UCRT), SQLite 3.53.4, C11, and size optimization. `build.bat` completed without compiler warnings. The resulting `build/NetPulse.exe` is **809,984 bytes** and imports only Windows system DLLs; SQLite is statically linked.



### Native tests: passed



`build/test_core.exe`: time-input validation, daytime/overnight schedules, exact byte conservation across boundaries, counter resets, monthly reset calculation, short months, leap years, and DST boundary ordering.



`build/test_db.exe`: pending usage, flush idempotency, cycle queries, manual corrections, resetting usage, and retaining/retrying data after a write failure.



### SQL tests: 4 passed



`python tests/test_sql.py` exercises the SQL statements extracted from `src/db.c`: half-open cycle queries, corrections without deleting raw traffic, rollback/retry and disk reopen, session updates, and export query ordering.



### Windows smoke test: passed



`python tests/smoke_windows.py` on **Windows 11 build 26200**, 1920x1080, 125% taskbar scaling:



- Launched the actual executable with an isolated profile in `build/smoke-32211be1`.

- Confirmed the widget is a child of Explorer's taskbar.

- Verified distinct left, center, and right positions inside the taskbar rectangle.

- Saved configurable hours, caps, monthly reset, and offset to the profile INI.

- Recreated the widget with the Reattach command.

- Committed a manual usage adjustment through the settings confirmation dialog.

- Reopened settings and checked saved values.

- Exited cleanly; verified the session uptime record and SQLite integrity.

- Visually inspected settings and an on-screen capture of the taskbar widget. Fixed the Windows 11 transparency issue by using an opaque layered child window.



Evidence: `build/smoke-32211be1/report.json`, `settings.png`, `settings-configured.png`, and `widget-on-screen.png`. The test leaves normal `%LOCALAPPDATA%/NetPulse` settings and history untouched.



### Performance observations



| Metric | Observation |

| --- | --- |

| Executable | 791 KiB |

| Working set after settings closed | 18,419,712 bytes (17.6 MiB) |

| Peak working set during smoke test | 18,599,936 bytes (17.7 MiB) |

| CPU over a 15-second smoke interval | 0.52% of one logical CPU; approximately 0.033% of 16-thread system capacity |



The original **less than 10 MB memory target is not met**. The CPU sample is short and includes normal network activity; extended profiling is still needed. The disk-write target of less than 10 KB/hour conflicts with SQLite WAL commits every minute and is not claimed.



## Remaining compatibility checks



- MSVC and GitHub Actions builds; only LLVM-MinGW was run locally.

- Windows 10 and other Windows 11 builds, multi-monitor/DPI configurations, vertical taskbar, auto-hide, and full-screen apps.

- Actual Explorer process restart (the Reattach command was tested without restarting the user's Explorer).

- Theme transitions, keyboard-only navigation, and the CSV save dialog (the export SQL was tested).

- Long-running throughput accuracy, VPN transitions, physical reconnects, suspend/resume, and actual OS shutdown.

- Extended memory/CPU/disk profiling and live billing/DST transitions (schedule calculations were tested natively).



The widget cannot reserve space in Explorer's app-button layout. The update below adds periodic free-space detection and a tray-only fallback.





## Compact transparent taskbar update — 2026-10-07



- Built with LLVM-MinGW without warnings: standalone executable 819,712 bytes.

- All four native suites pass: core, database, network, and placement.

- Placement regressions cover left/right/center preferences, crowded fallback, full taskbars, exact-fit gaps, overlapping obstacles, and too-small bounds.

- Read-only live probe on Windows 11 at 125% DPI found a fitting 170-physical-pixel gap at x=1396 on a 1920×60 taskbar. The first call correctly hid pending an accessibility snapshot; subsequent calls returned the gap. The sandbox denied accessibility access and correctly produced no verified fit; the authorized external probe succeeded.

- Widget is 136×32 logical pixels with a color-key transparent background, compact byte units, and system-theme text. Unknown or stale layouts fall back to the notification icon without stopping recording.

- Visual verification of transparency, light/dark transitions, and live crowded-taskbar animations remains pending: desktop inspection was blocked by automatic approval review authentication (401). Earlier screenshots and smoke tests above describe older builds, not this rendering change.

- A disposable `build/compact-profile` instance was launched for inspection; its running state could not subsequently be checked through desktop tools. It does not use the normal profile.

- Accessibility scans run independently of sampling. The current periodic scan/placement design can briefly overlap controls during animations; it does not reserve taskbar space. CPU/memory impact of the scanner still needs measurement.



## 1.1.0 release candidate - 2026-10-08

Implemented:
- Neutral light/dark settings, understated tabs, restrained blue primary actions, a single top-right `?`, accessible Help and about name, and a native About dialog with Avishka Udara's GitHub link.
- Themed native dropdowns, bounded visible rows, typed/preset time inputs, removal of internal filter/endpoint interfaces from adapter choices, appearance customization, and current-user startup toggle.
- Persistent daily-display resets independent of billing-cycle adjustments and raw history.
- Exact integer proportional byte allocation, including overflow-safe large-counter cases. Pending storage flush reserves capacity for a boundary-spanning sample.
- Taskbar menu detection preserves placement during menus and briefly during recovery. Layout-event-triggered scans plus a five-second fallback replace continuous scanning. A full UI Automation cache experiment was rejected after a worse CPU sample.
- GPL-3.0-only source, contributor/release/porting documentation, Windows installer, matching source archive packaging, and CI configuration.

Verification:
- Five native suites pass: core, database, network, settings persistence, placement. Database tests cover repeated daily reset, failed reset, raw-history preservation, future traffic, midnight, and restart.
- Isolated registry integration test passes startup enable/disable/idempotency without modifying the real Run entry.
- Live network deltas: 34,453,644 download / 899,655 upload bytes within independent GetIfEntry2 bounds.
- Current overview, top-right help placement, restrained tabs, and dropdown colors visually inspected on Windows 11. Final adapter-list filtering and seven-row cap were added after this inspection and still need visual confirmation.
- Fresh background-only 30.0-second sample: 0.625% of one CPU core, 22.55 MiB working set, 3.75 MiB private memory. This is a short smoke measurement, not a sustained guarantee; the sub-10-MiB target remains unmet.
- Tests use separate project-local profiles; normal usage history is preserved.

Release limits: unsigned binaries; Windows 10, fresh-account sign-in, mixed-DPI, keyboard-only end-to-end navigation, and long-duration performance still require release QA. The right-click recovery code has not yet been exercised end-to-end with Explorer menus. No claim that all driver conditions or every possible accounting edge case have been verified. ISP totals can differ from adapter counters.

Installer smoke test: Inno Setup 6.7.3 compiled successfully. Clean per-user installation to an isolated build folder, executable hash equality, same-version upgrade, and silent uninstall all passed. Unmanaged files and the user's unrelated startup entry were preserved; uninstall registration and installed executable were removed. Evidence: `build/installer-validation.txt` and `build/installer-smoke.log`.


## 1.1.1 resource and UI update - 2026-10-08

- Scoped WinEvent hooks to Explorer and refresh the hook after an Explorer PID change. Skip unnecessary accessibility property reads for layout containers.
- Reuse the widget bitmap/DC; skip unchanged tray tooltip updates and avoid resending the icon on tooltip changes.
- Reduce the periodic safety scan from 5 to 15 seconds; layout events still trigger scans with a one-second throttle. Invalid snapshots retry every three seconds. This saves idle work but missed accessibility events may delay placement updates up to the fallback interval; no taskbar-space reservation is claimed.
- Remove decorative outlines on panels, buttons, and unfocused combo frames. Keyboard-focus accents remain. Final stroke changes have build validation but still need broad visual/DPI QA.
- All five native suites, startup registry integration, and four SQL tests pass. The live placement probe continued to locate a gap after worker optimization.
- Same-session 30-second baseline: 0.365% of one CPU core, 22.12 MiB working set, 3.36 MiB private memory. First optimization sample was 0.469%, 22.57 MiB, 3.72 MiB: no improvement demonstrated in that sample. Final reduced-scan measurement follows below. Short samples are noisy and do not establish sustained savings.

Final 1.1.1 reduced-scan sample: 0.364% of one CPU core, 22.36 MiB working set, 3.34 MiB private memory over 30.0 seconds. CPU was effectively unchanged from the same-session baseline; fewer scheduled scans and allocations are established by implementation, not a proven sustained CPU/RAM reduction.
