import QtQuick
import QtQuick.Layouts
import Muse.Ui
import Muse.UiComponents as MU
import MuseScore 3.0

MuseScore {
    id: root
    version: "0.3"
    title: "Vim Echo"
    description: "M1 interception spike (instrumented)."
    pluginType: "dialog"
    requiresScore: true
    width: 480
    height: 220

    property string lastInfo: "waiting for keys…"
    property int tickCount: 0

    NotationInput { id: input }

    onRun: {
        console.log("VIMDBG onRun start; input=" + input)
        try {
            input.setKeyHandler(function(key, mods, text, wouldConsume) {
                console.log("VIMDBG handler key=" + key + " text=" + JSON.stringify(text) + " phase=" + wouldConsume)
                root.lastInfo = "key=" + key + " text=" + JSON.stringify(text) + " phase=" + wouldConsume
                              + (text === "j" ? "  [EATEN]" : "")
                return text === "j"
            })
            console.log("VIMDBG setKeyHandler returned OK")
        } catch (e) {
            console.log("VIMDBG setKeyHandler THREW: " + e)
        }
    }

    Timer {
        interval: 2000; running: true; repeat: true
        onTriggered: { root.tickCount++; console.log("VIMDBG alive tick " + root.tickCount) }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10
        MU.StyledTextLabel {
            Layout.fillWidth: true
            text: "VIM ECHO (instrumented). Alive ticks: " + root.tickCount
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
