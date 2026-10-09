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
#pragma once

#include <QTimer>
#include <qqmlintegration.h>

#include "modularity/ioc.h"

#include "notation/inotationconfiguration.h"

#include "actions/actionable.h"
#include "actions/iactionsdispatcher.h"
#include "async/asyncable.h"
#include "context/iglobalcontext.h"
#include "playback/iplaybackcontroller.h"
#include "ui/imainwindow.h"
#include "ui/iuiactionsregister.h"
#include "ui/iuiconfiguration.h"
#include "ui/iuicontextresolver.h"
#include "uicomponents/qml/Muse/UiComponents/quickpaintedview.h"

#include "notationscene/inotationsceneconfiguration.h"
#include "notationviewinputcontroller.h"
#include "noteinputcursor.h"
#include "notationruler.h"
#include "playbackcursor.h"
#include "loopmarker.h"
#include "continuouspanel.h"
#include "notation/internal/annotationlayer.h"
#include "strokerecognizer.h"
#include "abstractelementpopupmodel.h"

namespace mu::notation {
class AbstractNotationPaintView : public muse::uicomponents::QuickPaintedView, public IControlledView, public muse::Contextable,
    public muse::async::Asyncable, public muse::actions::Actionable
{
    Q_OBJECT
    QML_ELEMENT;
    QML_UNCREATABLE("Not creatable as it is an abstract base class")

    Q_PROPERTY(qreal startHorizontalScrollPosition READ startHorizontalScrollPosition NOTIFY horizontalScrollChanged)
    Q_PROPERTY(qreal horizontalScrollbarSize READ horizontalScrollbarSize NOTIFY horizontalScrollChanged)
    Q_PROPERTY(qreal startVerticalScrollPosition READ startVerticalScrollPosition NOTIFY verticalScrollChanged)
    Q_PROPERTY(qreal verticalScrollbarSize READ verticalScrollbarSize NOTIFY verticalScrollChanged)

    Q_PROPERTY(QVariant matrix READ matrix NOTIFY matrixChanged)
    Q_PROPERTY(QRectF viewport READ viewport_property NOTIFY viewportChanged)

    Q_PROPERTY(bool publishMode READ publishMode WRITE setPublishMode NOTIFY publishModeChanged)

    Q_PROPERTY(bool isMainView READ isMainView WRITE setIsMainView NOTIFY isMainViewChanged)

    Q_PROPERTY(bool readOnly READ readonly WRITE setReadonly NOTIFY readonlyChanged)

    Q_PROPERTY(bool annotationActive READ annotationActive WRITE setAnnotationActive NOTIFY annotationStateChanged)
    Q_PROPERTY(int annotationTool READ annotationTool WRITE setAnnotationTool NOTIFY annotationStateChanged)
    Q_PROPERTY(QColor annotationColor READ annotationColor WRITE setAnnotationColor NOTIFY annotationStateChanged)
    Q_PROPERTY(double annotationWidth READ annotationWidth WRITE setAnnotationWidth NOTIFY annotationStateChanged)
    Q_PROPERTY(bool annotationCanUndo READ annotationCanUndo NOTIFY annotationStateChanged)
    Q_PROPERTY(bool annotationCanRedo READ annotationCanRedo NOTIFY annotationStateChanged)
    Q_PROPERTY(bool writeModeActive READ writeModeActive WRITE setWriteModeActive NOTIFY annotationStateChanged)
    Q_PROPERTY(bool addToSelectionActive READ addToSelectionActive WRITE setAddToSelectionActive NOTIFY annotationStateChanged)

    muse::GlobalInject<INotationConfiguration> notationConfiguration;
    muse::GlobalInject<INotationSceneConfiguration> configuration;
    muse::GlobalInject<engraving::IEngravingConfiguration> engravingConfiguration;
    muse::GlobalInject<muse::ui::IUiConfiguration> uiConfiguration;
    muse::ContextInject<muse::actions::IActionsDispatcher> dispatcher = { this };
    muse::ContextInject<context::IGlobalContext> globalContext = { this };
    muse::ContextInject<playback::IPlaybackController> playbackController = { this };
    muse::ContextInject<muse::ui::IUiContextResolver> uiContextResolver = { this };
    muse::ContextInject<muse::ui::IMainWindow> mainWindow = { this };
    muse::ContextInject<muse::ui::IUiActionsRegister> actionsRegister = { this };
    int m_tbX = 0, m_tbY = 0, m_tbW = 0, m_tbH = 0, m_tbCols = 0, m_tbWinW = 0, m_tbWinH = 0;
    bool m_tbMenuOpen = false;
    bool m_tbLabels = true;
    QString m_tbButtons;   // "focus=x,y;save=x,y;..." in the same units as the box

public:
    explicit AbstractNotationPaintView(QQuickItem* parent = nullptr);
    ~AbstractNotationPaintView() override;

    Q_INVOKABLE void load();

    // Annotation (ink) tool state, exposed to the QML annotation toolbar.
    enum AnnotationTool { AnnotationPen, AnnotationHighlighter, AnnotationEraser };
    Q_ENUM(AnnotationTool)

    enum class WriteGesture { None, Pending, Drawing, Dragging };

    bool annotationActive() const;
    void setAnnotationActive(bool active);
    bool writeModeActive() const;
    void setWriteModeActive(bool active);
    bool addToSelectionActive() const;
    void setAddToSelectionActive(bool active);
    int annotationTool() const;
    void setAnnotationTool(int tool);
    QColor annotationColor() const;
    void setAnnotationColor(const QColor& color);
    double annotationWidth() const;
    void setAnnotationWidth(double width);
    bool annotationCanUndo() const;
    bool annotationCanRedo() const;

    Q_INVOKABLE void panViewByPixels(qreal dxPx, qreal dyPx);
    Q_INVOKABLE void cancelCurrentStroke();
    Q_INVOKABLE void setPointerMode();
    Q_INVOKABLE bool pointerModeActive() const;
    Q_INVOKABLE void toggleAnnotation();
    Q_INVOKABLE void toggleWriteMode();   // pen gestures -> recognized notation
    Q_INVOKABLE void toggleAddToSelection();   // sticky "Shift" for additive lasso
    Q_INVOKABLE void annotationUndo();
    Q_INVOKABLE void annotationRedo();
    Q_INVOKABLE void annotationClear();

    // Dispatch a MuseScore action from the pen toolbar (play, zoomin, file-save, ...).
    Q_INVOKABLE void dispatchAction(const QString& code);
    // Is a toggle action currently ON? The panel actions
    // (toggle-palettes, toggle-noteinput, ...) are TOGGLES, so a toolbar
    // that wants to HIDE a panel has to know whether it is showing --
    // dispatching blind turns hidden panels back on, which is the opposite
    // of what a "hide the chrome" button is for.
    // Let the toolbar publish where it is. Tests cannot find it any other
    // way: it is a QML Item inside this one window, so xdotool reports the
    // editor's geometry, and locating it by colour does not work either --
    // the strip's panel is 245,245,246 and the score page is 249,249,249.
    // Four greys apart is not a discriminator, and threshold-tuning against
    // it produced a "locator" that matched half the page.
    Q_INVOKABLE void reportToolbarGeometry(int x, int y, int w, int hgt, int cols, int winW, int winH,
                                           bool menuOpen = false, bool labels = true,
                                           const QString& buttons = QString());
    Q_INVOKABLE bool isActionChecked(const QString& code) const;
    // Drive a toggle to a specific state, dispatching only when it differs.
    Q_INVOKABLE void setActionChecked(const QString& code, bool checked);
    Q_INVOKABLE void toggleViewMode();   // page <-> continuous

    Q_INVOKABLE void scrollHorizontal(qreal position);
    Q_INVOKABLE void scrollVertical(qreal position);

    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();

    Q_INVOKABLE void selectOnNavigationActive();

    Q_INVOKABLE void forceFocusIn();

    Q_INVOKABLE void onContextMenuIsOpenChanged(bool open);
    Q_INVOKABLE void onElementPopupIsOpenChanged(const PopupModelType& popupType = PopupModelType::TYPE_UNDEFINED);

    Q_INVOKABLE void setPlaybackCursorItem(QQuickItem* cursor);

    qreal width() const override;
    qreal height() const override;

    muse::PointF toLogical(const muse::PointF& point) const override;
    muse::PointF toLogical(const QPointF& point) const override;
    muse::RectF toLogical(const muse::RectF& rect) const;

    muse::PointF fromLogical(const muse::PointF& point) const override;
    muse::RectF fromLogical(const muse::RectF& rect) const override;

    Q_INVOKABLE bool moveCanvas(qreal dx, qreal dy) override;
    void moveCanvasVertical(qreal dy) override;
    void moveCanvasHorizontal(qreal dx) override;

    qreal currentScaling() const override;
    void setScaling(qreal scaling, const muse::PointF& pos, bool overrideZoomType = true) override;
    void scale(qreal factor, const muse::PointF& pos, bool overrideZoomType = true);

    Q_INVOKABLE void pinchToZoom(qreal scaleFactor, const QPointF& pos);

    bool isNoteEnterMode() const override;

    void showContextMenu(const ElementType& elementType, const QPointF& pos) override;
    void hideContextMenu() override;

    void showElementPopup(const ElementType& elementType) override;
    void hideElementPopup(const ElementType& elementType) override;
    void hideElementPopup(PopupModelType modelType = PopupModelType::TYPE_UNDEFINED) override;
    void toggleElementPopup(const ElementType& elementType) override;

    bool elementPopupIsOpen(const ElementType& elementType) const override;

    INotationInteractionPtr notationInteraction() const override;
    INotationPlaybackPtr notationPlayback() const override;

    QQuickItem* asItem() override;

    qreal startHorizontalScrollPosition() const;
    qreal horizontalScrollbarSize() const;
    qreal startVerticalScrollPosition() const;
    qreal verticalScrollbarSize() const;

    QVariant matrix() const;

    muse::PointF viewportTopLeft() const override;
    muse::RectF viewport() const;
    QRectF viewport_property() const;

    bool publishMode() const;
    void setPublishMode(bool arg);

    bool isMainView() const;
    void setIsMainView(bool isMainView);

    bool readonly() const;
    void setReadonly(bool readonly);

signals:
    void showContextMenuRequested(int elementType, const QPointF& viewPos);
    void hideContextMenuRequested();

    void showElementPopupRequested(mu::notation::AbstractElementPopupModel::PopupModelType modelType);
    void hideElementPopupRequested();
    void isPopupOpenChanged(bool isPopupOpen);

    void horizontalScrollChanged();
    void verticalScrollChanged();

    void backgroundColorChanged(QColor color);
    void matrixChanged();
    void viewportChanged();
    void publishModeChanged();

    void activeFocusRequested();

    void isMainViewChanged(bool isMainView);

    void readonlyChanged();

    void annotationStateChanged();

protected:
    INotationPtr notation() const;
    void setNotation(INotationPtr notation);

    NotationViewInputController* inputController() const;

    void setMatrix(const muse::draw::Transform& matrix);

    void moveCanvasToCenter();
    bool moveCanvasToPosition(const muse::PointF& logicPos);

    muse::RectF notationContentRect() const override;

    // Draw
    void paint(QPainter* painter) override;

    virtual void onNotationSetup();

    virtual void onLoadNotation(INotationPtr notation);
    virtual void onUnloadNotation(INotationPtr notation);

    virtual void initZoomAndPosition();

    virtual void onMatrixChanged(const muse::draw::Transform& oldMatrix, const muse::draw::Transform& newMatrix, bool overrideZoomType);

protected slots:
    virtual void onViewSizeChanged();

private:
    INotationNoteInputPtr notationNoteInput() const;
    INotationElementsPtr notationElements() const;
    INotationStylePtr notationStyle() const;
    INotationSelectionPtr notationSelection() const;

    void clear();
    void initBackground();
    void initNavigatorOrientation();

    bool canReceiveAction(const muse::actions::ActionCode& actionCode) const override;
    void onCurrentNotationChanged();
    bool isInited() const;

    bool doMoveCanvas(qreal dx, qreal dy);

    void scheduleRedraw(const muse::RectF& rect = muse::RectF());
    muse::RectF correctDrawRect(const muse::RectF& rect) const;

    // Input
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    bool event(QEvent* event) override;
    bool shortcutOverride(QKeyEvent* event);
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragLeaveEvent(QDragLeaveEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

    bool ensureViewportInsideScrollableArea();

    muse::RectF scrollableAreaRect() const;

    qreal horizontalScrollableSize() const;
    qreal verticalScrollableSize() const;

    bool adjustCanvasPosition(const muse::RectF& logicRect, bool adjustVertically = true);
    bool adjustCanvasPositionSmoothPan(const muse::RectF& cursorRect);

    void onNoteInputStateChanged();

    void onShowItemRequested(const INotationInteraction::ShowItemRequest& request);

    void onPlayingChanged();
    void movePlaybackCursor(muse::midi::tick_t tick);
    bool needAdjustCanvasVerticallyWhilePlayback(const muse::RectF& cursorRect);

    void onPlaybackCursorRectChanged();

    void updateLoopMarkers();
    void updateShadowNoteVisibility();

    const Page* pageByPoint(const muse::PointF& point) const;
    muse::PointF alignToCurrentPageBorder(const muse::RectF& showRect, const muse::PointF& pos) const;

    void paintBackground(const muse::RectF& rect, muse::draw::Painter* painter);

    void writeAnnotationStatus();   // env-gated status file for automated tests
    void recognizeAccumulated();   // write mode: recognize the accumulated multi-stroke symbol
    void onHoldTimeout();          // write mode: long-press -> grab the element under the pen
    void dragMoveTo(const muse::PointF& logicalPos);   // move the grabbed element (native drag)

    muse::PointF canvasCenter() const;
    std::pair<qreal, qreal> constraintCanvas(qreal dx, qreal dy) const;

    INotationPtr m_notation;
    muse::draw::Transform m_matrix;

    bool m_loadCalled = false;
    std::unique_ptr<NotationViewInputController> m_inputController;
    std::unique_ptr<PlaybackCursor> m_playbackCursor;
    std::unique_ptr<NoteInputCursor> m_noteInputCursor;
    std::unique_ptr<NotationRuler> m_ruler;
    std::unique_ptr<LoopMarker> m_loopInMarker;
    std::unique_ptr<LoopMarker> m_loopOutMarker;
    std::unique_ptr<ContinuousPanel> m_continuousPanel;
    AnnotationLayer* m_annotationLayer = nullptr;   // owned by the current Notation, not by the view
    std::unique_ptr<StrokeRecognizer> m_strokeRecognizer;
    bool m_annotationMode = false;
    bool m_writeMode = false;   // pen gestures -> recognized notation (via neume)
    std::vector<std::vector<muse::PointF> > m_writeStrokes;   // accumulated multi-stroke symbol
    QTimer* m_writeTimer = nullptr;                            // debounce before recognizing
    QTimer* m_holdTimer = nullptr;                             // long-press -> grab timer
    WriteGesture m_writeGesture = WriteGesture::None;
    QPoint m_pressScreenPos;                                   // press point (physical px) for move threshold
    muse::PointF m_pressLogical;                               // press point (score coords) for hit/drag
    muse::PointF m_dragOffset;                                 // grabbed element's offset at grab time
    bool m_addToSelection = false;                             // sticky "Shift": additive lasso
    bool m_writeAdditive = false;                              // additive state captured at gesture end
    bool m_penBarrelDown = false;                              // pen barrel/side button (from tabletEvent)
    AnnotationTool m_annotationTool = AnnotationPen;
    QColor m_penColor = QColor(224, 48, 48);   // current pen colour
    double m_penWidth = 15.0;                   // current pen width (logical units)
    bool m_erasing = false;
    QString m_annotationStatusPath;

    qreal m_previousVerticalScrollPosition = 0;
    qreal m_previousHorizontalScrollPosition = 0;

    bool m_readonly = false;
    bool m_publishMode = false;
    int m_lastAcceptedKey = -1;
    bool m_isMainView = false;

    bool m_autoScrollEnabled = true;
    bool m_isAutomaticallyPanEnabled = false;
    bool m_isSmoothPanningEnabled = false;
    QTimer m_enableAutoScrollTimer;

    PopupModelType m_currentElementPopupType = PopupModelType::TYPE_UNDEFINED;
    bool m_isContextMenuOpen = false;

    muse::RectF m_shadowNoteRect;

    QQuickItem* m_playbackCursorItem = nullptr;
};
}
