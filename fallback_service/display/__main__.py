#!/usr/bin/env python3
"""
display/__main__.py — Display fallback service

用法: python3 -m display
服务名: top.gxde.dock.fallback.service.Display
对象路径: /com/deepin/daemon/Display
接口名: com.deepin.daemon.Display
"""

import logging
import signal
from collections import OrderedDict
from typing import Any, Dict, List

import dbus
import dbus.service
import dbus.mainloop.glib
from gi.repository import GLib, Gdk  # type: ignore

SERVICE_NAME = "top.gxde.dock.fallback.service.Display"
DBUS_PATH = "/com/deepin/daemon/Display"
DBUS_IFACE = "com.deepin.daemon.Display"

log = logging.getLogger("display")
log.setLevel(logging.DEBUG)
h = logging.StreamHandler()
h.setFormatter(logging.Formatter("[%(levelname)s] %(name)s: %(message)s"))
log.addHandler(h)


def _screen_rect() -> tuple:
    """返回主屏幕 (x, y, width, height)"""
    try:
        display = Gdk.Display.get_default()
        if not display:
            return (0, 0, 1920, 1080)
        monitor = display.get_primary_monitor() or display.get_monitor(0)
        if monitor:
            g = monitor.get_geometry()
            return (g.x, g.y, g.width, g.height)
    except Exception:
        pass
    return (0, 0, 1920, 1080)


def _screen_size() -> tuple:
    """返回 (width, height) — 所有 monitor 并集"""
    try:
        display = Gdk.Display.get_default()
        if not display:
            return (1920, 1080)
        max_x, max_y = 1920, 1080
        for i in range(display.get_n_monitors()):
            m = display.get_monitor(i)
            if m:
                g = m.get_geometry()
                if g.x + g.width > max_x:
                    max_x = g.x + g.width
                if g.y + g.height > max_y:
                    max_y = g.y + g.height
        return (max_x, max_y)
    except Exception:
        return (1920, 1080)


class DisplayManager(dbus.service.Object):

    def __init__(self, conn, bus_name):
        super().__init__(conn, DBUS_PATH, bus_name)
        self._brightness: Dict[str, float] = {"default": 1.0}
        self._has_changed = False
        self._primary = "eDP-1"
        log.info(f"DisplayManager ready at {DBUS_PATH}")

    def _props(self, iface: str) -> Dict[str, Any]:
        if iface != DBUS_IFACE:
            return {}
        x, y, w, h = _screen_rect()
        sw, sh = _screen_size()
        return OrderedDict([
            ("Monitors", []),
            ("PrimaryRect", (dbus.Int16(x), dbus.Int16(y),
                             dbus.UInt16(w), dbus.UInt16(h))),
            ("ScreenWidth", dbus.UInt16(sw)),
            ("ScreenHeight", dbus.UInt16(sh)),
            ("Primary", self._primary),
            ("Brightness", self._brightness),
            ("DisplayMode", dbus.Int16(1)),
            ("HasChanged", self._has_changed),
            ("BuiltinOutput", dbus.ObjectPath("/")),
        ])

    @dbus.service.method(dbus.PROPERTIES_IFACE,
                         in_signature="s", out_signature="a{sv}")
    def GetAll(self, iface: str) -> Dict[str, Any]:
        return self._props(iface)

    @dbus.service.method(dbus.PROPERTIES_IFACE,
                         in_signature="ss", out_signature="v")
    def Get(self, iface: str, prop: str) -> Any:
        return self._props(iface).get(prop)

    @dbus.service.method(dbus.PROPERTIES_IFACE,
                         in_signature="ssv", out_signature="")
    def Set(self, iface: str, prop: str, value: Any):
        if prop == "HasChanged":
            self._has_changed = bool(value)
            self.PropertiesChanged(DBUS_IFACE, {prop: self._has_changed}, [])

    @dbus.service.signal(dbus.PROPERTIES_IFACE, signature="sa{sv}as")
    def PropertiesChanged(self, iface: str, changed: Dict[str, Any],
                          invalidated: List[str]):
        pass

    # --- Methods (no-op fallback) ---

    @dbus.service.method(DBUS_IFACE, in_signature="", out_signature="")
    def Apply(self): pass

    @dbus.service.method(DBUS_IFACE, in_signature="", out_signature="")
    def Reset(self): pass

    @dbus.service.method(DBUS_IFACE, in_signature="", out_signature="")
    def ResetChanges(self):
        self._has_changed = False

    @dbus.service.method(DBUS_IFACE, in_signature="", out_signature="")
    def SaveChanges(self):
        self._has_changed = False

    @dbus.service.method(DBUS_IFACE, in_signature="sd", out_signature="")
    def ChangeBrightness(self, output: str, brightness: float):
        self._brightness[output] = brightness

    @dbus.service.method(DBUS_IFACE, in_signature="sd", out_signature="")
    def SetBrightness(self, output: str, brightness: float):
        self.ChangeBrightness(output, brightness)

    @dbus.service.method(DBUS_IFACE, in_signature="s", out_signature="")
    def SetPrimary(self, output: str):
        self._primary = output

    @dbus.service.method(DBUS_IFACE, in_signature="ns", out_signature="")
    def SwitchMode(self, mode: int, monitor: str): pass

    @dbus.service.method(DBUS_IFACE, in_signature="ss", out_signature="")
    def JoinMonitor(self, a: str, b: str): pass

    @dbus.service.method(DBUS_IFACE, in_signature="s", out_signature="")
    def SplitMonitor(self, output: str): pass

    @dbus.service.method(DBUS_IFACE, in_signature="s", out_signature="")
    def ResetBrightness(self, output: str):
        self._brightness[output] = 1.0

    @dbus.service.method(DBUS_IFACE, in_signature="", out_signature="s")
    def QueryCurrentPlanName(self) -> str:
        return "fallback"

    @dbus.service.method(DBUS_IFACE, in_signature="s", out_signature="i")
    def QueryOutputFeature(self, output: str) -> int:
        return 0

    @dbus.service.method(DBUS_IFACE, in_signature="ss", out_signature="")
    def AssociateTouchScreen(self, output: str, touch: str): pass


def main():
    dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
    GLib.set_prgname("gxde-dock-fallback-display")

    bus = dbus.SessionBus()
    bus_name = dbus.service.BusName(SERVICE_NAME, bus)
    mgr = DisplayManager(bus, bus_name)

    log.info(f"Service registered: {SERVICE_NAME}")

    loop = GLib.MainLoop()

    def _sig_handler(signum, frame):
        loop.quit()

    signal.signal(signal.SIGTERM, _sig_handler)
    signal.signal(signal.SIGINT, _sig_handler)

    try:
        loop.run()
    except KeyboardInterrupt:
        pass
    log.info("Exited.")


if __name__ == "__main__":
    main()
