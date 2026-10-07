"""Exercise the built app in an isolated data folder on an interactive Windows desktop."""
import ctypes as C
from ctypes import wintypes as W
import configparser
import json
import pathlib
import sqlite3
import struct
import subprocess
import time
import uuid
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
user = C.WinDLL("user32", use_last_error=True)
gdi = C.WinDLL("gdi32", use_last_error=True)
kernel = C.WinDLL("kernel32", use_last_error=True)
psapi = C.WinDLL("psapi", use_last_error=True)


def bind(dll, name, result, *args):
    fn = getattr(dll, name)
    fn.restype = result
    fn.argtypes = args
    return fn


enum_cb = C.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
bind(user, "EnumWindows", W.BOOL, enum_cb, W.LPARAM)
bind(user, "GetClassNameW", C.c_int, W.HWND, W.LPWSTR, C.c_int)
bind(user, "GetWindowThreadProcessId", W.DWORD, W.HWND, C.POINTER(W.DWORD))
bind(user, "GetWindowTextW", C.c_int, W.HWND, W.LPWSTR, C.c_int)
bind(user, "GetDlgItem", W.HWND, W.HWND, C.c_int)
bind(user, "SetWindowTextW", W.BOOL, W.HWND, W.LPCWSTR)
bind(user, "FindWindowW", W.HWND, W.LPCWSTR, W.LPCWSTR)
bind(user, "FindWindowExW", W.HWND, W.HWND, W.HWND, W.LPCWSTR, W.LPCWSTR)
bind(user, "GetParent", W.HWND, W.HWND)
bind(user, "GetWindowRect", W.BOOL, W.HWND, C.POINTER(W.RECT))
bind(user, "SendMessageW", C.c_ssize_t, W.HWND, W.UINT, W.WPARAM, W.LPARAM)
bind(user, "PostMessageW", W.BOOL, W.HWND, W.UINT, W.WPARAM, W.LPARAM)
bind(user, "ShowWindow", W.BOOL, W.HWND, C.c_int)
bind(user, "SetProcessDpiAwarenessContext", W.BOOL, C.c_void_p)
bind(user, "GetDC", W.HDC, W.HWND)
bind(user, "ReleaseDC", C.c_int, W.HWND, W.HDC)
bind(user, "PrintWindow", W.BOOL, W.HWND, W.HDC, W.UINT)
bind(gdi, "CreateCompatibleDC", W.HDC, W.HDC)
bind(gdi, "CreateCompatibleBitmap", W.HBITMAP, W.HDC, C.c_int, C.c_int)
bind(gdi, "SelectObject", W.HGDIOBJ, W.HDC, W.HGDIOBJ)
bind(gdi, "DeleteObject", W.BOOL, W.HGDIOBJ)
bind(gdi, "DeleteDC", W.BOOL, W.HDC)
bind(gdi, "GetDIBits", C.c_int, W.HDC, W.HBITMAP, W.UINT, W.UINT, C.c_void_p, C.c_void_p, W.UINT)
bind(gdi, "BitBlt", W.BOOL, W.HDC, C.c_int, C.c_int, C.c_int, C.c_int, W.HDC, C.c_int, C.c_int, W.DWORD)


def wait_for(check, description, timeout=10):
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        result = check()
        if result:
            return result
        time.sleep(0.05)
    raise AssertionError(f"Timed out: {description}")


def find(pid, cls):
    found = []

    @enum_cb
    def visit(hwnd, _):
        value = W.DWORD()
        user.GetWindowThreadProcessId(hwnd, C.byref(value))
        name = C.create_unicode_buffer(256)
        user.GetClassNameW(hwnd, name, len(name))
        if value.value == pid and name.value == cls:
            found.append(hwnd)
        return True

    user.EnumWindows(visit, 0)
    return found[0] if found else None


def title(hwnd):
    value = C.create_unicode_buffer(512)
    user.SendMessageW(hwnd, 0xD, len(value), C.addressof(value))
    return value.value


def set_text(hwnd, value):
    text = C.create_unicode_buffer(value)
    assert user.SendMessageW(hwnd, 0xC, 0, C.addressof(text))
    assert title(hwnd) == value


def bounds(hwnd):
    rect = W.RECT()
    assert user.GetWindowRect(hwnd, C.byref(rect))
    return [rect.left, rect.top, rect.right, rect.bottom]


def screenshot(hwnd, path, screen=False):
    left, top, right, bottom = bounds(hwnd)
    width, height = right - left, bottom - top
    dc = user.GetDC(None if screen else hwnd)
    mem = gdi.CreateCompatibleDC(dc)
    bitmap = gdi.CreateCompatibleBitmap(dc, width, height)
    old = gdi.SelectObject(mem, bitmap)
    try:
        if screen:
            assert gdi.BitBlt(mem, 0, 0, width, height, dc, left, top, 0x00CC0020)
        else:
            assert user.PrintWindow(hwnd, mem, 2), "PrintWindow failed"
        gdi.SelectObject(mem, old)
        info = C.create_string_buffer(struct.pack("<IiiHHIIiiII", 40, width, -height, 1, 32, 0, width * height * 4, 0, 0, 0, 0))
        pixels = C.create_string_buffer(width * height * 4)
        assert gdi.GetDIBits(mem, bitmap, 0, height, pixels, info, 0) == height
        raw = pixels.raw
        rgb = bytearray(width * height * 3)
        rgb[0::3], rgb[1::3], rgb[2::3] = raw[2::4], raw[1::4], raw[0::4]
        scanlines = b"".join(b"\0" + rgb[y * width * 3:(y + 1) * width * 3] for y in range(height))

        def chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

        path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(scanlines)) + chunk(b"IEND", b""))
    finally:
        gdi.SelectObject(mem, old)
        gdi.DeleteObject(bitmap)
        gdi.DeleteDC(mem)
        user.ReleaseDC(None if screen else hwnd, dc)


class Memory(C.Structure):
    _fields_ = [("cb", W.DWORD), ("faults", W.DWORD)] + [(name, C.c_size_t) for name in ("peak_working_set", "working_set", "peak_paged_pool", "paged_pool", "peak_nonpaged_pool", "nonpaged_pool", "pagefile", "peak_pagefile", "private_bytes")]


bind(psapi, "GetProcessMemoryInfo", W.BOOL, W.HANDLE, C.POINTER(Memory), W.DWORD)
bind(kernel, "GetProcessTimes", W.BOOL, W.HANDLE, C.POINTER(W.FILETIME), C.POINTER(W.FILETIME), C.POINTER(W.FILETIME), C.POINTER(W.FILETIME))


def cpu_ticks(handle):
    values = [W.FILETIME() for _ in range(4)]
    assert kernel.GetProcessTimes(handle, *(C.byref(v) for v in values))
    return sum((v.dwHighDateTime << 32) | v.dwLowDateTime for v in values[2:])


def main():
    user.SetProcessDpiAwarenessContext(C.c_void_p(-4))
    assert not user.FindWindowW("NetPulse.Controller", "NetPulse"), "Close the running NetPulse before smoke testing."
    folder = ROOT / "build" / ("smoke-" + uuid.uuid4().hex[:8])
    folder.mkdir()
    report = {"data_directory": str(folder), "checks": []}
    startup = subprocess.STARTUPINFO()
    startup.dwFlags = subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    exe = ROOT / "build" / "NetPulse.exe"
    proc = subprocess.Popen([str(exe), "--data-dir", str(folder), "--settings"], startupinfo=startup)
    controller = None
    try:
        controller = wait_for(lambda: find(proc.pid, "NetPulse.Controller"), "controller creation")
        settings = wait_for(lambda: find(proc.pid, "NetPulse.Settings"), "settings creation")
        bar = user.FindWindowW("Shell_TrayWnd", None)
        widget = wait_for(lambda: user.FindWindowExW(bar, None, "NetPulse.Widget", None), "embedded taskbar widget")
        assert user.GetParent(widget) == bar
        report["checks"].append("widget is a child of Explorer taskbar")
        user.ShowWindow(settings, 4)  # Show without activating during QA.
        time.sleep(1)
        screenshot(settings, folder / "settings.png")
        screenshot(widget, folder / "widget.png")
        screenshot(widget, folder / "widget-on-screen.png", screen=True)
        report["initial_settings"] = title(settings)
        positions = []
        user.SendMessageW(settings, 0x111, 202, 0)
        for position in range(3):
            user.SendMessageW(user.GetDlgItem(settings, 107), 0x14E, position, 0)
            user.SendMessageW(settings, 0x111, 110, 0)
            assert title(settings) == "NetPulse - settings saved"
            positions.append(bounds(widget))
        assert positions[0][0] < positions[1][0] < positions[2][0], positions
        report["placement_bounds"] = positions
        report["checks"].append("left, center, and right placement")
        user.SendMessageW(settings, 0x111, 201, 0)
        for control, value in [(101, "22:00"), (102, "06:00"), (103, "125.5"), (104, "250"), (105, "31"), (106, "08:15"), (108, "-12")]:
            set_text(user.GetDlgItem(settings, control), value)
        user.SendMessageW(settings, 0x111, 110, 0)
        ini = configparser.ConfigParser()
        ini.read(folder / "config.ini")
        assert ini["plan"]["peak_start"] == "22:00"
        assert ini["plan"]["reset_day"] == "31"
        assert ini["widget"]["offset"] == "-12"
        report["checks"].append("settings persisted to isolated INI")
        # Recreate the embedded widget without restarting Explorer or touching other apps.
        user.SendMessageW(controller, 0x111, 4003, 0)
        widget = wait_for(lambda: user.FindWindowExW(bar, None, "NetPulse.Widget", None), "reattachment")
        assert user.GetParent(widget) == bar
        report["checks"].append("taskbar reattachment")
        # Confirm a manual adjustment in this disposable test profile only.
        user.SendMessageW(settings, 0x111, 203, 0)
        set_text(user.GetDlgItem(settings, 112), "1.25")
        set_text(user.GetDlgItem(settings, 113), "2.5")
        user.PostMessageW(settings, 0x111, 114, 0)
        dialog = wait_for(lambda: find(proc.pid, "#32770"), "manual-adjustment confirmation")
        user.PostMessageW(dialog, 0x111, 6, 0)  # IDYES
        wait_for(lambda: not find(proc.pid, "#32770"), "confirmation closing")
        with sqlite3.connect(folder / "netpulse.db") as db:
            assert db.execute("SELECT count(*) FROM corrections").fetchone()[0] == 1
        report["checks"].append("manual usage adjustment committed")
        user.SendMessageW(settings, 0x111, 116, 0)
        before = cpu_ticks(proc._handle)
        started = time.monotonic()
        time.sleep(15)
        elapsed = time.monotonic() - started
        report["cpu_percent_one_core_15s"] = (cpu_ticks(proc._handle) - before) / 1e7 / elapsed * 100
        memory = Memory()
        memory.cb = C.sizeof(memory)
        assert psapi.GetProcessMemoryInfo(proc._handle, C.byref(memory), memory.cb)
        report["working_set_bytes"] = memory.working_set
        report["peak_working_set_bytes"] = memory.peak_working_set
        user.SendMessageW(controller, 0x111, 4001, 0)
        settings = wait_for(lambda: find(proc.pid, "NetPulse.Settings"), "settings reopened")
        assert title(user.GetDlgItem(settings, 101)) == "22:00"
        screenshot(settings, folder / "settings-configured.png")
        user.PostMessageW(controller, 0x10, 0, 0)
        assert proc.wait(timeout=10) == 0
        with sqlite3.connect(folder / "netpulse.db") as db:
            assert db.execute("PRAGMA integrity_check").fetchone()[0] == "ok"
            assert db.execute("SELECT count(*) FROM sessions WHERE system_uptime_ms > 0").fetchone()[0] == 1
        report["checks"].append("clean exit, uptime record, database integrity")
        report["passed"] = True
    finally:
        if proc.poll() is None:
            if controller:
                user.PostMessageW(controller, 0x10, 0, 0)
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
        (folder / "report.json").write_text(json.dumps(report, indent=2))
        print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    main()
