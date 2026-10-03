import QtQuick 2.15
import MuseScore 3.0

MuseScore {
    version: "0.1"
    description: "M1 interception spike — logs score-view keys and eats 'j' to prove interception."
    title: "Vim Echo"
    pluginType: "dialog"   // deprecated no-op; residency comes from the open viewer panel

    NotationInput { id: input }

    onRun: {
        input.setKeyHandler(function(key, mods, text, wouldConsume) {
            console.log("vimecho:", key, mods, JSON.stringify(text), "phase=" + wouldConsume)
            // Consume the 'j' key to prove interception; let everything else through.
            return text === "j"
        })
    }

    // A minimal always-present panel keeps the Form/viewer open ⇒ the plugin
    // (and thus the registered key handler) stays resident while you edit.
    Rectangle {
        width: 160; height: 40; color: "#222222"
        Text {
            anchors.centerIn: parent
            color: "white"
            text: "VIM ECHO (j = eaten)"
        }
    }
}
