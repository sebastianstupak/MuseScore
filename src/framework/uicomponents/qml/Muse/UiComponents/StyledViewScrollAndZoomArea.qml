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
import QtQuick

import "internal"

PinchArea {
    id: root

    required default property Item view

    property alias horizontalScrollbarSize: horizontalScrollBar.size
    property alias startHorizontalScrollPosition: horizontalScrollBar.position

    property alias verticalScrollbarSize: verticalScrollBar.size
    property alias startVerticalScrollPosition: verticalScrollBar.position

    signal pinchToZoom(real scale, var pos)
    // Two-finger drag. PinchArea has always reported the translation --
    // pinch.center moves with the fingers -- and this component threw it
    // away, keeping only the scale. So pinching zoomed but there was no
    // way to move the page.
    signal panView(real dx, real dy)
    signal pinchBegan()
    signal scrollHorizontal(real newPos)
    signal scrollVertical(real newPos)

    onPinchStarted: function(pinch) {
        root.pinchBegan()
    }

    onPinchUpdated: function(pinch) {
        root.pinchToZoom(pinch.scale / pinch.previousScale, pinch.center)
        root.panView(pinch.center.x - pinch.previousCenter.x,
                     pinch.center.y - pinch.previousCenter.y)
    }

    // A macOS feature which allows double-tapping with two fingers to zoom in or out
    onSmartZoom: function(pinch) {
        root.pinchToZoom(pinch.scale === 0 ? 0.5 : 2, pinch.center)
    }

    children: [ view, horizontalScrollBar, verticalScrollBar ]

    ViewScrollBar {
        id: horizontalScrollBar
        orientation: Qt.Horizontal

        onMoved: function(newPosition) {
            root.scrollHorizontal(newPosition)
        }
    }

    ViewScrollBar {
        id: verticalScrollBar
        orientation: Qt.Vertical

        onMoved: function(newPosition) {
            root.scrollVertical(newPosition)
        }
    }
}
