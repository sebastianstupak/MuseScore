/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Stylus notation plugin — reads intents from the mailbox and applies them.
 *
 * Skeleton: polls the mailbox file (see ../bridge/intents.schema.json), parses
 * intents, and applies each via the notation API. Intents arrive in SCREEN space;
 * the plugin maps them to score coordinates with the fork's T1 API
 * (mapToScore / hitElementAt) and edits with T2 (putNote / putRest / select /
 * deleteSelection / dropSingle).
 *
 * NOTE: resident-panel polling is unverified in MU4 (dockArea is unused; Form
 * plugins open as dialogs). See docs/ARCHITECTURE.md "Open questions".
 */
import QtQuick
import MuseScore 3.0
import FileIO 3.0

MuseScore {
    id: root
    version: "0.1"
    title: "Stylus Notation"
    description: "Applies pen-drawn intents from the stylus-ink app to the score."
    pluginType: "dialog"
    requiresScore: true
    width: 320
    height: 120

    FileIO { id: mailbox }

    // Must match the ink app's default ($STYLUS_MAILBOX / temp/stylus-intents.json).
    function mailboxPath() {
        return mailbox.tempPath() + "/stylus-intents.json"
    }

    // Hit-test tolerance in SCORE (logical) units.
    readonly property real hitWidth: 2.0

    function poll() {
        mailbox.source = mailboxPath()
        if (!mailbox.exists())
            return
        var text = mailbox.read()
        mailbox.remove()                 // consume
        if (!text)
            return
        var obj
        try {
            obj = JSON.parse(text)        // neume output: {"intents":[...]}
        } catch (e) {
            console.log("[stylus] bad mailbox json")
            return
        }
        var list = (obj && obj.intents) ? obj.intents : []
        for (var i = 0; i < list.length; ++i)
            applyIntent(list[i])
    }

    function applyIntent(it) {
        switch (it.type) {
        case "put_note": {
            if (!it.at)
                break
            var p = mapToScore(Qt.point(it.at.x, it.at.y))     // T1 screen->score
            root.putNote(p.x, p.y, false, false)               // T2
            break
        }
        case "rest": {
            if (!it.at)
                break
            var pr = mapToScore(Qt.point(it.at.x, it.at.y))     // T1 screen->score
            root.putRest(pr.x, pr.y, it.duration)               // T2
            break
        }
        case "erase":
            if (selectAlong(it.path))                           // T1 hit + T2 select
                root.deleteSelection()                         // T2
            break
        case "lasso":
            selectAlong(it.path)                               // approximate: select hits along the path
            break
        case "drop": {
            if (!it.at)
                break
            var pd = mapToScore(Qt.point(it.at.x, it.at.y))     // T1 screen->score
            root.dropSingle(it.element, pd.x, pd.y)             // T2
            break
        }
        }
    }

    // Select every element hit along `path` (score-mapped). First hit replaces the
    // selection, the rest extend it. Returns true if anything was selected.
    function selectAlong(path) {
        if (!path || path.length === 0)
            return false
        var any = false
        for (var i = 0; i < path.length; ++i) {
            var p = mapToScore(Qt.point(path[i].x, path[i].y))
            var hits = root.hitElementsAt(p.x, p.y, root.hitWidth)   // T1
            for (var j = 0; j < hits.length; ++j) {
                root.selectElement(hits[j], any)                     // T2: add once we've selected one
                any = true
            }
        }
        return any
    }

    onRun: pollTimer.running = true

    Timer {
        id: pollTimer
        interval: 100
        repeat: true
        running: false
        onTriggered: root.poll()
    }
}
