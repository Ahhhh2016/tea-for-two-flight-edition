#pragma once

// Defined before including GLEW to suppress deprecation messages on macOS
#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#endif
#include <GL/glew.h>
#include <glm/glm.hpp>

#include <unordered_map>
#include <QElapsedTimer>
#include <QOpenGLWidget>
#include <QTime>
#include <QTimer>

// Student-added includes
#include <vector>
#include "utils/sceneparser.h"
#include "utils/Camera.h"

enum class SceneRenderMode {
    FullscreenProcedural,
    PlanetGeometryScene,
    GeometryScene
};

class Realtime : public QOpenGLWidget
{
public:
    Realtime(QWidget *parent = nullptr);
    void finish();                                      // Called on program exit
    void sceneChanged(bool preserveCamera = false);
    void settingsChanged();
    void saveViewportImage(std::string filePath);

public slots:
    void tick(QTimerEvent* event);                      // Called once per tick of m_timer

protected:
    void initializeGL() override;                       // Called once at the start of the program
    void paintGL() override;                            // Called whenever the OpenGL context changes or by an update() request
    void resizeGL(int width, int height) override;      // Called when window size changes

private:
    void rebuildGeometryFromRenderData();               // Rebuild VBO/VAO from current m_render without resetting camera
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void timerEvent(QTimerEvent *event) override;
    void buildPlanetScene();
    void runGeometryPass(GLint &prevFBO, glm::mat4 &V, glm::mat4 &P);
    void renderGeometryScene();
    void renderPlanetScene();
    void renderFullscreenProcedural();
    SceneRenderMode computeRenderMode() const;

    // Tick Related Variables
    int m_timer;                                        // Stores timer which attempts to run ~60 times per second
    QElapsedTimer m_elapsedTimer;                       // Stores timer which keeps track of actual time between frames

    // Input Related Variables
    bool m_mouseDown = false;                           // Stores state of left mouse button
    glm::vec2 m_prev_mouse_pos;                         // Stores mouse position
    std::unordered_map<Qt::Key, bool> m_keyMap;         // Stores whether keys are pressed or not

    // Device Correction Variables
    double m_devicePixelRatio;

    // Student-added rendering state
    struct DrawItem {
        int first;              // starting vertex index
        int count;              // vertex count
        glm::mat4 model;        // model matrix (CTM)
        glm::mat4 invModel;     // inverse model matrix
        glm::mat3 normalMat;    // normal matrix = mat3(transpose(inverse(model)))
        glm::mat4 prevModel;    // previous frame's model matrix (for motion blur)
        glm::vec3 kd;
        glm::vec3 ka;
        glm::vec3 ks;
        float shininess;

        // Texture mapping
        bool hasTexture = false;
        GLuint texture = 0;
        glm::vec2 texRepeat = glm::vec2(1.f);
        float blend = 1.f;
        // Planet scene
        bool isPlanet = false;
        bool isSand = false;
        glm::vec3 planetColorA; //light band color
        glm::vec3 planetColorB; //dark band color
    };

    GLuint m_prog = 0;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLsizei m_vertexCount = 0;
    std::vector<DrawItem> m_draws;
    RenderData m_render;
    Camera m_camera;
    Camera m_cameraWater;                                // Independent camera for Water scene
    // Cache for textures by absolute file path
    std::unordered_map<std::string, GLuint> m_textureCache;
    // LOD rebuild tracking
    glm::vec3 m_lastLodCamPos = glm::vec3(0.f);
    bool m_hasLastLodCamPos = false;
    float m_lodRebuildDistanceThreshold = 0.25f;        // world units to trigger LOD rebuild

    // Offscreen rendering (for post-processing)
    GLuint m_sceneFBO = 0;
    GLuint m_sceneColorTex = 0;
    GLuint m_sceneDepthTex = 0;
    GLuint m_sceneVelocityTex = 0;
    GLuint m_sceneNormalTex = 0;
    GLuint m_skyTex = 0;

    // Screen-quad for post-processing
    GLuint m_postProg = 0;          // DoF
    GLuint m_postProgMotion = 0;    // Motion blur
    GLuint m_postProgDepth = 0;     // Depth debug
    GLuint m_postProgIQ = 0;        // Shadertoy rainforest full-screen shader
	GLuint m_postProgWater = 0;     // Water full-screen shader
    GLuint m_postProgToon = 0;      // Toon shader
    GLuint m_postProgDirectional = 0; // Simple screen directional blur (fullscreen IQ sprint)

    GLuint m_screenVAO = 0;
    GLuint m_screenVBO = 0;

	// Renders the Planet scene into the portal FBO at portal resolution
	void renderPlanetIntoPortalFBO();

    // Portal rendering (Scene B -> texture, composited into Scene A)
    bool   m_portalEnabled = false;
    GLuint m_portalFBO = 0;
    GLuint m_portalColorTex = 0;
    int    m_portalWidth = 0;
    int    m_portalHeight = 0;
    GLuint m_portalProg = 0;    // simple textured quad shader
    GLuint m_portalVAO = 0;
    GLuint m_portalVBO = 0;
	glm::mat4 m_portalModel = glm::mat4(1.f); // world transform of the portal quad (XY plane)
	float     m_portalHalfSize = 0.5f;        // half-extent in local X/Y
	bool      m_portalAutoCenterY = false;    // if true, keep portal centered at active camera's Y every frame

    // Portal traversal state
    float m_portalDepthMax = 1.0f;    // virtual distance to traverse through portal
    float m_portalDepth    = 1.0f;    // current remaining distance
    float m_portalCooldownSec   = 0.4f; // time window to prevent rapid toggles
    float m_portalCooldownTimer = 0.0f; // countdown

    // Cached framebuffer size for textures
    int m_fbWidth = 0;
    int m_fbHeight = 0;

    // Fullscreen offscreen target for IQ when applying sprint blur
    GLuint m_fullscreenFBO = 0;
    GLuint m_fullscreenColorTex = 0;

    // Sprint speed accumulation and live speed
    float m_moveSpeedBase = 5.0f;          // base units/sec
    float m_sprintAccum = 0.0f;            // 0..m_sprintAccumMax
    float m_sprintAccumMax = 2.0f;         // +200% -> 3x base
	float m_sprintAccelPerSec = 1.5f;     // accumulation rate while holding sprint (slower ramp)
    float m_sprintDecayPerSec = 2.0f;      // decay rate when not holding sprint
    float m_currentSpeedUnits = 5.0f;      // last computed current speed (units/sec)
	// Shift-hold gating for motion blur
	float m_shiftHoldSec = 0.0f;           // seconds Shift has been held continuously
	bool  m_sprintBlurUnlocked = false;    // becomes true after holding Shift >= threshold; resets when speed returns to base
	float m_shiftHoldRamp01 = 0.0f;        // ramp factor 0..1 during first 2s hold for smooth blur-in

    // Previous camera matrices (for motion blur reprojection)
    glm::mat4 m_prevV = glm::mat4(1.f);
    glm::mat4 m_prevP = glm::mat4(1.f);

    // Debug toggles
    bool m_debugDepth = false;

    // Time and frame counters for iTime/iFrame style shaders
    float m_timeSec = 0.f;
    int   m_frameCount = 0;

    // Helpers for offscreen rendering and post-processing
    void createOrResizeSceneFBO(int width, int height);
    void releaseSceneFBO();
    void createScreenQuad();
    void releaseScreenQuad();
    // Portal helpers
    void createOrResizePortalFBO(int width, int height);
    void releasePortalFBO();
    void createPortalQuad();
    void releasePortalQuad();
    // Fullscreen helpers for IQ sprint blur
    void createOrResizeFullscreenFBO(int width, int height);
    void releaseFullscreenFBO();
};
