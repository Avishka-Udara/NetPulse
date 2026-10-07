# Release process

1. Build and run `build.bat`, then `python tests/test_sql.py`.
2. Verify settings, typed/preset times, theme/scale changes, menu open/close, crowded taskbar fallback, daily/cycle resets and restart persistence. Use a disposable `--data-dir` profile.
3. Verify startup on/off and sign-in in a test Windows account. Windows Task Manager can independently disable startup; the app does not override that OS choice.
4. Compile `installer/NetPulse.iss` with Inno Setup 6.7 or later. Test clean installation, upgrade, uninstall, and data preservation on Windows 10/11. The installer should not require elevation or erase the usage database.
5. Publish matching binary and source archives together under GPL-3.0-only. Include LICENSE, third-party runtime notices, and SHA-256 checksums. Do not include build profiles, databases, compiler downloads, or credentials in source archives.
6. Authenticode-sign the executable and installer using the maintainer's certificate before a signed public release. Never commit signing keys. Unsigned local builds are not signed production releases.

Maintainer: [Avishka Udara](https://github.com/avishka-Udara).

An installer and passing unit tests do not establish production readiness. Outstanding manual checks and performance measurements must remain visible in VALIDATION.md.
