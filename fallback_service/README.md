# Fall Back Service

> **Note**: When talking about Wayland support, the ONLY Wayland compositor supported here is `gxde-wlcom`.
>
> **注意**: 当谈及Wayland支持时，当前仅支持`gxde-wlcom`这一个合成器。



## Introduction | 简介

Currently GXDE-OS's `dde-daemon` does NOT support Wayland. In case of Wayland, `dde-daemon`'s `Dock` module will do no-ops. This fall back service's aim is to provide a fall back daemon for our Wayland session when `dde-daemon` is unavailable.



当前GXDE的`dde-daemon`尚不支持Wayland，在Wayland下，`dde-daemon`的`Dock`模块不工作。此服务的目标是在Wayland会话下代替不可用的`dde-daemon`的`Dock`模块。



Just like the GXDE-OS version of `dde-daemon` not supporting Wayland, this daemon removes X11 support. The concern is that this service is a temporary fix for Wayland session. This fall back service will eventually be depreciated when `gxde-daemon` brings back Wayland support some day...



正如GXDE版本的`dde-daemon`不支持Wayland一样，这个daemon也不支持X11。这是因为这个服务是针对Wayland会话的临时修复，最终会在`gxde-daemon`支持Wayland会话后被替代...



## Copying | 许可证

This service is licensed under GPL v3+.



本程序使用GPL v3+授权。