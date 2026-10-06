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

#include "qmlpluginapi.h"

#include <QQmlEngine>
#include <QJSValueIterator>

#include "engraving/compat/scoreaccess.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/interval.h"
#include "engraving/dom/masterscore.h"
#include "engraving/types/types.h"

#include "draw/types/geometry.h"
#include "draw/types/transform.h"

#include "notation/inotation.h"
#include "notation/inotationelements.h" // IWYU pragma: keep
#include "notation/inotationinteraction.h"
#include "notation/inotationnoteinput.h"
#include "notation/inotationviewstate.h"

#include "engraving/dom/mscore.h" // SelectType
#include "engraving/dom/score.h"       // T2: Score::dummy() for element construction
#include "engraving/dom/accidental.h"  // T2: Accidental / AccidentalType (dropSingle)
#include "engraving/dom/clef.h"         // T2: Clef / ClefType / ClefTypeList (dropSingle)
#include "engraving/dom/barline.h"      // T2: BarLine (dropSingle barline)
#include "engraving/dom/timesig.h"      // T2: TimeSig / TimeSigType (putTimeSig)
#include "engraving/types/fraction.h"   // T2: Fraction (putTimeSig)

// api
#include "engravingapiv1.h"
#include "score.h"
#include "instrument.h"
#include "cursor.h"
#include "elements.h"
#include "selection.h"
#include "util.h"

#include "log.h"

using namespace mu::engraving;
using namespace mu::engraving::apiv1;

Enum* PluginAPI::elementTypeEnum = nullptr;
Enum* PluginAPI::accidentalTypeEnum = nullptr;
Enum* PluginAPI::accidentalBracketEnum = nullptr;
Enum* PluginAPI::ornamentStyleEnum = nullptr;
Enum* PluginAPI::alignEnum = nullptr;
Enum* PluginAPI::placementEnum = nullptr;
Enum* PluginAPI::placementHEnum = nullptr;
Enum* PluginAPI::textPlaceEnum = nullptr;
Enum* PluginAPI::directionEnum = nullptr;
Enum* PluginAPI::directionHEnum = nullptr;
Enum* PluginAPI::orientationEnum = nullptr;
Enum* PluginAPI::autoOnOffEnum = nullptr;
Enum* PluginAPI::autoCustomHideEnum = nullptr;
Enum* PluginAPI::voiceAssignmentEnum = nullptr;
Enum* PluginAPI::spacerTypeEnum = nullptr;
Enum* PluginAPI::layoutBreakTypeEnum = nullptr;
Enum* PluginAPI::durationTypeEnum = nullptr;
Enum* PluginAPI::noteValueTypeEnum = nullptr;
Enum* PluginAPI::beamModeEnum = nullptr;
Enum* PluginAPI::glissandoTypeEnum = nullptr;
Enum* PluginAPI::glissandoStyleEnum = nullptr;
Enum* PluginAPI::harmonyTypeEnum = nullptr;
Enum* PluginAPI::harmonyVoicingEnum = nullptr;
Enum* PluginAPI::hDurationEnum = nullptr;
Enum* PluginAPI::frameTypeEnum = nullptr;
Enum* PluginAPI::verticalAlignmentEnum = nullptr;
Enum* PluginAPI::tremoloBarTypeEnum = nullptr;
Enum* PluginAPI::preferSharpFlatEnum = nullptr;
Enum* PluginAPI::noteHeadTypeEnum = nullptr;
Enum* PluginAPI::noteHeadSchemeEnum = nullptr;
Enum* PluginAPI::noteHeadGroupEnum = nullptr;
Enum* PluginAPI::noteTypeEnum = nullptr;
Enum* PluginAPI::playEventTypeEnum = nullptr;
Enum* PluginAPI::segmentTypeEnum = nullptr;
Enum* PluginAPI::barLineTypeEnum = nullptr;
Enum* PluginAPI::tidEnum = nullptr;
Enum* PluginAPI::lyricsSyllabicEnum = nullptr;
Enum* PluginAPI::spannerAnchorEnum = nullptr;
Enum* PluginAPI::mMRestRangeBracketTypeEnum = nullptr;
Enum* PluginAPI::tupletNumberTypeEnum = nullptr;
Enum* PluginAPI::tupletBracketTypeEnum = nullptr;
Enum* PluginAPI::tripletFeelTypeEnum = nullptr;
Enum* PluginAPI::guitarBendTypeEnum = nullptr;
Enum* PluginAPI::guitarBendShowHoldLineEnum = nullptr;
Enum* PluginAPI::clefTypeEnum = nullptr;
Enum* PluginAPI::clefToBarlinePositionEnum = nullptr;
Enum* PluginAPI::dynamicTypeEnum = nullptr;
Enum* PluginAPI::dynamicSpeedEnum = nullptr;
Enum* PluginAPI::lineTypeEnum = nullptr;
Enum* PluginAPI::hookTypeEnum = nullptr;
Enum* PluginAPI::keyModeEnum = nullptr;
Enum* PluginAPI::arpeggioTypeEnum = nullptr;
Enum* PluginAPI::intervalStepEnum = nullptr;
Enum* PluginAPI::intervalTypeEnum = nullptr;
Enum* PluginAPI::instrumentLabelVisibilityEnum = nullptr;
Enum* PluginAPI::ornamentShowAccidentalEnum = nullptr;
Enum* PluginAPI::partialSpannerDirectionEnum = nullptr;
Enum* PluginAPI::chordStylePresetEnum = nullptr;
Enum* PluginAPI::playingTechniqueTypeEnum = nullptr;
Enum* PluginAPI::gradualTempoChangeTypeEnum = nullptr;
Enum* PluginAPI::changeMethodEnum = nullptr;
Enum* PluginAPI::changeDirectionEnum = nullptr;
Enum* PluginAPI::accidentalRoleEnum = nullptr;
Enum* PluginAPI::accidentalValEnum = nullptr;
Enum* PluginAPI::fermataTypeEnum = nullptr;
Enum* PluginAPI::chordLineTypeEnum = nullptr;
Enum* PluginAPI::slurStyleTypeEnum = nullptr;
Enum* PluginAPI::tremoloTypeEnum = nullptr;
Enum* PluginAPI::tremoloChordTypeEnum = nullptr;
Enum* PluginAPI::bracketTypeEnum = nullptr;
Enum* PluginAPI::jumpTypeEnum = nullptr;
Enum* PluginAPI::markerTypeEnum = nullptr;
Enum* PluginAPI::measureNumberModeEnum = nullptr;
Enum* PluginAPI::staffGroupEnum = nullptr;
Enum* PluginAPI::ottavaTypeEnum = nullptr;
Enum* PluginAPI::hairpinTypeEnum = nullptr;
Enum* PluginAPI::trillTypeEnum = nullptr;
Enum* PluginAPI::vibratoTypeEnum = nullptr;
Enum* PluginAPI::articulationTextTypeEnum = nullptr;
Enum* PluginAPI::lyricsDashSystemStartEnum = nullptr;
Enum* PluginAPI::noteLineEndPlacementEnum = nullptr;
Enum* PluginAPI::spannerSegmentTypeEnum = nullptr;
Enum* PluginAPI::tiePlacementEnum = nullptr;
Enum* PluginAPI::tieDotsPlacementEnum = nullptr;
Enum* PluginAPI::timeSigTypeEnum = nullptr;
Enum* PluginAPI::timeSigPlacementEnum = nullptr;
Enum* PluginAPI::timeSigStyleEnum = nullptr;
Enum* PluginAPI::timeSigVSMarginEnum = nullptr;
Enum* PluginAPI::noteSpellingTypeEnum = nullptr;
Enum* PluginAPI::keyEnum = nullptr;
Enum* PluginAPI::updateModeEnum = nullptr;
Enum* PluginAPI::layoutFlagEnum = nullptr;
Enum* PluginAPI::layoutModeEnum = nullptr;
Enum* PluginAPI::tappingHandEnum = nullptr;
Enum* PluginAPI::lHTappingSymbolEnum = nullptr;
Enum* PluginAPI::rHTappingSymbolEnum = nullptr;
Enum* PluginAPI::lHTappingShowItemsEnum = nullptr;
Enum* PluginAPI::parenthesesModeEnum = nullptr;
Enum* PluginAPI::repeatPlayCountPresetEnum = nullptr;
Enum* PluginAPI::measureNumberPlacementEnum = nullptr;
Enum* PluginAPI::symIdEnum = nullptr;
Enum* PluginAPI::cursorEnum = nullptr;

//---------------------------------------------------------
//   PluginAPI::registerQmlTypes
//---------------------------------------------------------

void PluginAPI::registerQmlTypes()
{
    static bool qmlTypesRegistered = false;
    if (qmlTypesRegistered) {
        return;
    }

    if (-1 == qmlRegisterType<PluginAPI>("MuseScore", 3, 0, "MuseScore")) {
        LOGW("qmlRegisterType failed: MuseScore");
    }

    qmlRegisterUncreatableType<Enum>("MuseScore", 3, 0, "MuseScoreEnum", "Cannot create an enumeration");

    //qmlRegisterType<ScoreView>("MuseScore", 3, 0, "ScoreView");

    qmlRegisterType<Cursor>("MuseScore", 3, 0, "Cursor");
    qmlRegisterAnonymousType<ScoreElement>("MuseScore", 3);
    qmlRegisterAnonymousType<Score>("MuseScore", 3);
    qmlRegisterAnonymousType<EngravingItem>("MuseScore", 3);
    qmlRegisterAnonymousType<Chord>("MuseScore", 3);
    qmlRegisterAnonymousType<Note>("MuseScore", 3);
    qmlRegisterAnonymousType<Tuplet>("MuseScore", 3);
    qmlRegisterAnonymousType<DurationElement>("MuseScore", 3);
    qmlRegisterAnonymousType<Segment>("MuseScore", 3);
    qmlRegisterAnonymousType<Measure>("MuseScore", 3);
    qmlRegisterAnonymousType<Part>("MuseScore", 3);
    qmlRegisterAnonymousType<Staff>("MuseScore", 3);
    qmlRegisterAnonymousType<Instrument>("MuseScore", 3);
    qmlRegisterAnonymousType<Channel>("MuseScore", 3);
    qmlRegisterAnonymousType<StringData>("MuseScore", 3);
    qmlRegisterAnonymousType<Excerpt>("MuseScore", 3);
    qmlRegisterAnonymousType<Selection>("MuseScore", 3);
    qmlRegisterAnonymousType<Tie>("MuseScore", 3);
    qmlRegisterAnonymousType<Harmony>("MuseScore", 3);
    qmlRegisterAnonymousType<FretDiagram>("MuseScore", 3);
    qmlRegisterAnonymousType<Drumset>("MuseScore", 3);
    qmlRegisterAnonymousType<MeasureBase>("MuseScore", 3);
    qmlRegisterAnonymousType<System>("MuseScore", 3);
    qmlRegisterAnonymousType<Spanner>("MuseScore", 3);
    qmlRegisterAnonymousType<SpannerSegment>("MuseScore", 3);
    qmlRegisterAnonymousType<Ornament>("MuseScore", 3);
    qmlRegisterType<PlayEvent>("MuseScore", 3, 0, "PlayEvent");

    qmlRegisterAnonymousType<Fraction>("MuseScore", 3);
    qRegisterMetaType<Fraction*>("Fraction*");
    qmlRegisterAnonymousType<IntervalWrapper>("MuseScore", 3);
    qRegisterMetaType<IntervalWrapper*>("IntervalWrapper*");
    qmlRegisterAnonymousType<OrnamentIntervalWrapper>("MuseScore", 3);
    qRegisterMetaType<OrnamentIntervalWrapper*>("OrnamentIntervalWrapper*");

    qmlRegisterType<MsProcess>("MuseScore", 3, 0, "QProcess");
    qmlRegisterType<FileIO, 1>("FileIO",    3, 0, "FileIO");

    qmlTypesRegistered = true;
}

void PluginAPI::setup(QQmlEngine* e)
{
    // Sync PluginAPI and EngravingApiV1

    QJSValue apiVal = e->globalObject().property("api");
    if (apiVal.isNull()) {
        LOGE() << "not found api object";
        return;
    }

    QJSValue engravingApiVal = apiVal.property("engraving");
    QObject* engravingApiObj = engravingApiVal.toQObject();
    if (!engravingApiObj) {
        LOGE() << "not found api.engraving object";
        return;
    }

    EngravingApiV1* engravingApi = dynamic_cast<EngravingApiV1*>(engravingApiObj);
    if (!engravingApi) {
        LOGE() << "api.engraving object not EngravingApiV1";
        return;
    }

    engravingApi->setApi(this);
    m_engine = engravingApi->engine();
}

PluginAPI::PluginAPI(QQuickItem* parent)
    : QQuickItem(parent), muse::Contextable(muse::iocCtxForQmlObject(this))
{
    setRequiresScore(true); // by default plugins require a score to work
}

PluginAPI::PluginAPI(const muse::modularity::ContextPtr& ctx)
    : muse::Contextable(ctx)
{
    setRequiresScore(true); // by default plugins require a score to work
}

apiv1::Score* PluginAPI::curScore() const
{
    if (currentScore()) {
        return wrap<apiv1::Score>(currentScore(), Ownership::SCORE);
    }

    return nullptr;
}

QQmlListProperty<apiv1::Score> PluginAPI::scores()
{
    NOT_IMPLEMENTED;

    static std::vector<mu::engraving::Score*> scores;

    return wrapContainerProperty<Score>(this, scores);
}

//---------------------------------------------------------
//   writeScore
///   Writes a score to a file.
///   \param s The score which should be saved.
///   \param name Path where to save the score, with or
///   without the filename extension (the extension is
///   determined by \p ext parameter).
///   \param ext Filename extension \b without the dot,
///   e.g. \p "mscz" or \p "pdf". Determines the file
///   format to be used.
//---------------------------------------------------------

bool PluginAPI::writeScore(Score* s, const QString& name, const QString& ext)
{
    if (!s || !s->score()) {
        LOGW() << "No score provided";
        return false;
    }

    if (s->score() != currentScore()) {
        LOGW() << "Only writing the selected score is currently supported";
        return false;
    }

    return helper()->writeScore(name, ext);
}

//---------------------------------------------------------
//   readScore
///   Reads the score from a file and opens it in a new tab
///   \param name Path to the file to be opened.
///   \param noninteractive Can be used to avoid a "save
///   changes" dialog on closing a score that is either
///   imported or was created with an older version of
///   MuseScore.
//---------------------------------------------------------

apiv1::Score* PluginAPI::readScore(const QString& name, bool noninteractive)
{
    const bool hadScoreOpened = currentScore();

    if (hadScoreOpened) {
        LOGW() << "Will open a score in a new window";
    }

    if (noninteractive) {
        LOGW() << "Noninteractive flag is not yet implemented";
        return nullptr;
    }

    mu::engraving::Score* score = helper()->readScore(name);
    if (score) {
        return wrap<apiv1::Score>(score, Ownership::SCORE);
    }
    return nullptr;
}

//---------------------------------------------------------
//   closeScore
//---------------------------------------------------------

void PluginAPI::closeScore()
{
    return closeScore(curScore());
}

void PluginAPI::closeScore(apiv1::Score* score)
{
    if (!score || !score->score()) {
        LOGW() << "No score provided";
        return;
    }

    if (score->score() != currentScore()) {
        LOGW() << "Only closing the selected score is currently supported";
        return;
    }

    helper()->closeScore();
}

//---------------------------------------------------------
//   newElement
///   Creates a new element with the given type. The
///   element can be then added to a score via Cursor::add.
///   \param elementType EngravingItem type, should be the value
///   from PluginAPI::PluginAPI::EngravingItem enumeration.
//---------------------------------------------------------

apiv1::EngravingItem* PluginAPI::newElement(int elementType)
{
    mu::engraving::Score* score = currentScore();

    if (!score) {
        return nullptr;
    }

    if (elementType <= int(ElementType::INVALID) || elementType >= int(ElementType::ROOT_ITEM)) {
        LOGW("PluginAPI::newElement: Wrong type ID: %d", elementType);
        return nullptr;
    }

    const ElementType type = ElementType(elementType);
    mu::engraving::EngravingItem* e = Factory::createItem(type, score->dummy());
    return wrap(e, Ownership::PLUGIN);
}

//---------------------------------------------------------
//   removeElement
///   Disposes of an EngravingItem and its children.
///   \param EngravingItem type.
///   \since MuseScore 3.3
//---------------------------------------------------------

void PluginAPI::removeElement(apiv1::EngravingItem* wrapped)
{
    mu::engraving::Score* score = wrapped->element()->score();
    score->deleteItem(wrapped->element());
}

//---------------------------------------------------------
//   newScore
//---------------------------------------------------------

apiv1::Score* PluginAPI::newScore(const QString& /*name*/, const QString& part, int measures)
{
    if (currentScore()) {
        currentScore()->endCmd();
    }

    MasterScore* score = mu::engraving::compat::ScoreAccess::createMasterScoreWithDefaultStyle(iocContext());

    // TODO: Set path/filename
    NOT_IMPLEMENTED << "setting path/filename";

    score->appendPart(Score::instrTemplateFromName(part));
    score->appendMeasures(measures);
    score->doLayout();

    // TODO: Open score
    NOT_IMPLEMENTED << "opening the newly created score";

    qApp->processEvents();
    Q_ASSERT(currentScore() == score);
    score->startCmd(TranslatableString("undoableAction", "New score"));
    return wrap<Score>(score, Ownership::SCORE);
}

void PluginAPI::cmd(const QString& s)
{
    static const QMap<QString, QString> COMPAT_CMD_MAP = {
        { "escape", "command://notation/cancel" },
        { "cut", "command://notation/cut" },
        { "copy", "command://notation/copy" },
        { "paste", "command://notation/paste" },
        { "paste-half", "notation-paste-half" },
        { "paste-double", "notation-paste-double" },
        { "select-all", "notation-select-all" },
        { "delete", "command://notation/delete" },
        { "next-chord", "notation-move-right" },
        { "prev-chord", "notation-move-left" },
        { "prev-measure", "notation-move-left-quickly" }
    };

    actionsDispatcher()->dispatch(COMPAT_CMD_MAP.value(s, s).toStdString());
}

void PluginAPI::openLog(const QString&)
{
    DEPRECATED;
}

void PluginAPI::closeLog()
{
    DEPRECATED;
}

void PluginAPI::log(const QString& txt)
{
    LOGD() << txt;
}

void PluginAPI::logn(const QString& txt)
{
    LOGD() << txt;
}

void PluginAPI::log2(const QString& txt, const QString& txt2)
{
    LOGD() << txt << txt2;
}

//---------------------------------------------------------
//   newQProcess
///   Not enabled currently (so excluded from plugin docs)
//---------------------------------------------------------

MsProcess* PluginAPI::newQProcess()
{
    NOT_IMPLEMENTED;
    return nullptr;
}

//---------------------------------------------------------
//   PluginAPI::fraction
///  Creates a new fraction with the given numerator and
///  denominator
//---------------------------------------------------------

apiv1::Fraction* PluginAPI::fraction(int num, int den) const
{
    return wrap(mu::engraving::Fraction(num, den));
}

//---------------------------------------------------------
//   PluginAPI::fractionFromTicks
///  Converts an integer tick value to an equivalent fraction.
/// \since MuseScore 4.6
//---------------------------------------------------------

apiv1::Fraction* PluginAPI::fractionFromTicks(int ticks) const
{
    return wrap(mu::engraving::Fraction::fromTicks(ticks));
}

void PluginAPI::quit()
{
    emit closeRequested();
    m_closeRequested.notify();
}

mu::engraving::Score* PluginAPI::currentScore() const
{
    if (notation::INotationPtr notation = context()->currentNotation()) {
        return notation->elements()->msScore();
    }

    return nullptr;
}

QVariantMap PluginAPI::viewMatrix() const
{
    notation::INotationPtr notation = context()->currentNotation();
    if (!notation) {
        return {};
    }

    const muse::draw::Transform& m = notation->viewState()->matrix();
    QVariantMap result;
    result["m11"] = m.m11();
    result["m12"] = m.m12();
    result["m21"] = m.m21();
    result["m22"] = m.m22();
    result["dx"] = m.dx();
    result["dy"] = m.dy();
    return result;
}

int PluginAPI::viewZoomPercentage() const
{
    notation::INotationPtr notation = context()->currentNotation();
    if (!notation) {
        return 0;
    }

    return notation->viewState()->zoomPercentage().val;
}

QPointF PluginAPI::mapToScore(const QPointF& viewPoint) const
{
    notation::INotationPtr notation = context()->currentNotation();
    if (!notation) {
        return {};
    }

    const muse::draw::Transform& m = notation->viewState()->matrix();
    return m.inverted().map(muse::PointF::fromQPointF(viewPoint)).toQPointF();
}

QPointF PluginAPI::mapFromScore(const QPointF& scorePoint) const
{
    notation::INotationPtr notation = context()->currentNotation();
    if (!notation) {
        return {};
    }

    const muse::draw::Transform& m = notation->viewState()->matrix();
    return m.map(muse::PointF::fromQPointF(scorePoint)).toQPointF();
}

apiv1::EngravingItem* PluginAPI::hitElementAt(qreal x, qreal y, float width)
{
    notation::INotationPtr notation = context()->currentNotation();
    if (!notation) {
        return nullptr;
    }

    if (width <= 0.0f) {
        // ~3 device px expressed in logical units (matrix.m11() is the score->px scale).
        const double scaling = notation->viewState()->matrix().m11();
        width = static_cast<float>(3.0 / (scaling != 0.0 ? scaling : 1.0));
    }

    mu::engraving::EngravingItem* item = notation->interaction()->hitElement(muse::PointF(x, y), width);
    return wrap(item, Ownership::SCORE);
}

QQmlListProperty<apiv1::EngravingItem> PluginAPI::hitElementsAt(qreal x, qreal y, float width)
{
    static std::vector<mu::engraving::EngravingItem*> hitList;
    hitList.clear();

    if (notation::INotationPtr notation = context()->currentNotation()) {
        hitList = notation->interaction()->hitElements(muse::PointF(x, y), width);
    }

    return wrapContainerProperty<apiv1::EngravingItem>(this, hitList);
}

void PluginAPI::putNote(qreal x, qreal y, bool replace, bool insert)
{
    notation::INotationPtr notation = context()->currentNotation();
    if (!notation) {
        return;
    }
    notation->interaction()->noteInput()->putNote(muse::PointF(x, y), replace, insert);
}

void PluginAPI::putRest(qreal x, qreal y, const QString& duration)
{
    using namespace mu::engraving;
    notation::INotationPtr notation = context()->currentNotation();
    if (!notation) {
        return;
    }
    auto noteInput = notation->interaction()->noteInput();
    if (!noteInput) {
        return;
    }

    DurationType d = DurationType::V_QUARTER;
    if (duration == "whole") {
        d = DurationType::V_WHOLE;
    } else if (duration == "half") {
        d = DurationType::V_HALF;
    } else if (duration == "quarter") {
        d = DurationType::V_QUARTER;
    } else if (duration == "eighth") {
        d = DurationType::V_EIGHTH;
    } else if (duration == "16th") {
        d = DurationType::V_16TH;
    } else if (duration == "32nd") {
        d = DurationType::V_32ND;
    } else if (duration == "64th") {
        d = DurationType::V_64TH;
    }

    // Place the rest via the same pointer path as putNote, with rest mode on.
    // Enter note input if it isn't already active, and restore the prior state.
    const bool wasActive = noteInput->isNoteInputMode();
    if (!wasActive) {
        noteInput->startNoteInput();
    }
    noteInput->setDuration(d);
    noteInput->setRestMode(true);
    noteInput->putNote(muse::PointF(x, y), false, false);
    noteInput->setRestMode(false);
    if (!wasActive) {
        noteInput->endNoteInput();
    }
}

void PluginAPI::selectElement(apiv1::EngravingItem* element, bool add)
{
    notation::INotationPtr notation = context()->currentNotation();
    if (!notation || !element) {
        return;
    }
    mu::engraving::EngravingItem* item = element->element();
    if (!item) {
        return;
    }
    const mu::engraving::SelectType type = add ? mu::engraving::SelectType::ADD : mu::engraving::SelectType::REPLACE;
    notation->interaction()->select({ item }, type);
}

void PluginAPI::deleteSelection()
{
    notation::INotationPtr notation = context()->currentNotation();
    if (!notation) {
        return;
    }
    notation->interaction()->deleteSelection();
}

bool PluginAPI::dropSingle(const QString& element, qreal x, qreal y)
{
    // NOTE: this method lives in namespace mu::engraving::apiv1, where bare
    // `Score`/`EngravingItem` name the apiv1 WRAPPER types — so the engraving
    // DOM types must be fully qualified as mu::engraving::...
    notation::INotationPtr notation = context()->currentNotation();
    if (!notation) {
        return false;
    }
    mu::engraving::Score* score = notation->elements() ? notation->elements()->msScore() : nullptr;
    if (!score) {
        return false;
    }
    auto interaction = notation->interaction();

    // Target the element under the point so the applied element lands there
    // (accidental -> selected note; clef -> selected measure/segment).
    const double scaling = notation->viewState()->matrix().m11();
    const float w = static_cast<float>(3.0 / (scaling != 0.0 ? scaling : 1.0));
    if (mu::engraving::EngravingItem* hit = interaction->hitElement(muse::PointF(x, y), w)) {
        interaction->select({ hit }, mu::engraving::SelectType::REPLACE);
    }

    // A dot is not a droppable palette element — it toggles an augmentation dot
    // on the note/input at the point.
    if (element == "dot") {
        if (const auto& noteInput = interaction->noteInput()) {
            noteInput->toggleDots(1);
            return true;
        }
        return false;
    }

    // Build the element palette-style; keep it alive across applyPaletteElement
    // (which clones it, exactly like a palette double-click).
    std::shared_ptr<mu::engraving::EngravingItem> built;
    if (element == "sharp" || element == "flat" || element == "natural" || element == "double_sharp") {
        mu::engraving::AccidentalType at = mu::engraving::AccidentalType::NATURAL;
        if (element == "sharp") {
            at = mu::engraving::AccidentalType::SHARP;
        } else if (element == "flat") {
            at = mu::engraving::AccidentalType::FLAT;
        } else if (element == "double_sharp") {
            at = mu::engraving::AccidentalType::SHARP2;
        }
        auto ac = mu::engraving::Factory::makeAccidental(score->dummy());
        ac->setAccidentalType(at);
        built = ac;
    } else if (element == "g_clef" || element == "f_clef" || element == "c_clef") {
        mu::engraving::ClefType ct = mu::engraving::ClefType::G;
        if (element == "f_clef") {
            ct = mu::engraving::ClefType::F;
        } else if (element == "c_clef") {
            ct = mu::engraving::ClefType::C3;
        }
        auto clef = mu::engraving::Factory::makeClef(score->dummy()->segment());
        clef->setClefType(mu::engraving::ClefTypeList(ct, ct));
        built = clef;
    } else if (element == "barline") {
        built = mu::engraving::Factory::makeBarLine(score->dummy()->segment()); // default NORMAL
    } else {
        return false;
    }

    if (!built) {
        return false;
    }
    return interaction->applyPaletteElement(built.get(), {});
}

bool PluginAPI::putTimeSig(int num, int den, qreal x, qreal y)
{
    if (num <= 0 || den <= 0) {
        return false;
    }
    notation::INotationPtr notation = context()->currentNotation();
    if (!notation) {
        return false;
    }
    mu::engraving::Score* score = notation->elements() ? notation->elements()->msScore() : nullptr;
    if (!score) {
        return false;
    }
    auto interaction = notation->interaction();

    // Target the measure under the point, then apply the time signature there.
    const double scaling = notation->viewState()->matrix().m11();
    const float w = static_cast<float>(3.0 / (scaling != 0.0 ? scaling : 1.0));
    if (mu::engraving::EngravingItem* hit = interaction->hitElement(muse::PointF(x, y), w)) {
        interaction->select({ hit }, mu::engraving::SelectType::REPLACE);
    }

    auto ts = mu::engraving::Factory::makeTimeSig(score->dummy()->segment());
    ts->setSig(mu::engraving::Fraction(num, den), mu::engraving::TimeSigType::NORMAL);
    return interaction->applyPaletteElement(ts.get(), {});
}

QString PluginAPI::pluginType() const
{
    return m_pluginType;
}

void PluginAPI::setPluginType(const QString& newPluginType)
{
    m_pluginType = newPluginType;
}

QString PluginAPI::menuPath() const
{
    return QString();
}

void PluginAPI::setMenuPath(const QString&)
{
    DEPRECATED;
}

QString PluginAPI::title() const
{
    return m_title;
}

void PluginAPI::setTitle(const QString& newTitle)
{
    m_title = newTitle;
}

QString PluginAPI::version() const
{
    return m_version;
}

void PluginAPI::setVersion(const QString& newVersion)
{
    m_version = newVersion;
}

QString PluginAPI::description() const
{
    return m_description;
}

void PluginAPI::setDescription(const QString& newDescription)
{
    m_description = newDescription;
}

QString PluginAPI::dockArea() const
{
    return QString();
}

void PluginAPI::setDockArea(const QString&)
{
    DEPRECATED;
}

bool PluginAPI::requiresScore() const
{
    return m_requiresScore;
}

void PluginAPI::setRequiresScore(bool newRequiresScore)
{
    m_requiresScore = newRequiresScore;
}

QString PluginAPI::thumbnailName() const
{
    return m_thumbnailName;
}

void PluginAPI::setThumbnailName(const QString& newThumbnailName)
{
    m_thumbnailName = newThumbnailName;
}

QString PluginAPI::categoryCode() const
{
    return m_categoryCode;
}

void PluginAPI::setCategoryCode(const QString& newCategoryCode)
{
    m_categoryCode = newCategoryCode;
}

int PluginAPI::division() const
{
    return engraving::Constants::DIVISION;
}

int PluginAPI::mscoreVersion() const
{
    return mscoreMajorVersion() * 10000 + mscoreMinorVersion() * 100 + mscoreUpdateVersion();
}

int PluginAPI::mscoreMajorVersion() const
{
    return application()->version().major();
}

int PluginAPI::mscoreMinorVersion() const
{
    return application()->version().minor();
}

int PluginAPI::mscoreUpdateVersion() const
{
    return application()->version().patch();
}

qreal PluginAPI::mscoreDPI() const
{
    return engraving::DPI;
}

OrnamentIntervalWrapper* PluginAPI::defaultOrnamentInterval() const
{
    return wrap(mu::engraving::DEFAULT_ORNAMENT_INTERVAL);
}

//---------------------------------------------------------
//   PluginAPI::ornamentInterval
///  Creates a new ornament interval with the given step and type
//---------------------------------------------------------

OrnamentIntervalWrapper* PluginAPI::ornamentInterval(int step, int type) const
{
    return wrap(mu::engraving::OrnamentInterval(mu::engraving::IntervalStep(step), mu::engraving::IntervalType(type)));
}

//---------------------------------------------------------
//   PluginAPI::interval
///  Creates a new interval with the given chromatic and diatonic steps
//---------------------------------------------------------

IntervalWrapper* PluginAPI::interval(int diatonic, int chromatic) const
{
    return wrap(mu::engraving::Interval(diatonic, chromatic));
}

//---------------------------------------------------------
//   PluginAPI::intervalFromOrnamentInterval
///  Creates a new interval from a given ornament interval
//---------------------------------------------------------

IntervalWrapper* PluginAPI::intervalFromOrnamentInterval(OrnamentIntervalWrapper* o) const
{
    return wrap(mu::engraving::Interval::fromOrnamentInterval(o->ornamentInterval()));
}
