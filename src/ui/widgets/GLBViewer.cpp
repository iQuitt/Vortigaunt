#include "GLBViewer.h"
#include "LoLModelDownloader.h"
#include "core/VortigauntLog.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLShader>
#include <QElapsedTimer>
#include <QPainter>
#include <QFont>
#include <QPaintEvent>
#include <QCoreApplication>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/anim.h>
#include <QImage>
#include <QOpenGLTexture>
#include <cmath>
#include <map>
#include <unordered_map>
#include <string>
#include <cstring>
#include <vector>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QUrl>




class GLBViewer::AssimpImporterHolder {
public:
    Assimp::Importer importer;
};

void GLBViewer::log(const QString& message)
{
    if (m_logCallback) {
        m_logCallback(message);
    } else {
        VortigauntLog::Vortigaunt_Printf(QString("[GLBViewer] %1").arg(message));
    }
}

GLBViewer::GLBViewer(QWidget* parent)
    : QOpenGLWidget(parent)
    , m_shaderProgram(nullptr)
    , m_cameraPosition(0, 0, 5)
    , m_cameraTarget(0, 0, 0)
    , m_cameraDistance(5.0f)
    , m_cameraRotationX(30.0f)
    , m_cameraRotationY(45.0f)
    , m_mousePressed(false)
    , m_modelCenter(0, 0, 0)
    , m_modelRadius(1.0f)
    , m_cameraChanged(true)
    , m_projectionChanged(true)
    , m_currentAnimationIndex(-1)
    , m_animationTime(0.0f)
    , m_animationPlaying(false)
    , m_animationTimer(nullptr)
    , m_scene(nullptr)
    , m_importerHolder(nullptr)
    , m_glInitialized(false)
    , m_frameCount(0)
    , m_currentFPS(0.0f)
    , m_lastFpsUpdate(0)
    , m_lastAnimationTime(-1.0f)
{
    setMinimumSize(400, 400);
    m_fpsTimer.start();
    
    // Initialize network manager
    m_networkManager = new QNetworkAccessManager(this);

    m_animationTimer = new QTimer(this);
    m_animationTimer->setTimerType(Qt::PreciseTimer);
    m_animationTimer->setInterval(16); // ~60 FPS
    connect(m_animationTimer, &QTimer::timeout, [this]() {
        if (m_animationPlaying && m_currentAnimationIndex >= 0 && m_currentAnimationIndex < (int)m_animations.size()) {
            float duration = m_animations[m_currentAnimationIndex].duration;
            
            // Use real elapsed time instead of fixed increment for smooth animation
            float elapsedSeconds = m_animationElapsedTimer.elapsed() / 1000.0f;
            m_animationElapsedTimer.restart();
            
            m_animationTime += elapsedSeconds;
            if (m_animationTime >= duration) {
                m_animationTime = fmod(m_animationTime, duration);  // Loop animation
            }
        }
        // Always request repaint (for animations and camera updates)
        update();
    });

    // Timer will be started when model is loaded or widget becomes visible
    
    log("GLBViewer created");
}

GLBViewer::~GLBViewer()
{
    // LINUX FIX: Only cleanup OpenGL resources if we have a valid context
    QOpenGLContext* ctx = context();
    if (ctx && ctx->isValid() && ctx->surface()) {
        makeCurrent();
        cleanup();
        doneCurrent();
    }
}

void GLBViewer::initializeGL()
{
    log("initializeGL() called");
    
    if (!initializeOpenGLFunctions()) {
        log("ERROR: Failed to initialize OpenGL functions!");
        return;
    }
    
    log(QString("OpenGL Version: %1").arg((const char*)glGetString(GL_VERSION)));
    log(QString("OpenGL Renderer: %1").arg((const char*)glGetString(GL_RENDERER)));
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);  // Enable depth writing
    
    // Disable blending to avoid transparency issues
    glDisable(GL_BLEND);

    
    // Set clear color
    glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
    
    // Setup shaders
    setupShaders();
    
    // Ensure timer is created and ready (in case constructor didn't run properly)
    if (!m_animationTimer) {
        m_animationTimer = new QTimer(this);
        m_animationTimer->setTimerType(Qt::PreciseTimer);
        m_animationTimer->setInterval(16); // ~60 FPS
        connect(m_animationTimer, &QTimer::timeout, [this]() {
            if (m_animationPlaying && m_currentAnimationIndex >= 0 && m_currentAnimationIndex < (int)m_animations.size()) {
                float duration = m_animations[m_currentAnimationIndex].duration;
                float elapsedSeconds = m_animationElapsedTimer.elapsed() / 1000.0f;
                m_animationElapsedTimer.restart();
                m_animationTime += elapsedSeconds;
                if (m_animationTime >= duration) {
                    m_animationTime = fmod(m_animationTime, duration);
                }
            }
            update();
        });
        log("Timer created in initializeGL()");
    }
    
    m_glInitialized = true;
    log("initializeGL() completed successfully");
    
#ifdef Q_OS_WIN
    doneCurrent();
#endif
    
    // Deferred loading will be handled naturally in paintGL() now.
}

void GLBViewer::setupShaders()
{
    m_shaderProgram = new QOpenGLShaderProgram(this);
    
    // Vertex shader with skeletal animation support
    const char* vertexShaderSource = R"(
        #version 330 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec2 aTexCoord;
        layout (location = 3) in ivec4 aBoneIndices;
        layout (location = 4) in vec4 aBoneWeights;
        
        uniform mat4 model;
        uniform mat4 view;
        uniform mat4 projection;
        uniform mat4 boneTransforms[219];  // Max 219 bones (for LoL models)
        uniform bool useBones;
        
        out vec3 FragPos;
        out vec3 Normal;
        out vec2 TexCoord;
        
        void main()
        {
            vec4 position = vec4(aPos, 1.0);
            vec3 normal = aNormal;
            
            if (useBones) {
                // Check if any bone weights are non-zero
                float totalWeight = aBoneWeights[0] + aBoneWeights[1] + aBoneWeights[2] + aBoneWeights[3];
                if (totalWeight > 0.001) {
                    // Apply bone transforms - blend multiple bones
                    // Bone blending: weighted sum of bone transforms
                    mat4 boneMatrix = mat4(0.0);
                    float weightSum = 0.0;
                    
                    if (aBoneWeights[0] > 0.001 && aBoneIndices[0] >= 0 && aBoneIndices[0] < 219) {
                        boneMatrix += boneTransforms[aBoneIndices[0]] * aBoneWeights[0];
                        weightSum += aBoneWeights[0];
                    }
                    if (aBoneWeights[1] > 0.001 && aBoneIndices[1] >= 0 && aBoneIndices[1] < 219) {
                        boneMatrix += boneTransforms[aBoneIndices[1]] * aBoneWeights[1];
                        weightSum += aBoneWeights[1];
                    }
                    if (aBoneWeights[2] > 0.001 && aBoneIndices[2] >= 0 && aBoneIndices[2] < 219) {
                        boneMatrix += boneTransforms[aBoneIndices[2]] * aBoneWeights[2];
                        weightSum += aBoneWeights[2];
                    }
                    if (aBoneWeights[3] > 0.001 && aBoneIndices[3] >= 0 && aBoneIndices[3] < 219) {
                        boneMatrix += boneTransforms[aBoneIndices[3]] * aBoneWeights[3];
                        weightSum += aBoneWeights[3];
                    }
                    
                    // Normalize if weights don't sum to 1.0 (shouldn't happen, but safety check)
                    if (weightSum > 0.001) {
                        boneMatrix = boneMatrix / weightSum;
                        position = boneMatrix * position;
                        normal = mat3(boneMatrix) * normal;
                    }
                    // If weightSum is 0, don't apply bone transform (use original position)
                }
            }
            
            FragPos = vec3(model * position);
            Normal = mat3(transpose(inverse(model))) * normal;
            TexCoord = aTexCoord;
            gl_Position = projection * view * vec4(FragPos, 1.0);
        }
    )";
    
    // Fragment shader
    const char* fragmentShaderSource = R"(
        #version 330 core
        out vec4 FragColor;
        
        in vec3 FragPos;
        in vec3 Normal;
        in vec2 TexCoord;
        
        uniform vec3 lightDir;
        uniform vec3 viewPos;
        uniform sampler2D diffuseTexture;
        uniform bool useTexture;
        
        void main()
        {
            // Improved lighting with ambient and diffuse
            vec3 lightDirection = normalize(-lightDir);
            vec3 normal = normalize(Normal);
            
            // Ambient lighting (base brightness)
            float ambient = 0.6;
            
            // Diffuse lighting
            float diff = max(dot(normal, lightDirection), 0.0);
            diff = max(diff, 0.3); // Minimum brightness
            
            // Combine ambient and diffuse
            float lighting = ambient + diff * 0.8;
            
            // Base color
            vec3 baseColor;
            if (useTexture) {
                baseColor = texture(diffuseTexture, TexCoord).rgb;
            } else {
                baseColor = vec3(0.8, 0.8, 0.9);
            }
            
            // Apply lighting with increased brightness
            vec3 color = baseColor * lighting * 1.2; // 1.2x brightness multiplier
            
            FragColor = vec4(color, 1.0);
        }
    )";
    
    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        log(QString("ERROR: Vertex shader compilation failed: %1").arg(m_shaderProgram->log()));
        return;
    }
    
    if (!m_shaderProgram->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        log(QString("ERROR: Fragment shader compilation failed: %1").arg(m_shaderProgram->log()));
        return;
    }
    
    if (!m_shaderProgram->link()) {
        log(QString("ERROR: Shader program linking failed: %1").arg(m_shaderProgram->log()));
        return;
    }
    
    log("Shaders compiled and linked successfully");
}

void GLBViewer::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    
    // Update projection matrix
    float aspect = (float)w / (float)h;
    m_projectionMatrix.setToIdentity();
    m_projectionMatrix.perspective(45.0f, aspect, 0.1f, 1000.0f);
    m_projectionChanged = true;
}

void GLBViewer::paintGL()
{
    // If we have a deferred model, load it NOW. 
    // In paintGL, the OpenGL context is 100% guaranteed to be valid, current, and active.
    if (!m_scene && !m_currentModelPath.isEmpty()) {
        log("paintGL: Executing deferred model load for " + m_currentModelPath);
        QString pathToLoad = m_currentModelPath;
        m_currentModelPath.clear(); // Clear to prevent infinite loop
        
        bool success = loadModelWithAssimp(pathToLoad);
        if (success) {
            if (m_modelRadius > 0.001f) {
                m_cameraDistance = m_modelRadius * 3.0f;
            }
            if (!m_animationTimer) {
                m_animationTimer = new QTimer(this);
                m_animationTimer->setTimerType(Qt::PreciseTimer);
                m_animationTimer->setInterval(6);
                connect(m_animationTimer, &QTimer::timeout, [this]() {
                    if (m_animationPlaying && m_currentAnimationIndex >= 0 && m_currentAnimationIndex < (int)m_animations.size()) {
                        float duration = m_animations[m_currentAnimationIndex].duration;
                        float elapsedSeconds = m_animationElapsedTimer.elapsed() / 1000.0f;
                        m_animationElapsedTimer.restart();
                        m_animationTime += elapsedSeconds;
                        if (m_animationTime >= duration) {
                            m_animationTime = fmod(m_animationTime, duration);
                        }
                    }
                    update();
                });
            }
            if (!m_animationTimer->isActive()) {
                m_animationTimer->start();
            }
            m_loadedGlbPath = pathToLoad; 
            emit modelLoaded(true);
        } else {
            emit modelLoaded(false);
        }
    }

    // FPS calculation - update every frame for smoother display
    m_frameCount++;
    qint64 currentTime = m_fpsTimer.elapsed();
    
    // FPS calculation - FIXED: Calculate every frame
    static qint64 lastFrameTime = 0;
    if (lastFrameTime == 0) {
        lastFrameTime = currentTime;
        m_currentFPS = 0.0f;
    } else {
        qint64 frameElapsed = currentTime - lastFrameTime;
        if (frameElapsed > 0) {
            float frameFPS = 1000.0f / frameElapsed;
            if (m_currentFPS == 0.0f) {
                m_currentFPS = frameFPS;
            } else {
                m_currentFPS = m_currentFPS * 0.9f + frameFPS * 0.1f;
            }
        }
        lastFrameTime = currentTime;
    }
    
    // Also update frame count for periodic logging
    if (m_lastFpsUpdate == 0) {
        m_lastFpsUpdate = currentTime;
    } else {
        qint64 elapsed = currentTime - m_lastFpsUpdate;
        if (elapsed >= 1000) {
            m_frameCount = 0;
            m_lastFpsUpdate = currentTime;
        }
    }
    
    // Clear with depth buffer to fix transparency issues
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Ensure depth testing and writing are enabled (fix transparency issues)
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    
    if (m_meshes.empty()) {
        return;
    }
    
    if (!m_shaderProgram || !m_shaderProgram->bind()) {
        return;
    }
    
    // Update camera (always recalculate for smooth movement)
    float radX = m_cameraRotationX * M_PI / 180.0f;
    float radY = m_cameraRotationY * M_PI / 180.0f;
    
    m_cameraPosition.setX(m_cameraDistance * sin(radY) * cos(radX));
    m_cameraPosition.setY(m_cameraDistance * sin(radX));
    m_cameraPosition.setZ(m_cameraDistance * cos(radY) * cos(radX));
    
    // View matrix (look at origin)
    m_viewMatrix.setToIdentity();
    m_viewMatrix.lookAt(m_cameraPosition, m_cameraTarget, QVector3D(0, 1, 0));
    
    // Set uniforms (always set for consistency, but only recalculate camera if changed)
    m_shaderProgram->setUniformValue("view", m_viewMatrix);
    m_shaderProgram->setUniformValue("viewPos", m_cameraPosition);
    m_cameraChanged = false;
    
    if (m_projectionChanged) {
        m_shaderProgram->setUniformValue("projection", m_projectionMatrix);
        m_projectionChanged = false;
    }
    
    // Set static uniforms once per frame (in case shader was rebound)
    m_shaderProgram->setUniformValue("lightDir", QVector3D(0.5f, 1.0f, 0.5f).normalized());
    
    // Update animation transforms - only when animation time changes (performance optimization)
    if (m_currentAnimationIndex >= 0 && m_currentAnimationIndex < (int)m_animations.size() && m_scene) {
        if (m_animationPlaying) {
            // Only update if animation time changed significantly (cache check for performance)
            // Use epsilon comparison to avoid floating point precision issues
            // This prevents recalculating 1837+ bone transforms every frame
            float timeDelta = qAbs(m_animationTime - m_lastAnimationTime);
            if (timeDelta > 0.001f || m_lastAnimationTime < 0.0f) {
                updateAnimationTransforms(m_animationTime);
                updateBoneTransforms();
                m_lastAnimationTime = m_animationTime;
            }
        } else {
            // Not playing - use bind pose (only update once)
            if (m_lastAnimationTime != -2.0f) {
                updateBoneTransforms();
                m_lastAnimationTime = -2.0f;  // Mark as bind pose updated
            }
        }
    } else if (m_scene) {
        // No animation selected - use bind pose (only update once)
        if (m_lastAnimationTime != -3.0f) {
            updateBoneTransforms();
            m_lastAnimationTime = -3.0f;  // Mark as bind pose updated
        }
    }
    
    // Model matrix (center the model)
    QMatrix4x4 baseModelMatrix;
    baseModelMatrix.setToIdentity();
    baseModelMatrix.translate(-m_modelCenter);
    
    // Enable texture unit 0 once
    glActiveTexture(GL_TEXTURE0);
    m_shaderProgram->setUniformValue("diffuseTexture", 0);
    
    // Render all meshes efficiently - batch by texture to reduce state changes
    unsigned int currentTexture = 0;
    bool currentUseTexture = false;
    
    for (size_t meshIdx = 0; meshIdx < m_meshes.size(); ++meshIdx) {
        const auto& mesh = m_meshes[meshIdx];
        if (!mesh->vao) {
            continue;
        }
        
        // Skip invisible meshes
        if (!mesh->visible) {
            continue;
        }
        
        // Calculate model matrix
        QMatrix4x4 modelMatrix = baseModelMatrix;
        if (mesh->node) {
            modelMatrix = modelMatrix * mesh->baseTransform;
        }
        m_shaderProgram->setUniformValue("model", modelMatrix);
        
        // Set bone transforms if mesh has bones
        if (mesh->hasBones && mesh->numBones > 0) {
            m_shaderProgram->setUniformValue("useBones", true);
            
            // Upload bone transforms directly to shader (max 219 bones for LoL models)
            int boneCount = qMin((int)mesh->numBones, 219);
            GLint boneTransformsLoc = m_shaderProgram->uniformLocation("boneTransforms");
            if (boneTransformsLoc >= 0) {
                // Convert QMatrix4x4 array to contiguous float array for OpenGL
                // Use static array to avoid stack allocation every frame
                static float boneData[219 * 16];  // 219 matrices * 16 floats each
                for (int b = 0; b < boneCount; ++b) {
                    const float* data = mesh->boneTransforms[b].constData();
                    memcpy(&boneData[b * 16], data, 16 * sizeof(float));
                }
                
                // Upload bone transforms to shader
                glUniformMatrix4fv(boneTransformsLoc, boneCount, GL_FALSE, boneData);
            }
        } else {
            m_shaderProgram->setUniformValue("useBones", false);
        }
        
        mesh->vao->bind();
        
        // Only change texture if different from previous mesh (reduces state changes)
        bool useTexture = mesh->hasTexture && mesh->diffuseTexture > 0;
        if (useTexture != currentUseTexture || (useTexture && mesh->diffuseTexture != currentTexture)) {
            if (useTexture) {
                glBindTexture(GL_TEXTURE_2D, mesh->diffuseTexture);
                m_shaderProgram->setUniformValue("useTexture", true);
                currentTexture = mesh->diffuseTexture;
            } else {
                glBindTexture(GL_TEXTURE_2D, 0);
                m_shaderProgram->setUniformValue("useTexture", false);
                currentTexture = 0;
            }
            currentUseTexture = useTexture;
        }
        
        glDrawElements(GL_TRIANGLES, mesh->indexCount, GL_UNSIGNED_INT, 0);
        
        mesh->vao->release();
    }
    
    // Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);
    
    m_shaderProgram->release();
}

void GLBViewer::paintEvent(QPaintEvent* event)
{
    // Call base class to do OpenGL rendering
    QOpenGLWidget::paintEvent(event);
    
    // Draw FPS overlay on top (always show)
    drawFPS();
}

void GLBViewer::drawFPS()
{
    // Use QPainter to draw text over OpenGL content
    // FONT FIX: Save and restore state to prevent affecting other widgets
    QPainter painter(this);
    painter.save();  // Save current state
    
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    
    // Set green color for FPS text
    painter.setPen(QColor(0, 255, 0));
    QFont font("Arial", 14, QFont::Bold);
    painter.setFont(font);
    
    // Draw FPS in top-right corner (always show, even if 0)
    QString fpsText = QString("FPS: %1").arg((int)m_currentFPS);
    QRect textRect = painter.fontMetrics().boundingRect(fpsText);
    int x = width() - textRect.width() - 10;
    int y = textRect.height() + 10;
    painter.drawText(x, y, fpsText);
    
    painter.restore();  // Restore state
    painter.end();
}

void GLBViewer::showEvent(QShowEvent* event)
{
    QOpenGLWidget::showEvent(event);
    log("showEvent() - Widget is now visible");
    
    // Check if we have a model pending to be loaded
    if (!m_currentModelPath.isEmpty() && !m_scene) {
        log("showEvent: Triggering update to load deferred model");
        update(); // Force a paintGL which will load the model
    }
}

void GLBViewer::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_mousePressed = true;
        m_lastMousePos = event->pos();
    }
}

void GLBViewer::mouseMoveEvent(QMouseEvent* event)
{
    if (m_mousePressed) {
        QPoint delta = event->pos() - m_lastMousePos;
        m_cameraRotationY += delta.x() * 0.5f;
        m_cameraRotationX = qBound(-89.0f, m_cameraRotationX - delta.y() * 0.5f, 89.0f);
        m_lastMousePos = event->pos();
        m_cameraChanged = true;
        // Timer will handle update() - no need to call it here
        // This reduces redundant repaints during mouse drag
    }
}

void GLBViewer::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_mousePressed = false;
    }
}

void GLBViewer::wheelEvent(QWheelEvent* event)
{
    float zoomFactor = event->angleDelta().y() > 0 ? 0.9f : 1.1f;
    m_cameraDistance = qBound(0.1f, m_cameraDistance * zoomFactor, 1000.0f);
    m_cameraChanged = true;
    // Force immediate update for zoom (user expects immediate feedback)
    update();
}

bool GLBViewer::loadGLB(const QString& filepath)
{
    log(QString("loadGLB() called with: %1").arg(filepath));
    
    if (!QFileInfo::exists(filepath)) {
        log(QString("ERROR: GLB file not found: %1").arg(filepath));
        emit modelLoaded(false);
        return false;
    }
    
    m_currentModelPath = filepath;
    
    // If not initialized, not visible, or no context yet, DEFER the load!
    // It will be loaded securely in paintGL() when everything is absolutely ready.
    if (!m_glInitialized || !isVisible()) {
        log("WARNING: OpenGL not clearly ready. Deferring model load to paintGL.");
        update(); // Ask Qt to schedule a paint event, which leads to paintGL()
        return false;
    }
    
    makeCurrent();
    
    // Final safety check
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx || !ctx->isValid()) {
        log("WARNING: OpenGL context invalid despite initialized. Deferring model load to paintGL.");
        update();
        return false;
    }
    
    cleanup();
    
    // Re-initialize shader program if it was cleaned up
    if (!m_shaderProgram || !m_shaderProgram->isLinked()) {
        log("Re-initializing shader program after cleanup");
        setupShaders();
    }
    
    bool success = loadModelWithAssimp(filepath);
    
    if (success) {
        // Calculate camera distance based on model size
        if (m_modelRadius > 0.001f) {
            m_cameraDistance = m_modelRadius * 3.0f;
        }
        
        // Start render timer for continuous updates (even without animation)
        if (!m_animationTimer) {
            log("ERROR: Timer is null in loadGLB! Creating it now...");
            m_animationTimer = new QTimer(this);
            m_animationTimer->setTimerType(Qt::PreciseTimer);
            m_animationTimer->setInterval(6);
            connect(m_animationTimer, &QTimer::timeout, [this]() {
                if (m_animationPlaying && m_currentAnimationIndex >= 0 && m_currentAnimationIndex < (int)m_animations.size()) {
                    float duration = m_animations[m_currentAnimationIndex].duration;
                    float elapsedSeconds = m_animationElapsedTimer.elapsed() / 1000.0f;
                    m_animationElapsedTimer.restart();
                    m_animationTime += elapsedSeconds;
                    if (m_animationTime >= duration) {
                        m_animationTime = fmod(m_animationTime, duration);
                    }
                }
                update();
            });
        }
        if (!m_animationTimer->isActive()) {
            m_animationTimer->start();
            log(QString("Render timer started (active: %1, interval: %2)").arg(m_animationTimer->isActive()).arg(m_animationTimer->interval()));
        }
        
        // Force immediate update
        update();
        // Clear deferred path to prevent paintGL from reloading
        m_loadedGlbPath = filepath;
        m_currentModelPath.clear();
        emit modelLoaded(true);
    } else {
        m_currentModelPath.clear();
        emit modelLoaded(false);
    }
    
    doneCurrent();
    return success;
}
// Helper: Convert Assimp matrix to Qt matrix (Assimp is row-major, Qt/OpenGL is column-major)
static inline QMatrix4x4 aiMatrixToQt(const aiMatrix4x4& m) {
    // Assimp struct members (a1, a2, ...) map to logical (row, col) as (1,1), (1,2)...
    // QMatrix4x4 constructor takes arguments in row-major order: m11, m12, m13, m14...
    // We want a direct mapping, NOT a transpose.
    return QMatrix4x4(
        m.a1, m.a2, m.a3, m.a4,
        m.b1, m.b2, m.b3, m.b4,
        m.c1, m.c2, m.c3, m.c4,
        m.d1, m.d2, m.d3, m.d4
    );
}
bool GLBViewer::loadModelWithAssimp(const QString& filepath)
{
    // LINUX FIX: Check if we have a valid surface before makeCurrent()
    if (!isVisible()) {
        log("WARNING: Widget not visible in loadModelWithAssimp, OpenGL context may not work");
    }
    
    // Ensure OpenGL context is current
    makeCurrent();
    
    // Verify context is valid
    QOpenGLContext* ctx = QOpenGLContext::currentContext();
    if (!ctx || !ctx->isValid()) {
        log("ERROR: OpenGL context is not valid in loadModelWithAssimp");
        return false;
    }
    
    // Verify shader program is valid
    if (!m_shaderProgram) {
        log("ERROR: Shader program is null in loadModelWithAssimp");
        return false;
    }
    if (!m_shaderProgram->isLinked()) {
        log("ERROR: Shader program is not linked in loadModelWithAssimp");
        return false;
    }
    
    // Create new importer holder (this will keep the scene alive)
    m_importerHolder = std::make_unique<AssimpImporterHolder>();
    
    // Use post-processing flags to optimize meshes and improve performance
    unsigned int flags = aiProcess_Triangulate 
                       | aiProcess_GenNormals 
                       | aiProcess_FlipUVs 
                       | aiProcess_CalcTangentSpace
                       | aiProcess_OptimizeMeshes  // Merge meshes where possible
                       | aiProcess_JoinIdenticalVertices;  // Join identical vertices
    
    const aiScene* scene = m_importerHolder->importer.ReadFile(
        filepath.toStdString(),
        flags
    );
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        log(QString("ERROR: Assimp failed to load file: %1").arg(m_importerHolder->importer.GetErrorString()));
        m_importerHolder.reset();
        return false;
    }
    
    m_scene = scene;
    
    // Calculate global inverse transform from root node
    // This is needed to transform bone world coordinates back to model space
    // For glTF, this should be the inverse of the root node's transform
    QMatrix4x4 rootTransformQt = aiMatrixToQt(scene->mRootNode->mTransformation);
    m_globalInverseTransform = rootTransformQt.inverted();
    
    // Debug: Log root transform and global inverse transform diagonal
    const float* rootData = rootTransformQt.constData();
    const float* invData = m_globalInverseTransform.constData();
    log(QString("Root transform diag: [%1, %2, %3] trans: [%4, %5, %6]")
        .arg(rootData[0], 0, 'f', 3).arg(rootData[5], 0, 'f', 3).arg(rootData[10], 0, 'f', 3)
        .arg(rootData[12], 0, 'f', 3).arg(rootData[13], 0, 'f', 3).arg(rootData[14], 0, 'f', 3));
    log(QString("Global inverse diag: [%1, %2, %3] trans: [%4, %5, %6]")
        .arg(invData[0], 0, 'f', 3).arg(invData[5], 0, 'f', 3).arg(invData[10], 0, 'f', 3)
        .arg(invData[12], 0, 'f', 3).arg(invData[13], 0, 'f', 3).arg(invData[14], 0, 'f', 3));
    
    log(QString("Scene loaded successfully - %1 meshes, %2 animations").arg(scene->mNumMeshes).arg(scene->mNumAnimations));
    
    // Extract animation information
    m_animations.clear();
    for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
        aiAnimation* anim = scene->mAnimations[i];
        AnimationData animData;
        animData.name = QString::fromUtf8(anim->mName.C_Str());
        if (animData.name.isEmpty()) {
            animData.name = QString("Animation %1").arg(i);
        }
        animData.duration = anim->mDuration / anim->mTicksPerSecond; // Convert to seconds
        animData.numChannels = anim->mNumChannels;
        animData.animation = anim;  // Store pointer to animation
        m_animations.push_back(animData);
        log(QString("Animation %1: %2 (duration: %3s, channels: %4)").arg(i).arg(animData.name).arg(animData.duration).arg(animData.numChannels));
    }
    
    if (m_animations.empty()) {
        log("No animations found in model");
    }
    
    // Helper function to find node containing a mesh
    std::function<const aiNode*(const aiNode*, unsigned int)> findNodeForMesh = [&](const aiNode* node, unsigned int meshIndex) -> const aiNode* {
        for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
            if (node->mMeshes[i] == meshIndex) {
                return node;
            }
        }
        for (unsigned int i = 0; i < node->mNumChildren; ++i) {
            const aiNode* found = findNodeForMesh(node->mChildren[i], meshIndex);
            if (found) return found;
        }
        return nullptr;
    };
    
    // Helper function to calculate base transform from node hierarchy
    std::function<QMatrix4x4(const aiNode*)> getNodeBaseTransform = [&](const aiNode* node) -> QMatrix4x4 {
        QMatrix4x4 transform;
        aiMatrix4x4 aiMat = node->mTransformation;
        transform(0, 0) = aiMat.a1; transform(0, 1) = aiMat.a2; transform(0, 2) = aiMat.a3; transform(0, 3) = aiMat.a4;
        transform(1, 0) = aiMat.b1; transform(1, 1) = aiMat.b2; transform(1, 2) = aiMat.b3; transform(1, 3) = aiMat.b4;
        transform(2, 0) = aiMat.c1; transform(2, 1) = aiMat.c2; transform(2, 2) = aiMat.c3; transform(2, 3) = aiMat.c4;
        transform(3, 0) = aiMat.d1; transform(3, 1) = aiMat.d2; transform(3, 2) = aiMat.d3; transform(3, 3) = aiMat.d4;
        
        // Multiply with parent transforms
        if (node->mParent && node->mParent != scene->mRootNode) {
            return getNodeBaseTransform(node->mParent) * transform;
        }
        return transform;
    };
    
    // Calculate bounds
    float minX = 1e30f, minY = 1e30f, minZ = 1e30f;
    float maxX = -1e30f, maxY = -1e30f, maxZ = -1e30f;
    int totalVertices = 0;
    
    // First pass: calculate bounds
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[i];
        for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
            aiVector3D pos = mesh->mVertices[j];
            minX = qMin(minX, pos.x);
            minY = qMin(minY, pos.y);
            minZ = qMin(minZ, pos.z);
            maxX = qMax(maxX, pos.x);
            maxY = qMax(maxY, pos.y);
            maxZ = qMax(maxZ, pos.z);
        }
        totalVertices += mesh->mNumVertices;
    }
    
    m_modelCenter = QVector3D((minX + maxX) * 0.5f, (minY + maxY) * 0.5f, (minZ + maxZ) * 0.5f);
    float sizeX = maxX - minX;
    float sizeY = maxY - minY;
    float sizeZ = maxZ - minZ;
    m_modelRadius = qMax(qMax(sizeX, sizeY), sizeZ) * 0.5f;
    
    log(QString("Model bounds - center: (%1, %2, %3), radius: %4")
        .arg(m_modelCenter.x()).arg(m_modelCenter.y()).arg(m_modelCenter.z()).arg(m_modelRadius));
    
    // Second pass: create OpenGL buffers
    for (unsigned int i = 0; i < scene->mNumMeshes; ++i) {
        aiMesh* mesh = scene->mMeshes[i];
        
        if (!mesh->HasPositions()) {
            continue;
        }
        
        auto meshData = std::make_unique<MeshData>();
        meshData->vao = new QOpenGLVertexArrayObject(this);
        meshData->vertexBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        meshData->indexBuffer = new QOpenGLBuffer(QOpenGLBuffer::IndexBuffer);
        meshData->boneIndexBuffer = nullptr;
        meshData->boneWeightBuffer = nullptr;
        meshData->diffuseTexture = 0;
        meshData->baseTexture = 0;  // Will be set after texture load
        meshData->hasTexture = false;
        meshData->hasBones = mesh->HasBones();
        meshData->visible = true;  // Default visible
        
        // Improve mesh naming - use material name as primary source (matches Blender's glTF mesh naming)
        // In splitMeshPrimitives (LoLModelDownloader), meshes are named after their materials
        // So we should prioritize: 1) Material name, 2) Mesh name, 3) Node name
        QString meshName;
        
        // First, try to get material name (this is what Blender shows for glTF meshes)
        if (mesh->mMaterialIndex < scene->mNumMaterials) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            aiString materialName;
            if (material->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS) {
                QString matName = QString::fromUtf8(materialName.C_Str());
                if (!matName.isEmpty() && matName != "DefaultMaterial" && !matName.startsWith("Material", Qt::CaseInsensitive)) {
                    meshName = matName;
                }
            }
        }
        
        // If no material name, try mesh name
        if (meshName.isEmpty()) {
            meshName = QString::fromStdString(mesh->mName.C_Str());
        }
        
        // If mesh name is empty or generic, try node name
        if (meshName.isEmpty() || meshName.startsWith("Mesh", Qt::CaseInsensitive)) {
            const aiNode* node = findNodeForMesh(scene->mRootNode, i);
            if (node) {
                QString nodeName = QString::fromStdString(node->mName.C_Str());
                if (!nodeName.isEmpty()) {
                    meshName = nodeName;
                }
            }
        }
        
        // If still empty, use default name
        if (meshName.isEmpty()) {
            meshName = QString("Mesh_%1").arg(i);
        }
        
        meshData->name = meshName;
        
        meshData->numBones = mesh->mNumBones;
        meshData->sceneMeshIndex = i;  // Store scene mesh index
        
        // Check if mesh has bones
        if (meshData->hasBones) {
            this->log(QString("Mesh %1 has %2 bones").arg(i).arg(meshData->numBones));
            meshData->boneTransforms.resize(meshData->numBones);
            meshData->bonePointers.resize(meshData->numBones);
            meshData->boneNodes.resize(meshData->numBones);
            meshData->boneOffsetMatrices.resize(meshData->numBones);
            
            // Store bone pointers, cache bone nodes and offset matrices (for performance)
            for (unsigned int b = 0; b < meshData->numBones; ++b) {
                meshData->bonePointers[b] = mesh->mBones[b];
                meshData->boneTransforms[b].setToIdentity();
                
                // Cache bone node (avoid FindNode() calls every frame)
                if (mesh->mBones[b]) {
                    meshData->boneNodes[b] = scene->mRootNode->FindNode(mesh->mBones[b]->mName);
                    
                    // Cache offset matrix - FIXED: Assimp stores matrices in row-major, Qt/OpenGL uses column-major
                    // We need to transpose when converting
                    if (meshData->boneNodes[b]) {
                        const aiMatrix4x4& aiMat = mesh->mBones[b]->mOffsetMatrix;
                        meshData->boneOffsetMatrices[b] = aiMatrixToQt(aiMat);
                        
                        // Debug first bone's offset matrix
                        if (b == 0 && i == 0) {
                            const float* offData = meshData->boneOffsetMatrices[b].constData();
                            log(QString("Bone 0 offset matrix diag: [%1, %2, %3] trans: [%4, %5, %6]")
                                .arg(offData[0]).arg(offData[5]).arg(offData[10])
                                .arg(offData[12]).arg(offData[13]).arg(offData[14]));
                        }
                    }
                } else {
                    meshData->boneNodes[b] = nullptr;
                }
            }
        }
        
        meshData->vao->create();
        meshData->vao->bind();
        
        // CRITICAL: Shader program must be bound when setting up VAO attributes
        // Check if shader program is valid and linked
        if (!m_shaderProgram) {
            log(QString("ERROR: Shader program is null when setting up mesh %1").arg(i));
            continue;
        }
        if (!m_shaderProgram->isLinked()) {
            log(QString("ERROR: Shader program is not linked when setting up mesh %1").arg(i));
            continue;
        }
        if (!m_shaderProgram->bind()) {
            log(QString("ERROR: Failed to bind shader program when setting up mesh %1 (program ID: %2, linked: %3)")
                .arg(i).arg(m_shaderProgram->programId()).arg(m_shaderProgram->isLinked()));
            // Check OpenGL context
            QOpenGLContext* ctx = QOpenGLContext::currentContext();
            if (!ctx) {
                log(QString("ERROR: No OpenGL context current!"));
            } else {
                log(QString("OpenGL context is valid: %1").arg(ctx->isValid()));
            }
            continue;
        }
        
        // Prepare vertex data
        std::vector<float> vertices;
        std::vector<int> boneIndices;
        std::vector<float> boneWeights;
        std::vector<unsigned int> indices;
        
        // Build bone index map for quick lookup
        std::map<unsigned int, std::vector<std::pair<unsigned int, float>>> vertexBoneWeights;
        if (meshData->hasBones) {
            for (unsigned int b = 0; b < mesh->mNumBones; ++b) {
                aiBone* bone = mesh->mBones[b];
                for (unsigned int w = 0; w < bone->mNumWeights; ++w) {
                    aiVertexWeight weight = bone->mWeights[w];
                    vertexBoneWeights[weight.mVertexId].push_back({b, weight.mWeight});
                }
            }
        }
        
        for (unsigned int j = 0; j < mesh->mNumVertices; ++j) {
            // Position
            vertices.push_back(mesh->mVertices[j].x);
            vertices.push_back(mesh->mVertices[j].y);
            vertices.push_back(mesh->mVertices[j].z);
            
            // Normal
            if (mesh->HasNormals()) {
                vertices.push_back(mesh->mNormals[j].x);
                vertices.push_back(mesh->mNormals[j].y);
                vertices.push_back(mesh->mNormals[j].z);
            } else {
                vertices.push_back(0.0f);
                vertices.push_back(1.0f);
                vertices.push_back(0.0f);
            }
            
            // TexCoord
            if (mesh->HasTextureCoords(0)) {
                vertices.push_back(mesh->mTextureCoords[0][j].x);
                vertices.push_back(mesh->mTextureCoords[0][j].y);
            } else {
                vertices.push_back(0.0f);
                vertices.push_back(0.0f);
            }
            
            // Bone indices and weights
            if (meshData->hasBones && vertexBoneWeights.find(j) != vertexBoneWeights.end()) {
                auto& weights = vertexBoneWeights[j];
                // Sort by weight descending and take top 4
                std::sort(weights.begin(), weights.end(), 
                    [](const std::pair<unsigned int, float>& a, const std::pair<unsigned int, float>& b) {
                        return a.second > b.second;
                    });
                
                int indices[4] = {0, 0, 0, 0};
                float weightsArray[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                
                float totalWeight = 0.0f;
                for (size_t w = 0; w < qMin(weights.size(), size_t(4)); ++w) {
                    indices[w] = weights[w].first;
                    weightsArray[w] = weights[w].second;
                    totalWeight += weightsArray[w];
                }
                
                // Normalize weights
                if (totalWeight > 0.0f) {
                    for (int w = 0; w < 4; ++w) {
                        weightsArray[w] /= totalWeight;
                    }
                }
                
                boneIndices.push_back(indices[0]);
                boneIndices.push_back(indices[1]);
                boneIndices.push_back(indices[2]);
                boneIndices.push_back(indices[3]);
                boneWeights.push_back(weightsArray[0]);
                boneWeights.push_back(weightsArray[1]);
                boneWeights.push_back(weightsArray[2]);
                boneWeights.push_back(weightsArray[3]);
            } else {
                boneIndices.push_back(0);
                boneIndices.push_back(0);
                boneIndices.push_back(0);
                boneIndices.push_back(0);
                boneWeights.push_back(0.0f);
                boneWeights.push_back(0.0f);
                boneWeights.push_back(0.0f);
                boneWeights.push_back(0.0f);
            }
        }
        
        // Debug: Check bone weight statistics
        if (meshData->hasBones && i == 0) {
            int verticesWithWeights = 0;
            int verticesWithoutWeights = 0;
            for (size_t v = 0; v < boneWeights.size(); v += 4) {
                float totalWeight = boneWeights[v] + boneWeights[v+1] + boneWeights[v+2] + boneWeights[v+3];
                if (totalWeight > 0.001f) {
                    verticesWithWeights++;
                } else {
                    verticesWithoutWeights++;
                }
            }
            log(QString("Mesh %1 bone weight stats: %2 vertices with weights, %3 without")
                .arg(i).arg(verticesWithWeights).arg(verticesWithoutWeights));
            
            // Log first few bone indices/weights for debugging
            if (boneIndices.size() >= 8) {
                log(QString("  First vertex bone data: indices=[%1,%2,%3,%4] weights=[%5,%6,%7,%8]")
                    .arg(boneIndices[0]).arg(boneIndices[1]).arg(boneIndices[2]).arg(boneIndices[3])
                    .arg(boneWeights[0], 0, 'f', 3).arg(boneWeights[1], 0, 'f', 3)
                    .arg(boneWeights[2], 0, 'f', 3).arg(boneWeights[3], 0, 'f', 3));
            }
        }
        
        // Indices
        for (unsigned int j = 0; j < mesh->mNumFaces; ++j) {
            aiFace face = mesh->mFaces[j];
            for (unsigned int k = 0; k < face.mNumIndices; ++k) {
                indices.push_back(face.mIndices[k]);
            }
        }
        
        // Upload to GPU
        meshData->vertexBuffer->create();
        meshData->vertexBuffer->bind();
        meshData->vertexBuffer->allocate(vertices.data(), vertices.size() * sizeof(float));
        
        meshData->indexBuffer->create();
        meshData->indexBuffer->bind();
        meshData->indexBuffer->allocate(indices.data(), indices.size() * sizeof(unsigned int));
        
        // Setup vertex attributes
        int stride = 8 * sizeof(float);
        m_shaderProgram->enableAttributeArray(0);
        m_shaderProgram->setAttributeBuffer(0, GL_FLOAT, 0, 3, stride);
        
        m_shaderProgram->enableAttributeArray(1);
        m_shaderProgram->setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 3, stride);
        
        m_shaderProgram->enableAttributeArray(2);
        m_shaderProgram->setAttributeBuffer(2, GL_FLOAT, 6 * sizeof(float), 2, stride);
        
        // Create separate buffers for bone data (only if mesh has bones)
        if (meshData->hasBones) {
            meshData->boneIndexBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
            meshData->boneWeightBuffer = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
            meshData->boneIndexBuffer->create();
            meshData->boneWeightBuffer->create();
            
            // Bone indices - MUST use glVertexAttribIPointer for integer attributes!
            // Qt's setAttributeBuffer uses glVertexAttribPointer which converts to float
            meshData->boneIndexBuffer->bind();
            meshData->boneIndexBuffer->allocate(boneIndices.data(), boneIndices.size() * sizeof(int));
            glEnableVertexAttribArray(3);
            glVertexAttribIPointer(3, 4, GL_INT, 4 * sizeof(int), nullptr);  // Integer pointer for ivec4
            
            // Bone weights - float, so setAttributeBuffer is fine
            meshData->boneWeightBuffer->bind();
            meshData->boneWeightBuffer->allocate(boneWeights.data(), boneWeights.size() * sizeof(float));
            m_shaderProgram->setAttributeBuffer(4, GL_FLOAT, 0, 4, 4 * sizeof(float));
            m_shaderProgram->enableAttributeArray(4);
        } else {
            // Disable bone attributes for meshes without bones
            glDisableVertexAttribArray(3);
            glDisableVertexAttribArray(4);
        }
        
        meshData->indexCount = indices.size();
        meshData->vao->release();
        
        // Release shader program after VAO setup
        m_shaderProgram->release();
        
        // Load texture if material has one
        if (mesh->mMaterialIndex < scene->mNumMaterials) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            
            // Try to get diffuse texture
            if (material->GetTextureCount(aiTextureType_DIFFUSE) > 0) {
                aiString texturePath;
                if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
                    // Try to load texture from embedded data or file
                    unsigned int textureId = this->loadTexture(scene, texturePath.C_Str(), filepath);
                    if (textureId > 0) {
                        meshData->diffuseTexture = textureId;
                        meshData->baseTexture = textureId;  // Save for chroma switching
                        meshData->hasTexture = true;
                      
                        const char* tp = texturePath.C_Str();
                        if (tp[0] == '*') {
                            meshData->textureImageIndex = atoi(tp + 1);
                        }
                        
                        log(QString("Texture loaded for mesh %1: %2").arg(i).arg(texturePath.C_Str()));
                    }
                }
            }
        }
        
        m_meshes.push_back(std::move(meshData));
        
        log(QString("Mesh %1 loaded - %2 vertices, %3 indices").arg(i).arg(mesh->mNumVertices).arg(indices.size()));
    }
    
    log(QString("Total meshes loaded: %1").arg(m_meshes.size()));
    return true;
}

void GLBViewer::clearModel()
{
    makeCurrent();
    cleanup();
    doneCurrent();
    update();
}

unsigned int GLBViewer::loadTexture(const aiScene* scene, const char* texturePath, const QString& modelPath)
{
    // Check if texture is embedded
    const aiTexture* embeddedTexture = scene->GetEmbeddedTexture(texturePath);
    
    QImage image;
    
    if (embeddedTexture) {
        // Load from embedded data
        if (embeddedTexture->mHeight == 0) {
            // Compressed texture (e.g., PNG, JPG)
            QByteArray data(reinterpret_cast<const char*>(embeddedTexture->pcData), embeddedTexture->mWidth);
            if (!image.loadFromData(data)) {
                this->log(QString("WARNING: Failed to load embedded texture: %1").arg(texturePath));
                return 0;
            }
        } else {
            // Uncompressed texture
            this->log(QString("WARNING: Uncompressed embedded texture not supported: %1").arg(texturePath));
            return 0;
        }
    } else {
        // Load from file
        QString fullPath = QFileInfo(modelPath).absolutePath() + "/" + QString::fromUtf8(texturePath);
        if (!QFileInfo::exists(fullPath)) {
            // Try just the filename
            fullPath = QFileInfo(modelPath).absolutePath() + "/" + QFileInfo(QString::fromUtf8(texturePath)).fileName();
        }
        
        if (QFileInfo::exists(fullPath)) {
            if (!image.load(fullPath)) {
                this->log(QString("WARNING: Failed to load texture file: %1").arg(fullPath));
                return 0;
            }
        } else {
            this->log(QString("WARNING: Texture file not found: %1").arg(fullPath));
            return 0;
        }
    }
    
    // Convert to RGBA format
    image = image.convertToFormat(QImage::Format_RGBA8888);
    
    // Create OpenGL texture
    unsigned int textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    
    // Upload image data
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    // Generate mipmaps (glGenerateMipmap is in OpenGL 3.0+, available via QOpenGLFunctions_3_3_Core)
    glGenerateMipmap(GL_TEXTURE_2D);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    this->log(QString("Texture created: %1 (%2x%3)").arg(texturePath).arg(image.width()).arg(image.height()));
    return textureId;
}

void GLBViewer::playAnimation()
{
    if (m_animations.empty()) {
        this->log("No animations available");
        return;
    }
    
    if (m_currentAnimationIndex < 0) {
        m_currentAnimationIndex = 0;
    }
    
    m_animationPlaying = true;
    m_animationTime = 0.0f;  // Reset animation time when starting
    m_animationElapsedTimer.restart();  // Start real-time tracking
    
    // Ensure timer is running (it should already be running after model load)
    if (m_animationTimer && !m_animationTimer->isActive()) {
        m_animationTimer->start(16); // ~60 FPS target
    }
    
    // Reset cache to force immediate update
    m_lastAnimationTime = -1.0f;
    
    this->log(QString("Playing animation: %1 (timer active: %2)").arg(m_animations[m_currentAnimationIndex].name).arg(m_animationTimer->isActive()));
}

void GLBViewer::pauseAnimation()
{
    m_animationPlaying = false;
    // Don't stop timer - keep it running for camera updates
    this->log("Animation paused");
}

void GLBViewer::stopAnimation()
{
    m_animationPlaying = false;
    m_animationTime = 0.0f;
    m_lastAnimationTime = -1.0f;  // Reset cache to force update to bind pose
    // Don't stop timer - keep it running for camera updates
    update();
    this->log("Animation stopped");
}

void GLBViewer::setAnimationTime(float time)
{
    m_animationTime = time;
    if (m_currentAnimationIndex >= 0 && m_currentAnimationIndex < (int)m_animations.size()) {
        float duration = m_animations[m_currentAnimationIndex].duration;
        if (m_animationTime > duration) {
            m_animationTime = duration;
        }
    }
    update();
}

float GLBViewer::getAnimationTime() const
{
    return m_animationTime;
}

float GLBViewer::getAnimationDuration() const
{
    if (m_currentAnimationIndex >= 0 && m_currentAnimationIndex < (int)m_animations.size()) {
        return m_animations[m_currentAnimationIndex].duration;
    }
    return 0.0f;
}

QStringList GLBViewer::getAnimationNames() const
{
    QStringList names;
    // Add "Bind Pose" as first option - shows original T-pose/A-pose
    names.append("Bind Pose");
    for (const auto& anim : m_animations) {
        names.append(anim.name);
    }
    return names;
}

void GLBViewer::setCurrentAnimation(const QString& animationName)
{
    // Special handling for "Bind Pose" - show original T-pose/A-pose
    if (animationName == "Bind Pose") {
        m_currentAnimationIndex = -1;
        m_animationPlaying = false;
        m_animationTime = 0.0f;
        m_lastAnimationTime = -1.0f;  // Reset cache to force update
        this->log("Switched to Bind Pose (T-pose/A-pose)");
        update();  // Force immediate update
        return;
    }
    
    for (size_t i = 0; i < m_animations.size(); ++i) {
        if (m_animations[i].name == animationName) {
            m_currentAnimationIndex = (int)i;
            m_animationTime = 0.0f;
            m_lastAnimationTime = -1.0f;  // Reset cache to force update
            this->log(QString("Switched to animation: %1").arg(animationName));
            update();  // Force immediate update
            return;
        }
    }
    this->log(QString("WARNING: Animation not found: %1").arg(animationName));
}

// ========== MESH VISIBILITY METHODS ==========

QStringList GLBViewer::getMeshNames() const
{
    QStringList names;
    for (const auto& mesh : m_meshes) {
        names.append(mesh->name.isEmpty() ? QString("Mesh %1").arg(names.size()) : mesh->name);
    }
    return names;
}

void GLBViewer::setMeshVisible(int index, bool visible)
{
    if (index >= 0 && index < (int)m_meshes.size()) {
        m_meshes[index]->visible = visible;
        update();
    }
}

bool GLBViewer::isMeshVisible(int index) const
{
    if (index >= 0 && index < (int)m_meshes.size()) {
        return m_meshes[index]->visible;
    }
    return false;
}

int GLBViewer::getMeshCount() const
{
    return (int)m_meshes.size();
}

std::set<unsigned int> GLBViewer::getHiddenMeshIndices() const
{
    std::set<unsigned int> hiddenIndices;
    for (size_t i = 0; i < m_meshes.size(); ++i) {
        if (!m_meshes[i]->visible) {
            hiddenIndices.insert(static_cast<unsigned int>(i));
        }
    }
    return hiddenIndices;
}


void GLBViewer::loadChromaTextures(const QString& championName, const QString& modelId, const QStringList& chromaIds)
{
    m_currentChampionName = championName;
    m_currentModelId = modelId;
    m_chromas.clear();
    m_currentChromaIndex = -1;
    m_pendingChromaDownloads = 0;
    
    if (chromaIds.isEmpty()) {
        emit chromasLoaded(0);
        return;
    }
    
    QStringList textureNames;
    if (!m_loadedGlbPath.isEmpty()) {
        LoLModelDownloader tempDownloader;
        textureNames = tempDownloader.getTextureNamesFromGLB(m_loadedGlbPath);
    }
    
    if (textureNames.isEmpty()) {
        this->log("No texture names found in model for chroma loading");
        emit chromasLoaded(0);
        return;
    }
    
    this->log(QString("Found %1 texture(s) for chroma loading: %2").arg(textureNames.size()).arg(textureNames.join(", ")));
    
    // Only process chromas that are different from base model
    QStringList validChromas;
    for (const QString& chromaId : chromaIds) {
        if (chromaId != modelId) {
            validChromas.append(chromaId);
        }
    }
    
    if (validChromas.isEmpty()) {
        emit chromasLoaded(0);
        return;
    }
    
    if (!m_networkManager) {
        m_networkManager = new QNetworkAccessManager(this);
    }
    
    m_pendingChromaDownloads = validChromas.size() * textureNames.size();
    this->log(QString("Starting async download of %1 chromas (%2 textures each)...")
        .arg(validChromas.size()).arg(textureNames.size()));
    
    for (const QString& chromaId : validChromas) {
        // Pre-create ChromaData for this chroma
        ChromaData chroma;
        chroma.id = chromaId;
        chroma.name = QString("Chroma %1").arg(chromaId);
        chroma.meshTextures.resize(m_meshes.size(), 0);
        m_chromas.push_back(chroma);
        int chromaIndex = (int)m_chromas.size() - 1;
        
        for (int texIdx = 0; texIdx < textureNames.size(); ++texIdx) {
            const QString& textureName = textureNames[texIdx];
            
            QString textureUrl = QString("https://cdn.modelviewer.lol/lol/models/%1/%2/chromas/%3/%4")
                .arg(championName.toLower(), modelId, chromaId, textureName);
            
            QUrl urlObj(textureUrl);
            QNetworkRequest request(urlObj);
            request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
            
            QNetworkReply* reply = m_networkManager->get(request);
            
            connect(reply, &QNetworkReply::finished, this, [this, reply, chromaId, chromaIndex, texIdx]() {
                reply->deleteLater();
                
                if (reply->error() == QNetworkReply::NoError) {
                    QByteArray data = reply->readAll();
                    QImage image;
                    if (image.loadFromData(data)) {
                        image = image.convertToFormat(QImage::Format_RGBA8888);
                        
                        // Create OpenGL texture
                        makeCurrent();
                        
                        unsigned int textureId;
                        glGenTextures(1, &textureId);
                        glBindTexture(GL_TEXTURE_2D, textureId);
                        
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image.width(), image.height(), 
                                    0, GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
                        glGenerateMipmap(GL_TEXTURE_2D);
                        glBindTexture(GL_TEXTURE_2D, 0);
                        
                        doneCurrent();
                        
                        // Assign texture to meshes that use this image index
                        if (chromaIndex >= 0 && chromaIndex < (int)m_chromas.size()) {
                            for (size_t i = 0; i < m_meshes.size(); ++i) {
                                if (m_meshes[i]->hasTexture && m_meshes[i]->textureImageIndex == texIdx) {
                                    m_chromas[chromaIndex].meshTextures[i] = textureId;
                                }
                            }
                        }
                    }
                }
                
                m_pendingChromaDownloads--;
                if (m_pendingChromaDownloads <= 0) {
                    m_chromas.erase(
                        std::remove_if(m_chromas.begin(), m_chromas.end(), [](const ChromaData& c) {
                            for (auto t : c.meshTextures) {
                                if (t > 0) return false;
                            }
                            return true;
                        }),
                        m_chromas.end()
                    );
                    
                    this->log(QString("All chromas loaded. Total: %1").arg(m_chromas.size()));
                    
                    // Sort chromas by ID to keep consistent order
                    std::sort(m_chromas.begin(), m_chromas.end(), [](const ChromaData& a, const ChromaData& b) {
                        return a.id.toInt() < b.id.toInt();
                    });
                    
                    emit chromasLoaded((int)m_chromas.size());
                }
            });
        }
    }
}

QStringList GLBViewer::getChromaNames() const
{
    QStringList names;
    names.append("Base"); // Index -1 maps to "Base"
    for (const auto& chroma : m_chromas) {
        names.append(chroma.name);
    }
    return names;
}

void GLBViewer::setCurrentChroma(int index)
{
    if (index < -1 || index >= (int)m_chromas.size()) {
        return;
    }
    
    m_currentChromaIndex = index;
    
    // Update mesh textures
    for (size_t i = 0; i < m_meshes.size(); ++i) {
        if (m_meshes[i]->hasTexture) {
            if (index == -1) {
                // Restore base texture
                m_meshes[i]->diffuseTexture = m_meshes[i]->baseTexture;
            } else if (index >= 0 && m_chromas[index].meshTextures[i] > 0) {
                // Use chroma texture
                m_meshes[i]->diffuseTexture = m_chromas[index].meshTextures[i];
            }
        }
    }
    
    update();
}

int GLBViewer::getCurrentChromaIndex() const
{
    return m_currentChromaIndex;
}

void GLBViewer::updateAnimationTransforms(float time)
{
    if (m_currentAnimationIndex < 0 || m_currentAnimationIndex >= (int)m_animations.size() || !m_scene) {
        return;
    }
    
    const aiAnimation* animation = m_animations[m_currentAnimationIndex].animation;
    if (!animation) return;
    
    float ticksPerSecond = animation->mTicksPerSecond != 0 ? animation->mTicksPerSecond : 25.0f;
    float timeInTicks = time * ticksPerSecond;
    float animationTime = fmod(timeInTicks, animation->mDuration);
    
    // Update node transforms for non-skeletal meshes
    for (const auto& mesh : m_meshes) {
        if (!mesh->node || mesh->hasBones) continue; 
        
        QMatrix4x4 animatedTransform = getNodeTransform(mesh->node, animationTime, animation);
        mesh->baseTransform = animatedTransform;
    }
}



void GLBViewer::updateBoneTransforms()
{
    if (!m_scene) {
        return;
    }
    
    const aiAnimation* animation = nullptr;
    float animationTime = 0.0f;
    
    if (m_currentAnimationIndex >= 0 && m_currentAnimationIndex < (int)m_animations.size()) {
        animation = m_animations[m_currentAnimationIndex].animation;
        if (animation && m_animationPlaying) {
            float ticksPerSecond = animation->mTicksPerSecond;
            if (ticksPerSecond == 0.0f) {
                ticksPerSecond = 25.0f;
            }
            float timeInTicks = m_animationTime * ticksPerSecond;
            animationTime = fmod(timeInTicks, animation->mDuration);
        }
    }
    
    // Build animation channel cache using node pointer for O(1) lookup
    static const aiAnimation* cachedAnimation = nullptr;
    static const aiScene* cachedScene = nullptr;
    static std::unordered_map<const aiNode*, const aiNodeAnim*> channelCache;
    
    // Clear cache if scene or animation changed
    if (cachedScene != m_scene || cachedAnimation != animation) {
        channelCache.clear();
        cachedScene = m_scene;
        cachedAnimation = animation;
        if (animation && m_scene) {
            channelCache.reserve(animation->mNumChannels);
            for (unsigned int i = 0; i < animation->mNumChannels; ++i) {
                const aiNodeAnim* channel = animation->mChannels[i];
                const aiNode* node = m_scene->mRootNode->FindNode(channel->mNodeName);
                if (node) {
                    channelCache[node] = channel;
                }
            }
        }
    }
    
    // Node transform cache - reused WITHIN a single frame for shared nodes
    // Static to avoid heap allocation every frame (which causes stuttering)
    // Cleared at the start of each updateBoneTransforms() call
    static std::unordered_map<const aiNode*, QMatrix4x4> nodeTransformCache;
    nodeTransformCache.clear();  // Clear for this frame, but reuse container
    
    // Reusable path buffer
    static thread_local std::vector<const aiNode*> nodePath;
    
    // Compute node world transform with proper matrix conversion
    auto computeNodeWorldTransform = [&](const aiNode* targetNode) -> QMatrix4x4 {
        if (!targetNode) return QMatrix4x4();

        auto cacheIt = nodeTransformCache.find(targetNode);
        if (cacheIt != nodeTransformCache.end()) {
            return cacheIt->second;
        }

        // Build path from target to root using static buffer (reused, no alloc)
        nodePath.clear();  // Clear the static buffer, don't create a new one!
        const aiNode* currentNode = targetNode;
        while (currentNode != nullptr) {
            // Skip "Skeleton" nodes using raw C string comparison
            const char* nodeName = currentNode->mName.data;
            if (strcmp(nodeName, "Skeleton") == 0 || strcmp(nodeName, "skeleton") == 0) {
                currentNode = currentNode->mParent;
                continue;
            }
            nodePath.push_back(currentNode);
            if (currentNode == m_scene->mRootNode) {
                break;
            }
            currentNode = currentNode->mParent;
        }

        QMatrix4x4 worldTransform;
        worldTransform.setToIdentity();

        for (int i = (int)nodePath.size() - 1; i >= 0; --i) {
            const aiNode* node = nodePath[i];

            QMatrix4x4 localTransform;
            auto chIt = channelCache.find(node);

            if (chIt != channelCache.end()) {
                const aiNodeAnim* nodeAnim = chIt->second;
                QMatrix4x4 T = interpolateTranslation(nodeAnim->mPositionKeys, nodeAnim->mNumPositionKeys, animationTime);
                QMatrix4x4 R = interpolateRotation(nodeAnim->mRotationKeys, nodeAnim->mNumRotationKeys, animationTime);
                QMatrix4x4 S = interpolateScaling(nodeAnim->mScalingKeys, nodeAnim->mNumScalingKeys, animationTime);
                localTransform = T * R * S;
            }
            else {
                localTransform = aiMatrixToQt(node->mTransformation);
            }

            worldTransform = worldTransform * localTransform;
        }

        nodeTransformCache[targetNode] = worldTransform;
        return worldTransform;
    };
    
    // Update bone transforms for all skeletal meshes
    for (size_t meshIdx = 0; meshIdx < m_meshes.size(); ++meshIdx) {
        const auto& mesh = m_meshes[meshIdx];
        if (!mesh->hasBones || mesh->numBones == 0) {
            continue;
        }
        
        for (unsigned int b = 0; b < mesh->numBones && b < 219; ++b) {
            if (b >= mesh->boneNodes.size()) continue;
            
            const aiNode* boneNode = mesh->boneNodes[b];
            if (!boneNode) {
                mesh->boneTransforms[b].setToIdentity();
                continue;
            }
            
            QMatrix4x4 boneWorldTransform = computeNodeWorldTransform(boneNode);
            mesh->boneTransforms[b] = boneWorldTransform * mesh->boneOffsetMatrices[b];
        }
    }
}

QMatrix4x4 GLBViewer::getNodeTransform(const aiNode* node, float animationTime, const aiAnimation* animation)
{
    QMatrix4x4 nodeTransform;
    
    // Get local transform (from animation or bind pose)
    const aiNodeAnim* nodeAnim = nullptr;
    if (animation) {
        for (unsigned int i = 0; i < animation->mNumChannels; ++i) {
            if (animation->mChannels[i]->mNodeName == node->mName) {
                nodeAnim = animation->mChannels[i];
                break;
            }
        }
    }
    
    if (nodeAnim) {
        QMatrix4x4 T = interpolateTranslation(nodeAnim->mPositionKeys, nodeAnim->mNumPositionKeys, animationTime);
        QMatrix4x4 R = interpolateRotation(nodeAnim->mRotationKeys, nodeAnim->mNumRotationKeys, animationTime);
        QMatrix4x4 S = interpolateScaling(nodeAnim->mScalingKeys, nodeAnim->mNumScalingKeys, animationTime);
        nodeTransform = T * R * S;
    } else {
        // Use default node transform (bind pose) - TRANSPOSE for OpenGL!
        nodeTransform = aiMatrixToQt(node->mTransformation);
    }
    
    // Multiply with parent transform
    if (node->mParent && node->mParent != m_scene->mRootNode) {
        return getNodeTransform(node->mParent, animationTime, animation) * nodeTransform;
    }
    
    return nodeTransform;
}

QMatrix4x4 GLBViewer::interpolateTranslation(const aiVectorKey* keys, unsigned int numKeys, float time)
{
    QMatrix4x4 result;
    result.setToIdentity();
    
    if (numKeys == 0) return result;
    if (numKeys == 1) {
        result.translate(keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z);
        return result;
    }
    
    // Find keys to interpolate between (binary search for better performance)
    // Handle edge cases first
    if (time <= keys[0].mTime) {
        result.translate(keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z);
        return result;
    }
    if (time >= keys[numKeys - 1].mTime) {
        result.translate(keys[numKeys - 1].mValue.x, keys[numKeys - 1].mValue.y, keys[numKeys - 1].mValue.z);
        return result;
    }
    
    // Binary search for the correct keyframe pair
    unsigned int index = 0;
    unsigned int left = 0;
    unsigned int right = numKeys - 1;
    while (right - left > 1) {
        unsigned int mid = (left + right) / 2;
        if (time < keys[mid].mTime) {
            right = mid;
        } else {
            left = mid;
        }
    }
    index = left;
    
    float deltaTime = keys[index + 1].mTime - keys[index].mTime;
    float factor = deltaTime > 0 ? (time - keys[index].mTime) / deltaTime : 0.0f;
    factor = qBound(0.0f, factor, 1.0f);
    
    aiVector3D start = keys[index].mValue;
    aiVector3D end = keys[index + 1].mValue;
    aiVector3D interpolated = start + (end - start) * factor;
    
    result.translate(interpolated.x, interpolated.y, interpolated.z);
    return result;
}

QMatrix4x4 GLBViewer::interpolateRotation(const aiQuatKey* keys, unsigned int numKeys, float time)
{
    QMatrix4x4 result;
    result.setToIdentity();
    
    if (numKeys == 0) return result;
    if (numKeys == 1) {
        QQuaternion q(keys[0].mValue.w, keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z);
        result.rotate(q);
        return result;
    }
    
    // Find keys to interpolate between (binary search for better performance)
    // Handle edge cases first
    if (time <= keys[0].mTime) {
        QQuaternion q(keys[0].mValue.w, keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z);
        result.rotate(q);
        return result;
    }
    if (time >= keys[numKeys - 1].mTime) {
        QQuaternion q(keys[numKeys - 1].mValue.w, keys[numKeys - 1].mValue.x, keys[numKeys - 1].mValue.y, keys[numKeys - 1].mValue.z);
        result.rotate(q);
        return result;
    }
    
    // Binary search for the correct keyframe pair
    unsigned int index = 0;
    unsigned int left = 0;
    unsigned int right = numKeys - 1;
    while (right - left > 1) {
        unsigned int mid = (left + right) / 2;
        if (time < keys[mid].mTime) {
            right = mid;
        } else {
            left = mid;
        }
    }
    index = left;
    
    float deltaTime = keys[index + 1].mTime - keys[index].mTime;
    float factor = deltaTime > 0 ? (time - keys[index].mTime) / deltaTime : 0.0f;
    factor = qBound(0.0f, factor, 1.0f);
    
    aiQuaternion start = keys[index].mValue;
    aiQuaternion end = keys[index + 1].mValue;
    aiQuaternion interpolated;
    aiQuaternion::Interpolate(interpolated, start, end, factor);
    
    QQuaternion q(interpolated.w, interpolated.x, interpolated.y, interpolated.z);
    result.rotate(q);
    return result;
}

QMatrix4x4 GLBViewer::interpolateScaling(const aiVectorKey* keys, unsigned int numKeys, float time)
{
    QMatrix4x4 result;
    result.setToIdentity();
    
    if (numKeys == 0) return result;
    if (numKeys == 1) {
        result.scale(keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z);
        return result;
    }
    
    // Find keys to interpolate between (binary search for better performance)
    // Handle edge cases first
    if (time <= keys[0].mTime) {
        result.scale(keys[0].mValue.x, keys[0].mValue.y, keys[0].mValue.z);
        return result;
    }
    if (time >= keys[numKeys - 1].mTime) {
        result.scale(keys[numKeys - 1].mValue.x, keys[numKeys - 1].mValue.y, keys[numKeys - 1].mValue.z);
        return result;
    }
    
    // Binary search for the correct keyframe pair
    unsigned int index = 0;
    unsigned int left = 0;
    unsigned int right = numKeys - 1;
    while (right - left > 1) {
        unsigned int mid = (left + right) / 2;
        if (time < keys[mid].mTime) {
            right = mid;
        } else {
            left = mid;
        }
    }
    index = left;
    
    float deltaTime = keys[index + 1].mTime - keys[index].mTime;
    float factor = deltaTime > 0 ? (time - keys[index].mTime) / deltaTime : 0.0f;
    factor = qBound(0.0f, factor, 1.0f);
    
    aiVector3D start = keys[index].mValue;
    aiVector3D end = keys[index + 1].mValue;
    aiVector3D interpolated = start + (end - start) * factor;
    
    result.scale(interpolated.x, interpolated.y, interpolated.z);
    return result;
}

void GLBViewer::cleanup()
{
    makeCurrent();
    
    // Stop animation
    if (m_animationTimer) {
        m_animationTimer->stop();
        m_animationTimer->deleteLater();
        m_animationTimer = nullptr;
    }
    m_animationPlaying = false;
    m_animations.clear();
    m_currentAnimationIndex = -1;
    m_animationTime = 0.0f;
    
    // Delete all OpenGL resources
    for (auto& mesh : m_meshes) {
        if (mesh) {
            // Delete textures
            if (mesh->hasTexture && mesh->diffuseTexture > 0) {
                glDeleteTextures(1, &mesh->diffuseTexture);
                mesh->diffuseTexture = 0;
            }
            
            // Delete VAO, VBO, IBO
            if (mesh->vao) {
                mesh->vao->destroy();
                delete mesh->vao;
                mesh->vao = nullptr;
            }
            if (mesh->vertexBuffer) {
                mesh->vertexBuffer->destroy();
                delete mesh->vertexBuffer;
                mesh->vertexBuffer = nullptr;
            }
            if (mesh->indexBuffer) {
                mesh->indexBuffer->destroy();
                delete mesh->indexBuffer;
                mesh->indexBuffer = nullptr;
            }
            if (mesh->boneIndexBuffer) {
                mesh->boneIndexBuffer->destroy();
                delete mesh->boneIndexBuffer;
                mesh->boneIndexBuffer = nullptr;
            }
            if (mesh->boneWeightBuffer) {
                mesh->boneWeightBuffer->destroy();
                delete mesh->boneWeightBuffer;
                mesh->boneWeightBuffer = nullptr;
            }
        }
    }
    
    // Clear the vector (unique_ptr will automatically delete MeshData objects)
    m_meshes.clear();
    m_scene = nullptr;
    m_importerHolder.reset();
    
    // Reset camera
    m_cameraDistance = 5.0f;
    m_cameraRotationX = 0.0f;
    m_cameraRotationY = 0.0f;
    m_cameraTarget = QVector3D(0, 0, 0);
    m_modelCenter = QVector3D(0, 0, 0);
    m_modelRadius = 1.0f;
    
    // Reset flags
    m_cameraChanged = true;
    m_projectionChanged = true;
    
    if (m_shaderProgram) {
        m_shaderProgram->release();
    }
    
    doneCurrent();
}


