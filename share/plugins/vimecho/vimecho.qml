/*
 * SPDX-License-Identifier: GPL-3.0-only
 * M1 interception spike: proves the generic input-interceptor enabler works.
 * Opens as a resident GUI dialog (same form as stylus_test). While it is open,
 * it registers a key handler via NotationInput; focus the SCORE and press keys —
 * every score-view key shows here, and 'j' is consumed to prove interception.
 */
import QtQuick
import QtQuick.Layouts
import Muse.Ui
import Muse.UiComponents as MU
import MuseScore 3.0

MuseScore {
    id: root
    version: "0.2"
    title: "Vim Echo"
    description: "M1 interception spike: shows intercepted score-view keys; eats 'j'."
    pluginType: "dialog"
    requiresScore: true
    width: 480
    height: 200

    property string lastInfo: "Keep this open, click into the score, then press keys…"

    NotationInput { id: input }

    onRun: {
        input.setKeyHandler(function(key, mods, text, wouldConsume) {
            // Called by the C++ interceptor for every score-view key (both phases).
            root.lastInfo = "key=" + key + "   mods=" + mods + "   text=" + JSON.stringify(text)
                          + "   phase=" + wouldConsume + (text === "j" ? "     [EATEN]" : "")
            console.log("vimecho:", key, mods, JSON.stringify(text), "phase=" + wouldConsume)
            return text === "j"   // consume 'j'; let everything else pass through
        })
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        MU.StyledTextLabel {
            Layout.fillWidth: true
            text: "VIM ECHO — 'j' is intercepted / eaten.\nKeep this dialog open, click into the score, then press keys."
            horizontalAlignment: Text.AlignLeft
        }
        MU.StyledTextLabel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            font.family: "Consolas"
            text: root.lastInfo
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignTop
        }
    }
}
