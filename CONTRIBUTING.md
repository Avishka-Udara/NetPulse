# Contributing

Maintainer: [Avishka Udara](https://github.com/avishka-Udara).

Contributions are accepted under GPL-3.0-only. Keep changes scoped and include a reproduction for bugs. Never submit usage databases, personal network details, or credentials.

Run `build.bat` on Windows with MSVC or LLVM-MinGW. This compiles the application and executes core, database, network, settings, and placement tests. Run `build/test_startup.exe` for the isolated current-user registry integration test. Run `python tests/test_sql.py` for the SQL checks. Test UI changes at 100%, 125%, and 150% scaling, light/dark themes, and with keyboard navigation.

Keep accounting independent of rendering. Do not block sampling on Explorer or accessibility calls. Test byte conservation and persistence for changes to tracking or resets. See RELEASE.md for release gates and ROADMAP.md for platform boundaries.
