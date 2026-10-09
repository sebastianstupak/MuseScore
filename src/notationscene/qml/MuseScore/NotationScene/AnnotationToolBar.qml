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

    // Size steps, written out rather than derived from one multiplier.
    // A single scale factor grew the icon, the label, the padding and the
    // row height all at once, so "Large" multiplied the entire strip: at
    // 2.0 it measured 776 QML units of an 1102-wide window -- 70% of the
    // screen to hold a toolbar. Shrink the window and it reached 1421,
    // wider than the panel. The touch target is the thing that needs to
    // grow with the size setting; the text only has to stay readable.
    property int sizeStep: 0                                // 0 = S, 1 = M, 2 = L
    readonly property var btnSteps:   [28, 36, 44]
    readonly property var glyphSteps: [15, 19, 23]
    readonly property var labelSteps: [10, 12, 14]
    readonly property real btn: btnSteps[sizeStep]
    readonly property int glyph: glyphSteps[sizeStep]
    readonly property int labelPx: labelSteps[sizeStep]
    readonly property real gap: 3
    // Only the dock menu's own width and the active outline still scale.
    readonly property real uiScale: [1.0, 1.3, 1.6][sizeStep]

    // Labels, because this strip is used with a pen and a pen does not
    // hover. Every button carried a toolTipTitle and not one of them
    // could ever be read.
    property bool showLabels: true

    // Secondary controls are behind this. Twenty-five labelled buttons
    // cannot be made small -- the only honest way to shrink the strip is
    // to show fewer of them, so the ten that get used stay out and the
    // rest are one tap away.
    property bool showMore: false

    readonly property var allLabels: [
        "Play", "Stop", "Rewind", "Loop", "Metronome",
        "Zoom out", "Zoom in", "Page view", "Hide panels",
        "Save", "Export", "Undo", "Redo",
        "Note input", "Write", "Multi select",
        "Select", "Draw", "Pen", "Marker", "Erase",
        "Undo draw", "Redo draw", "Erase all", "Colour", "Size",
        "More", "Less"
    ]
    // Labels sit BESIDE the icon on the vertical dock, so each one is a
    // single line and the widest whole label sets the width. Stacking
    // them underneath instead cost a second line of height on every row
    // and is what turned the strip into a wall.
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
    readonly property real labelW: Math.ceil(labelMetrics.width)
    readonly property real cellW: !showLabels
        ? btn
        : (root.horizontal ? Math.max(btn, labelW + 8) : btn + 6 + labelW + 8)
    readonly property real cellH: !showLabels
        ? btn
        : (root.horizontal ? btn + labelPx + 8 : btn)
    readonly property real headH: showLabels ? Math.round(labelPx * 1.5) + 2 : 7

    readonly property var palette: ["#1a1a1a", "#e03030", "#2a6be0", "#28a745", "#f0a020", "#a020c0"]
    readonly property var widths: [6, 15, 30]

    // One place that knows how big a tool button is. Without this the sizes
    // were repeated on every button and the glyph size was not set at all,
    // so a scale control could only ever have resized empty boxes.
    // A labelled group. Sections are the unit the Flow wraps, so a group
    // is never split across two columns and a heading can never be
    // orphaned at the bottom of one -- which is exactly what happens if
    // the heading is just another item in a flat Flow of buttons.
    //
    // Grid, not Column: `columns: 1` stacks it for the vertical dock and a
    // large `columns` makes it one row for the horizontal dock, so both
    // orientations come out of the same component. Positioners skip
    // invisible children, so the ink tools can appear and disappear
    // without leaving holes in the group.
    component Section: Grid {
        property string title: ""

        // An untitled group is just a rule, not a heading. Giving it the
        // full heading height cost ~20px and at Large that was the
        // difference between the strip fitting one column and needing
        // two.
        readonly property real headExtent: root.horizontal
            ? 0
            : (title === "" ? 5 : root.headH)
        // Everything except the heading item.
        readonly property int cells: Math.max(1, visibleChildren.length - 1)
        readonly property real needed: headExtent + cells * (root.cellH + root.gap)

        // One column normally. If the group is taller than the strip is
        // allowed to be, it splits instead: keeping a group together is
        // pointless if keeping it together hangs half of it off the
        // panel, and the window does get short -- shrink it and the
        // five-cell Play group no longer fits a single column.
        columns: root.horizontal
                 ? 99
                 : Math.max(1, Math.ceil(needed / Math.max(1, root.maxExtent)))
        spacing: root.gap

        Item {
            width: root.horizontal ? Math.round(root.labelPx * 3.4) : root.cellW
            height: root.horizontal ? root.cellH : parent.headExtent

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                width: root.horizontal ? 1 : parent.width
                height: root.horizontal ? parent.height : 1
                color: ui.theme.strokeColor
                opacity: 0.6
            }
            StyledTextLabel {
                visible: root.showLabels && title !== ""
                anchors.fill: parent
                anchors.topMargin: root.horizontal ? 0 : 4
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                text: title
                opacity: 0.6
                font: Qt.font({
                    family: ui.theme.bodyFont.family,
                    pixelSize: Math.max(8, Math.round(root.labelPx * 0.85)),
                    capitalization: Font.AllUppercase,
                    bold: true
                })
            }
        }
    }

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
        // Beside the icon when the strip is vertical, underneath when it
        // is horizontal -- whichever keeps the strip thin on the axis it
        // is docked against.
        orientation: root.horizontal ? Qt.Vertical : Qt.Horizontal
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
                                   + ";handle=" + btnCentre(menuBtn)
                                   + ";zoomin=" + btnCentre(zoomInBtn)
                                   + ";more=" + btnCentre(moreBtn))
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
    // Worst-case header reservation. Deliberately computed from plain
    // properties and never from the header's actual size: the header is
    // sized FROM the flow, so a maxExtent that read the header back would
    // close the loop flow -> header -> maxExtent -> flow.
    readonly property real headerReserve: horizontal ? 0 : root.btn * 2 + root.gap * 2

    readonly property real maxExtent: !parent ? 1200
        : (horizontal ? Math.max(240, parent.width - 48)
                      : Math.max(240, parent.height - 96 - headerReserve))

    Rectangle {
        id: bg
        // childrenRect, not implicitWidth: after Flow has wrapped, this is
        // the area actually occupied. implicit* would describe one
        // unwrapped line and the panel would be the wrong size around it.
        // The header is a full-width row of its own, above (or left of)
        // the wrapping content -- NOT an item inside the Flow. As a Flow
        // item it was simply the first cell, so as soon as the strip
        // wrapped, column two began beside it and the Ink toggle ended up
        // sitting in the header row next to the settings cog. Settings and
        // tools do not share a row.
        width: (root.horizontal ? headerRow.width + root.gap : 0)
               + content.childrenRect.width + 8
        height: (root.horizontal ? 0 : headerRow.height + root.gap)
                + content.childrenRect.height + 8
        radius: 8
        color: ui.theme.backgroundPrimaryColor
        border.width: 1
        border.color: ui.theme.strokeColor

        onWidthChanged: Qt.callLater(root.reportGeometry)
        onHeightChanged: Qt.callLater(root.reportGeometry)

        MouseArea { anchors.fill: parent }   // absorb clicks on the strip body

        // Strip header: a grip you drag, and a button that opens the
        // dock menu. These used to be the same control -- two dots
        // that moved the strip when dragged and opened a menu when
        // tapped. Nothing on screen said either was possible, and the
        // two gestures on one 12px target meant a slightly draggy tap
        // did the wrong one.
        Item {
            id: headerRow
            x: 4
            y: 4

            // Side by side when the strip is wide enough for two, stacked
            // when it is not. Measured against the FLOW's width, not one
            // cell: the header spans the whole strip now, so at two
            // columns there is room for both even though a single cell is
            // narrower than the pair.
            readonly property bool stacked: !root.horizontal
                                            && content.childrenRect.width < root.btn * 2 + 2

            width: root.horizontal ? root.btn * 2 : content.childrenRect.width
            height: root.horizontal
                    ? Math.max(root.btn, content.childrenRect.height)
                    : (stacked ? root.btn * 2 + root.gap : root.btn)

            Item {
                id: handle
                anchors.left: parent.left
                anchors.top: parent.top
                width: root.btn
                height: root.btn

                StyledIconLabel {
                    anchors.centerIn: parent
                    iconCode: IconCode.TOOLBAR_GRIP
                    font: Qt.font({ family: ui.theme.iconsFont.family, pixelSize: root.glyph })
                    opacity: 0.65
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.SizeAllCursor
                    drag.target: root
                    drag.minimumX: 0
                    drag.minimumY: 44   // stay below the window title bar (its drag moves the whole window)
                    drag.maximumX: root.parent ? Math.max(0, root.parent.width - root.width) : 0
                    drag.maximumY: root.parent ? Math.max(0, root.parent.height - root.height) : 0
                }
            }

            FlatButton {
                id: menuBtn
                anchors.right: parent.stacked ? undefined : parent.right
                anchors.left: parent.stacked ? parent.left : undefined
                y: parent.stacked ? root.btn + root.gap : 0
                width: root.btn
                height: root.btn
                minWidth: 0
                margins: 0
                transparent: true
                icon: IconCode.SETTINGS_COG
                iconFont: Qt.font({ family: ui.theme.iconsFont.family, pixelSize: root.glyph })
                toolTipTitle: qsTrc("notation", "Toolbar options")
                onClicked: root.menuOpen = !root.menuOpen
            }
        }

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
            x: root.horizontal ? 4 + headerRow.width + root.gap : 4
            y: root.horizontal ? 4 : 4 + headerRow.height + root.gap
            flow: root.horizontal ? Flow.LeftToRight : Flow.TopToBottom
            width: root.horizontal ? root.maxExtent : childrenRect.width
            height: root.horizontal ? childrenRect.height : root.maxExtent
            spacing: root.gap

            Section {
                title: qsTrc("notation", "Play")
                ToolBtn { icon: IconCode.PLAY; label: qsTrc("notation", "Play"); toolTipTitle: qsTrc("notation", "Play / Pause"); onClicked: root.view.dispatchAction("play") }
                ToolBtn { icon: IconCode.STOP; label: qsTrc("notation", "Stop"); toolTipTitle: qsTrc("notation", "Stop"); onClicked: root.view.dispatchAction("stop") }
                ToolBtn { visible: root.showMore; icon: IconCode.REWIND; label: qsTrc("notation", "Rewind"); toolTipTitle: qsTrc("notation", "Rewind to start"); onClicked: root.view.dispatchAction("rewind") }
                ToolBtn { visible: root.showMore; icon: IconCode.LOOP; label: qsTrc("notation", "Loop"); toolTipTitle: qsTrc("notation", "Toggle loop"); onClicked: root.view.dispatchAction("loop") }
                ToolBtn { visible: root.showMore; icon: IconCode.METRONOME; label: qsTrc("notation", "Metronome"); toolTipTitle: qsTrc("notation", "Toggle metronome"); onClicked: root.view.dispatchAction("metronome") }
            }

            Section {
                title: qsTrc("notation", "Notes")
                ToolBtn { id: noteBtn; icon: IconCode.NOTE_QUARTER; label: qsTrc("notation", "Note input"); toolTipTitle: qsTrc("notation", "Note input (N)"); accentButton: root.view.isActionChecked("note-input"); onClicked: root.view.dispatchAction("note-input") }
                ToolBtn { visible: root.view.writeModeActive; icon: IconCode.PLUS; label: qsTrc("notation", "Multi select"); toolTipTitle: qsTrc("notation", "Add to selection (multi-select loops)"); accentButton: root.view.addToSelectionActive; onClicked: root.view.toggleAddToSelection() }
            }

            Section {
                // "action://notation/undo", not "undo": that is the code
                // NotationUiActions actually registers. A bare "undo" is
                // not an unknown-action error, it is a silent no-op -- the
                // button would have looked fine and done nothing.
                title: qsTrc("notation", "Edit")
                ToolBtn { icon: IconCode.UNDO; label: qsTrc("notation", "Undo"); toolTipTitle: qsTrc("notation", "Undo"); onClicked: root.view.dispatchAction("action://notation/undo") }
                ToolBtn { icon: IconCode.REDO; label: qsTrc("notation", "Redo"); toolTipTitle: qsTrc("notation", "Redo"); onClicked: root.view.dispatchAction("action://notation/redo") }
                ToolBtn { id: saveBtn; icon: IconCode.SAVE; label: qsTrc("notation", "Save"); toolTipTitle: qsTrc("notation", "Save"); onClicked: root.view.dispatchAction("file-save") }
                ToolBtn { visible: root.showMore; icon: IconCode.SHARE_FILE; label: qsTrc("notation", "Export"); toolTipTitle: qsTrc("notation", "Export to PDF / audio…"); onClicked: root.view.dispatchAction("file-export") }
            }

            Section {
                title: qsTrc("notation", "View")
                ToolBtn { id: focusBtn; icon: root.focusMode ? IconCode.EYE_OPEN : IconCode.EYE_CLOSED; label: qsTrc("notation", "Hide panels"); toolTipTitle: qsTrc("notation", "Focus mode — hide panels and toolbars"); accentButton: root.focusMode; onClicked: root.setFocusMode(!root.focusMode) }
                ToolBtn { visible: root.showMore; icon: IconCode.ZOOM_OUT; label: qsTrc("notation", "Zoom out"); toolTipTitle: qsTrc("notation", "Zoom out"); onClicked: root.view.dispatchAction("zoomout") }
                ToolBtn { id: zoomInBtn; visible: root.showMore; icon: IconCode.ZOOM_IN; label: qsTrc("notation", "Zoom in"); toolTipTitle: qsTrc("notation", "Zoom in"); onClicked: root.view.dispatchAction("zoomin") }
                ToolBtn { visible: root.showMore; icon: IconCode.PAGE_VIEW; label: qsTrc("notation", "Page view"); toolTipTitle: qsTrc("notation", "Page / continuous view"); onClicked: root.view.toggleViewMode() }
            }

            Section {
                title: qsTrc("notation", "Pen mode")
                // Three mutually exclusive modes, shown as three buttons
                // rather than two toggles. With only Draw and Write there
                // was no way to express "neither": turning Draw off left
                // Write running, so the pen kept drawing and the strip
                // showed nothing to explain why.
                ToolBtn {
                    icon: IconCode.POSITION_ARROWS
                    label: qsTrc("notation", "Select")
                    toolTipTitle: qsTrc("notation", "Select and drag with the pen")
                    accentButton: !root.view.annotationActive && !root.view.writeModeActive
                    onClicked: root.view.setPointerMode()
                }
                ToolBtn { icon: IconCode.BRUSH; label: qsTrc("notation", "Draw"); toolTipTitle: qsTrc("notation", "Draw on the score (Ctrl+Alt+A)"); accentButton: root.view.annotationActive; onClicked: root.view.toggleAnnotation() }
                ToolBtn { icon: IconCode.MUSIC_NOTES; label: qsTrc("notation", "Write"); toolTipTitle: qsTrc("notation", "Write notation by hand (Ctrl+Alt+W)"); accentButton: root.view.writeModeActive; onClicked: root.view.toggleWriteMode() }
            }

            Section {
                // The tools that belong to Draw, and only exist while it
                // is on -- kept apart from the three modes above so the
                // mode row is always the same three buttons.
                title: qsTrc("notation", "Pen")
                visible: root.view.annotationActive
                ToolBtn { icon: IconCode.EDIT; label: qsTrc("notation", "Pen"); toolTipTitle: qsTrc("notation", "Pen (P)"); accentButton: root.view.annotationTool === 0; onClicked: root.view.annotationTool = 0 }
                ToolBtn { icon: IconCode.LINE_NORMAL; label: qsTrc("notation", "Marker"); toolTipTitle: qsTrc("notation", "Highlighter (H)"); accentButton: root.view.annotationTool === 1; onClicked: root.view.annotationTool = 1 }
                ToolBtn { icon: IconCode.CLOSE_X_ROUNDED; label: qsTrc("notation", "Erase"); toolTipTitle: qsTrc("notation", "Eraser (E)"); accentButton: root.view.annotationTool === 2; onClicked: root.view.annotationTool = 2 }
            }

            Section {
                // Separate from "Draw" because these act on the ink, not
                // on the score: the Undo in Edit is a different stack.
                title: qsTrc("notation", "Ink")
                visible: root.view.annotationActive
                ToolBtn { icon: IconCode.UNDO; label: qsTrc("notation", "Undo draw"); enabled: root.view.annotationCanUndo; toolTipTitle: qsTrc("notation", "Undo the last ink stroke"); onClicked: root.view.annotationUndo() }
                ToolBtn { icon: IconCode.REDO; label: qsTrc("notation", "Redo draw"); enabled: root.view.annotationCanRedo; toolTipTitle: qsTrc("notation", "Redo the last ink stroke"); onClicked: root.view.annotationRedo() }
                ToolBtn { icon: IconCode.DELETE_TANK; label: qsTrc("notation", "Erase all"); toolTipTitle: qsTrc("notation", "Clear all annotations"); onClicked: root.view.annotationClear() }

                // Colour — current swatch, tap cycles the palette
                Item {
                    width: root.cellW; height: root.cellH
                    Rectangle {
                        id: colourSwatch
                        anchors.left: parent.left
                        anchors.leftMargin: Math.round((root.btn - width) / 2)
                        anchors.verticalCenter: parent.verticalCenter
                        width: root.btn - 10; height: root.btn - 10
                        radius: 4
                        color: root.view.annotationColor
                        border.width: 1; border.color: ui.theme.strokeColor
                    }
                    StyledTextLabel {
                        visible: root.showLabels
                        anchors.left: parent.left
                        anchors.leftMargin: root.btn + 6
                        anchors.verticalCenter: parent.verticalCenter
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
                    width: root.cellW; height: root.cellH
                    Rectangle {
                        anchors.left: parent.left
                        anchors.leftMargin: Math.round((root.btn - width) / 2)
                        anchors.verticalCenter: parent.verticalCenter
                        width: root.btn - 10; height: root.btn - 10
                        radius: 4
                        color: "transparent"; border.width: 1; border.color: ui.theme.strokeColor
                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width - 6
                            height: Math.max(2, root.view.annotationWidth / 4)
                            radius: height / 2
                            color: ui.theme.fontPrimaryColor
                        }
                    }
                    StyledTextLabel {
                        visible: root.showLabels
                        anchors.left: parent.left
                        anchors.leftMargin: root.btn + 6
                        anchors.verticalCenter: parent.verticalCenter
                        font: Qt.font({ family: ui.theme.bodyFont.family, pixelSize: root.labelPx })
                        text: qsTrc("notation", "Size")
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

            Section {
                title: ""
                ToolBtn {
                    id: moreBtn
                    icon: root.showMore ? IconCode.SMALL_ARROW_UP : IconCode.SMALL_ARROW_DOWN
                    label: root.showMore ? qsTrc("notation", "Less") : qsTrc("notation", "More")
                    accentButton: root.showMore
                    toolTipTitle: qsTrc("notation", "Show the less-used tools")
                    onClicked: { root.showMore = !root.showMore; Qt.callLater(root.applyDock) }
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
                    accentButton: root.sizeStep === 0
                    onClicked: { root.sizeStep = 0; Qt.callLater(root.applyDock) }
                }
                FlatButton {
                    width: parent.cell
                    text: qsTrc("notation", "M")
                    toolTipTitle: qsTrc("notation", "Medium toolbar")
                    accentButton: root.sizeStep === 1
                    onClicked: { root.sizeStep = 1; Qt.callLater(root.applyDock) }
                }
                FlatButton {
                    width: parent.cell
                    text: qsTrc("notation", "L")
                    toolTipTitle: qsTrc("notation", "Large toolbar")
                    accentButton: root.sizeStep === 2
                    onClicked: { root.sizeStep = 2; Qt.callLater(root.applyDock) }
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
