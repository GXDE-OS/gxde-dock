/*
 * Copyright (C) 2026 CharOfString <markus_verify@126.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QByteArray>

#include <cstdlib>
#include <cstring>

#include <xcb/xcb.h>

#include "xsettings.h"

namespace {

/**
 * @brief 按照XSETTINGS blob字节顺序读取CARD16
 * 
 * @details 从内存中读取连续的2个字节，并根据处理器使用大端序/小端序拼接成一个16-bit的无符号整数
 * @param (unsigned char*) p           buffer指针
 * @param bool             isBigEndian 系统是否使用大端序
 * @return (qunit16) 拼接完毕的数据
 */

quint16 read16(const unsigned char* p, bool isBigEndian) {
    if (isBigEndian) {
        return (quint16(p[0]) << 8 | p[1]);
    } else {
        return (quint16(p[1]) << 8 | p[0]);
    }
}


/**
 * @brief 按照XSETTINGS blob字节顺序读取CARD32
 * 
 * @details 从内存中读取连续的4个字节，并根据处理器使用大端序/小端序拼接成一个32-bit的无符号整数
 * @param (unsigned char*) p           buffer指针
 * @param bool             isBigEndian 系统是否使用大端序
 * @return (qunit32) 拼接完毕的数据
 */

quint32 read32(const unsigned char* p, bool isBigEndian) {
    if (isBigEndian) {
        return (quint32(p[0]) << 24 | quint32(p[1]) << 16
            | quint32(p[2]) << 8 | p[3]);
    } else {
        return (quint32(p[3]) << 24 | quint32(p[2]) << 16
            | quint32(p[1]) << 8 | p[0]);
    }
}


/**
 * @brief 将字符串转为X11 Atom
 * 
 * @details 向X Server发送一个 @c InternAtom 请求，XCB会返回一个cookie，等待X Server的回复
 *          如果回复不是空指针则代表成功，提取其中的atom数字
 *          否则，视作提取失败，返回 @c XCB_ATOM_NONE.
 * @param (xcb_connection_t*) conn XCB连接结构体指针
 * @param char*               name Atom名
 * @return (xcb_atom_t) X11 Atom
 */

xcb_atom_t internAtom(xcb_connection_t* conn, const char* name) {
    xcb_intern_atom_cookie_t cookie = xcb_intern_atom(conn, 0, qstrlen(name),
        name);
    xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(conn, cookie,
        nullptr);

    xcb_atom_t atom = XCB_ATOM_NONE;
    if (reply) {
        atom = reply->atom;
    }

    free(reply);
    return atom;
}

}  // namespace


namespace Wayland {

/**
 * @brief 从底层 XSETTINGS 守护进程中读取指定的字符串类型配置项。
 *
 * @details 建立一个临时的XCB连接，寻找当前屏幕上 @c _XSETTINGS_S0 的 @c Selection @c Owner 窗口
 *          并拉取其挂载的 @c _XSETTINGS_SETTINGS blob, 遍历blob，返回与 @c key 相同的字符串的值
 * @note 为什么一个Wayland会话要用XSETTINGS？实际上，原版的主题获取逻辑还是从XSETTINGS读取的，这里尊重
 *       原版的逻辑默认还继续从XSETTINGS读取设定的外观参数。
 * @warning 在XSETTINGS不可用时 @c main 应该从QSettings/GSettings直接读取字符串兜底，但这就不是这个函数的
 *          职责所在了。
 * @param (const QString &) key 需要查询的XSETTINGS键
 * @return (QString) 查询到的值 (解析后的UTF-8字符串)，失败时返回空 @c QString()
 */

QString xsettingsString(const QString& key) {
    int screen = 0;
    xcb_connection_t* conn = xcb_connect(nullptr, &screen);
    if (!conn || xcb_connection_has_error(conn)) {
        if (conn) {
            xcb_disconnect(conn);
        }
        return QString();
    }

    QString result;
    xcb_get_property_reply_t* prop = nullptr;

    do {
        const QByteArray sel = "_XSETTINGS_S" + QByteArray::number(screen);
        const xcb_atom_t selAtom = internAtom(conn, sel.constData());
        const xcb_atom_t setAtom = internAtom(conn, "_XSETTINGS_SETTINGS");
        if (selAtom == XCB_ATOM_NONE || setAtom == XCB_ATOM_NONE) {
            break;
        }

        xcb_get_selection_owner_reply_t* own = xcb_get_selection_owner_reply(
            conn, xcb_get_selection_owner(conn, selAtom), nullptr);
        const xcb_window_t owner = own ? own->owner : XCB_WINDOW_NONE;
        free(own);

        if (owner == XCB_WINDOW_NONE) {
            break;   // 没有 owner，调用方负责回退
        }

        prop = xcb_get_property_reply(conn, xcb_get_property(conn, 0, owner,
            setAtom, setAtom, 0, 0x4000), nullptr);
        if (!prop) {
            break;
        }

        const int len = xcb_get_property_value_length(prop);
        const unsigned char* d =
            static_cast<const unsigned char*>(xcb_get_property_value(prop));
        if (len < 12) {
            break;
        }

        const bool be = d[0] != 0;  // 0 = LSB第一位
        const quint32 count = read32(d + 8, be);
        const QByteArray want = key.toUtf8();

        int off = 12;
        for (quint32 i = 0; i < count && off + 4 <= len; ++i) {
            const quint8 type = d[off];
            const quint16 nameLen = read16(d + off + 2, be);
            const int nameOff = off + 4;
            if (nameOff + nameLen > len) {
                break;
            }
            const QByteArray name(reinterpret_cast<const char*>(d) + nameOff,
                nameLen);

            int p = (nameOff + nameLen + 3) & ~3;  // padding (4字节)
            p += 4;  // last-change-serial
            if (p + 4 > len) {
                break;
            }

            if (type == 1) {  // string
                const quint32 vlen = read32(d + p, be);
                const int vOff = p + 4;
                if (vOff + static_cast<int>(vlen) > len) {
                    break;
                }

                if (name == want) {
                    result = QString::fromUtf8(
                        reinterpret_cast<const char*>(d) + vOff, vlen);
                    break;
                }
                p = (vOff + static_cast<int>(vlen) + 3) & ~3;
            } else if (type == 0) {  // integer
                p += 4;
            } else if (type == 2) {  // color
                p += 8;
            } else {
                break;
            }
            off = p;
        }
    } while (false);

    free(prop);
    xcb_disconnect(conn);
    return result;
}

}  // namespace Wayland
