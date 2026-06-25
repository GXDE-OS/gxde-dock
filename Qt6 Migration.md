# Qt6 Migration | Qt6迁移说明
You need to install these new pakage from APT source: 

从APT源安装如下包：
```bash
$ sudo apt install layer-shell-qt
```


Note that `layer-shell-qt` conflicts/replaces with `layer-shell-qt5`.

需要注意的是`layer-shell-qt`与`layer-shell-qt5`冲突并会被自动替换。

---

Install those Qt6-ported packages before you even start to complie (they're NOT in APT source yet, your complie sequence is simply the same as how they are listed from top to bottom): 

在尝试编译前先编译安装如下包（还没进源，是Qt6移植版本，编译顺序即列表顺序）：

* https://gitee.com/GXDE-OS/dframework-dbus-qt6
* https://gitee.com/GXDE-OS/gxde-network-utils-qt6

---

You MUST know that we vendored `libdbusmenu-lxqt` inside because the system libdbusmenu is built for Qt5. `dframework-dbus` is another thing that has no relation with that library except their name look similar.

您亦必须知道我们集成了`libdbusmenu-lxqt`的源码，系统的libdbusmenu使用Qt5编译，在这不起作用。另外这与`dframework-dbus`完全是两码事，两个库职责不同，他们也不相干，除了名字看着很像以外。
