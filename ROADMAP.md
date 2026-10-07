# Platform and optimization roadmap

## Windows release

Native C, Win32/GDI, IP Helper counters, and SQLite require no browser engine or managed runtime. The current release targets Windows 10/11 x64. A per-user installer and a portable ZIP use the same Local AppData usage database. Startup is optional and uses the current user's Run registration.

The taskbar widget depends on Explorer internals. Keep the notification icon as a fallback. Collision scans cannot reserve space and may briefly lag animations. Test supported Windows builds before claiming compatibility.

## macOS and Linux

These are planned ports, not supported installation targets yet. Share `core.c` scheduling/counter math and placement tests. First remove Windows types, UTF-16 file APIs, and Windows time dependencies from database/build boundaries. Add a small platform interface for counters, paths, autostart, clocks, and tray presentation.

- macOS: native menu-bar status item, network counters via system interfaces, login items through the supported OS API, notarized universal app/DMG.
- Linux: network statistics through netlink, StatusNotifierItem for compatible desktops, XDG autostart, and DEB/RPM or Flatpak packaging. Desktop environments control panel placement; embedded Windows-style positioning cannot be promised.

Add platform CI, counter reset/reconnect fixtures, persistence tests, and installation/uninstallation checks before distributing a port. Do not ship nonfunctional platform installers.

## Further optimization

Measure sustained CPU, working set, wakeups, and bytes written before accepting optimization claims. Prioritize narrower taskbar accessibility scans, network-change notifications for adapter/route discovery, and repaint-on-text-change. Preserve all counter deltas and keep theme/settings changes responsive. One-second sampling and minute-batched SQLite commits remain the baseline; longer sampling trades live responsiveness for fewer wakeups.
