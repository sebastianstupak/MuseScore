/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents
import MuseScore.NotationScene

StyledDialogView {
    id: root

    // Populated from the "helpText" open() param (the engine is the source of truth).
    property string helpText: ""

    contentWidth: 580
    contentHeight: 540
    margins: 16

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        StyledTextLabel {
            Layout.fillWidth: true
            text: qsTrc("notation", "motus — Vim mode")
            font: ui.theme.bodyBoldFont
            horizontalAlignment: Text.AlignLeft
        }

        StyledFlickable {
            id: flick

            Layout.fillWidth: true
            Layout.fillHeight: true

            contentWidth: helpBody.implicitWidth
            contentHeight: helpBody.implicitHeight

            Text {
                id: helpBody

                text: root.helpText
                textFormat: Text.PlainText
                wrapMode: Text.NoWrap
                color: ui.theme.fontPrimaryColor
                font.family: "Consolas"
                font.pixelSize: 13
            }
        }

        ButtonBox {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom

            buttons: [ ButtonBoxModel.Ok ]

            onStandardButtonClicked: function(buttonId) {
                if (buttonId === ButtonBoxModel.Ok) {
                    root.hide()
                }
            }
        }
    }
}
