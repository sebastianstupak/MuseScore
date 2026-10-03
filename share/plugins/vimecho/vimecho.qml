import QtQuick 2.15
import MuseScore 3.0

MuseScore {
    version: "0.1"
    description: "M1 interception spike — shows intercepted score-view keys in the panel; eats 'j' to prove interception."
    title: "Vim Echo"
    pluginType: "dialog"   // deprecated no-op; residency comes from the open viewer panel

    property string lastInfo: "focus the score and press keys…"

    NotationInput { id: input }

    onRun: {
        input.setKeyHandler(function(key, mods, text, wouldConsume) {
            // Called by the C++ interceptor for every score-view key (both phases).
            lastInfo = "key=" + key + "  mods=" + mods + "  text=" + JSON.stringify(text)
                     + "  phase=" + wouldConsume + (text === "j" ? "   [EATEN]" : "")
            console.log("vimecho:", key, mods, JSON.stringify(text), "phase=" + wouldConsume)
            return text === "j"   // consume the 'j' key; let everything else pass through
        })
    }

    // Minimal always-present panel keeps the plugin resident (so the handler stays
    // registered) AND gives live visual feedback of what got intercepted.
    Rectangle {
        width: 420; height: 76; color: "#202020"
        Column {
            anchors.centerIn: parent
            spacing: 6
            Text { color: "#88ff88"; font.bold: true; text: "VIM ECHO  —  'j' is intercepted / eaten" }
            Text { color: "white"; text: lastInfo }
        }
    }
}
