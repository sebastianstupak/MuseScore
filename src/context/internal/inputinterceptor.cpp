/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "inputinterceptor.h"

#include <QGuiApplication>
#include <QEvent>
#include <QKeyEvent>

using namespace mu::context;

InputInterceptor::~InputInterceptor()
{
    removeFilter();
}

void InputInterceptor::setHandler(Handler h)
{
    m_handler = std::move(h);
    if (m_handler) {
        installFilter();
    } else {
        removeFilter();
    }
}

void InputInterceptor::clearHandler()
{
    m_handler = nullptr;
    removeFilter();
}

bool InputInterceptor::hasHandler() const
{
    return static_cast<bool>(m_handler);
}

bool InputInterceptor::wouldConsume(const RawKeyEvent& e)
{
    return m_handler ? m_handler(e, true) : false;
}

bool InputInterceptor::handleKey(const RawKeyEvent& e)
{
    return m_handler ? m_handler(e, false) : false;
}

void InputInterceptor::installFilter()
{
    if (!m_filterInstalled && qApp) {
        qApp->installEventFilter(this);
        m_filterInstalled = true;
    }
}

void InputInterceptor::removeFilter()
{
    if (m_filterInstalled && qApp) {
        qApp->removeEventFilter(this);
    }
    m_filterInstalled = false;
}

bool InputInterceptor::eventFilter(QObject* watched, QEvent* event)
{
    if (m_handler && event) {
        const QEvent::Type t = event->type();
        if (t == QEvent::ShortcutOverride || t == QEvent::KeyPress) {
            const QKeyEvent* ke = static_cast<QKeyEvent*>(event);
            const RawKeyEvent rk{ ke->key(), int(ke->modifiers()), ke->text(), ke->isAutoRepeat() };
            // phase=true on ShortcutOverride (pre-shortcut decision), false on KeyPress.
            const bool consume = m_handler(rk, t == QEvent::ShortcutOverride);
            if (consume) {
                event->accept();     // suppress the window-scoped QShortcut
                return true;         // consume: stop further delivery
            }
        }
    }
    return QObject::eventFilter(watched, event);
}
