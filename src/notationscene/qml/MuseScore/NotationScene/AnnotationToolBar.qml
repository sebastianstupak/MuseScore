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

    // Toolbar scale. 30px buttons are a mouse size; on a 10" e-ink panel held
    // at arm's length and tapped with a pen they are small targets and the
    // glyphs are hard to read. Scale is user-chosen from the dock menu and
    // multiplies BOTH the button box and the glyph inside it -- scaling the
    // box alone just puts more padding around the same tiny icon.
    // FOCUS MODE: hide MuseScore's own chrome and work from this strip.
    //
    // On a 10" panel the palettes panel alone takes ~27% of the width and
    // the note-input bar and playback bar another ~190px of height, which
    // is a lot of furniture around a page you are trying to write on with
    // a pen. Everything those two bars provide that matters here already
    // has a button on this strip, so they can go.
    //
    // The panels are driven with setActionChecked(), not dispatchAction():
    // these are TOGGLES, so firing them blind would switch hidden panels
    // back ON. Restoring puts back exactly what was showing when focus
    // mode was entered, rather than a guess at a default layout.
    property bool focusMode: false
    readonly property var chromeActions: [
        "toggle-palettes",     // Palettes
        "toggle-instruments",  // Layout
        "inspector",           // Properties
        "toggle-noteinput",    // the note-duration bar across the top
        "toggle-transport"     // the playback bar
    ]
    property var chromeWasOn: ({})

    function setFocusMode(on) {
        if (!view) {
            return
        }
        if (on) {
            var was = {}
            for (var i = 0; i < chromeActions.length; ++i) {
                var a = chromeActions[i]
                was[a] = root.view.isActionChecked(a)
                root.view.setActionChecked(a, false)
            }
            chromeWasOn = was
        } else {
            for (var k = 0; k < chromeActions.length; ++k) {
                var c = chromeActions[k]
                // Default to showing it again if we have no record (focus
                // mode was on at startup, say) -- leaving the UI emptier
                // than we found it is the worse failure.
                var want = (chromeWasOn[c] === undefined) ? true : chromeWasOn[c]
                root.view.setActionChecked(c, want)
            }
        }
        focusMode = on
        Qt.callLater(applyDock)
    }

    property real uiScale: 1.0
    readonly property real btn: Math.round(30 * uiScale)
    readonly property int glyph: Math.round(16 * uiScale)
    readonly property real gap: Math.max(2, Math.round(3 * uiScale))

    readonly property var palette: ["#1a1a1a", "#e03030", "#2a6be0", "#28a745", "#f0a020", "#a020c0"]
    readonly property var widths: [6, 15, 30]

    // One place that knows how big a tool button is. Without this the sizes
    // were repeated on every button and the glyph size was not set at all,
    // so a scale control could only ever have resized empty boxes.
    component ToolBtn: FlatButton {
        Layout.preferredWidth: root.btn
        Layout.preferredHeight: root.btn
        // textFont, NOT font: FlatButton declares `property font iconFont`
        // and `property font textFont`, and has no plain `font`. Assigning
        // to it is not a no-op -- QML refuses to load the component, which
        // takes down NotationView, PublishPage, WindowContent and finally
        // the whole main window: "Failed to load main qml file ... Cannot
        // assign to non-existent property \"font\"". The app got as far as
        // the splash screen and stopped there.
        //
        // Built with Qt.font() rather than the grouped form so the family
        // comes from the theme and only the size is ours.
        iconFont: Qt.font({ family: ui.theme.iconsFont.family, pixelSize: root.glyph })
        textFont: Qt.font({ family: ui.theme.bodyFont.family, pixelSize: root.glyph })
    }

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
            columnSpacing: root.gap
            rowSpacing: root.gap

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
            ToolBtn { icon: IconCode.PLAY; toolTipTitle: qsTrc("notation", "Play / Pause"); onClicked: root.view.dispatchAction("play") }
            ToolBtn { icon: IconCode.STOP; toolTipTitle: qsTrc("notation", "Stop"); onClicked: root.view.dispatchAction("stop") }
            ToolBtn { icon: IconCode.REWIND; toolTipTitle: qsTrc("notation", "Rewind to start"); onClicked: root.view.dispatchAction("rewind") }
            ToolBtn { icon: IconCode.LOOP; toolTipTitle: qsTrc("notation", "Toggle loop"); onClicked: root.view.dispatchAction("loop") }
            ToolBtn { icon: IconCode.METRONOME; toolTipTitle: qsTrc("notation", "Toggle metronome"); onClicked: root.view.dispatchAction("metronome") }

            Rectangle { Layout.preferredWidth: root.horizontal ? 1 : root.btn; Layout.preferredHeight: root.horizontal ? root.btn : 1; color: ui.theme.strokeColor; opacity: 0.5 }

            // Zoom + view + file
            ToolBtn { icon: IconCode.ZOOM_OUT; toolTipTitle: qsTrc("notation", "Zoom out"); onClicked: root.view.dispatchAction("zoomout") }
            ToolBtn { icon: IconCode.ZOOM_IN; toolTipTitle: qsTrc("notation", "Zoom in"); onClicked: root.view.dispatchAction("zoomin") }
            ToolBtn { icon: IconCode.PAGE; toolTipTitle: qsTrc("notation", "Page / continuous view"); onClicked: root.view.toggleViewMode() }
            ToolBtn { icon: IconCode.SAVE; toolTipTitle: qsTrc("notation", "Save"); onClicked: root.view.dispatchAction("file-save") }
            ToolBtn { text: "PDF"; toolTipTitle: qsTrc("notation", "Export…"); onClicked: root.view.dispatchAction("file-export") }

            Rectangle { Layout.preferredWidth: root.horizontal ? 1 : root.btn; Layout.preferredHeight: root.horizontal ? root.btn : 1; color: ui.theme.strokeColor; opacity: 0.5 }

            // Score-level undo/redo. The ink undo/redo further down only
            // touches annotations; with the note-input bar hidden there is
            // otherwise no way to undo an actual edit from the strip.
            // "action://notation/undo", not "undo": that is the code
            // NotationUiActions actually registers. A bare "undo" is not an
            // unknown-action error, it is a silent no-op -- the button
            // would have looked fine and done nothing.
            ToolBtn { icon: IconCode.UNDO; toolTipTitle: qsTrc("notation", "Undo"); onClicked: root.view.dispatchAction("action://notation/undo") }
            ToolBtn { icon: IconCode.REDO; toolTipTitle: qsTrc("notation", "Redo"); onClicked: root.view.dispatchAction("action://notation/redo") }

            // Note input, which normally lives in the bar we hide.
            ToolBtn { text: "N"; toolTipTitle: qsTrc("notation", "Note input (N)"); accentButton: root.view.isActionChecked("note-input"); onClicked: root.view.dispatchAction("note-input") }

            // Focus mode: hide the palettes, Layout, Properties, the
            // note-input bar and the playback bar, leaving the page and
            // this strip.
            ToolBtn { text: "⤢"; toolTipTitle: qsTrc("notation", "Focus mode — hide panels and toolbars"); accentButton: root.focusMode; onClicked: root.setFocusMode(!root.focusMode) }

            Rectangle { Layout.preferredWidth: root.horizontal ? 1 : root.btn; Layout.preferredHeight: root.horizontal ? root.btn : 1; color: ui.theme.strokeColor; opacity: 0.5 }

            // Annotate toggle
            ToolBtn { icon: IconCode.EDIT; toolTipTitle: qsTrc("notation", "Annotate (Ctrl+Alt+A)"); accentButton: root.view.annotationActive; onClicked: root.view.toggleAnnotation() }

            // Write (recognize handwriting -> notation) toggle
            ToolBtn { text: "♪"; toolTipTitle: qsTrc("notation", "Write notation (Ctrl+Alt+W)"); accentButton: root.view.writeModeActive; onClicked: root.view.toggleWriteMode() }

            // Sticky "Shift" for additive lasso (multi-select) — a pen-friendly modifier
            ToolBtn { visible: root.view.writeModeActive; text: "⊕"; toolTipTitle: qsTrc("notation", "Add to selection (multi-select loops)"); accentButton: root.view.addToSelectionActive; onClicked: root.view.toggleAddToSelection() }

            // --- Ink tools (only while annotating) ---
            ToolBtn { visible: root.view.annotationActive; icon: IconCode.EDIT; toolTipTitle: qsTrc("notation", "Pen (P)"); accentButton: root.view.annotationTool === 0; onClicked: root.view.annotationTool = 0 }
            ToolBtn { visible: root.view.annotationActive; icon: IconCode.BRUSH; toolTipTitle: qsTrc("notation", "Highlighter (H)"); accentButton: root.view.annotationTool === 1; onClicked: root.view.annotationTool = 1 }
            ToolBtn { visible: root.view.annotationActive; text: "⌫"; toolTipTitle: qsTrc("notation", "Eraser (E)"); accentButton: root.view.annotationTool === 2; onClicked: root.view.annotationTool = 2 }
            ToolBtn { visible: root.view.annotationActive; icon: IconCode.UNDO; enabled: root.view.annotationCanUndo; toolTipTitle: qsTrc("notation", "Undo"); onClicked: root.view.annotationUndo() }
            ToolBtn { visible: root.view.annotationActive; icon: IconCode.REDO; enabled: root.view.annotationCanRedo; toolTipTitle: qsTrc("notation", "Redo"); onClicked: root.view.annotationRedo() }
            ToolBtn { visible: root.view.annotationActive; icon: IconCode.DELETE_TANK; toolTipTitle: qsTrc("notation", "Clear all annotations"); onClicked: root.view.annotationClear() }

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
        // Keep the popover ON SCREEN.
        //
        // It used to be pinned to one side unconditionally: a vertical strip
        // put it at x = bg.width + 6, i.e. to the RIGHT of the strip -- and
        // the strip's default dock is the right edge, so the menu opened
        // past the edge of the panel and could not be read or tapped. The
        // horizontal case had the same bug upwards: y = -(height + 6) is
        // above the strip, which is off the top when docked to the top.
        //
        // Flip to the other side when the preferred one does not fit, then
        // clamp into the parent. Coordinates are relative to root, so the
        // on-screen position is root.x + x and the limits carry root.x.
        x: {
            var pref = root.horizontal ? 0 : (bg.width + 6)
            if (!root.parent) {
                return pref
            }
            if (!root.horizontal && root.x + pref + width > root.parent.width) {
                pref = -(width + 6)      // no room right: open to the left
            }
            return Math.max(-root.x,
                            Math.min(pref, root.parent.width - width - root.x))
        }
        y: {
            var pref = root.horizontal ? -(height + 6) : 0
            if (!root.parent) {
                return pref
            }
            if (root.horizontal && root.y + pref < 0) {
                pref = bg.height + 6     // no room above: open below
            }
            return Math.max(-root.y,
                            Math.min(pref, root.parent.height - height - root.y))
        }
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

            // Size. The default 30px button is a mouse target; this panel is
            // tapped with a pen and read at arm's length.
            Row {
                spacing: 4
                FlatButton {
                    text: qsTrc("notation", "S")
                    toolTipTitle: qsTrc("notation", "Small toolbar")
                    accentButton: Math.abs(root.uiScale - 1.0) < 0.01
                    onClicked: { root.uiScale = 1.0; Qt.callLater(root.applyDock) }
                }
                FlatButton {
                    text: qsTrc("notation", "M")
                    toolTipTitle: qsTrc("notation", "Medium toolbar")
                    accentButton: Math.abs(root.uiScale - 1.5) < 0.01
                    onClicked: { root.uiScale = 1.5; Qt.callLater(root.applyDock) }
                }
                FlatButton {
                    text: qsTrc("notation", "L")
                    toolTipTitle: qsTrc("notation", "Large toolbar")
                    accentButton: Math.abs(root.uiScale - 2.0) < 0.01
                    onClicked: { root.uiScale = 2.0; Qt.callLater(root.applyDock) }
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
