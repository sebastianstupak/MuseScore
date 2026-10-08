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
    onMenuOpenChanged: Qt.callLater(root.reportGeometry)
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
        Qt.callLater(reportGeometry)
    }

    property real uiScale: 1.0
    readonly property real btn: Math.round(30 * uiScale)
    readonly property int glyph: Math.round(16 * uiScale)
    readonly property real gap: Math.max(2, Math.round(3 * uiScale))

    // Labels, because this strip is used with a pen and a pen does not
    // hover. Every button carried a toolTipTitle and not one of them could
    // ever be read: a tooltip needs a pointer resting on the control. The
    // icons were therefore the whole interface -- and two pairs of them
    // (annotate/pen, score undo/ink undo) were drawn identically.
    property bool showLabels: true
    readonly property int labelPx: Math.max(9, Math.round(10 * uiScale))

    // Cell width comes from MEASURING the longest label, not from some
    // multiple of the button size. Guessing a multiple is how the export
    // button came to render "PDF" as "PD".
    readonly property var allLabels: [
        "Play", "Stop", "Start", "Loop", "Metro",
        "Zoom -", "Zoom +", "View", "Save", "Export",
        "Undo", "Redo", "Notes", "Focus",
        "Ink", "Write", "Multi",
        "Pen", "Marker", "Eraser", "Undo ink", "Redo ink", "Clear",
        "Colour", "Width"
    ]
    readonly property string longestLabel: {
        var best = ""
        for (var i = 0; i < allLabels.length; ++i) {
            if (allLabels[i].length > best.length) {
                best = allLabels[i]
            }
        }
        return best
    }
    TextMetrics {
        id: labelMetrics
        font: Qt.font({ family: ui.theme.bodyFont.family, pixelSize: root.labelPx })
        text: root.longestLabel
    }
    readonly property real cellW: root.showLabels
        ? Math.max(root.btn, Math.ceil(labelMetrics.width) + 10)
        : root.btn
    readonly property real cellH: root.showLabels
        ? root.btn + root.labelPx + 10
        : root.btn

    readonly property var palette: ["#1a1a1a", "#e03030", "#2a6be0", "#28a745", "#f0a020", "#a020c0"]
    readonly property var widths: [6, 15, 30]

    // One place that knows how big a tool button is. Without this the sizes
    // were repeated on every button and the glyph size was not set at all,
    // so a scale control could only ever have resized empty boxes.
    component ToolBtn: FlatButton {
        // width/height, not Layout.preferred*: inside a Flow there is no
        // layout attached to honour those, and every button would collapse
        // to its implicit size.
        property string label: ""

        width: root.cellW
        height: root.cellH
        // FlatButton defaults to minWidth 132 and margins 16 for anything
        // that is not icon-only -- sized for a dialog, not for a cell on a
        // tool strip. Adding a label silently switches it into that mode.
        minWidth: 0
        margins: 2
        orientation: Qt.Vertical
        maximumLineCount: 1
        text: root.showLabels ? label : ""
        accessible.name: label !== "" ? label : toolTipTitle
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
        textFont: Qt.font({ family: ui.theme.bodyFont.family, pixelSize: root.labelPx })

        // The theme's accent is a pale blue. This panel is greyscale
        // e-ink, where that sits about 35 levels from the normal button
        // fill: a tint you have to go looking for. An outline survives the
        // conversion to grey, so which tool is ON stays readable.
        Rectangle {
            anchors.fill: parent
            z: 100
            visible: parent.accentButton
            color: "transparent"
            radius: 3
            border.width: Math.max(2, Math.round(root.uiScale))
            border.color: ui.theme.fontPrimaryColor
        }
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

    // Publish where the strip is, so a test can tap the handle and the size
    // buttons from measured geometry rather than from an offset off the
    // screen edge -- which stopped being the strip at all once it wrapped
    // into two columns.
    //
    // The column count comes from the laid-out width: Flow decides it, and
    // nothing else in the file knows how many columns there are.
    // Centre of a named button, in the same coordinate space as the strip box
    // the function below reports. Tests tap these; nothing else may.
    function btnCentre(item) {
        if (!item || !item.visible || !root.parent) {
            return "0,0"
        }
        var p = item.mapToItem(root.parent, item.width / 2, item.height / 2)
        return Math.round(p.x) + "," + Math.round(p.y)
    }

    function reportGeometry() {
        if (!view || !bg) {
            return
        }
        var per = root.cellW + root.gap
        var cols = root.horizontal ? 1 : Math.max(1, Math.round(bg.width / per))
        view.reportToolbarGeometry(Math.round(root.x), Math.round(root.y),
                                   Math.round(bg.width), Math.round(bg.height), cols,
                                   Math.round(parent ? parent.width : 0),
                                   Math.round(parent ? parent.height : 0),
                                   root.menuOpen, root.showLabels,
                                   "focus=" + btnCentre(focusBtn)
                                   + ";save=" + btnCentre(saveBtn)
                                   + ";note=" + btnCentre(noteBtn)
                                   + ";handle=" + btnCentre(handle))
    }
    onXChanged: Qt.callLater(reportGeometry)
    onYChanged: Qt.callLater(reportGeometry)
    onUiScaleChanged: Qt.callLater(reportGeometry)

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

    // How far the strip may run before it has to wrap onto another
    // row/column. At L scale the button column is taller than the panel, so
    // a single column simply ran off the bottom and the tools at the end --
    // colour, width, clear -- were unreachable. 44px of clearance at the top
    // for the title bar, the same at the bottom so the last button is not
    // flush with the edge.
    readonly property real maxExtent: !parent ? 1200
        : (horizontal ? Math.max(240, parent.width - 48)
                      : Math.max(240, parent.height - 96))

    Rectangle {
        id: bg
        // childrenRect, not implicitWidth: after Flow has wrapped, this is
        // the area actually occupied. implicit* would describe one
        // unwrapped line and the panel would be the wrong size around it.
        width: content.childrenRect.width + 8
        height: content.childrenRect.height + 8
        radius: 8
        color: ui.theme.backgroundPrimaryColor
        border.width: 1
        border.color: ui.theme.strokeColor

        onWidthChanged: Qt.callLater(root.reportGeometry)
        onHeightChanged: Qt.callLater(root.reportGeometry)

        MouseArea { anchors.fill: parent }   // absorb clicks on the strip body

        // Flow, not GridLayout. A grid needs to be told how many rows and
        // columns, and the button count is not fixed -- six of these only
        // exist while annotating, one only in write mode. Flow wraps on the
        // bound below whatever is visible at the time.
        //
        // The bound is a FIXED number (maxExtent), never derived from the
        // children: binding the wrap limit to the content that the wrapping
        // produces is a loop.
        Flow {
            id: content
            x: 4
            y: 4
            flow: root.horizontal ? Flow.LeftToRight : Flow.TopToBottom
            width: root.horizontal ? root.maxExtent : childrenRect.width
            height: root.horizontal ? childrenRect.height : root.maxExtent
            spacing: root.gap

            // Drag handle
            Rectangle {
                id: handle
                width: root.horizontal ? 12 : root.cellW
                height: root.horizontal ? root.cellH : 12
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
            ToolBtn { icon: IconCode.PLAY; label: qsTrc("notation", "Play"); toolTipTitle: qsTrc("notation", "Play / Pause"); onClicked: root.view.dispatchAction("play") }
            ToolBtn { icon: IconCode.STOP; label: qsTrc("notation", "Stop"); toolTipTitle: qsTrc("notation", "Stop"); onClicked: root.view.dispatchAction("stop") }
            ToolBtn { icon: IconCode.REWIND; label: qsTrc("notation", "Start"); toolTipTitle: qsTrc("notation", "Rewind to start"); onClicked: root.view.dispatchAction("rewind") }
            ToolBtn { icon: IconCode.LOOP; label: qsTrc("notation", "Loop"); toolTipTitle: qsTrc("notation", "Toggle loop"); onClicked: root.view.dispatchAction("loop") }
            ToolBtn { icon: IconCode.METRONOME; label: qsTrc("notation", "Metro"); toolTipTitle: qsTrc("notation", "Toggle metronome"); onClicked: root.view.dispatchAction("metronome") }

            Rectangle { width: root.horizontal ? 1 : root.cellW; height: root.horizontal ? root.cellH : 1; color: ui.theme.strokeColor; opacity: 0.5 }

            // Zoom + view + file
            ToolBtn { icon: IconCode.ZOOM_OUT; label: qsTrc("notation", "Zoom -"); toolTipTitle: qsTrc("notation", "Zoom out"); onClicked: root.view.dispatchAction("zoomout") }
            ToolBtn { icon: IconCode.ZOOM_IN; label: qsTrc("notation", "Zoom +"); toolTipTitle: qsTrc("notation", "Zoom in"); onClicked: root.view.dispatchAction("zoomin") }
            ToolBtn { icon: IconCode.PAGE_VIEW; label: qsTrc("notation", "View"); toolTipTitle: qsTrc("notation", "Page / continuous view"); onClicked: root.view.toggleViewMode() }
            ToolBtn { id: saveBtn; icon: IconCode.SAVE; label: qsTrc("notation", "Save"); toolTipTitle: qsTrc("notation", "Save"); onClicked: root.view.dispatchAction("file-save") }
            ToolBtn { icon: IconCode.SHARE_FILE; label: qsTrc("notation", "Export"); toolTipTitle: qsTrc("notation", "Export to PDF / audio…"); onClicked: root.view.dispatchAction("file-export") }

            Rectangle { width: root.horizontal ? 1 : root.cellW; height: root.horizontal ? root.cellH : 1; color: ui.theme.strokeColor; opacity: 0.5 }

            // Score-level undo/redo. The ink undo/redo further down only
            // touches annotations; with the note-input bar hidden there is
            // otherwise no way to undo an actual edit from the strip.
            // "action://notation/undo", not "undo": that is the code
            // NotationUiActions actually registers. A bare "undo" is not an
            // unknown-action error, it is a silent no-op -- the button
            // would have looked fine and done nothing.
            ToolBtn { icon: IconCode.UNDO; label: qsTrc("notation", "Undo"); toolTipTitle: qsTrc("notation", "Undo"); onClicked: root.view.dispatchAction("action://notation/undo") }
            ToolBtn { icon: IconCode.REDO; label: qsTrc("notation", "Redo"); toolTipTitle: qsTrc("notation", "Redo"); onClicked: root.view.dispatchAction("action://notation/redo") }

            // Note input, which normally lives in the bar we hide.
            ToolBtn { id: noteBtn; icon: IconCode.NOTE_QUARTER; label: qsTrc("notation", "Notes"); toolTipTitle: qsTrc("notation", "Note input (N)"); accentButton: root.view.isActionChecked("note-input"); onClicked: root.view.dispatchAction("note-input") }

            // Focus mode: hide the palettes, Layout, Properties, the
            // note-input bar and the playback bar, leaving the page and
            // this strip.
            ToolBtn { id: focusBtn; icon: root.focusMode ? IconCode.EYE_OPEN : IconCode.EYE_CLOSED; label: qsTrc("notation", "Focus"); toolTipTitle: qsTrc("notation", "Focus mode — hide panels and toolbars"); accentButton: root.focusMode; onClicked: root.setFocusMode(!root.focusMode) }

            Rectangle { width: root.horizontal ? 1 : root.cellW; height: root.horizontal ? root.cellH : 1; color: ui.theme.strokeColor; opacity: 0.5 }

            // Annotate toggle
            ToolBtn { icon: IconCode.BRUSH; label: qsTrc("notation", "Ink"); toolTipTitle: qsTrc("notation", "Annotate (Ctrl+Alt+A)"); accentButton: root.view.annotationActive; onClicked: root.view.toggleAnnotation() }

            // Write (recognize handwriting -> notation) toggle
            ToolBtn { icon: IconCode.MUSIC_NOTES; label: qsTrc("notation", "Write"); toolTipTitle: qsTrc("notation", "Write notation (Ctrl+Alt+W)"); accentButton: root.view.writeModeActive; onClicked: root.view.toggleWriteMode() }

            // Sticky "Shift" for additive lasso (multi-select) — a pen-friendly modifier
            ToolBtn { visible: root.view.writeModeActive; icon: IconCode.PLUS; label: qsTrc("notation", "Multi"); toolTipTitle: qsTrc("notation", "Add to selection (multi-select loops)"); accentButton: root.view.addToSelectionActive; onClicked: root.view.toggleAddToSelection() }

            // --- Ink tools (only while annotating) ---
            ToolBtn { visible: root.view.annotationActive; icon: IconCode.EDIT; label: qsTrc("notation", "Pen"); toolTipTitle: qsTrc("notation", "Pen (P)"); accentButton: root.view.annotationTool === 0; onClicked: root.view.annotationTool = 0 }
            ToolBtn { visible: root.view.annotationActive; icon: IconCode.MARKER; label: qsTrc("notation", "Marker"); toolTipTitle: qsTrc("notation", "Highlighter (H)"); accentButton: root.view.annotationTool === 1; onClicked: root.view.annotationTool = 1 }
            ToolBtn { visible: root.view.annotationActive; icon: IconCode.CLOSE_X_ROUNDED; label: qsTrc("notation", "Eraser"); toolTipTitle: qsTrc("notation", "Eraser (E)"); accentButton: root.view.annotationTool === 2; onClicked: root.view.annotationTool = 2 }
            ToolBtn { visible: root.view.annotationActive; icon: IconCode.UNDO; label: qsTrc("notation", "Undo ink"); enabled: root.view.annotationCanUndo; toolTipTitle: qsTrc("notation", "Undo the last ink stroke"); onClicked: root.view.annotationUndo() }
            ToolBtn { visible: root.view.annotationActive; icon: IconCode.REDO; label: qsTrc("notation", "Redo ink"); enabled: root.view.annotationCanRedo; toolTipTitle: qsTrc("notation", "Redo the last ink stroke"); onClicked: root.view.annotationRedo() }
            ToolBtn { visible: root.view.annotationActive; icon: IconCode.DELETE_TANK; label: qsTrc("notation", "Clear"); toolTipTitle: qsTrc("notation", "Clear all annotations"); onClicked: root.view.annotationClear() }

            // Colour — current swatch, tap cycles the palette
            Item {
                visible: root.view.annotationActive
                width: root.cellW; height: root.cellH
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 2
                    width: root.btn - 4; height: root.btn - 4
                    radius: 4
                    color: root.view.annotationColor
                    border.width: 1; border.color: ui.theme.strokeColor
                }
                StyledTextLabel {
                    visible: root.showLabels
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3
                    font: Qt.font({ family: ui.theme.bodyFont.family, pixelSize: root.labelPx })
                    text: qsTrc("notation", "Colour")
                }
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
            Item {
                visible: root.view.annotationActive
                width: root.cellW; height: root.cellH
                Rectangle {
                    id: widthBox
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 2
                    width: root.btn - 4; height: root.btn - 4
                    radius: 4
                    color: "transparent"; border.width: 1; border.color: ui.theme.strokeColor
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width - 8
                        height: Math.max(2, root.view.annotationWidth / 4)
                        radius: height / 2
                        color: ui.theme.fontPrimaryColor
                    }
                }
                StyledTextLabel {
                    visible: root.showLabels
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3
                    font: Qt.font({ family: ui.theme.bodyFont.family, pixelSize: root.labelPx })
                    text: qsTrc("notation", "Width")
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

    // Dismissing the dock menu. It is a plain Rectangle rather than a Popup,
    // so it came with none of a Popup's behaviour: Escape did nothing and a
    // tap outside did nothing, leaving "choose one of the options" as the
    // only way out of a menu opened by accident -- on a device with no
    // keyboard, a trap. Both are wired up by hand here.
    //
    // It also made the handle a pure toggle with no way to tell open from
    // closed, which is what let a test tap the handle, see the menu VANISH,
    // and report that it had opened.
    // A child of root at z 9, NOT a sibling of root. As a sibling it sat
    // above the whole toolbar -- including the menu, whose z 10 only
    // orders it against its own siblings -- so every tap meant for S, M or
    // L would have dismissed the menu instead of pressing the button. Here
    // the menu is above it and the strip below, which is the behaviour a
    // popup is supposed to have.
    MouseArea {
        x: -root.x
        y: -root.y
        width: root.parent ? root.parent.width : 0
        height: root.parent ? root.parent.height : 0
        z: 9
        visible: root.menuOpen
        enabled: root.menuOpen
        onPressed: function(mouse) { root.menuOpen = false; mouse.accepted = true }
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.menuOpen
        context: Qt.WindowShortcut
        onActivated: root.menuOpen = false
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

            // Fixed, so the equal-width rows below cannot form a binding loop
            // by sizing themselves from the width they themselves determine.
            readonly property real colw: Math.round(150 * Math.max(1, root.uiScale * 0.8))

            Row {
                spacing: 4
                readonly property real cell: (menuCol.colw - spacing) / 2
                FlatButton {
                    width: parent.cell
                    text: qsTrc("notation", "Horizontal")
                    accentButton: root.horizontal
                    onClicked: {
                        root.floatHorizontal = true
                        if (root.dockEdge === "left" || root.dockEdge === "right") { root.dockEdge = "" }
                        root.applyDock(); root.menuOpen = false
                    }
                }
                FlatButton {
                    width: parent.cell
                    text: qsTrc("notation", "Vertical")
                    accentButton: !root.horizontal
                    onClicked: {
                        root.floatHorizontal = false
                        if (root.dockEdge === "top" || root.dockEdge === "bottom") { root.dockEdge = "" }
                        root.applyDock(); root.menuOpen = false
                    }
                }
            }

            Rectangle { width: menuCol.colw; height: 1; color: ui.theme.strokeColor; opacity: 0.5 }

            // Size. The default 30px button is a mouse target; this panel is
            // tapped with a pen and read at arm's length.
            Row {
                spacing: 4
                readonly property real cell: (menuCol.colw - 2 * spacing) / 3
                FlatButton {
                    width: parent.cell
                    text: qsTrc("notation", "S")
                    toolTipTitle: qsTrc("notation", "Small toolbar")
                    accentButton: Math.abs(root.uiScale - 1.0) < 0.01
                    onClicked: { root.uiScale = 1.0; Qt.callLater(root.applyDock) }
                }
                FlatButton {
                    width: parent.cell
                    text: qsTrc("notation", "M")
                    toolTipTitle: qsTrc("notation", "Medium toolbar")
                    accentButton: Math.abs(root.uiScale - 1.5) < 0.01
                    onClicked: { root.uiScale = 1.5; Qt.callLater(root.applyDock) }
                }
                FlatButton {
                    width: parent.cell
                    text: qsTrc("notation", "L")
                    toolTipTitle: qsTrc("notation", "Large toolbar")
                    accentButton: Math.abs(root.uiScale - 2.0) < 0.01
                    onClicked: { root.uiScale = 2.0; Qt.callLater(root.applyDock) }
                }
            }

            Rectangle { width: menuCol.colw; height: 1; color: ui.theme.strokeColor; opacity: 0.5 }

            // Labels are the point of the redesign, so they default on --
            // but they roughly double the width of the strip, and on a
            // small float that matters. One tap to get the compact icons
            // back.
            FlatButton {
                width: menuCol.colw
                text: root.showLabels ? qsTrc("notation", "Labels: on") : qsTrc("notation", "Labels: off")
                accentButton: root.showLabels
                onClicked: { root.showLabels = !root.showLabels; Qt.callLater(root.applyDock) }
            }

            Rectangle { width: menuCol.colw; height: 1; color: ui.theme.strokeColor; opacity: 0.5 }

            FlatButton { width: menuCol.colw; text: qsTrc("notation", "Float"); accentButton: root.dockEdge === ""; onClicked: { root.dockEdge = ""; root.applyDock(); root.menuOpen = false } }
            FlatButton { width: menuCol.colw; text: qsTrc("notation", "Dock left"); accentButton: root.dockEdge === "left"; onClicked: { root.dockEdge = "left"; root.applyDock(); root.menuOpen = false } }
            FlatButton { width: menuCol.colw; text: qsTrc("notation", "Dock right"); accentButton: root.dockEdge === "right"; onClicked: { root.dockEdge = "right"; root.applyDock(); root.menuOpen = false } }
            FlatButton { width: menuCol.colw; text: qsTrc("notation", "Dock bottom"); accentButton: root.dockEdge === "bottom"; onClicked: { root.dockEdge = "bottom"; root.applyDock(); root.menuOpen = false } }
        }
    }
}
