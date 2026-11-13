// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include "ui/widgets/StudioModelView.h"

#include <QMainWindow>

#include <assimp/vector3.h>

#include <functional>
#include <memory>
#include <vector>

class QCheckBox;
class QAction;
class QComboBox;
class QGroupBox;
class QRadioButton;
class QDragEnterEvent;
class QDropEvent;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSlider;
class QStyle;
class QSplitter;
class QTimer;
class QSpinBox;
class QStackedWidget;
class QTabBar;
class QUndoGroup;
class QUndoStack;
class StudioModel;
class UvMapEditor;
struct StudioTexture;

// GoldSrc model viewer: model tabs, a 3D view, docked panels and a timeline
class ModelViewerWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ModelViewerWindow(QWidget* parent = nullptr);
    ~ModelViewerWindow() override;

    // opens a model in a new tab, or switches to its tab
    bool load(const QString& path);
    // saves the shown model to path, which becomes its file
    bool saveAs(const QString& path);
    // asks about unsaved models; false if the user cancels
    bool closeAll();
    // finds an event's sound file in the game's sound folders
    static QString findSound(const QString& modelPath, const QString& sound);
    // sound event ids played, as in Half-Life and Opposing Force
    static bool isSoundEvent(int event);
    // image to an 8-bit texture, quantized to 256 colours
    static bool textureFromImage(const QImage& source, StudioTexture& texture);
    StudioModelView* view() const { return m_view; }
    QMainWindow* editWindow() const { return m_edit; }
    // restore the default layout
    void resetLayout();
    // the panels, the UV editor's split and, as a window of its own, its place on screen
    QByteArray layoutState() const;
    bool restoreLayout(const QByteArray& state);
    // remember the layout: read on first show, written as it changes and on close
    void setRemembersLayout(bool remembers)
    {
        m_remembersLayout = remembers;
        m_layoutRestored = m_layoutRestored && remembers;
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    friend class ModelEdit;

    // per-tab state
    struct Asset
    {
        QString path;
        std::shared_ptr<StudioModel> model;
        QUndoStack* undo = nullptr;
        int sequence = 0;
        int skin = 0;
        std::vector<int> body;
        std::vector<float> controllers;
    };

    // spin boxes
    struct VectorEdit
    {
        QWidget* widget = nullptr;
        QDoubleSpinBox* axis[3] = {};
        void set(const aiVector3D& value) const;
        aiVector3D value() const;
    };
    VectorEdit vectorEdit(bool editable, double range = 16384.0);

    // adds a dock tabbed with tabWith; the untranslated title keys the saved layout
    QDockWidget* addPanel(const char* title, QWidget* panel, QDockWidget* tabWith = nullptr, bool fill = false);
    QWidget* buildView();
    QWidget* buildCameras();
    QWidget* buildScene();
    QWidget* buildDisplay();
    QWidget* buildSequences();
    QWidget* buildBody();
    QWidget* buildTextures();
    QWidget* buildModelData();
    QWidget* buildBones();
    QWidget* buildControllers();
    QWidget* buildAttachments();
    QWidget* buildHitboxes();
    QWidget* buildConsole();
    QWidget* buildTransform();
    QWidget* buildUvMap();
    void showUvMap();
    QWidget* buildReduce();
    // counts of the chosen faces; a model changed elsewhere starts a new reduction
    void showReduce();
    void fillReduceSubModels();
    void applyReduce();
    void applyTransform();
    void applyScene();
    void importTexture();
    void importAllTextures();
    // plays the sounds of events just passed
    void playEvents();
    void playSound(const QString& sound);
    QWidget* buildTimeline();
    void buildMenus();

    Asset* asset();
    void showAsset(int index);
    void closeAsset(int index);
    // edits a copy and pushes it as an undo step; the same key merges
    void editModel(const QString& what, const QString& key, const std::function<void(StudioModel&)>& change);
    // sets the tab's model after an edit or undo
    void setModel(QUndoStack* stack, const std::shared_ptr<StudioModel>& model);
    void connectEdits();
    void refreshPanels();
    void showModelData();
    void updateTabTitle(int index);
    bool confirmClose(int index);
    // offers to split the parts GoldSrc cannot draw whole; false when the save is called off
    bool splitOversized();
    void saveCurrent();
    void saveCurrentAs();
    void showSequence(int index);
    // fills the sequence fields without restarting it
    void fillSequence(int index);
    void showSequenceEvent(int index);
    void showPart(int part);
    void showSubModel(int sub);
    void showTexture();
    void showBone(int index);
    void showController(int index);
    void setControllerValue(float value);
    void showAttachment(int index);
    // refills after an add or remove
    void fillAttachments();
    void showHitbox(int index);
    void applyHighlight();
    void applyDisplay();
    void applyPlacement();
    void applyCamera();
    void applyCameraPlace();
    void stepFrames(int frames);
    void centerOn(float yaw, float pitch);
    void refresh();
    void openFiles();
    void saveScreenshot();
    void dumpInfo();
    // texture, UV map or both, scaled
    QImage textureView(int texture, double scale, bool picture, bool lines);
    void exportTexture();
    void exportAllTextures();
    void exportUvMap();

    QMainWindow* m_edit = nullptr;
    StudioModelView* m_view = nullptr;
    QTabBar* m_tabs = nullptr;
    std::vector<Asset> m_assets;
    int m_current = -1;
    StudioModelView::ViewPlace m_savedPlace;
    bool m_hasSavedPlace = false;
    QMenu* m_panelMenu = nullptr;
    QUndoGroup* m_undoGroup = nullptr;
    bool m_filling = false;  // panels being filled, not edited
    QDockWidget* m_displayDock = nullptr;
    QDockWidget* m_texturesDock = nullptr;
    QDockWidget* m_uvDock = nullptr;
    QWidget* m_consolePanel = nullptr;
    QByteArray m_defaultLayout;
    bool m_remembersLayout = true;
    bool m_layoutRestored = false;
    bool m_restoringLayout = false;
    QSplitter* m_viewSplit = nullptr;
    QTimer* m_layoutSave = nullptr;
    void saveLayout();
    // whole names on the panel tabs: tight tabs, and arrows rather than "..." when still too long
    void fitPanelTabs();
    QStyle* m_tabStyle = nullptr;
    QString m_modelInfo;

    QWidget* m_controlsBar = nullptr;
    QStackedWidget* m_sceneStack = nullptr;
    QTabBar* m_sceneTabs = nullptr;
    QLabel* m_fpsLabel = nullptr;
    QLabel* m_polygonsLabel = nullptr;
    int m_refreshTicks = 0;
    QComboBox* m_poseCombo = nullptr;
    QLabel* m_textureImage = nullptr;
    UvMapEditor* m_uvEditor = nullptr;

    QWidget* m_timeline = nullptr;
    QSlider* m_frameSlider = nullptr;
    QDoubleSpinBox* m_frameSpin = nullptr;
    QPushButton* m_playButton = nullptr;
    QSlider* m_speedSlider = nullptr;
    QDoubleSpinBox* m_speedSpin = nullptr;

    QComboBox* m_cameraCombo = nullptr;
    QStackedWidget* m_cameraStack = nullptr;
    QDoubleSpinBox* m_fovSpins[3] = {};
    QDoubleSpinBox* m_pitchSpins[2] = {};
    QDoubleSpinBox* m_yawSpins[2] = {};
    QDoubleSpinBox* m_distanceSpin = nullptr;
    QComboBox* m_projectionCombo = nullptr;
    VectorEdit m_cameraCenter;
    VectorEdit m_freeOrigin;

    VectorEdit m_origin;
    QCheckBox* m_showGround = nullptr;
    QDoubleSpinBox* m_groundLength = nullptr;
    QLineEdit* m_groundPath = nullptr;
    QCheckBox* m_groundTextured = nullptr;
    QCheckBox* m_mirrorGround = nullptr;
    QGroupBox* m_tiling = nullptr;
    QSpinBox* m_tileSize = nullptr;
    VectorEdit m_groundOrigin;
    QLineEdit* m_backgroundPath = nullptr;
    QCheckBox* m_showBackground = nullptr;
    QSpinBox* m_aspectWidth = nullptr;
    QSpinBox* m_aspectHeight = nullptr;

    QRadioButton* m_rotateMode = nullptr;
    QRadioButton* m_scaleMode = nullptr;
    VectorEdit m_rotateValues, m_moveValues;
    QDoubleSpinBox* m_scaleValue = nullptr;
    std::vector<std::pair<QCheckBox*, int>> m_scaleParts;
    QCheckBox* m_resetOnApply = nullptr;
    QRadioButton* m_doubleSideMode = nullptr;
    QDoubleSpinBox* m_solidThickness = nullptr;
    QCheckBox* m_solidRims = nullptr;
    QComboBox* m_solidFaces = nullptr;

    QAction* m_playSounds = nullptr;
    QAction* m_firstPersonViewModels = nullptr;
    int m_soundSequence = -1;
    float m_soundFrame = -1.0f;  // sequence position at the last sound check

    QComboBox* m_modeCombo = nullptr;
    QSlider* m_opacitySlider = nullptr;
    QLabel* m_opacityLabel = nullptr;
    std::vector<std::pair<QCheckBox*, bool StudioModelView::Display::*>> m_displayChecks;

    QComboBox* m_sequenceCombo = nullptr;
    QComboBox* m_loopCombo = nullptr;
    QLabel* m_blendCount = nullptr;
    QLabel* m_duration = nullptr;
    QLabel* m_frameCount = nullptr;
    QSlider* m_blendSlider = nullptr;
    QDoubleSpinBox* m_blendSpin = nullptr;
    QCheckBox* m_isLooping = nullptr;
    QDoubleSpinBox* m_sequenceFps = nullptr;
    QSpinBox* m_activity = nullptr;
    QSpinBox* m_activityWeight = nullptr;
    VectorEdit m_linearMovement;
    QComboBox* m_eventCombo = nullptr;
    QSpinBox* m_eventFrame = nullptr;
    QSpinBox* m_eventId = nullptr;
    QLineEdit* m_eventOptions = nullptr;

    QComboBox* m_partCombo = nullptr;
    QComboBox* m_subModelCombo = nullptr;
    QComboBox* m_skinCombo = nullptr;
    QLineEdit* m_subModelName = nullptr;
    QLabel* m_meshCount = nullptr;
    QLabel* m_vertexCount = nullptr;
    QLabel* m_normalCount = nullptr;
    QLineEdit* m_bodyValue = nullptr;

    QComboBox* m_textureCombo = nullptr;
    QLineEdit* m_textureName = nullptr;
    QSlider* m_scaleSlider = nullptr;
    QDoubleSpinBox* m_scaleSpin = nullptr;
    QComboBox* m_meshCombo = nullptr;
    QCheckBox* m_showUv = nullptr;
    QCheckBox* m_overlayUv = nullptr;
    QCheckBox* m_smoothUv = nullptr;
    std::vector<std::pair<QCheckBox*, int>> m_textureFlags;
    QSlider* m_topSlider = nullptr;
    QSlider* m_bottomSlider = nullptr;
    QSpinBox* m_topSpin = nullptr;
    QSpinBox* m_bottomSpin = nullptr;
    QLineEdit* m_colormapValue = nullptr;

    QComboBox* m_uvTexture = nullptr;
    QComboBox* m_uvMesh = nullptr;
    QCheckBox* m_uvPicture = nullptr;
    QLabel* m_uvSelection = nullptr;
    QDoubleSpinBox* m_uvMoveU = nullptr;
    QDoubleSpinBox* m_uvMoveV = nullptr;
    QDoubleSpinBox* m_uvAngle = nullptr;
    QDoubleSpinBox* m_uvScaleU = nullptr;
    QDoubleSpinBox* m_uvScaleV = nullptr;
    int m_uvStep = 0;  // one undo step per drag or button

    QComboBox* m_reducePart = nullptr;
    QComboBox* m_reduceSubModel = nullptr;
    QComboBox* m_reduceTexture = nullptr;
    QLabel* m_reduceCount = nullptr;
    QLabel* m_reduceResult = nullptr;
    QSlider* m_reduceSlider = nullptr;
    QSpinBox* m_reduceTarget = nullptr;
    QCheckBox* m_reduceNormals = nullptr;
    QCheckBox* m_reduceJoints = nullptr;
    QCheckBox* m_reduceWireframe = nullptr;
    QTimer* m_reduceTimer = nullptr;
    // the model a reduction starts from, and the one it made last
    std::shared_ptr<StudioModel> m_reduceBase, m_reduceMade;
    float m_reduceKeep = 1.0f;
    bool m_reducing = false;
    int m_reduceStep = 0;

    VectorEdit m_eyePosition, m_boundsMin, m_boundsMax, m_clipMin, m_clipMax;
    std::vector<std::pair<QCheckBox*, int>> m_modelFlags;

    QComboBox* m_boneCombo = nullptr;
    QCheckBox* m_highlightBone = nullptr;
    QLabel* m_rootBones = nullptr;
    QLineEdit* m_boneName = nullptr;
    QLineEdit* m_parentBone = nullptr;
    VectorEdit m_bonePosition, m_bonePositionScale, m_boneRotation, m_boneRotationScale;

    QComboBox* m_controllerCombo = nullptr;
    QSlider* m_controllerSlider = nullptr;
    QDoubleSpinBox* m_controllerSpin = nullptr;
    QDoubleSpinBox* m_controllerStart = nullptr;
    QDoubleSpinBox* m_controllerEnd = nullptr;
    QComboBox* m_controllerBone = nullptr;
    QSpinBox* m_controllerRest = nullptr;
    QComboBox* m_controllerIndex = nullptr;
    QComboBox* m_controllerAxis = nullptr;

    QComboBox* m_attachmentCombo = nullptr;
    QCheckBox* m_highlightAttachment = nullptr;
    QLineEdit* m_attachmentName = nullptr;
    QComboBox* m_attachmentBone = nullptr;
    VectorEdit m_attachmentOrigin;
    QLineEdit* m_attachmentQc = nullptr;
    QPushButton* m_placeAttachment = nullptr;
    QLabel* m_attachmentLimit = nullptr;

    QComboBox* m_hitboxCombo = nullptr;
    QCheckBox* m_highlightHitbox = nullptr;
    QComboBox* m_hitboxBone = nullptr;
    QSpinBox* m_hitboxGroup = nullptr;
    VectorEdit m_hitboxMin, m_hitboxMax;
    QLineEdit* m_hitboxQc = nullptr;
};
