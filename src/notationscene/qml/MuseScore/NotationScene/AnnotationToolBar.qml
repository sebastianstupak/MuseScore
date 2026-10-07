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
import QtQuick.Window
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents

// A slim, floating, draggable pen toolbar that overlays the whole MuseScore window
// (reparented to the window content item) and snaps to any edge when dragged near it
// (bottom/top => horizontal bar; left/right => vertical strip). Persistent playback +
// zoom/view + file; ink tools expand when annotating. Bound to the NotationPaintView.
Item {
    id: root

    property var view
    // Docked to the right edge by DEFAULT, not floating.
    //
    // Floating put it at x:420, which on this 2480px panel is squarely over
    // the first page -- it covered the music every single launch, and the
    // dock choice is not persisted, so "drag it out of the way" had to be
    // redone every time. Right rather than left because the palettes panel
    // already occupies the left.
    property string dockEdge: "right"   // "" (floating), "left", "right", "top", "bottom"
    property bool floatHorizontal: false   // orientation while floating
    property bool menuOpen: false
    readonly property bool horizontal: dockEdge === "top" || dockEdge === "bottom" || (dockEdge === "" && floatHorizontal)

    visible: !!view
    implicitWidth: bg.width
    implicitHeight: bg.height
    z: 1000

    // Initial floating position over the canvas (past the palette panel / toolbars),
    // not over the menu bar. Drag the handle to any edge to dock.
    x: 420
    y: 180

    readonly property real btn: 30
    readonly property var palette: ["#1a1a1a", "#e03030", "#2a6be0", "#28a745", "#f0a020", "#a020c0"]
    readonly property var widths: [6, 15, 30]

    // Float over the WHOLE window, not just the canvas.
    Component.onCompleted: {
        if (Window.contentItem) {
            root.parent = Window.contentItem
        }
        // applyDock() otherwise only runs from onDockEdgeChanged, which does
        // NOT fire for the initial value -- so a default of "right" would
        // have left the strip sitting at its floating x:420, over the music,
        // while claiming to be docked.
        Qt.callLater(applyDock)
    }

    onDockEdgeChanged: Qt.callLater(applyDock)
    onWidthChanged: if (dockEdge !== "") applyDock()
    onHeightChanged: if (dockEdge !== "") applyDock()

    // Re-dock when the WINDOW resizes, not just when this strip does.
    //
    // Without this, docking right lands on the left. applyDock() computes
    // x = parent.width - width, and at Component.onCompleted the parent has
    // no width yet, so that is negative and the clamp below pins it to 0.
    // The strip then sits at the left edge insisting it is docked right --
    // measured on the tablet, where it covered the palettes panel and the
    // Layout tab instead of the music it was moved to avoid.
    Connections {
        target: root.parent
        enabled: !!root.parent
        function onWidthChanged()  { if (root.dockEdge !== "") root.applyDock() }
        function onHeightChanged() { if (root.dockEdge !== "") root.applyDock() }
    }

    function applyDock() {
        if (!parent) {
            return
        }
        if (dockEdge === "left") {
            x = 0
        } else if (dockEdge === "right") {
            x = parent.width - width
        } else if (dockEdge === "top") {
            y = 0
        } else if (dockEdge === "bottom") {
            y = parent.height - height
        }
        x = Math.max(0, Math.min(x, parent.width - width))
        y = Math.max(44, Math.min(y, parent.height - height))   // keep clear of the title bar
    }

    Rectangle {
        id: bg
        width: content.implicitWidth + 8
        height: content.implicitHeight + 8
        radius: 8
        color: ui.theme.backgroundPrimaryColor
        border.width: 1
        border.color: ui.theme.strokeColor

        MouseArea { anchors.fill: parent }   // absorb clicks on the strip body

        GridLayout {
            id: content
            anchors.centerIn: parent
            columns: root.horizontal ? -1 : 1
            rows: root.horizontal ? 1 : -1
            columnSpacing: 3
            rowSpacing: 3

            // Drag handle
            Rectangle {
                Layout.preferredWidth: root.horizontal ? 12 : root.btn
                Layout.preferredHeight: root.horizontal ? root.btn : 12
                color: "transparent"
                Grid {
                    anchors.centerIn: parent
                    rows: root.horizontal ? 2 : 1
                    columns: root.horizontal ? 1 : 2
                    rowSpacing: 3
                    columnSpacing: 3
                    Repeater { model: 2; delegate: Rectangle { width: 4; height: 4; radius: 2; color: ui.theme.fontPrimaryColor; opacity: 0.6 } }
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SizeAllCursor
                    drag.target: root
                    drag.minimumX: 0
                    drag.minimumY: 44   // stay below the window title bar (its drag moves the whole window)
                    drag.maximumX: root.parent ? Math.max(0, root.parent.width - root.width) : 0
                    drag.maximumY: root.parent ? Math.max(0, root.parent.height - root.height) : 0
                    onClicked: root.menuOpen = !root.menuOpen   // tap handle => dock menu
                }
            }

            // Playback
            FlatButton { Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.PLAY; toolTipTitle: qsTrc("notation", "Play / Pause"); onClicked: root.view.dispatchAction("play") }
            FlatButton { Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.STOP; toolTipTitle: qsTrc("notation", "Stop"); onClicked: root.view.dispatchAction("stop") }
            FlatButton { Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.REWIND; toolTipTitle: qsTrc("notation", "Rewind to start"); onClicked: root.view.dispatchAction("rewind") }
            FlatButton { Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.LOOP; toolTipTitle: qsTrc("notation", "Toggle loop"); onClicked: root.view.dispatchAction("loop") }
            FlatButton { Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.METRONOME; toolTipTitle: qsTrc("notation", "Toggle metronome"); onClicked: root.view.dispatchAction("metronome") }

            Rectangle { Layout.preferredWidth: root.horizontal ? 1 : root.btn; Layout.preferredHeight: root.horizontal ? root.btn : 1; color: ui.theme.strokeColor; opacity: 0.5 }

            // Zoom + view + file
            FlatButton { Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.ZOOM_OUT; toolTipTitle: qsTrc("notation", "Zoom out"); onClicked: root.view.dispatchAction("zoomout") }
            FlatButton { Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.ZOOM_IN; toolTipTitle: qsTrc("notation", "Zoom in"); onClicked: root.view.dispatchAction("zoomin") }
            FlatButton { Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.PAGE; toolTipTitle: qsTrc("notation", "Page / continuous view"); onClicked: root.view.toggleViewMode() }
            FlatButton { Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.SAVE; toolTipTitle: qsTrc("notation", "Save"); onClicked: root.view.dispatchAction("file-save") }
            FlatButton { Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; text: "PDF"; toolTipTitle: qsTrc("notation", "Export…"); onClicked: root.view.dispatchAction("file-export") }

            Rectangle { Layout.preferredWidth: root.horizontal ? 1 : root.btn; Layout.preferredHeight: root.horizontal ? root.btn : 1; color: ui.theme.strokeColor; opacity: 0.5 }

            // Annotate toggle
            FlatButton { Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.EDIT; toolTipTitle: qsTrc("notation", "Annotate (Ctrl+Alt+A)"); accentButton: root.view.annotationActive; onClicked: root.view.toggleAnnotation() }

            // Write (recognize handwriting -> notation) toggle
            FlatButton { Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; text: "♪"; toolTipTitle: qsTrc("notation", "Write notation (Ctrl+Alt+W)"); accentButton: root.view.writeModeActive; onClicked: root.view.toggleWriteMode() }

            // Sticky "Shift" for additive lasso (multi-select) — a pen-friendly modifier
            FlatButton { visible: root.view.writeModeActive; Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; text: "⊕"; toolTipTitle: qsTrc("notation", "Add to selection (multi-select loops)"); accentButton: root.view.addToSelectionActive; onClicked: root.view.toggleAddToSelection() }

            // --- Ink tools (only while annotating) ---
            FlatButton { visible: root.view.annotationActive; Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.EDIT; toolTipTitle: qsTrc("notation", "Pen (P)"); accentButton: root.view.annotationTool === 0; onClicked: root.view.annotationTool = 0 }
            FlatButton { visible: root.view.annotationActive; Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.BRUSH; toolTipTitle: qsTrc("notation", "Highlighter (H)"); accentButton: root.view.annotationTool === 1; onClicked: root.view.annotationTool = 1 }
            FlatButton { visible: root.view.annotationActive; Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; text: "⌫"; toolTipTitle: qsTrc("notation", "Eraser (E)"); accentButton: root.view.annotationTool === 2; onClicked: root.view.annotationTool = 2 }
            FlatButton { visible: root.view.annotationActive; Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.UNDO; enabled: root.view.annotationCanUndo; toolTipTitle: qsTrc("notation", "Undo"); onClicked: root.view.annotationUndo() }
            FlatButton { visible: root.view.annotationActive; Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.REDO; enabled: root.view.annotationCanRedo; toolTipTitle: qsTrc("notation", "Redo"); onClicked: root.view.annotationRedo() }
            FlatButton { visible: root.view.annotationActive; Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn; icon: IconCode.DELETE_TANK; toolTipTitle: qsTrc("notation", "Clear all annotations"); onClicked: root.view.annotationClear() }

            // Colour — current swatch, tap cycles the palette
            Rectangle {
                visible: root.view.annotationActive
                Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn
                radius: 4
                color: root.view.annotationColor
                border.width: 1; border.color: ui.theme.strokeColor
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        var idx = -1
                        for (var k = 0; k < root.palette.length; ++k) {
                            if (Qt.colorEqual(root.view.annotationColor, root.palette[k])) { idx = k; break }
                        }
                        root.view.annotationColor = root.palette[(idx + 1) % root.palette.length]
                    }
                }
            }

            // Width — current dot, tap cycles presets
            Rectangle {
                visible: root.view.annotationActive
                Layout.preferredWidth: root.btn; Layout.preferredHeight: root.btn
                radius: 4
                color: "transparent"; border.width: 1; border.color: ui.theme.strokeColor
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width - 8
                    height: Math.max(2, root.view.annotationWidth / 4)
                    radius: height / 2
                    color: ui.theme.fontPrimaryColor
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        var ci = 0
                        for (var k = 0; k < root.widths.length; ++k) {
                            if (Math.abs(root.view.annotationWidth - root.widths[k]) < 0.5) { ci = k; break }
                        }
                        root.view.annotationWidth = root.widths[(ci + 1) % root.widths.length]
                    }
                }
            }
        }
    }

    // Dock menu — tap the handle to open. Choose orientation + where to dock.
    Rectangle {
        id: dockMenu
        visible: root.menuOpen
        z: 10
        x: root.horizontal ? 0 : (bg.width + 6)
        y: root.horizontal ? -(height + 6) : 0
        width: menuCol.implicitWidth + 12
        height: menuCol.implicitHeight + 12
        radius: 6
        color: ui.theme.backgroundPrimaryColor
        border.width: 1
        border.color: ui.theme.strokeColor

        Column {
            id: menuCol
            anchors.centerIn: parent
            spacing: 4

            Row {
                spacing: 4
                FlatButton {
                    text: qsTrc("notation", "Horizontal")
                    accentButton: root.horizontal
                    onClicked: {
                        root.floatHorizontal = true
                        if (root.dockEdge === "left" || root.dockEdge === "right") { root.dockEdge = "" }
                        root.applyDock(); root.menuOpen = false
                    }
                }
                FlatButton {
                    text: qsTrc("notation", "Vertical")
                    accentButton: !root.horizontal
                    onClicked: {
                        root.floatHorizontal = false
                        if (root.dockEdge === "top" || root.dockEdge === "bottom") { root.dockEdge = "" }
                        root.applyDock(); root.menuOpen = false
                    }
                }
            }

            Rectangle { width: menuCol.width; height: 1; color: ui.theme.strokeColor; opacity: 0.5 }

            FlatButton { width: menuCol.width; text: qsTrc("notation", "Float"); accentButton: root.dockEdge === ""; onClicked: { root.dockEdge = ""; root.applyDock(); root.menuOpen = false } }
            FlatButton { width: menuCol.width; text: qsTrc("notation", "Dock left"); accentButton: root.dockEdge === "left"; onClicked: { root.dockEdge = "left"; root.applyDock(); root.menuOpen = false } }
            FlatButton { width: menuCol.width; text: qsTrc("notation", "Dock right"); accentButton: root.dockEdge === "right"; onClicked: { root.dockEdge = "right"; root.applyDock(); root.menuOpen = false } }
            FlatButton { width: menuCol.width; text: qsTrc("notation", "Dock bottom"); accentButton: root.dockEdge === "bottom"; onClicked: { root.dockEdge = "bottom"; root.applyDock(); root.menuOpen = false } }
        }
    }
}
