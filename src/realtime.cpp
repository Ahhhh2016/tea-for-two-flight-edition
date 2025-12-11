#include "realtime.h"

#include <QCoreApplication>
#include <QMouseEvent>
#include <QKeyEvent>
#include <iostream>
#include "settings.h"
// Student-added includes
#include "utils/shaderloader.h"
#include "utils/sceneparser.h"
#include "utils/ObjLoader.h"
#include "shapes/Cube.h"
#include "shapes/ShapeFactory.h"
#include "terraingenerator.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform.hpp>
#include <QImage>
#include <cmath>
#include <algorithm>
// ================== Rendering the Scene!

Realtime::Realtime(QWidget *parent)
    : QOpenGLWidget(parent)
{
    m_prev_mouse_pos = glm::vec2(size().width()/2, size().height()/2);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    m_keyMap[Qt::Key_W]       = false;
    m_keyMap[Qt::Key_A]       = false;
    m_keyMap[Qt::Key_S]       = false;
    m_keyMap[Qt::Key_D]       = false;
    m_keyMap[Qt::Key_Alt]     = false;
    m_keyMap[Qt::Key_Space]   = false;
    m_keyMap[Qt::Key_Shift]   = false;

    // If you must use this function, do not edit anything above this
    // Initialize Water camera with sensible defaults (independent from rainforest camera)
    {
        m_cameraWater.setPosition(glm::vec3(0.f, 1.5f, 4.f));
        m_cameraWater.setLook(glm::normalize(glm::vec3(0.f, 0.f, -1.f)));
        m_cameraWater.setUp(glm::vec3(0.f, 1.f, 0.f));
        m_cameraWater.setFovYRadians(glm::radians(60.f));
        float aspect = size().height() > 0 ? float(size().width()) / float(size().height()) : 1.f;
        m_cameraWater.setAspectRatio(aspect);
        m_cameraWater.setClipPlanes(0.1f, 1000.f);
    }
}

void Realtime::finish() {
    killTimer(m_timer);
    this->makeCurrent();

    // Students: anything requiring OpenGL calls when the program exits should be done here

    releaseSceneFBO();
    releaseFullscreenFBO();
    releaseScreenQuad();
    if (m_postProg) {
        glDeleteProgram(m_postProg);
        m_postProg = 0;
    }
    if (m_postProgMotion) {
        glDeleteProgram(m_postProgMotion);
        m_postProgMotion = 0;
    }
    if (m_postProgDepth) {
        glDeleteProgram(m_postProgDepth);
        m_postProgDepth = 0;
    }
    if (m_postProgIQ) {
        glDeleteProgram(m_postProgIQ);
        m_postProgIQ = 0;
    }
	if (m_postProgWater) {
		glDeleteProgram(m_postProgWater);
		m_postProgWater = 0;
	}
    if (m_postProgDirectional) {
        glDeleteProgram(m_postProgDirectional);
        m_postProgDirectional = 0;
    }
    if (m_portalProg) {
        glDeleteProgram(m_portalProg);
        m_portalProg = 0;
    }
    if (m_postProgToon) {
        glDeleteProgram(m_postProgToon);
        m_postProgToon = 0;
    }
    if (m_skyTex) {
        glDeleteTextures(1, &m_skyTex);
        m_skyTex = 0;
    }
    if (m_shadowDepthTex) {
        glDeleteTextures(1, &m_shadowDepthTex);
        m_shadowDepthTex = 0;
    }
    if (m_shadowFBO) {
        glDeleteFramebuffers(1, &m_shadowFBO);
        m_shadowFBO = 0;
    }
    if (m_shadowShader) {
        glDeleteProgram(m_shadowShader);
        m_shadowShader = 0;
    }
    releasePortalQuad();
    releasePortalFBO();

    if (m_vbo) {
        glDeleteBuffers(1, &m_vbo);
        m_vbo = 0;
    }
    if (m_vao) {
        glDeleteVertexArrays(1, &m_vao);
        m_vao = 0;
    }
    if (m_prog) {
        glDeleteProgram(m_prog);
        m_prog = 0;
    }

    for (auto &kv : m_textureCache) { if (kv.second) { glDeleteTextures(1, &kv.second); } }
    m_textureCache.clear();

    this->doneCurrent();
}

void Realtime::initializeGL() {
    m_devicePixelRatio = this->devicePixelRatio();

    m_timer = startTimer(1000/60);
    m_elapsedTimer.start();

    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "Error while initializing GL: " << glewGetErrorString(err) << std::endl;
    }
    std::cout << "Initialized GL: Version " << glewGetString(GLEW_VERSION) << std::endl;

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Set clear color to blue-white fog color
    glClearColor(0.85f, 0.9f, 1.0f, 1.f);
    // Set viewport to the entire window
    glViewport(0, 0, size().width() * m_devicePixelRatio, size().height() * m_devicePixelRatio);

    // Students: anything requiring OpenGL calls when the program starts should be done here
    // Compile shaders and create VAO/VBO
    try {
        // Use Qt resource path provided by qt6_add_resources
        m_prog = ShaderLoader::createShaderProgram(":/resources/shaders/default.vert",
                                                   ":/resources/shaders/default.frag");
        m_postProg = ShaderLoader::createShaderProgram(":/resources/shaders/post.vert",
                                                       ":/resources/shaders/dof.frag");
        m_postProgMotion = ShaderLoader::createShaderProgram(":/resources/shaders/post.vert",
                                                             ":/resources/shaders/motionblur.frag");
        m_postProgDepth = ShaderLoader::createShaderProgram(":/resources/shaders/post.vert",
                                                            ":/resources/shaders/debug_depth.frag");
        // Rainforest fullscreen shader (IQ)
        m_postProgIQ = ShaderLoader::createShaderProgram(":/resources/shaders/post.vert",
                                                         ":/resources/shaders/iq_rainforest.frag");
		// Water fullscreen shader
		m_postProgWater = ShaderLoader::createShaderProgram(":/resources/shaders/post.vert",
															":/resources/shaders/water.frag");
        // Simple directional blur for fullscreen IQ sprint
        m_postProgDirectional = ShaderLoader::createShaderProgram(":/resources/shaders/post.vert",
                                                                  ":/resources/shaders/directional_blur.frag");
        // Portal compositing shader
        m_portalProg = ShaderLoader::createShaderProgram(":/resources/shaders/portal.vert",
                                                         ":/resources/shaders/portal.frag");
        // Toon shader
        m_postProgToon = ShaderLoader::createShaderProgram(
            ":/resources/shaders/post.vert",
            ":/resources/shaders/toon.frag"
            );
        // Shadow
        m_shadowShader = ShaderLoader::createShaderProgram(
            ":/resources/shaders/shadow.vert",
            ":/resources/shaders/shadow.frag"
            );

    } catch (const std::exception &e) {
        std::cerr << "Shader error: " << e.what() << std::endl;
        if (m_prog == 0) m_prog = 0;
        if (m_postProg == 0) m_postProg = 0;
        if (m_postProgMotion == 0) m_postProgMotion = 0;
        if (m_postProgDepth == 0) m_postProgDepth = 0;
		if (m_postProgIQ == 0) m_postProgIQ = 0;
		if (m_postProgWater == 0) m_postProgWater = 0;
        if (m_postProgDirectional == 0) m_postProgDirectional = 0;
        if (m_portalProg == 0) m_portalProg = 0;
        if (m_postProgToon == 0) m_postProgToon = 0;
    }
    // Load sky texture for Planet/Toon mode
    {
        QImage img(":/resources/images/sky1.png");
        if (!img.isNull()) {
            QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);

            glGenTextures(1, &m_skyTex);
            glBindTexture(GL_TEXTURE_2D, m_skyTex);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                         rgba.width(), rgba.height(),
                         0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.constBits());
            glGenerateMipmap(GL_TEXTURE_2D);

            glBindTexture(GL_TEXTURE_2D, 0);
        } else {
            std::cerr << "Failed to load sky texture!" << std::endl;
        }
    }


    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));

    // Build initial scene (terrain if no scenefile loaded)
    sceneChanged(true);

    // Create post resources after initial viewport is known
    int fbw = size().width() * m_devicePixelRatio;
    int fbh = size().height() * m_devicePixelRatio;
    createOrResizeSceneFBO(fbw, fbh);
    createScreenQuad();
    createPortalQuad();
    createOrResizePortalFBO(fbw, fbh);
    createOrResizeFullscreenFBO(fbw, fbh);
    makeShadowMapFBO();
	// Place portal in world: at y=1.2, z=0 facing +Z (camera starts at z=5 looking -Z)
	m_portalModel = glm::translate(glm::mat4(1.f), glm::vec3(0.f, 1.2f, 0.f));
    // Lift camera slightly at start to avoid Water-in-portal artifacts when too low
    {
        const float moveSpeed = 10.0f;        // keep in sync with Space/W nudges
        const float assumedDt = 1.0f / 60.0f; // one "frame" worth
        glm::vec3 p = m_camera.getPosition();
        p += glm::vec3(0.f, moveSpeed * assumedDt, 0.f);
        m_camera.setPosition(p);
    }
    // If starting in IQ fullscreen mode (no scenefile), place camera farther back like IQ's original vantage
    if (settings.sceneFilePath.empty() && settings.fullscreenScene == FullscreenScene::IQ) {
        const glm::vec3 ro(0.f, 401.5f, 6.f);
        const glm::vec3 ta(0.f, 403.5f, -84.0f); // -90 + ro.z
        const glm::vec3 look = glm::normalize(ta - ro);
        m_camera.setPosition(ro);
        m_camera.setLook(look);
        m_camera.setUp(glm::vec3(0.f, 1.f, 0.f));
        // Keep portal centered at the camera's height so it's visible in front
        // Facing +Z, located at z=0 like before
        m_portalModel = glm::translate(glm::mat4(1.f), glm::vec3(0.f, ro.y, 0.f));
    }

    // Initialize previous camera matrices
    m_prevV = m_camera.getViewMatrix();
    m_prevP = m_camera.getProjectionMatrix();

    // Initialize time/frame counters for IQ shader
    m_timeSec = 0.f;
    m_frameCount = 0;
}
void Realtime::makeShadowMapFBO() {
    if (m_shadowFBO == 0) {
        glGenFramebuffers(1, &m_shadowFBO);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);

    if (m_shadowDepthTex == 0) {
        glGenTextures(1, &m_shadowDepthTex);
    }
    glBindTexture(GL_TEXTURE_2D, m_shadowDepthTex);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_DEPTH_COMPONENT24,
                 m_shadowRes,
                 m_shadowRes,
                 0,
                 GL_DEPTH_COMPONENT,
                 GL_FLOAT,
                 nullptr);

    // sampler params
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // for shadow edges
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderCol[4] = {1.f, 1.f, 1.f, 1.f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderCol);

    // Attach as depth-only FBO
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D,
                           m_shadowDepthTex,
                           0);

    // No color buffer
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Shadow FBO incomplete: 0x"
                  << std::hex << status << std::dec << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
void Realtime::updateShadowLightSelection() {
    m_hasShadowLight = false;
    m_shadowLightIndex = -1;

    // pick the first directional light
    for (int i = 0; i < (int)m_render.lights.size(); ++i) {
        const auto &L = m_render.lights[i];
        if (L.type == LightType::LIGHT_DIRECTIONAL) {
            m_hasShadowLight = true;
            m_shadowLightIndex = i;
            return;
        }
    }

    // Fallback: if no directional, pick any light or just leave it off
    if (!m_render.lights.empty()) {
        m_hasShadowLight = true;
        m_shadowLightIndex = 0;
    }
}
void Realtime::updateLightViewProj() {
    if (!m_hasShadowLight ||
        m_shadowLightIndex < 0 ||
        m_shadowLightIndex >= (int)m_render.lights.size()) {
        return;
    }

    const SceneLightData &L = m_render.lights[m_shadowLightIndex];

    // Use the light direction to build a view matrix.
    glm::vec3 lightDir = glm::normalize(glm::vec3(L.dir));
    if (glm::length(lightDir) < 1e-4f) {
        lightDir = glm::normalize(glm::vec3(0.3f, -1.0f, 0.2f));
    }

    // Place light some distance "upstream" along this direction.
    glm::vec3 target(0.f, 0.f, 0.f); // focus around origin / terrain region
    glm::vec3 lightPos = target - lightDir * 30.0f;

    glm::mat4 lightView = glm::lookAt(lightPos,
                                      target,
                                      glm::vec3(0.f, 1.f, 0.f));

    // Orthographic frustum that covers planet/terrain region
    float orthoSize = 25.0f;
    float nearPlane = 1.0f;
    float farPlane  = 80.0f;

    glm::mat4 lightProj = glm::ortho(-orthoSize, orthoSize,
                                     -orthoSize, orthoSize,
                                     nearPlane,  farPlane);

    m_lightViewProj = lightProj * lightView;
}

void Realtime::renderShadowMap() {
    if (!m_hasShadowLight) return;
    if (m_shadowShader == 0 || m_shadowFBO == 0 || m_shadowDepthTex == 0) return;

    // Save state
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);

    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    GLint prevProgram = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);

    // Shadow pass
    glBindFramebuffer(GL_FRAMEBUFFER, m_shadowFBO);
    glViewport(0, 0, m_shadowRes, m_shadowRes);

    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_shadowShader);

    // Uniform locations
    GLint locM          = glGetUniformLocation(m_shadowShader, "u_M");
    GLint locLightVP    = glGetUniformLocation(m_shadowShader, "u_lightViewProj");
    GLint locTime       = glGetUniformLocation(m_shadowShader, "u_time");

    GLint locIsMoon     = glGetUniformLocation(m_shadowShader, "u_isMoon");
    GLint locMoonCenter = glGetUniformLocation(m_shadowShader, "u_moonCenter");
    GLint locOrbitSpeed = glGetUniformLocation(m_shadowShader, "u_orbitSpeed");
    GLint locOrbitPhase = glGetUniformLocation(m_shadowShader, "u_orbitPhase");

    GLint locIsCube     = glGetUniformLocation(m_shadowShader, "u_isFloatingCube");
    GLint locFloatSpeed = glGetUniformLocation(m_shadowShader, "u_floatSpeed");
    GLint locFloatAmp   = glGetUniformLocation(m_shadowShader, "u_floatAmp");
    GLint locFloatPhase = glGetUniformLocation(m_shadowShader, "u_floatPhase");

    // Global uniforms
    if (locLightVP >= 0) glUniformMatrix4fv(
            locLightVP, 1, GL_FALSE, glm::value_ptr(m_lightViewProj));
    if (locTime >= 0)    glUniform1f(locTime, m_timeSec);

    glBindVertexArray(m_vao);

    for (const DrawItem &d : m_draws) {
        if (locM >= 0) glUniformMatrix4fv(
                locM, 1, GL_FALSE, glm::value_ptr(d.model));

        if (locIsMoon >= 0)     glUniform1i(locIsMoon, d.isMoon);
        if (locMoonCenter >= 0) glUniform3fv(locMoonCenter, 1, glm::value_ptr(d.moonCenter));
        if (locOrbitSpeed >= 0) glUniform1f(locOrbitSpeed, d.orbitSpeed);
        if (locOrbitPhase >= 0) glUniform1f(locOrbitPhase, d.orbitPhase);

        if (locIsCube >= 0)     glUniform1i(locIsCube, d.isFloatingCube);
        if (locFloatSpeed >= 0) glUniform1f(locFloatSpeed, d.floatSpeed);
        if (locFloatAmp >= 0)   glUniform1f(locFloatAmp,   d.floatAmp);
        if (locFloatPhase >= 0) glUniform1f(locFloatPhase, d.floatPhase);

        glDrawArrays(GL_TRIANGLES, d.first, d.count);
    }

    glBindVertexArray(0);

    // Restore state
    glUseProgram(prevProgram);
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevViewport[0], prevViewport[1],
               prevViewport[2], prevViewport[3]);
}


SceneRenderMode Realtime::computeRenderMode() const {
    // Fullscreen IQ or Water → procedural shader
    if (settings.sceneFilePath.empty() &&
        (settings.fullscreenScene == FullscreenScene::IQ ||
         settings.fullscreenScene == FullscreenScene::Water)) {
        return SceneRenderMode::FullscreenProcedural;
    }

    // Fullscreen Planet → geometry, but type = planet
    if (settings.sceneFilePath.empty() &&
        settings.fullscreenScene == FullscreenScene::Planet) {
        return SceneRenderMode::PlanetGeometryScene;
    }

    // If there is a scene file:
    return SceneRenderMode::GeometryScene;
}

void Realtime::renderFullscreenProcedural(){
    bool fullscreenProcedural = settings.sceneFilePath.empty() &&
                                (settings.fullscreenScene == FullscreenScene::IQ ||
                                 settings.fullscreenScene == FullscreenScene::Water);

    // Fullscreen shader mode (used when no scene file is loaded)
    if (fullscreenProcedural) {
        // Portal path: IQ as Scene A + Water as Scene B
        bool portalActive = (settings.fullscreenScene == FullscreenScene::IQ) &&
                            m_portalEnabled &&
                            (m_postProgIQ != 0) && (m_postProgWater != 0) && (m_portalProg != 0) &&
                            (m_screenVAO != 0) && (m_portalFBO != 0) && (m_portalColorTex != 0);

        GLint prevFBO = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
        int outW = size().width() * m_devicePixelRatio;
        int outH = size().height() * m_devicePixelRatio;

        if (!portalActive) {
            GLuint prog = 0;
            if (settings.fullscreenScene == FullscreenScene::IQ) {
                prog = m_postProgIQ;
            } else if (settings.fullscreenScene == FullscreenScene::Water) {
                prog = m_postProgWater;
            }
            if (prog != 0) {
                // Compute speed-based blur activation and strength (persists while speed decays)
                float maxSpeedUI = m_moveSpeedBase * (1.0f + m_sprintAccumMax);
                float speedFracUI = 0.0f;
                if (maxSpeedUI > m_moveSpeedBase) {
                    speedFracUI = std::max(0.0f, std::min((m_currentSpeedUnits - m_moveSpeedBase) / (maxSpeedUI - m_moveSpeedBase), 1.0f));
                }
                // Delayed ramp: 0 for first 2s; ramp 0..1 over 2..4s while holding.
                // After unlock and on release, use full scale (1.0) with speed-decay persistence.
                bool shiftDownUI = m_keyMap[Qt::Key_Shift];
                float blurScaleUI = shiftDownUI ? m_shiftHoldRamp01 : (m_sprintBlurUnlocked ? 1.0f : 0.0f);
                float blurPixelsUI = 10.0f * speedFracUI * blurScaleUI; // 0..10px scaled by ramp/permit
                int numSamplesUI = 7 + int(10.0f * speedFracUI * blurScaleUI); // 7..17, ensure odd below
                if ((numSamplesUI % 2) == 0) numSamplesUI += 1;
                bool blurActiveIQ = (settings.fullscreenScene == FullscreenScene::IQ) &&
                                    (blurPixelsUI > 0.0f) &&
                                    m_postProgDirectional && m_fullscreenFBO && m_fullscreenColorTex;
                if (blurActiveIQ) {
                    // 1) Render IQ to fullscreen offscreen texture
                    glBindFramebuffer(GL_FRAMEBUFFER, m_fullscreenFBO);
                    glViewport(0, 0, outW, outH);
                    const float clearC[4] = {0.f, 0.f, 0.f, 1.f};
                    glClearBufferfv(GL_COLOR, 0, clearC);
                    glDisable(GL_DEPTH_TEST);
                    glUseProgram(m_postProgIQ);
                    glBindVertexArray(m_screenVAO);
                    GLint locRes  = glGetUniformLocation(m_postProgIQ, "iResolution");
                    GLint locTime = glGetUniformLocation(m_postProgIQ, "iTime");
                    GLint locFrame = glGetUniformLocation(m_postProgIQ, "iFrame");
                    if (locRes  >= 0) glUniform3f(locRes,  float(outW), float(outH), 1.0f);
                    if (locTime >= 0) glUniform1f(locTime, m_timeSec);
                    if (locFrame >= 0) glUniform1i(locFrame, m_frameCount);
                    // IQ camera/light uniforms
                    GLint locCamPos  = glGetUniformLocation(m_postProgIQ, "u_camPos");
                    GLint locCamLook = glGetUniformLocation(m_postProgIQ, "u_camLook");
                    GLint locCamUp   = glGetUniformLocation(m_postProgIQ, "u_camUp");
                    GLint locFovY    = glGetUniformLocation(m_postProgIQ, "u_camFovY");
                    GLint locCamTarget = glGetUniformLocation(m_postProgIQ, "u_camTarget");
                    if (locCamPos >= 0 || locCamLook >= 0 || locCamUp >= 0 || locFovY >= 0 || locCamTarget >= 0) {
                        glm::vec3 camPos  = m_camera.getPosition();
                        glm::vec3 camLook = m_camera.getLook();
                        glm::vec3 camUp   = m_camera.getUp();
                        float fovY        = 2.f * std::atan(1.f / 1.5f);
                        glm::vec3 camTarget = camPos + glm::normalize(camLook);
                        if (locCamPos  >= 0) glUniform3f(locCamPos,  camPos.x,  camPos.y,  camPos.z);
                        if (locCamLook >= 0) glUniform3f(locCamLook, camLook.x, camLook.y, camLook.z);
                        if (locCamUp   >= 0) glUniform3f(locCamUp,   camUp.x,   camUp.y,   camUp.z);
                        if (locFovY    >= 0) glUniform1f(locFovY,    fovY);
                        if (locCamTarget >= 0) glUniform3f(locCamTarget, camTarget.x, camTarget.y, camTarget.z);
                    }
                    GLint locSunDir = glGetUniformLocation(m_postProgIQ, "u_sunDir");
                    GLint locExposure = glGetUniformLocation(m_postProgIQ, "u_exposure");
                    if (locSunDir >= 0) {
                        const glm::vec3 sunDir(-0.624695f, 0.468521f, -0.624695f);
                        glUniform3f(locSunDir, sunDir.x, sunDir.y, sunDir.z);
                    }
                    if (locExposure >= 0) {
                        glUniform1f(locExposure, 1.0f);
                    }
                    // Rainforest intensity
                    {
                        GLint locIntensity = glGetUniformLocation(m_postProgIQ, "u_rainforestIntensity");
                        if (locIntensity >= 0) glUniform1f(locIntensity, settings.rainforestIntensity);
                    }
                    glDrawArrays(GL_TRIANGLES, 0, 6);

                // 2) Apply directional blur to screen
                    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
                    if (prevFBO == 0) {
                        glViewport(0, 0, outW, outH);
                    }
                    glDisable(GL_DEPTH_TEST);
                    glUseProgram(m_postProgDirectional);
                    glBindVertexArray(m_screenVAO);
                    GLint locColor = glGetUniformLocation(m_postProgDirectional, "u_colorTex");
                    GLint locTexel = glGetUniformLocation(m_postProgDirectional, "u_texelSize");
                    GLint locDir   = glGetUniformLocation(m_postProgDirectional, "u_directionUV");
                    GLint locPx    = glGetUniformLocation(m_postProgDirectional, "u_blurPixels");
                    GLint locNS    = glGetUniformLocation(m_postProgDirectional, "u_numSamples");
                    if (locColor >= 0) glUniform1i(locColor, 0);
                    if (locTexel >= 0) glUniform2f(locTexel, 1.0f / float(outW), 1.0f / float(outH));
                // Use a simple horizontal blur direction
                if (locDir >= 0) glUniform2f(locDir, 1.0f, 0.0f);
                    if (locPx  >= 0) glUniform1f(locPx, blurPixelsUI);
                    if (locNS  >= 0) glUniform1i(locNS, numSamplesUI);
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, m_fullscreenColorTex);
                    glDrawArrays(GL_TRIANGLES, 0, 6);

                    m_frameCount++;
                    return;
                } else {
                    if (prevFBO == 0) {
                        glViewport(0, 0, outW, outH);
                    }
                    glDisable(GL_DEPTH_TEST);
                    glUseProgram(prog);
                    glBindVertexArray(m_screenVAO);
                    // Common uniforms
                    GLint locRes  = glGetUniformLocation(prog, "iResolution");
                    GLint locTime = glGetUniformLocation(prog, "iTime");
                    if (locRes  >= 0) glUniform3f(locRes,  float(outW), float(outH), 1.0f);
                    if (locTime >= 0) glUniform1f(locTime, m_timeSec);
                    // iFrame (IQ only)
                    GLint locFrame = glGetUniformLocation(prog, "iFrame");
                    if (locFrame >= 0) glUniform1i(locFrame, m_frameCount);
                    // iMouse (water uses; safe if unused)
                    GLint locMouse = glGetUniformLocation(prog, "iMouse");
                    if (locMouse >= 0) {
                        float mouseX = m_prev_mouse_pos.x * float(m_devicePixelRatio);
                        float mouseY = (size().height() - m_prev_mouse_pos.y) * float(m_devicePixelRatio);
                        float clickX = m_mouseDown ? mouseX : 0.f;
                        float clickY = m_mouseDown ? mouseY : 0.f;
                        glUniform4f(locMouse, mouseX, mouseY, clickX, clickY);
                    }
                    // Camera uniforms: apply only for IQ shader
                    GLint locSunDir = glGetUniformLocation(prog, "u_sunDir");
                    GLint locExposure = glGetUniformLocation(prog, "u_exposure");
                    if (prog == m_postProgIQ) {
                        GLint locCamPos  = glGetUniformLocation(prog, "u_camPos");
                        GLint locCamLook = glGetUniformLocation(prog, "u_camLook");
                        GLint locCamUp   = glGetUniformLocation(prog, "u_camUp");
                        GLint locFovY    = glGetUniformLocation(prog, "u_camFovY");
                        GLint locCamTarget = glGetUniformLocation(prog, "u_camTarget");
                        if (locCamPos >= 0 || locCamLook >= 0 || locCamUp >= 0 || locFovY >= 0 || locCamTarget >= 0) {
                            glm::vec3 camPos  = m_camera.getPosition();
                            glm::vec3 camLook = m_camera.getLook();
                            glm::vec3 camUp   = m_camera.getUp();
                            float fovY        = 2.f * std::atan(1.f / 1.5f);
                            glm::vec3 camTarget = camPos + glm::normalize(camLook);
                            if (locCamPos  >= 0) glUniform3f(locCamPos,  camPos.x,  camPos.y,  camPos.z);
                            if (locCamLook >= 0) glUniform3f(locCamLook, camLook.x, camLook.y, camLook.z);
                            if (locCamUp   >= 0) glUniform3f(locCamUp,   camUp.x,   camUp.y,   camUp.z);
                            if (locFovY    >= 0) glUniform1f(locFovY,    fovY);
                            if (locCamTarget >= 0) glUniform3f(locCamTarget, camTarget.x, camTarget.y, camTarget.z);
                        }
                        // Rainforest intensity
                        GLint locIntensity = glGetUniformLocation(prog, "u_rainforestIntensity");
                        if (locIntensity >= 0) glUniform1f(locIntensity, settings.rainforestIntensity);
                    } else if (prog == m_postProgWater) {
                        // Upload Water camera (interactive when Water is fullscreen)
                        GLint locCamPosW  = glGetUniformLocation(prog, "u_camPos");
                        GLint locCamLookW = glGetUniformLocation(prog, "u_camLook");
                        GLint locCamUpW   = glGetUniformLocation(prog, "u_camUp");
                        GLint locFovYW    = glGetUniformLocation(prog, "u_camFovY");
                        if (locCamPosW >= 0 || locCamLookW >= 0 || locCamUpW >= 0 || locFovYW >= 0) {
                            const glm::vec3 camPosW  = m_cameraWater.getPosition();
                            const glm::vec3 camLookW = glm::normalize(m_cameraWater.getLook());
                            const glm::vec3 camUpW   = glm::normalize(m_cameraWater.getUp());
                            const float fovYW        = m_cameraWater.getFovYRadians();
                            if (locCamPosW  >= 0) glUniform3f(locCamPosW,  camPosW.x,  camPosW.y,  camPosW.z);
                            if (locCamLookW >= 0) glUniform3f(locCamLookW, camLookW.x, camLookW.y, camLookW.z);
                            if (locCamUpW   >= 0) glUniform3f(locCamUpW,   camUpW.x,   camUpW.y,   camUpW.z);
                            if (locFovYW    >= 0) glUniform1f(locFovYW,    fovYW);
                        }
                    }
                    if (locSunDir >= 0) {
                        // Original IQ rainforest sun direction
                        const glm::vec3 sunDir(-0.624695f, 0.468521f, -0.624695f);
                        glUniform3f(locSunDir, sunDir.x, sunDir.y, sunDir.z);
                    }
                    if (locExposure >= 0) {
                        // Match original brightness; let shader handle grading and gamma
                        glUniform1f(locExposure, 1.0f);
                    }
                    glDrawArrays(GL_TRIANGLES, 0, 6);
                    // If we're in Water and portal is enabled, composite portal showing Planet
                    if (settings.fullscreenScene == FullscreenScene::Water &&
                        m_portalEnabled &&
                        m_portalProg != 0 && m_portalVAO != 0 &&
                        m_portalFBO != 0 && m_portalColorTex != 0) {
                        // Render Planet into portal FBO
                        renderPlanetIntoPortalFBO();
                        // Back to default framebuffer for compositing
                        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
                        if (prevFBO == 0) glViewport(0, 0, outW, outH);
                        // Draw portal quad in world-space using Water camera matrices
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                        glUseProgram(m_portalProg);
                        GLint locSamp = glGetUniformLocation(m_portalProg, "u_portalTex");
                        GLint locAlpha = glGetUniformLocation(m_portalProg, "u_alpha");
                        GLint locM = glGetUniformLocation(m_portalProg, "u_M");
                        GLint locV = glGetUniformLocation(m_portalProg, "u_V");
                        GLint locP = glGetUniformLocation(m_portalProg, "u_P");

                        GLint locTime      = glGetUniformLocation(m_portalProg, "u_time");
                        GLint locRadius    = glGetUniformLocation(m_portalProg, "u_radius");
                        GLint locEdgeWidth = glGetUniformLocation(m_portalProg, "u_edgeWidth");
                        GLint locEdgeColor = glGetUniformLocation(m_portalProg, "u_edgeColor");

                        if (locSamp >= 0) glUniform1i(locSamp, 0);
                        if (locAlpha >= 0) glUniform1f(locAlpha, 1.0f);
                        if (locM >= 0) glUniformMatrix4fv(locM, 1, GL_FALSE, glm::value_ptr(m_portalModel));
                        if (locV >= 0) glUniformMatrix4fv(locV, 1, GL_FALSE, glm::value_ptr(m_cameraWater.getViewMatrix()));
                        if (locP >= 0) glUniformMatrix4fv(locP, 1, GL_FALSE, glm::value_ptr(m_cameraWater.getProjectionMatrix()));

                        if (locTime      >= 0) glUniform1f (locTime,      m_timeSec);
                        if (locRadius    >= 0) glUniform1f (locRadius,    0.8f);
                        if (locEdgeWidth >= 0) glUniform1f (locEdgeWidth, 0.08f);
                        if (locEdgeColor >= 0) glUniform3f (locEdgeColor, 0.5f, 0.9f, 1.2f);

                        glActiveTexture(GL_TEXTURE0);
                        glBindTexture(GL_TEXTURE_2D, m_portalColorTex);
                        glBindVertexArray(m_portalVAO);
                        glDrawArrays(GL_TRIANGLES, 0, 6);
                        glDisable(GL_BLEND);
                    }
                    m_frameCount++;
                    return;
                }
            }
        } else {
            // 1) Render Scene B (Water) into portal FBO
            glBindFramebuffer(GL_FRAMEBUFFER, m_portalFBO);
            glViewport(0, 0, m_portalWidth, m_portalHeight);
            const float clearC[4] = {0.f, 0.f, 0.f, 1.f};
            glClearBufferfv(GL_COLOR, 0, clearC);
            glDisable(GL_DEPTH_TEST);
            glUseProgram(m_postProgWater);
            glBindVertexArray(m_screenVAO);
            GLint locResB  = glGetUniformLocation(m_postProgWater, "iResolution");
            GLint locTimeB = glGetUniformLocation(m_postProgWater, "iTime");
            if (locResB  >= 0) glUniform3f(locResB,  float(m_portalWidth), float(m_portalHeight), 1.0f);
            if (locTimeB >= 0) glUniform1f(locTimeB, m_timeSec);
            GLint locMouseB = glGetUniformLocation(m_postProgWater, "iMouse");
            if (locMouseB >= 0) {
                float mouseX = m_prev_mouse_pos.x * float(m_devicePixelRatio);
                float mouseY = (size().height() - m_prev_mouse_pos.y) * float(m_devicePixelRatio);
                float clickX = m_mouseDown ? mouseX : 0.f;
                float clickY = m_mouseDown ? mouseY : 0.f;
                glUniform4f(locMouseB, mouseX, mouseY, clickX, clickY);
            }

			// Water camera for portal rendering (uses m_cameraWater)
            {
                GLint locCamPosW  = glGetUniformLocation(m_postProgWater, "u_camPos");
                GLint locCamLookW = glGetUniformLocation(m_postProgWater, "u_camLook");
                GLint locCamUpW   = glGetUniformLocation(m_postProgWater, "u_camUp");
                GLint locFovYW    = glGetUniformLocation(m_postProgWater, "u_camFovY");
                if (locCamPosW >= 0 || locCamLookW >= 0 || locCamUpW >= 0 || locFovYW >= 0) {
                    const glm::vec3 camPosW  = m_cameraWater.getPosition();
                    const glm::vec3 camLookW = glm::normalize(m_cameraWater.getLook());
                    const glm::vec3 camUpW   = glm::normalize(m_cameraWater.getUp());
                    const float fovYW        = m_cameraWater.getFovYRadians();
                    if (locCamPosW  >= 0) glUniform3f(locCamPosW,  camPosW.x,  camPosW.y,  camPosW.z);
                    if (locCamLookW >= 0) glUniform3f(locCamLookW, camLookW.x, camLookW.y, camLookW.z);
                    if (locCamUpW   >= 0) glUniform3f(locCamUpW,   camUpW.x,   camUpW.y,   camUpW.z);
                    if (locFovYW    >= 0) glUniform1f(locFovYW,    fovYW);
                }
            }
            glDrawArrays(GL_TRIANGLES, 0, 6);

            // 2) Render Scene A (IQ rainforest) then optional sprint blur to screen
            // Compute speed-based blur activation and strength
            float maxSpeedPB = m_moveSpeedBase * (1.0f + m_sprintAccumMax);
            float speedFracPB = 0.0f;
            if (maxSpeedPB > m_moveSpeedBase) {
                speedFracPB = std::max(0.0f, std::min((m_currentSpeedUnits - m_moveSpeedBase) / (maxSpeedPB - m_moveSpeedBase), 1.0f));
            }
            bool shiftDownPB = m_keyMap[Qt::Key_Shift];
            float blurScalePB = shiftDownPB ? m_shiftHoldRamp01 : (m_sprintBlurUnlocked ? 1.0f : 0.0f);
            float blurPixelsPB = 10.0f * speedFracPB * blurScalePB; // 0..10px, scaled by ramp/permit
            int numSamplesPB = 7 + int(10.0f * speedFracPB * blurScalePB); // 7..17
            if ((numSamplesPB % 2) == 0) numSamplesPB += 1;
            bool blurActiveIQPortal = (blurPixelsPB > 0.0f) &&
                                      m_postProgDirectional && m_fullscreenFBO && m_fullscreenColorTex;
            if (blurActiveIQPortal) {
                // Render IQ to offscreen
                glBindFramebuffer(GL_FRAMEBUFFER, m_fullscreenFBO);
                glViewport(0, 0, outW, outH);
                const float clearC2[4] = {0.f, 0.f, 0.f, 1.f};
                glClearBufferfv(GL_COLOR, 0, clearC2);
                glDisable(GL_DEPTH_TEST);
                glUseProgram(m_postProgIQ);
                glBindVertexArray(m_screenVAO);
                GLint locResA  = glGetUniformLocation(m_postProgIQ, "iResolution");
                GLint locTimeA = glGetUniformLocation(m_postProgIQ, "iTime");
                GLint locFrameA = glGetUniformLocation(m_postProgIQ, "iFrame");
                if (locResA  >= 0) glUniform3f(locResA,  float(outW), float(outH), 1.0f);
                if (locTimeA >= 0) glUniform1f(locTimeA, m_timeSec);
                if (locFrameA >= 0) glUniform1i(locFrameA, m_frameCount);
                GLint locCamPosA  = glGetUniformLocation(m_postProgIQ, "u_camPos");
                GLint locCamLookA = glGetUniformLocation(m_postProgIQ, "u_camLook");
                GLint locCamUpA   = glGetUniformLocation(m_postProgIQ, "u_camUp");
                GLint locFovYA    = glGetUniformLocation(m_postProgIQ, "u_camFovY");
                GLint locCamTargetA = glGetUniformLocation(m_postProgIQ, "u_camTarget");
                GLint locSunDirA  = glGetUniformLocation(m_postProgIQ, "u_sunDir");
                GLint locExposureA = glGetUniformLocation(m_postProgIQ, "u_exposure");
                if (locCamPosA >= 0 || locCamLookA >= 0 || locCamUpA >= 0 || locFovYA >= 0 || locCamTargetA >= 0) {
                    glm::vec3 camPos  = m_camera.getPosition();
                    glm::vec3 camLook = m_camera.getLook();
                    glm::vec3 camUp   = m_camera.getUp();
                    float fovY        = 2.f * std::atan(1.f / 1.5f);
                    glm::vec3 camTarget = camPos + glm::normalize(camLook);
                    if (locCamPosA  >= 0) glUniform3f(locCamPosA,  camPos.x,  camPos.y,  camPos.z);
                    if (locCamLookA >= 0) glUniform3f(locCamLookA, camLook.x, camLook.y, camLook.z);
                    if (locCamUpA   >= 0) glUniform3f(locCamUpA,   camUp.x,   camUp.y,   camUp.z);
                    if (locFovYA    >= 0) glUniform1f(locFovYA,    fovY);
                    if (locCamTargetA >= 0) glUniform3f(locCamTargetA, camTarget.x, camTarget.y, camTarget.z);
                }
                if (locSunDirA >= 0) {
                    const glm::vec3 sunDir(-0.624695f, 0.468521f, -0.624695f);
                    glUniform3f(locSunDirA, sunDir.x, sunDir.y, sunDir.z);
                }
                if (locExposureA >= 0) {
                    glUniform1f(locExposureA, 1.0f);
                }
                // Rainforest intensity
                {
                    GLint locIntensityA = glGetUniformLocation(m_postProgIQ, "u_rainforestIntensity");
                    if (locIntensityA >= 0) glUniform1f(locIntensityA, settings.rainforestIntensity);
                }
                glDrawArrays(GL_TRIANGLES, 0, 6);

                // Blur to screen
                glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
                if (prevFBO == 0) {
                    glViewport(0, 0, outW, outH);
                }
                glDisable(GL_DEPTH_TEST);
                glUseProgram(m_postProgDirectional);
                glBindVertexArray(m_screenVAO);
                GLint locColor = glGetUniformLocation(m_postProgDirectional, "u_colorTex");
                GLint locTexel = glGetUniformLocation(m_postProgDirectional, "u_texelSize");
                GLint locDir   = glGetUniformLocation(m_postProgDirectional, "u_directionUV");
                GLint locPx    = glGetUniformLocation(m_postProgDirectional, "u_blurPixels");
                GLint locNS    = glGetUniformLocation(m_postProgDirectional, "u_numSamples");
                if (locColor >= 0) glUniform1i(locColor, 0);
                if (locTexel >= 0) glUniform2f(locTexel, 1.0f / float(outW), 1.0f / float(outH));
                if (locDir >= 0) glUniform2f(locDir, 1.0f, 0.0f);
                if (locPx  >= 0) glUniform1f(locPx, blurPixelsPB);
                if (locNS  >= 0) glUniform1i(locNS, numSamplesPB);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_fullscreenColorTex);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            } else {
                glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
                if (prevFBO == 0) {
                    glViewport(0, 0, outW, outH);
                }
                glDisable(GL_DEPTH_TEST);
                glUseProgram(m_postProgIQ);
                glBindVertexArray(m_screenVAO);
                GLint locResA  = glGetUniformLocation(m_postProgIQ, "iResolution");
                GLint locTimeA = glGetUniformLocation(m_postProgIQ, "iTime");
                GLint locFrameA = glGetUniformLocation(m_postProgIQ, "iFrame");
                if (locResA  >= 0) glUniform3f(locResA,  float(outW), float(outH), 1.0f);
                if (locTimeA >= 0) glUniform1f(locTimeA, m_timeSec);
                if (locFrameA >= 0) glUniform1i(locFrameA, m_frameCount);
                GLint locCamPosA  = glGetUniformLocation(m_postProgIQ, "u_camPos");
                GLint locCamLookA = glGetUniformLocation(m_postProgIQ, "u_camLook");
                GLint locCamUpA   = glGetUniformLocation(m_postProgIQ, "u_camUp");
                GLint locFovYA    = glGetUniformLocation(m_postProgIQ, "u_camFovY");
                GLint locCamTargetA = glGetUniformLocation(m_postProgIQ, "u_camTarget");
                GLint locSunDirA  = glGetUniformLocation(m_postProgIQ, "u_sunDir");
                GLint locExposureA = glGetUniformLocation(m_postProgIQ, "u_exposure");
                if (locCamPosA >= 0 || locCamLookA >= 0 || locCamUpA >= 0 || locFovYA >= 0 || locCamTargetA >= 0) {
                    glm::vec3 camPos  = m_camera.getPosition();
                    glm::vec3 camLook = m_camera.getLook();
                    glm::vec3 camUp   = m_camera.getUp();
                    float fovY        = 2.f * std::atan(1.f / 1.5f);
                    glm::vec3 camTarget = camPos + glm::normalize(camLook);
                    if (locCamPosA  >= 0) glUniform3f(locCamPosA,  camPos.x,  camPos.y,  camPos.z);
                    if (locCamLookA >= 0) glUniform3f(locCamLookA, camLook.x, camLook.y, camLook.z);
                    if (locCamUpA   >= 0) glUniform3f(locCamUpA,   camUp.x,   camUp.y,   camUp.z);
                    if (locFovYA    >= 0) glUniform1f(locFovYA,    fovY);
                    if (locCamTargetA >= 0) glUniform3f(locCamTargetA, camTarget.x, camTarget.y, camTarget.z);
                }
                if (locSunDirA >= 0) {
                    const glm::vec3 sunDir(-0.624695f, 0.468521f, -0.624695f);
                    glUniform3f(locSunDirA, sunDir.x, sunDir.y, sunDir.z);
                }
                if (locExposureA >= 0) {
                    glUniform1f(locExposureA, 1.0f);
                }
                // Rainforest intensity
                {
                    GLint locIntensityA2 = glGetUniformLocation(m_postProgIQ, "u_rainforestIntensity");
                    if (locIntensityA2 >= 0) glUniform1f(locIntensityA2, settings.rainforestIntensity);
                }
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            m_frameCount++;

            // 3) Composite portal quad
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(m_portalProg);
            GLint locSamp = glGetUniformLocation(m_portalProg, "u_portalTex");
            GLint locAlpha = glGetUniformLocation(m_portalProg, "u_alpha");
            GLint locM = glGetUniformLocation(m_portalProg, "u_M");
            GLint locV = glGetUniformLocation(m_portalProg, "u_V");
            GLint locP = glGetUniformLocation(m_portalProg, "u_P");

            GLint locTime      = glGetUniformLocation(m_portalProg, "u_time");
            GLint locRadius    = glGetUniformLocation(m_portalProg, "u_radius");
            GLint locEdgeWidth = glGetUniformLocation(m_portalProg, "u_edgeWidth");
            GLint locEdgeColor = glGetUniformLocation(m_portalProg, "u_edgeColor");

            if (locSamp >= 0) glUniform1i(locSamp, 0);
            if (locAlpha >= 0) glUniform1f(locAlpha, 1.0f);
            if (locM >= 0) glUniformMatrix4fv(locM, 1, GL_FALSE, glm::value_ptr(m_portalModel));
            if (locV >= 0) glUniformMatrix4fv(locV, 1, GL_FALSE, glm::value_ptr(m_camera.getViewMatrix()));
            if (locP >= 0) glUniformMatrix4fv(locP, 1, GL_FALSE, glm::value_ptr(m_camera.getProjectionMatrix()));

            if (locTime >= 0)      glUniform1f(locTime, m_timeSec);
            if (locRadius >= 0)    glUniform1f(locRadius, 0.8f);    // tweak: smaller or larger hole
            if (locEdgeWidth >= 0) glUniform1f(locEdgeWidth, 0.08f); // thickness of the glow
            if (locEdgeColor >= 0) glUniform3f(locEdgeColor, 0.5f, 0.9f, 1.2f);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_portalColorTex);
            glBindVertexArray(m_portalVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glDisable(GL_BLEND);
            return;
        }
    }
}
void Realtime::renderPlanetScene() {

    GLint prevFBO;
    glm::mat4 V, P;

    // Shadow setup (Planet only)
    updateShadowLightSelection();
    updateLightViewProj();
    renderShadowMap();  // fills m_shadowDepthTex

    // 1. Geometry Pass (shared)
    runGeometryPass(prevFBO, V, P);

    // 2. Post-process: TOON ONLY
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
    int outW = size().width() * m_devicePixelRatio;
    int outH = size().height() * m_devicePixelRatio;
    glViewport(0, 0, outW, outH);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(m_postProgToon);
    glBindVertexArray(m_screenVAO);

    // Texture bindings
    glUniform1i(glGetUniformLocation(m_postProgToon, "u_sceneTex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_sceneColorTex);

    glUniform1i(glGetUniformLocation(m_postProgToon, "u_depthTex"), 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_sceneDepthTex);

    glUniform1i(glGetUniformLocation(m_postProgToon, "u_normalTex"), 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_sceneNormalTex);

    glUniform1f(glGetUniformLocation(m_postProgToon, "u_near"), settings.nearPlane);
    glUniform1f(glGetUniformLocation(m_postProgToon, "u_far"),  settings.farPlane);
    glUniform1i(glGetUniformLocation(m_postProgToon, "u_enablePost"), 1);

    // Sky texture
    GLint locSky = glGetUniformLocation(m_postProgToon, "u_skyTex");
    if (locSky >= 0 && m_skyTex != 0) {
        glUniform1i(locSky, 3);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, m_skyTex);
    }

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Update previous matrices
    m_prevV = V;
    m_prevP = P;
    for (auto &d : m_draws) d.prevModel = d.model;

}

void Realtime::renderPlanetIntoPortalFBO() {
    if (m_portalFBO == 0 || m_portalColorTex == 0 || m_portalWidth <= 0 || m_portalHeight <= 0) {
        return;
    }

    // Build planet geometry once if needed (does not switch render mode)
    static bool s_planetBuiltForPortal = false;
    if (!s_planetBuiltForPortal || m_vertexCount == 0 || m_draws.empty()) {
        buildPlanetScene();
        s_planetBuiltForPortal = true;
    }

    // Save current framebuffer and viewport
    GLint prevFBO = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    // Geometry pass renders to m_sceneFBO at m_fbWidth x m_fbHeight
    glm::mat4 V, P;
    {
        updateShadowLightSelection();
        updateLightViewProj();
        renderShadowMap();
        GLint ignoredPrev;
        runGeometryPass(ignoredPrev, V, P);
    }

    // Post-process toon to the portal FBO at portal resolution
    glBindFramebuffer(GL_FRAMEBUFFER, m_portalFBO);
    glViewport(0, 0, m_portalWidth, m_portalHeight);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(m_postProgToon);
    glBindVertexArray(m_screenVAO);

    // Bind G-buffer textures from geometry pass
    glUniform1i(glGetUniformLocation(m_postProgToon, "u_sceneTex"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_sceneColorTex);

    glUniform1i(glGetUniformLocation(m_postProgToon, "u_depthTex"), 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_sceneDepthTex);

    glUniform1i(glGetUniformLocation(m_postProgToon, "u_normalTex"), 2);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, m_sceneNormalTex);

    glUniform1f(glGetUniformLocation(m_postProgToon, "u_near"), settings.nearPlane);
    glUniform1f(glGetUniformLocation(m_postProgToon, "u_far"),  settings.farPlane);
    glUniform1i(glGetUniformLocation(m_postProgToon, "u_enablePost"), 1);

    // Optional sky texture
    GLint locSky = glGetUniformLocation(m_postProgToon, "u_skyTex");
    if (locSky >= 0 && m_skyTex != 0) {
        glUniform1i(locSky, 3);
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, m_skyTex);
    }

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Update previous matrices for consistent motion if needed later
    m_prevV = V;
    m_prevP = P;
    for (auto &d : m_draws) d.prevModel = d.model;

    // Restore framebuffer and viewport
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}

void Realtime::runGeometryPass(GLint &prevFBO, glm::mat4 &V, glm::mat4 &P) {

    if (!m_prog || m_vertexCount == 0) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }

    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);

    // Pass 1: render scene into offscreen FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFBO);
    glViewport(0, 0, m_fbWidth, m_fbHeight);

    {
        const float colorClear[4] = {1.f, 1.f, 1.f, 1.f};
        const float velocityClear[2] = {0.f, 0.f};
        const float depthClear = 1.f;
        const float normalClear[4] = {0.f, 0.f, 1.f, 1.f};
        glClearBufferfv(GL_COLOR, 0, colorClear);
        glClearBufferfv(GL_COLOR, 1, velocityClear);
        glClearBufferfv(GL_DEPTH, 0, &depthClear);
        glClearBufferfv(GL_COLOR, 2, normalClear);
    }

    glEnable(GL_DEPTH_TEST);

    if (settings.sceneFilePath.empty()) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
    }

    glUseProgram(m_prog);
    glBindVertexArray(m_vao);

    V = m_camera.getViewMatrix();
    P = m_camera.getProjectionMatrix();

    // Upload common uniforms
    GLint uM = glGetUniformLocation(m_prog, "u_M");
    GLint uV = glGetUniformLocation(m_prog, "u_V");
    GLint uP = glGetUniformLocation(m_prog, "u_P");
    GLint uN = glGetUniformLocation(m_prog, "u_N");

    GLint uPrevM = glGetUniformLocation(m_prog, "u_prevM");
    GLint uPrevV = glGetUniformLocation(m_prog, "u_prevV");
    GLint uPrevP = glGetUniformLocation(m_prog, "u_prevP");

    GLint uKa = glGetUniformLocation(m_prog, "u_ka");
    GLint uKd = glGetUniformLocation(m_prog, "u_kd");
    GLint uKs = glGetUniformLocation(m_prog, "u_ks");
    GLint uShininess = glGetUniformLocation(m_prog, "u_shininess");
    GLint uCamPos = glGetUniformLocation(m_prog, "u_camPos");
    GLint uGlobal = glGetUniformLocation(m_prog, "u_global");

    GLint uHasTex = glGetUniformLocation(m_prog, "u_hasTexture");
    GLint uTexRepeat = glGetUniformLocation(m_prog, "u_texRepeat");
    GLint uBlend = glGetUniformLocation(m_prog, "u_blend");
    GLint uTex = glGetUniformLocation(m_prog, "u_tex");

    // Fog
    GLint uFogColor = glGetUniformLocation(m_prog, "u_fogColor");
    GLint uFogDensity = glGetUniformLocation(m_prog, "u_fogDensity");
    GLint uFogEnable = glGetUniformLocation(m_prog, "u_fogEnable");

    // Planet-specific
    GLint uIsPlanet = glGetUniformLocation(m_prog, "u_isPlanet");
    GLint uTime     = glGetUniformLocation(m_prog, "u_time");
    GLint uPlanetColorA = glGetUniformLocation(m_prog, "u_planetColorA");
    GLint uPlanetColorB = glGetUniformLocation(m_prog, "u_planetColorB");
    GLint uIsSand   = glGetUniformLocation(m_prog, "u_isSand");

    GLint uIsMoon      = glGetUniformLocation(m_prog, "u_isMoon");
    GLint uMoonCenter  = glGetUniformLocation(m_prog, "u_moonCenter");
    GLint uOrbitSpeed  = glGetUniformLocation(m_prog, "u_orbitSpeed");
    GLint uOrbitPhase  = glGetUniformLocation(m_prog, "u_orbitPhase");
    GLint uIsCube      = glGetUniformLocation(m_prog, "u_isFloatingCube");
    GLint uFloatSpeed  = glGetUniformLocation(m_prog, "u_floatSpeed");
    GLint uFloatAmp    = glGetUniformLocation(m_prog, "u_floatAmp");
    GLint uFloatPhase  = glGetUniformLocation(m_prog, "u_floatPhase");

    if (uTime >= 0) glUniform1f(uTime, m_timeSec);

    glUniformMatrix4fv(uV, 1, GL_FALSE, glm::value_ptr(V));
    glUniformMatrix4fv(uP, 1, GL_FALSE, glm::value_ptr(P));

    if (uPrevV >= 0) glUniformMatrix4fv(uPrevV, 1, GL_FALSE, glm::value_ptr(m_prevV));
    if (uPrevP >= 0) glUniformMatrix4fv(uPrevP, 1, GL_FALSE, glm::value_ptr(m_prevP));

    glUniform3fv(uCamPos, 1, glm::value_ptr(m_camera.getPosition()));
    glm::vec3 globals(m_render.globalData.ka, m_render.globalData.kd, m_render.globalData.ks);
    glUniform3fv(uGlobal, 1, glm::value_ptr(globals));

    if (uTex >= 0) glUniform1i(uTex, 0);

    // Fog setup
    float nearZ = settings.nearPlane;
    float farZ = settings.farPlane;

    glm::vec3 fogColor(1.f, 0.5f, 1.0f); // blue-white fog color

    float target = 0.02f;
    float density = (farZ > nearZ)
                        ? (std::sqrt(std::max(0.0f, -std::log(target))) / farZ)
                        : 0.0f;

    if (uFogColor >= 0)   glUniform3fv(uFogColor, 1, glm::value_ptr(fogColor));
    if (uFogDensity >= 0) glUniform1f(uFogDensity, density);
    if (uFogEnable >= 0)  glUniform1i(uFogEnable, settings.fogEnabled ? 1 : 0);

    // Lights (unchanged)
    GLint uNumLights = glGetUniformLocation(m_prog, "u_numLights");
    GLint uLightType0 = glGetUniformLocation(m_prog, "u_lightType[0]");
    GLint uLightColor0 = glGetUniformLocation(m_prog, "u_lightColor[0]");
    GLint uLightPos0 = glGetUniformLocation(m_prog, "u_lightPos[0]");
    GLint uLightDir0 = glGetUniformLocation(m_prog, "u_lightDir[0]");
    GLint uLightFunc0 = glGetUniformLocation(m_prog, "u_lightFunc[0]");
    GLint uLightAngle0 = glGetUniformLocation(m_prog, "u_lightAngle[0]");
    GLint uLightPenumbra0 = glGetUniformLocation(m_prog, "u_lightPenumbra[0]");

    const int maxLights = 8;
    int n = std::min<int>(maxLights, static_cast<int>(m_render.lights.size()));
    glUniform1i(uNumLights, n);

    int types[maxLights] = {0};
    glm::vec3 colors[maxLights];
    glm::vec3 poss[maxLights];
    glm::vec3 dirs[maxLights];
    glm::vec3 funcs[maxLights];
    float angles[maxLights] = {0.f};
    float pens[maxLights] = {0.f};

    for (int i = 0; i < n; i++) {
        const auto &L = m_render.lights[i];
        types[i] = (L.type == LightType::LIGHT_DIRECTIONAL) ? 0
                                                            : (L.type == LightType::LIGHT_POINT ? 1 : 2);
        colors[i] = glm::vec3(L.color);
        poss[i]   = glm::vec3(L.pos);
        dirs[i]   = glm::normalize(glm::vec3(L.dir));
        funcs[i]  = L.function;
        angles[i] = L.angle;
        pens[i]   = L.penumbra;
    }

    glUniform1iv(uLightType0, n, types);
    glUniform3fv(uLightColor0, n, glm::value_ptr(colors[0]));
    glUniform3fv(uLightPos0, n, glm::value_ptr(poss[0]));
    glUniform3fv(uLightDir0, n, glm::value_ptr(dirs[0]));
    glUniform3fv(uLightFunc0, n, glm::value_ptr(funcs[0]));
    glUniform1fv(uLightAngle0, n, angles);
    glUniform1fv(uLightPenumbra0, n, pens);

    // Shadow uniforms (only in Planet fullscreen mode)
    bool planetMode = settings.sceneFilePath.empty() &&
                      (settings.fullscreenScene == FullscreenScene::Planet);

    GLint uUseShadow        = glGetUniformLocation(m_prog, "u_useShadows");
    GLint uShadowLightIndex = glGetUniformLocation(m_prog, "u_shadowLightIndex");
    GLint uLightVP          = glGetUniformLocation(m_prog, "u_lightViewProj");
    GLint uShadowMap        = glGetUniformLocation(m_prog, "u_shadowMap");

    if (planetMode && m_hasShadowLight && m_shadowDepthTex != 0) {
        if (uUseShadow        >= 0) glUniform1i(uUseShadow, 1);
        if (uShadowLightIndex >= 0) glUniform1i(uShadowLightIndex, m_shadowLightIndex);
        if (uLightVP          >= 0) glUniformMatrix4fv(
                uLightVP, 1, GL_FALSE,
                glm::value_ptr(m_lightViewProj));
        if (uShadowMap        >= 0) {
            glActiveTexture(GL_TEXTURE4);   // use texture unit 4 for shadow map
            glBindTexture(GL_TEXTURE_2D, m_shadowDepthTex);
            glUniform1i(uShadowMap, 4);
        }
    } else {
        if (uUseShadow >= 0) glUniform1i(uUseShadow, 0);
    }

    for (const auto &d : m_draws) {

        glUniformMatrix4fv(uM, 1, GL_FALSE, glm::value_ptr(d.model));
        glUniformMatrix3fv(uN, 1, GL_FALSE, glm::value_ptr(d.normalMat));
        if (uPrevM >= 0) glUniformMatrix4fv(uPrevM, 1, GL_FALSE, glm::value_ptr(d.prevModel));

        if (uIsPlanet >= 0) glUniform1i(uIsPlanet, d.isPlanet);
        if (uIsSand >= 0)   glUniform1i(uIsSand,   d.isSand);

        // per-object:
        if (uIsMoon >= 0)     glUniform1i(uIsMoon, d.isMoon);
        if (uMoonCenter >= 0) glUniform3fv(uMoonCenter, 1, glm::value_ptr(d.moonCenter));
        if (uOrbitSpeed >= 0) glUniform1f(uOrbitSpeed, d.orbitSpeed);
        if (uOrbitPhase >= 0) glUniform1f(uOrbitPhase, d.orbitPhase);

        if (uIsCube >= 0)     glUniform1i(uIsCube, d.isFloatingCube);
        if (uFloatSpeed >= 0) glUniform1f(uFloatSpeed, d.floatSpeed);
        if (uFloatAmp >= 0)   glUniform1f(uFloatAmp, d.floatAmp);
        if (uFloatPhase >= 0) glUniform1f(uFloatPhase, d.floatPhase);

        if (d.isPlanet) {
            if (uPlanetColorA >= 0) glUniform3fv(uPlanetColorA, 1, glm::value_ptr(d.planetColorA));
            if (uPlanetColorB >= 0) glUniform3fv(uPlanetColorB, 1, glm::value_ptr(d.planetColorB));
        }

        glUniform3fv(uKa, 1, glm::value_ptr(d.ka));
        glUniform3fv(uKd, 1, glm::value_ptr(d.kd));
        glUniform3fv(uKs, 1, glm::value_ptr(d.ks));
        glUniform1f(uShininess, d.shininess);

        if (uHasTex >= 0) glUniform1i(uHasTex, d.hasTexture);
        if (d.hasTexture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, d.texture);
            if (uTexRepeat >= 0) glUniform2fv(uTexRepeat, 1, glm::value_ptr(d.texRepeat));
            if (uBlend >= 0)     glUniform1f(uBlend, d.blend);
        }

        glDrawArrays(GL_TRIANGLES, d.first, d.count);
    }
}

void Realtime::renderGeometryScene() {

    GLint prevFBO;
    glm::mat4 V, P;

    // 1. Geometry Pass
    runGeometryPass(prevFBO, V, P);

    // 2. Post-process selection
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));

    int outW = size().width() * m_devicePixelRatio;
    int outH = size().height() * m_devicePixelRatio;
    glViewport(0, 0, outW, outH);
    glDisable(GL_DEPTH_TEST);

    // Select post program
    bool useMotion = !m_debugDepth && (settings.extraCredit4 || m_sprintBlurUnlocked);

    if (m_debugDepth && m_postProgDepth) {
        glUseProgram(m_postProgDepth);
    } else if (useMotion && m_postProgMotion) {
        glUseProgram(m_postProgMotion);
    } else {
        glUseProgram(m_postProg);  // DOF + fog
    }

    glBindVertexArray(m_screenVAO);

    if (m_debugDepth && m_postProgDepth) {
        glUniform1i(glGetUniformLocation(m_postProgDepth, "u_depthTex"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_sceneDepthTex);

    } else if (useMotion && m_postProgMotion) {
        glUniform1i(glGetUniformLocation(m_postProgMotion, "u_colorTex"), 0);
        glUniform1i(glGetUniformLocation(m_postProgMotion, "u_velocityTex"), 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_sceneColorTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_sceneVelocityTex);

    } else {
        // DOF + Fog
        glUniform1i(glGetUniformLocation(m_postProg, "u_colorTex"), 0);
        glUniform1i(glGetUniformLocation(m_postProg, "u_depthTex"), 1);
        glUniform1f(glGetUniformLocation(m_postProg, "u_near"), settings.nearPlane);
        glUniform1f(glGetUniformLocation(m_postProg, "u_far"),  settings.farPlane);
        glUniform1f(glGetUniformLocation(m_postProg, "u_focusDist"), settings.focusDist);
        glUniform1f(glGetUniformLocation(m_postProg, "u_focusRange"), settings.focusRange);
        glUniform1f(glGetUniformLocation(m_postProg, "u_maxBlurRadius"), settings.maxBlurRadius);
        glUniform1i(glGetUniformLocation(m_postProg, "u_enable"), settings.extraCredit3 ? 1 : 0);
        glUniform2f(glGetUniformLocation(m_postProg, "u_texelSize"),
                    1.f / float(m_fbWidth),
                    1.f / float(m_fbHeight));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_sceneColorTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_sceneDepthTex);
    }

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Update prev matrices
    m_prevV = V;
    m_prevP = P;
    for (auto &d : m_draws) d.prevModel = d.model;
}

void Realtime::paintGL() {
    SceneRenderMode mode = computeRenderMode();
    switch (mode) {

    case SceneRenderMode::FullscreenProcedural:
        renderFullscreenProcedural();
        return;

    case SceneRenderMode::PlanetGeometryScene:
        renderPlanetScene();
        return;

    case SceneRenderMode::GeometryScene:
        renderGeometryScene();
        return;
    }
}

void Realtime::resizeGL(int w, int h) {
    // Tells OpenGL how big the screen is
    glViewport(0, 0, size().width() * m_devicePixelRatio, size().height() * m_devicePixelRatio);

    // Students: anything requiring OpenGL calls when the program starts should be done here
	float aspect = float(size().width() * m_devicePixelRatio) / float(size().height() * m_devicePixelRatio);
	m_camera.setAspectRatio(aspect);
    m_cameraWater.setAspectRatio(aspect);

    int fbw = size().width() * m_devicePixelRatio;
    int fbh = size().height() * m_devicePixelRatio;
    createOrResizeSceneFBO(fbw, fbh);
    createOrResizePortalFBO(fbw, fbh);
    createOrResizeFullscreenFBO(fbw, fbh);
}

void Realtime::sceneChanged(bool preserveCamera) {
    if (settings.sceneFilePath.empty()) {
        // No JSON scene: just clear all geometry so we get a blank/clear screen

        m_draws.clear();
        m_vertexCount = 0;
        m_render.shapes.clear();
        m_render.lights.clear();

        // Keep camera valid
        m_camera.setClipPlanes(settings.nearPlane, settings.farPlane);
        float aspect = float(size().width() * m_devicePixelRatio) /
                       float(size().height() * m_devicePixelRatio);
        m_camera.setAspectRatio(aspect);
        // Keep Water camera clip/aspect in sync
        m_cameraWater.setClipPlanes(settings.nearPlane, settings.farPlane);
        m_cameraWater.setAspectRatio(aspect);

        update();
        return;
    }
    else {
        // build scene data from JSON scene file
        bool ok = SceneParser::parse(settings.sceneFilePath, m_render);
        if (!ok) {
            std::cerr << "Failed to parse scene: " << settings.sceneFilePath << std::endl;
            m_draws.clear();
            m_vertexCount = 0;
            m_render.shapes.clear();
            m_render.lights.clear();
            update();
            return;
        }

        if (!preserveCamera) {
            m_camera.setFromSceneCameraData(m_render.cameraData);
        }
        m_camera.setClipPlanes(settings.nearPlane, settings.farPlane);
        float aspect = float(size().width() * m_devicePixelRatio) / float(size().height() * m_devicePixelRatio);
        m_camera.setAspectRatio(aspect);
        m_cameraWater.setClipPlanes(settings.nearPlane, settings.farPlane);
        m_cameraWater.setAspectRatio(aspect);

        rebuildGeometryFromRenderData();
    }
}

void Realtime::rebuildGeometryFromRenderData() {
    std::vector<float> cpuData;
    cpuData.reserve(1 << 20);
    m_draws.clear();

    // Adaptive level of detail based on the number of objects in the scene
    int effectiveP1 = settings.shapeParameter1;
    int effectiveP2 = settings.shapeParameter2;
    if (settings.extraCredit1) {
        int shapeCount = static_cast<int>(m_render.shapes.size());
        if (shapeCount > 1) {
            float scale = 1.f / std::sqrt(static_cast<float>(shapeCount));
            if (scale < 0.25f) scale = 0.25f;
            if (scale > 1.0f) scale = 1.0f;
            auto scaleParam = [scale](int v) -> int {
                // Scale from (v - 1) so that v == 1 remains 1 after scaling
                int scaled = 1 + static_cast<int>(std::round((static_cast<float>(v - 1)) * scale));
                if (scaled < 1) scaled = 1;
                return scaled;
            };
            effectiveP1 = scaleParam(effectiveP1);
            effectiveP2 = scaleParam(effectiveP2);
        }
    }

    int first = 0;
    for (const auto &shape : m_render.shapes) {
        int count = 0;
        if (shape.primitive.type == PrimitiveType::PRIMITIVE_MESH) {
            std::string err;
            std::vector<float> meshPN;
            if (!ObjLoader::load(shape.primitive.meshfile, meshPN, err)) {
                std::cerr << "OBJ load error: " << err << std::endl;
                continue;
            }
            // Pad PN to PNUV (uv = 0,0) to match VAO stride of 8 floats
            std::vector<float> meshPNUV;
            meshPNUV.reserve((meshPN.size() / 6) * 8);
            for (size_t i = 0; i + 5 < meshPN.size(); i += 6) {
                // position
                meshPNUV.push_back(meshPN[i + 0]);
                meshPNUV.push_back(meshPN[i + 1]);
                meshPNUV.push_back(meshPN[i + 2]);
                // normal
                meshPNUV.push_back(meshPN[i + 3]);
                meshPNUV.push_back(meshPN[i + 4]);
                meshPNUV.push_back(meshPN[i + 5]);
                // uv
                meshPNUV.push_back(0.f);
                meshPNUV.push_back(0.f);
            }
            count = static_cast<int>(meshPNUV.size()) / 8;
            cpuData.insert(cpuData.end(), meshPNUV.begin(), meshPNUV.end());
        } else {
            auto generator = ShapeFactory::create(shape.primitive.type);
            if (!generator) { continue; }
            // Adaptive level of detail based on the distance from the object to the camera
            int p1ForShape = effectiveP1;
            int p2ForShape = effectiveP2;
            if (settings.extraCredit2) {
                glm::vec3 camPos = m_camera.getPosition();
                glm::vec3 objCenter = glm::vec3(shape.ctm * glm::vec4(0.f, 0.f, 0.f, 1.f));
                float d = glm::length(camPos - objCenter);
                float lod = 1.0f / (1.0f + d * 0.2f);
                if (lod < 0.25f) lod = 0.25f;
                if (lod > 1.0f) lod = 1.0f;
                auto scaleParam = [lod](int v) -> int {
                    int scaled = 1 + static_cast<int>(std::round((static_cast<float>(v - 1)) * lod));
                    if (scaled < 1) scaled = 1;
                    return scaled;
                };
                p1ForShape = scaleParam(p1ForShape);
                p2ForShape = scaleParam(p2ForShape);
            }
            generator->updateParams(p1ForShape, p2ForShape);
            auto data = generator->generateShape();
            count = static_cast<int>(data.size()) / 8;
            cpuData.insert(cpuData.end(), data.begin(), data.end());
        }

        const auto &mat = shape.primitive.material;
        glm::vec3 kd = glm::vec3(mat.cDiffuse);
        glm::vec3 ka = glm::vec3(mat.cAmbient);
        glm::vec3 ks = glm::vec3(mat.cSpecular);
        float shininess = mat.shininess;
        glm::mat4 model = shape.ctm;
        glm::mat4 invModel = glm::inverse(model);
        glm::mat3 normalMat = glm::mat3(glm::transpose(invModel));
        Realtime::DrawItem item;
        item.first = first;
        item.count = count;
        item.model = model;
        item.invModel = invModel;
        item.normalMat = normalMat;
        item.prevModel = model;
        item.kd = kd;
        item.ka = ka;
        item.ks = ks;
        item.shininess = shininess;
        item.isPlanet = false;
        item.isSand = false;
        if (mat.textureMap.isUsed) {
            // Load or get from cache
            auto it = m_textureCache.find(mat.textureMap.filename);
            if (it == m_textureCache.end()) {
                QImage img(QString::fromStdString(mat.textureMap.filename));
                if (!img.isNull()) {
                    QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
                    GLuint texId = 0;
                    glGenTextures(1, &texId);
                    glBindTexture(GL_TEXTURE_2D, texId);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, rgba.width(), rgba.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.constBits());
                    glGenerateMipmap(GL_TEXTURE_2D);
                    glBindTexture(GL_TEXTURE_2D, 0);
                    m_textureCache.emplace(mat.textureMap.filename, texId);
                    item.texture = texId;
                    item.hasTexture = true;
                } else {
                    std::cerr << "Failed to load texture: " << mat.textureMap.filename << std::endl;
                    item.hasTexture = false;
                }
            } else {
                item.texture = it->second;
                item.hasTexture = true;
            }
            item.texRepeat = glm::vec2(mat.textureMap.repeatU, mat.textureMap.repeatV);
            item.blend = mat.blend;
        }
        m_draws.push_back(item);
        first += count;
    }
    m_vertexCount = first;

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    if (!cpuData.empty()) {
        glBufferData(GL_ARRAY_BUFFER, cpuData.size() * sizeof(float), cpuData.data(), GL_STATIC_DRAW);
    } else {
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
    }

    // Update LOD baseline position
    if (settings.extraCredit2) {
        m_lastLodCamPos = m_camera.getPosition();
        m_hasLastLodCamPos = true;
    }

    update();
}
void Realtime::buildPlanetScene() {
    m_draws.clear();
    m_render.lights.clear();

    // Reasonable default globals
    m_render.globalData.ka = 0.1f;
    m_render.globalData.kd = 1.0f;
    m_render.globalData.ks = 0.3f;
    m_render.globalData.kt = 0.0f;

    // Provide a default directional light (sunlight)
    SceneLightData L{};
    L.id = 0;
    L.type = LightType::LIGHT_DIRECTIONAL;
    L.color = glm::vec4(1.f, 1.f, 1.f, 1.f);
    L.function = glm::vec3(1.f, 0.f, 0.f);
    L.dir = glm::normalize(glm::vec4(0.3f, -1.0f, 0.2f, 0.f));
    L.pos = glm::vec4(0.f, 0.f, 0.f, 1.f);
    L.angle = 0.f;
    L.penumbra = 0.f;
    m_render.lights.push_back(L);

    // terrain
    TerrainGenerator tg;
    std::vector<float> pnc = tg.generateTerrain();

    std::vector<float> cpuData;
    cpuData.reserve((pnc.size() / 9) * 8);

    for (size_t i = 0; i + 8 < pnc.size(); i += 9) {
        float px = pnc[i + 0];
        float py = pnc[i + 1];
        float pz = pnc[i + 2];
        float nx = pnc[i + 3];
        float ny = pnc[i + 4];
        float nz = pnc[i + 5];

        float u = px;
        float v = py;

        cpuData.insert(cpuData.end(), { px, py, pz, nx, ny, nz, u, v });
    }

    int terrainCount = cpuData.size() / 8;

    DrawItem terrain{};
    terrain.first = 0;
    terrain.count = terrainCount;

    // Transform terrain
    float terrainSize = 40.f;
    float heightScale = 5.f;

    glm::mat4 M =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.f, -2.f, 0.f)) *
        glm::rotate(glm::radians(-90.f), glm::vec3(1.f, 0.f, 0.f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(terrainSize, terrainSize, heightScale));
    glm::mat4 Tcenter = glm::translate(glm::mat4(1.f),
                                       glm::vec3(-0.5f, -0.5f, 0.f));
    terrain.model     = M*Tcenter;
    terrain.invModel  = glm::inverse(M);
    terrain.normalMat = glm::mat3(glm::transpose(terrain.invModel));
    terrain.prevModel = terrain.model;

    terrain.kd = glm::vec3(0.9f, 0.5f, 0.15f);
    terrain.ka = glm::vec3(0.3f, 0.15f, 0.05f);
    terrain.ks = glm::vec3(0.1f);
    terrain.shininess = 8.f;

    terrain.isPlanet = false;
    terrain.isSand   = true;

    m_draws.push_back(terrain);

    //sphere planet
    auto generator = ShapeFactory::create(PrimitiveType::PRIMITIVE_SPHERE);
    generator->updateParams(25, 25);
    auto sphereData = generator->generateShape();

    int sphereFirst = terrainCount;
    int sphereCount = sphereData.size() / 8;
    cpuData.insert(cpuData.end(), sphereData.begin(), sphereData.end());

    auto makePlanet = [&](glm::vec3 pos,
                          float radius,
                          glm::vec3 colA,
                          glm::vec3 colB) -> DrawItem {
        DrawItem p{};
        p.first = sphereFirst;
        p.count = sphereCount;

        glm::mat4 SM =
            glm::translate(glm::mat4(1.f), pos) *
            glm::scale(glm::mat4(1.f), glm::vec3(radius));

        p.model     = SM;
        p.invModel  = glm::inverse(SM);
        p.normalMat = glm::mat3(glm::transpose(p.invModel));
        p.prevModel = p.model;

        p.kd = glm::vec3(1.0f);
        p.ka = glm::vec3(0.0f);
        p.ks = glm::vec3(0.0f);
        p.shininess = 0.f;

        p.isPlanet     = true;
        p.planetColorA = colA;
        p.planetColorB = colB;
        p.isSand = false;

        return p;
    };

    // Planet 1 – purple/pink gas giant
    DrawItem planet1 = makePlanet(
        glm::vec3(0.f, 0.6f, 1.f),
        0.8f,
        glm::vec3(0.3f, 0.2f, 0.6f), // dark
        glm::vec3(0.9f, 0.4f, 0.8f) // light
        );
    m_draws.push_back(planet1);

    // Planet 2 – teal/blue
    DrawItem planet2 = makePlanet(
        glm::vec3(1.3f, 1.2f, -0.5f),
        0.35f,
        glm::vec3(0.1f, 0.4f, 0.4f),
        glm::vec3(0.6f, 0.9f, 1.0f)
        );
    m_draws.push_back(planet2);

    // Planet 3 – orange/brown
    DrawItem planet3 = makePlanet(
        glm::vec3(-1.2f, 0.9f, 0.8f),
        0.5f,
        glm::vec3(0.4f, 0.2f, 0.05f),
        glm::vec3(0.9f, 0.7f, 0.3f)
        );
    m_draws.push_back(planet3);

    // Planet 4 – large faded background planet up-left
    m_draws.push_back(makePlanet(
        glm::vec3(-1.5f, 2.0f, -3.0f),
        3.f,
        glm::vec3(0.18f, 0.12f, 0.30f),
        glm::vec3(0.75f, 0.55f, 0.95f)
        ));

    // Planet 5 – icy white one far right, fairly small
    m_draws.push_back(makePlanet(
        glm::vec3(2.3f, 1.8f, -3.0f),
        0.30f,
        glm::vec3(0.6f, 0.7f, 0.8f),
        glm::vec3(1.0f, 1.0f, 1.0f)
        ));

    glm::vec3 bigPlanetCenter = glm::vec3(-1.5f, 2.0f, -3.0f);
    glm::vec3 bigPlanetCenter1 = glm::vec3(0.f, 0.6f, 1.f);
    // Moon 1
    DrawItem moon1 = makePlanet(
        bigPlanetCenter + glm::vec3(4.f, 0.f, 0.0f), // initial position
        0.25f,
        glm::vec3(0.8f, 0.8f, 1.f),
        glm::vec3(1.f, 1.f, 1.f)
        );
    moon1.isMoon      = true;
    moon1.moonCenter  = bigPlanetCenter;
    moon1.orbitSpeed  = 0.8f; // radians per second
    moon1.orbitPhase  = 0.0f;
    m_draws.push_back(moon1);

    // Moon 2
    DrawItem moon2 = makePlanet(
        bigPlanetCenter1 + glm::vec3(-1.0f, -0.5f, 0.8f),
        0.18f,
        glm::vec3(0.25f, 0.18f, 0.20f),
        glm::vec3(0.95f, 0.85f, 0.9f)
        );
    moon2.isMoon      = true;
    moon2.moonCenter  = bigPlanetCenter;
    moon2.orbitSpeed  = -1.5f;// negative = opposite direction
    moon2.orbitPhase  = 1.7f; // start elsewhere
    m_draws.push_back(moon2);

    // Ring of distant small planets around the horizon
    std::vector<glm::vec3> ringDark = {
        {0.20f, 0.10f, 0.30f},
        {0.05f, 0.25f, 0.30f},
        {0.25f, 0.15f, 0.05f},
        {0.15f, 0.20f, 0.25f}
    };
    std::vector<glm::vec3> ringLight = {
        {0.90f, 0.60f, 1.00f},
        {0.70f, 0.90f, 1.00f},
        {1.00f, 0.85f, 0.40f},
        {0.95f, 0.95f, 0.95f}
    };

    int numRingPlanets = 30;
    float ringDist     = 7.0f;
    float ringHeight   = 1.6f;

    for (int i = 0; i < numRingPlanets; ++i) {
        float t   = float(i) / float(numRingPlanets);
        float ang = glm::two_pi<float>() * t;

        // Position on a horizontal circle around origin
        glm::vec3 pos(
            std::cos(ang) * ringDist,
            ringHeight, // small vertical wobble
            std::sin(ang) * ringDist
            );

        float radius = 0.25f + 0.05f * std::sin(ang * 3.0f);

        glm::vec3 colA = ringDark[i % ringDark.size()];
        glm::vec3 colB = ringLight[i % ringLight.size()];

        DrawItem p = makePlanet(pos, radius, colA, colB);
        p.isFloatingCube = true;
        p.floatAmp       = 0.6f; // how high it moves
        p.floatSpeed     = 0.8f; // how fast it moves
        p.floatPhase     = ang;

        m_draws.push_back(p);
    }


    // Cube primitive for floating boxes
    auto cubeGen = ShapeFactory::create(PrimitiveType::PRIMITIVE_CUBE);
    cubeGen->updateParams(1, 1); // params often ignored for cubes
    auto cubeData = cubeGen->generateShape();

    int cubeFirst = sphereFirst + sphereCount;
    int cubeCount = cubeData.size() / 8;
    cpuData.insert(cpuData.end(), cubeData.begin(), cubeData.end());

    // Update total vertex count
    m_vertexCount = cubeFirst + cubeCount;

    auto makeFloatingCube = [&](glm::vec3 pos,
                                glm::vec3 scale,
                                glm::vec3 kd,
                                float floatSpeed,
                                float floatAmp,
                                float floatPhase) -> DrawItem {
        DrawItem c{};
        c.first = cubeFirst;
        c.count = cubeCount;

        glm::mat4 SM =
            glm::translate(glm::mat4(1.f), pos) *
            glm::scale(glm::mat4(1.f), scale);

        c.model     = SM;
        c.invModel  = glm::inverse(SM);
        c.normalMat = glm::mat3(glm::transpose(c.invModel));
        c.prevModel = c.model;

        c.kd = kd;
        c.ka = kd * 0.3f;
        c.ks = glm::vec3(0.05f);
        c.shininess = 8.f;

        c.isPlanet       = false;
        c.isSand         = false;
        c.isFloatingCube = true;
        c.floatSpeed     = floatSpeed;
        c.floatAmp       = floatAmp;
        c.floatPhase     = floatPhase;
        return c;
    };
    // scatter 30 cubes over the terrain
    int numCubes = 30;
    for (int i = 0; i < numCubes; ++i) {
        float fx = ((i % 5) - 2) * 3.0f;
        float fz = ((i / 5) - 2) * 3.0f;

        glm::vec3 pos(
            fx,
            -1.0f + 0.4f * ((i % 3) - 1), // roughly above ground
            fz
            );

        glm::vec3 scale(0.2f, 0.2f, 0.2f);

        float speed = 0.3f + 0.1f * i; // slightly different per cube
        float amp   = 0.4f + 0.05f * (i % 4);
        float phase = 1.3f * i;

        glm::vec3 kd(0.4f, 0.2f, 0.05f);

        m_draws.push_back(makeFloatingCube(pos, scale, kd, speed, amp, phase));
    }

    // -------- Upload to GPU --------

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, cpuData.size() * sizeof(float),
                 cpuData.data(), GL_STATIC_DRAW);
}


void Realtime::settingsChanged() {
    if (!settings.sceneFilePath.empty()) {
		// Update camera parameters that depend on settings without rebuilding geometry unless scene path changed
		m_camera.setClipPlanes(settings.nearPlane, settings.farPlane);
		float aspect = float(size().width() * m_devicePixelRatio) / float(size().height() * m_devicePixelRatio);
		m_camera.setAspectRatio(aspect);
        m_cameraWater.setClipPlanes(settings.nearPlane, settings.farPlane);
        m_cameraWater.setAspectRatio(aspect);
		sceneChanged();
		return;
    }
    update(); // asks for a PaintGL() call to occur
}

// ================== Camera Movement!

void Realtime::keyPressEvent(QKeyEvent *event) {
    // Toggle depth debug on F3
    if (event->key() == Qt::Key_F3) {
        m_debugDepth = !m_debugDepth;
        update();
        return;
    }
	// Place/enable portal on 'O' at camera's 2 o'clock, rotate 70 deg around Y
    if (event->key() == Qt::Key_O) {
		m_portalEnabled = true;
		m_portalAutoCenterY = false; // lock placement; do not auto-follow Y afterward

		// Use active camera depending on current fullscreen scene
		const bool waterActive = (settings.fullscreenScene == FullscreenScene::Water);
		const glm::vec3 camPos = waterActive ? m_cameraWater.getPosition() : m_camera.getPosition();
		const glm::vec3 camLook = glm::normalize(waterActive ? m_cameraWater.getLook() : m_camera.getLook());

		// Compute horizontal forward and right (XZ plane)
		glm::vec3 forwardXZ = glm::vec3(camLook.x, 0.f, camLook.z);
		if (glm::length(forwardXZ) < 1e-4f) {
			forwardXZ = glm::vec3(0.f, 0.f, -1.f);
		} else {
			forwardXZ = glm::normalize(forwardXZ);
		}
		glm::vec3 rightXZ = glm::normalize(glm::cross(forwardXZ, glm::vec3(0.f, 1.f, 0.f)));

		// 2 o'clock direction = 30 degrees to the right from forward (on XZ plane)
		const float angle30 = glm::radians(30.f);
		const float c30 = std::cos(angle30);
		const float s30 = std::sin(angle30);
		glm::vec3 dir2oclock = glm::normalize(c30 * forwardXZ + s30 * rightXZ);

		// Place some units away from the camera, at same Y as camera
		const float placeDist = 3.0f;
		const glm::vec3 portalPos = camPos + dir2oclock * placeDist;

		// Build model: translate to position, then yaw by +70 degrees around world Y
		const float yawDeg = 70.f;
		glm::mat4 M(1.f);
		M = glm::translate(M, glm::vec3(portalPos.x, camPos.y, portalPos.z));
		M = glm::rotate(M, glm::radians(yawDeg), glm::vec3(0.f, 1.f, 0.f));
		m_portalModel = M;

        update();
        return;
    }
    // 'P' → In Water scene: toggle/show portal for Water → Planet
    // Otherwise keep existing IQ<->Water toggle
    if (event->key() == Qt::Key_P) {
        if (settings.fullscreenScene == FullscreenScene::Water) {
            // Toggle portal visibility and place in front of Water camera
            m_portalEnabled = !m_portalEnabled;
            m_portalAutoCenterY = false; // fixed placement when toggled
            if (m_portalEnabled) {
                // Place some units ahead of water camera, aligned with its height
                const glm::vec3 camPos  = m_cameraWater.getPosition();
                const glm::vec3 camLook = glm::normalize(m_cameraWater.getLook());
                const float placeDist = 3.0f;
                const glm::vec3 portalPos = camPos + camLook * placeDist;
                glm::mat4 M(1.f);
                // Face the camera: build yaw to face opposite of camera look on XZ
                glm::vec3 fwdXZ = glm::normalize(glm::vec3(camLook.x, 0.f, camLook.z));
                float yaw = std::atan2(fwdXZ.x, fwdXZ.z); // rotate around +Y
                M = glm::translate(M, glm::vec3(portalPos.x, camPos.y, portalPos.z));
                M = glm::rotate(M, yaw, glm::vec3(0.f, 1.f, 0.f));
                m_portalModel = M;
                // Reset traversal depth when showing
                m_portalDepth = m_portalDepthMax;
            }
            update();
            return;
        } else {
            // Preserve old behavior: P toggles IQ <-> Water when not in Water
            if (settings.fullscreenScene == FullscreenScene::IQ) {
                settings.fullscreenScene = FullscreenScene::Water;
                glm::vec3 p = m_camera.getPosition();
            } else {
                settings.fullscreenScene = FullscreenScene::IQ;
            }
            update();
            return;
        }
    }
    // Toon shading scene:
    if (event->key() == Qt::Key_T) {
        settings.sceneFilePath.clear();
        settings.fullscreenScene = FullscreenScene::Planet;
        buildPlanetScene();
        {
            // Put camera a few units above the terrain, looking toward origin
            m_camera.setPosition(glm::vec3(0.f, 2.5f, 6.f));

            glm::vec3 lookTarget(0.f, 0.f, 0.f);
            glm::vec3 lookDir = glm::normalize(lookTarget - m_camera.getPosition());
            m_camera.setLook(lookDir);
            m_camera.setUp(glm::vec3(0.f, 1.f, 0.f));

            // Make sure clip planes & aspect are sensible
            m_camera.setClipPlanes(settings.nearPlane, settings.farPlane);
            float aspect = float(size().width() * m_devicePixelRatio) /
                           float(size().height() * m_devicePixelRatio);
            m_camera.setAspectRatio(aspect);

            // Also reset the "previous" matrices so motion/velocity isn't garbage
            m_prevV = m_camera.getViewMatrix();
            m_prevP = m_camera.getProjectionMatrix();
        }
        update();
        return;
    }

    m_keyMap[Qt::Key(event->key())] = true;
}

void Realtime::keyReleaseEvent(QKeyEvent *event) {
    m_keyMap[Qt::Key(event->key())] = false;
}

void Realtime::mousePressEvent(QMouseEvent *event) {
    if (event->buttons().testFlag(Qt::LeftButton)) {
        m_mouseDown = true;
        m_prev_mouse_pos = glm::vec2(event->position().x(), event->position().y());
    }
}

void Realtime::mouseReleaseEvent(QMouseEvent *event) {
    if (!event->buttons().testFlag(Qt::LeftButton)) {
        m_mouseDown = false;
    }
}

void Realtime::mouseMoveEvent(QMouseEvent *event) {
    if (m_mouseDown) {
        int posX = event->position().x();
        int posY = event->position().y();
        int deltaX = posX - m_prev_mouse_pos.x;
        int deltaY = posY - m_prev_mouse_pos.y;
        m_prev_mouse_pos = glm::vec2(posX, posY);

        // Use deltaX and deltaY here to rotate
        const float sensitivity = 0.005f; // radians per pixel
        float yaw   = -static_cast<float>(deltaX) * sensitivity; // rotate around world up
        float pitch = -static_cast<float>(deltaY) * sensitivity; // rotate around camera right (perpendicular to look and up)

        // Route input to Water camera when Water is the fullscreen scene (no scenefile)
        bool routeToWater = settings.sceneFilePath.empty() && (settings.fullscreenScene == FullscreenScene::Water);
        Camera &cam = routeToWater ? m_cameraWater : m_camera;

        glm::vec3 look = cam.getLook();
        glm::vec3 up   = cam.getUp();
        const glm::vec3 worldUp(0.f, 1.f, 0.f);

        if (yaw != 0.f) {
            auto rodrigues = [](const glm::vec3 &v, const glm::vec3 &axis, float angle) -> glm::vec3 {
                glm::vec3 k = glm::normalize(axis);
                float c = cosf(angle);
                float s = sinf(angle);
                return v * c + glm::cross(k, v) * s + k * glm::dot(k, v) * (1.f - c);
            };
            look = rodrigues(look, worldUp, yaw);
            up   = rodrigues(up,   worldUp, yaw);
        }

        if (pitch != 0.f) {
            glm::vec3 right = glm::normalize(glm::cross(look, up));
            auto rodrigues = [](const glm::vec3 &v, const glm::vec3 &axis, float angle) -> glm::vec3 {
                glm::vec3 k = glm::normalize(axis);
                float c = cosf(angle);
                float s = sinf(angle);
                return v * c + glm::cross(k, v) * s + k * glm::dot(k, v) * (1.f - c);
            };
            look = rodrigues(look, right, pitch);
            up   = rodrigues(up,   right, pitch);
        }

        // Re-orthonormalize basis to avoid drift
        look = glm::normalize(look);
        glm::vec3 right = glm::normalize(glm::cross(look, up));
        up = glm::normalize(glm::cross(right, look));

        cam.setLook(look);
        cam.setUp(up);

        update(); // asks for a PaintGL() call to occur
    }
}

void Realtime::timerEvent(QTimerEvent *event) {
    int elapsedms   = m_elapsedTimer.elapsed();
    float deltaTime = elapsedms * 0.001f;
    m_elapsedTimer.restart();

    // Use deltaTime and m_keyMap here to move around
    // Accumulate time for shaders needing iTime
    m_timeSec += deltaTime;

	// Track Shift hold for gating motion blur
	if (m_keyMap[Qt::Key_Shift]) {
		m_shiftHoldSec += deltaTime;
		if (!m_sprintBlurUnlocked && m_shiftHoldSec >= 2.0f) {
			m_sprintBlurUnlocked = true;
		}
	} else {
		m_shiftHoldSec = 0.0f;
	}
	// Delayed ramp: 0 for first 2 seconds, then ramp 0..1 over next 2 seconds while holding
	float shiftHeldBeyond2 = m_shiftHoldSec - 2.0f;
	m_shiftHoldRamp01 = std::max(0.0f, std::min(shiftHeldBeyond2 / 2.0f, 1.0f));
    const float baseSpeed = m_moveSpeedBase; // world-space units per second
    // Sprint accumulation
    if (m_keyMap[Qt::Key_Shift]) {
        m_sprintAccum += m_sprintAccelPerSec * deltaTime;
    } else {
        m_sprintAccum -= m_sprintDecayPerSec * deltaTime;
    }
    m_sprintAccum = std::max(0.0f, std::min(m_sprintAccum, m_sprintAccumMax));
    const float currentSpeed = baseSpeed * (1.0f + m_sprintAccum);
    m_currentSpeedUnits = currentSpeed;
	// Reset blur unlock once back to base speed (stops persistent blur until next 2s hold)
	if (m_sprintBlurUnlocked && m_currentSpeedUnits <= (m_moveSpeedBase + 0.01f)) {
		m_sprintBlurUnlocked = false;
	}
    glm::vec3 displacement(0.f);

    // Route movement to Water camera when Water is the fullscreen scene (no scenefile)
    bool routeToWater = settings.sceneFilePath.empty() && (settings.fullscreenScene == FullscreenScene::Water);
    Camera &cam = routeToWater ? m_cameraWater : m_camera;

    const glm::vec3 lookDir = glm::normalize(cam.getLook());
    const glm::vec3 upDir   = glm::normalize(cam.getUp());
    const glm::vec3 rightDir = glm::normalize(glm::cross(lookDir, upDir)); // right, perpendicular to look and up
    const glm::vec3 worldUp(0.f, 1.f, 0.f);

    if (m_keyMap[Qt::Key_W]) {
        displacement += lookDir;
    }
    if (m_keyMap[Qt::Key_S]) {
        displacement -= lookDir;
    }
    if (m_keyMap[Qt::Key_A]) {
        displacement -= rightDir;
    }
    if (m_keyMap[Qt::Key_D]) {
        displacement += rightDir;
    }

    if (m_keyMap[Qt::Key_Space]) {
        displacement += worldUp;
    }
    if (m_keyMap[Qt::Key_Alt]) {
        displacement -= worldUp;
    }

    if (glm::length(displacement) > 0.f) {
        displacement = glm::normalize(displacement) * (currentSpeed * deltaTime);
        cam.setPosition(cam.getPosition() + displacement);
    }

	// Portal traversal via keyboard:
	// - When in IQ with portal enabled: while holding W and the cursor is inside the portal rect, "advance" through the portal.
	// - When in Water: while holding S and cursor inside the same rect, "walk back" to IQ.
	// We use a virtual distance (m_portalDepth) and a small cooldown to avoid flicker.
	{
		// countdown cooldown
		if (m_portalCooldownTimer > 0.f) {
			m_portalCooldownTimer = std::max(0.f, m_portalCooldownTimer - deltaTime);
		}
		const bool cooldownReady = (m_portalCooldownTimer <= 0.f);
 
		// Keep the portal centered at the active camera's height so the ray test
		// can hit the portal rectangle from either side (IQ or Water)
		if (settings.sceneFilePath.empty() && m_portalEnabled && m_portalAutoCenterY) {
			const bool waterActiveNow = (settings.fullscreenScene == FullscreenScene::Water);
			const float camY = waterActiveNow ? m_cameraWater.getPosition().y : m_camera.getPosition().y;
			m_portalModel = glm::translate(glm::mat4(1.f), glm::vec3(0.f, camY, 0.f));
		}

		// Ray-portal intersection test in world space using camera forward ray
		bool insidePortalRect = false;
        {
            // Use the active scene's camera for portal picking
            const bool waterActive = (settings.fullscreenScene == FullscreenScene::Water);
            const glm::vec3 camPos = waterActive ? m_cameraWater.getPosition() : m_camera.getPosition();
            const glm::vec3 rayDir = glm::normalize(waterActive ? m_cameraWater.getLook() : m_camera.getLook());
			const glm::vec3 center = glm::vec3(m_portalModel * glm::vec4(0.f, 0.f, 0.f, 1.f));
			const glm::vec3 axisX  = glm::vec3(m_portalModel * glm::vec4(1.f, 0.f, 0.f, 0.f));
			const glm::vec3 axisY  = glm::vec3(m_portalModel * glm::vec4(0.f, 1.f, 0.f, 0.f));
			const glm::vec3 nrm    = glm::normalize(glm::cross(axisX, axisY));
			const float denom = glm::dot(rayDir, nrm);
			if (std::abs(denom) > 1e-4f) {
				const float t = glm::dot(center - camPos, nrm) / denom;
				if (t > 0.f) {
					const glm::vec3 hit = camPos + t * rayDir;
					const glm::vec3 v = hit - center;
					const float halfW = 0.5f * glm::length(axisX);
					const float halfH = 0.5f * glm::length(axisY);
					const glm::vec3 uDir = glm::normalize(axisX);
					const glm::vec3 vDir = glm::normalize(axisY);
					const float u = glm::dot(v, uDir);
					const float w = glm::dot(v, vDir);
					insidePortalRect = (std::abs(u) <= halfW) && (std::abs(w) <= halfH);
				}
			}
		}

		const bool portalRenderable =
			settings.sceneFilePath.empty() &&
			(m_portalEnabled) &&
			(m_portalFBO != 0) && (m_portalColorTex != 0) && (m_portalProg != 0) && (m_portalVAO != 0);

		// Walk "into" portal from IQ
		if (portalRenderable &&
			settings.fullscreenScene == FullscreenScene::IQ) {
			if (insidePortalRect && m_keyMap[Qt::Key_W] && cooldownReady) {
				m_portalDepth -= currentSpeed * deltaTime;
				if (m_portalDepth <= 0.f) {
					settings.fullscreenScene = FullscreenScene::Water;
                    glm::vec3 p = m_camera.getPosition();
					m_portalDepth = m_portalDepthMax;
					m_portalCooldownTimer = m_portalCooldownSec;
					update();
					return;
				}
			} else {
				// recover when not actively pushing through
                m_portalDepth = std::min(m_portalDepthMax, m_portalDepth + baseSpeed * 0.75f * deltaTime);
			}
		}
		// Walk from Water into Planet: hold W while looking at the portal
		else if (settings.sceneFilePath.empty() &&
				 settings.fullscreenScene == FullscreenScene::Water) {
			if (insidePortalRect && m_keyMap[Qt::Key_W] && cooldownReady && m_portalEnabled) {
				m_portalDepth -= currentSpeed * deltaTime;
				if (m_portalDepth <= 0.f) {
                    // Switch to Planet scene and build it
                    settings.sceneFilePath.clear();
                    settings.fullscreenScene = FullscreenScene::Planet;
                    buildPlanetScene();
                    // Set a reasonable camera for planet scene
                    m_camera.setPosition(glm::vec3(0.f, 2.5f, 6.f));
                    {
                        glm::vec3 lookTarget(0.f, 0.f, 0.f);
                        glm::vec3 lookDir = glm::normalize(lookTarget - m_camera.getPosition());
                        m_camera.setLook(lookDir);
                        m_camera.setUp(glm::vec3(0.f, 1.f, 0.f));
                        m_camera.setClipPlanes(settings.nearPlane, settings.farPlane);
                        float aspect = float(size().width() * m_devicePixelRatio) /
                                       float(size().height() * m_devicePixelRatio);
                        m_camera.setAspectRatio(aspect);
                        m_prevV = m_camera.getViewMatrix();
                        m_prevP = m_camera.getProjectionMatrix();
                    }
                    // Reset portal state/cooldown
					m_portalDepth = m_portalDepthMax;
					m_portalCooldownTimer = m_portalCooldownSec;
                    m_portalEnabled = false; // hide portal after traversal
					update();
					return;
				}
			} else {
                m_portalDepth = std::min(m_portalDepthMax, m_portalDepth + baseSpeed * 0.75f * deltaTime);
			}
		}
	}

    // If EC2 is enabled, rebuild geometry when the camera has moved far enough to change LOD
    if (settings.extraCredit2 && !settings.sceneFilePath.empty() && !m_render.shapes.empty()) {
        if (!m_hasLastLodCamPos) {
            m_lastLodCamPos = m_camera.getPosition();
            m_hasLastLodCamPos = true;
        } else {
            float moveDist = glm::length(m_camera.getPosition() - m_lastLodCamPos);
            if (moveDist > m_lodRebuildDistanceThreshold) {
                m_lastLodCamPos = m_camera.getPosition();
                rebuildGeometryFromRenderData();
                return;
            }
        }
    }

    update(); // asks for a PaintGL() call to occur
}

// DO NOT EDIT
void Realtime::saveViewportImage(std::string filePath) {
    // Make sure we have the right context and everything has been drawn
    makeCurrent();

    // BUGGY [DELETE THIS]
    // int fixedWidth = 1024;
    // int fixedHeight = 768;

    // CORRECT [REPLACE WITH THIS]
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    int fixedWidth = viewport[2];
    int fixedHeight = viewport[3];

    // Create Frame Buffer
    GLuint fbo;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    // Create a color attachment texture
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fixedWidth, fixedHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

    // Optional: Create a depth buffer if your rendering uses depth testing
    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, fixedWidth, fixedHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Error: Framebuffer is not complete!" << std::endl;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    // Render to the FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, fixedWidth, fixedHeight);

    // Clear and render your scene here
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    paintGL();

    // Read pixels from framebuffer
    std::vector<unsigned char> pixels(fixedWidth * fixedHeight * 3);
    glReadPixels(0, 0, fixedWidth, fixedHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    // Unbind the framebuffer to return to default rendering to the screen
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Convert to QImage
    QImage image(pixels.data(), fixedWidth, fixedHeight, QImage::Format_RGB888);
    QImage flippedImage = image.mirrored(); // Flip the image vertically

    // Save to file using Qt
    QString qFilePath = QString::fromStdString(filePath);
    if (!flippedImage.save(qFilePath)) {
        std::cerr << "Failed to save image to " << filePath << std::endl;
    }

    // Clean up
    glDeleteTextures(1, &texture);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteFramebuffers(1, &fbo);
}

// G-buffer creation and resizing
void Realtime::createOrResizeSceneFBO(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (m_sceneFBO == 0) {
        glGenFramebuffers(1, &m_sceneFBO);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFBO);

    // Color
    if (m_sceneColorTex == 0) glGenTextures(1, &m_sceneColorTex);
    glBindTexture(GL_TEXTURE_2D, m_sceneColorTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_sceneColorTex, 0);

    // Velocity (RG16F)
    if (m_sceneVelocityTex == 0) glGenTextures(1, &m_sceneVelocityTex);
    glBindTexture(GL_TEXTURE_2D, m_sceneVelocityTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, width, height, 0, GL_RG, GL_HALF_FLOAT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, m_sceneVelocityTex, 0);

    //Normal
    if (m_sceneNormalTex == 0) glGenTextures(1, &m_sceneNormalTex);
    glBindTexture(GL_TEXTURE_2D, m_sceneNormalTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0,
                 GL_RGBA, GL_HALF_FLOAT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2,
                           GL_TEXTURE_2D, m_sceneNormalTex, 0);

    // Depth
    if (m_sceneDepthTex == 0) glGenTextures(1, &m_sceneDepthTex);
    glBindTexture(GL_TEXTURE_2D, m_sceneDepthTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_sceneDepthTex, 0);

    GLenum drawBuffers[3] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, drawBuffers);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Scene FBO incomplete: 0x" << std::hex << status << std::dec << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_fbWidth = width;
    m_fbHeight = height;
}

void Realtime::releaseSceneFBO() {
    if (m_sceneColorTex) { glDeleteTextures(1, &m_sceneColorTex); m_sceneColorTex = 0; }
    if (m_sceneDepthTex) { glDeleteTextures(1, &m_sceneDepthTex); m_sceneDepthTex = 0; }
    if (m_sceneVelocityTex) { glDeleteTextures(1, &m_sceneVelocityTex); m_sceneVelocityTex = 0; }
    if (m_sceneNormalTex)  { glDeleteTextures(1, &m_sceneNormalTex);  m_sceneNormalTex = 0; }
    if (m_sceneFBO) { glDeleteFramebuffers(1, &m_sceneFBO); m_sceneFBO = 0; }

    m_fbWidth = m_fbHeight = 0;
}

// Create screen-quad for post-processing, used for motion blur and dof
void Realtime::createScreenQuad() {
    if (m_screenVAO) return;
    glGenVertexArrays(1, &m_screenVAO);
    glGenBuffers(1, &m_screenVBO);
    glBindVertexArray(m_screenVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_screenVBO);
    float data[] = {
        // pos     // uv
        -1.f, -1.f, 0.f, 0.f,
         1.f, -1.f, 1.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f, -1.f, 0.f, 0.f,
         1.f,  1.f, 1.f, 1.f,
        -1.f,  1.f, 0.f, 1.f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Realtime::releaseScreenQuad() {
    if (m_screenVBO) { glDeleteBuffers(1, &m_screenVBO); m_screenVBO = 0; }
    if (m_screenVAO) { glDeleteVertexArrays(1, &m_screenVAO); m_screenVAO = 0; }
}

void Realtime::createOrResizePortalFBO(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (m_portalFBO == 0) {
        glGenFramebuffers(1, &m_portalFBO);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, m_portalFBO);
    if (m_portalColorTex == 0) {
        glGenTextures(1, &m_portalColorTex);
    }
    glBindTexture(GL_TEXTURE_2D, m_portalColorTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_portalColorTex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Portal FBO incomplete: 0x" << std::hex << status << std::dec << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    m_portalWidth = width;
    m_portalHeight = height;
}

void Realtime::releasePortalFBO() {
    if (m_portalColorTex) { glDeleteTextures(1, &m_portalColorTex); m_portalColorTex = 0; }
    if (m_portalFBO) { glDeleteFramebuffers(1, &m_portalFBO); m_portalFBO = 0; }
    m_portalWidth = m_portalHeight = 0;
}

void Realtime::createPortalQuad() {
    if (m_portalVAO) return;
	const float s = 1.0f; // size = 1.0 (half-size = 0.5)
	m_portalHalfSize = 0.5f;
	const float x0 = -m_portalHalfSize, x1 =  m_portalHalfSize;
	const float y0 = -m_portalHalfSize, y1 =  m_portalHalfSize;
	float verts[] = {
		// pos (xyz)           // uv
		x0, y0, 0.f,           0.f, 0.f,
		x1, y0, 0.f,           1.f, 0.f,
		x1, y1, 0.f,           1.f, 1.f,
		x0, y0, 0.f,           0.f, 0.f,
		x1, y1, 0.f,           1.f, 1.f,
		x0, y1, 0.f,           0.f, 1.f
	};
    glGenVertexArrays(1, &m_portalVAO);
    glGenBuffers(1, &m_portalVBO);
    glBindVertexArray(m_portalVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_portalVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Realtime::releasePortalQuad() {
    if (m_portalVBO) { glDeleteBuffers(1, &m_portalVBO); m_portalVBO = 0; }
    if (m_portalVAO) { glDeleteVertexArrays(1, &m_portalVAO); m_portalVAO = 0; }
}

void Realtime::createOrResizeFullscreenFBO(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (m_fullscreenFBO == 0) {
        glGenFramebuffers(1, &m_fullscreenFBO);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, m_fullscreenFBO);
    if (m_fullscreenColorTex == 0) {
        glGenTextures(1, &m_fullscreenColorTex);
    }
    glBindTexture(GL_TEXTURE_2D, m_fullscreenColorTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_fullscreenColorTex, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Fullscreen FBO incomplete: 0x" << std::hex << status << std::dec << std::endl;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Realtime::releaseFullscreenFBO() {
    if (m_fullscreenColorTex) { glDeleteTextures(1, &m_fullscreenColorTex); m_fullscreenColorTex = 0; }
    if (m_fullscreenFBO) { glDeleteFramebuffers(1, &m_fullscreenFBO); m_fullscreenFBO = 0; }
}
