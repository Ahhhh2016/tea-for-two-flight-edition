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

    // If you must use this function, do not edit anything above this
}

void Realtime::finish() {
    killTimer(m_timer);
    this->makeCurrent();

    // Students: anything requiring OpenGL calls when the program exits should be done here

    releaseSceneFBO();
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
    if (m_portalProg) {
        glDeleteProgram(m_portalProg);
        m_portalProg = 0;
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
        // Portal compositing shader
        m_portalProg = ShaderLoader::createShaderProgram(":/resources/shaders/portal.vert",
                                                         ":/resources/shaders/portal.frag");
    } catch (const std::exception &e) {
        std::cerr << "Shader error: " << e.what() << std::endl;
        if (m_prog == 0) m_prog = 0;
        if (m_postProg == 0) m_postProg = 0;
        if (m_postProgMotion == 0) m_postProgMotion = 0;
        if (m_postProgDepth == 0) m_postProgDepth = 0;
		if (m_postProgIQ == 0) m_postProgIQ = 0;
		if (m_postProgWater == 0) m_postProgWater = 0;
        if (m_portalProg == 0) m_portalProg = 0;
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

    // Initialize previous camera matrices
    m_prevV = m_camera.getViewMatrix();
    m_prevP = m_camera.getProjectionMatrix();

    // Initialize time/frame counters for IQ shader
    m_timeSec = 0.f;
    m_frameCount = 0;
}

void Realtime::paintGL() {
	// Students: anything requiring OpenGL calls every frame should be done here
	// Fullscreen shader mode (used when no scene file is loaded)
	if (settings.sceneFilePath.empty()) {
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
                // Camera uniforms for IQ shader (safe if missing)
                GLint locCamPos  = glGetUniformLocation(prog, "u_camPos");
                GLint locCamLook = glGetUniformLocation(prog, "u_camLook");
                GLint locCamUp   = glGetUniformLocation(prog, "u_camUp");
                GLint locFovY    = glGetUniformLocation(prog, "u_camFovY");
                if (locCamPos >= 0 || locCamLook >= 0 || locCamUp >= 0 || locFovY >= 0) {
                    glm::vec3 camPos  = m_camera.getPosition();
                    glm::vec3 camLook = m_camera.getLook();
                    glm::vec3 camUp   = m_camera.getUp();
                    float fovY        = m_camera.getFovYRadians();
                    if (locCamPos  >= 0) glUniform3f(locCamPos,  camPos.x,  camPos.y,  camPos.z);
                    if (locCamLook >= 0) glUniform3f(locCamLook, camLook.x, camLook.y, camLook.z);
                    if (locCamUp   >= 0) glUniform3f(locCamUp,   camUp.x,   camUp.y,   camUp.z);
                    if (locFovY    >= 0) glUniform1f(locFovY,    fovY);
                }
                glDrawArrays(GL_TRIANGLES, 0, 6);
                m_frameCount++;
                return;
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
            glDrawArrays(GL_TRIANGLES, 0, 6);

            // 2) Render Scene A (IQ rainforest) to screen
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
            if (locCamPosA >= 0 || locCamLookA >= 0 || locCamUpA >= 0 || locFovYA >= 0) {
                glm::vec3 camPos  = m_camera.getPosition();
                glm::vec3 camLook = m_camera.getLook();
                glm::vec3 camUp   = m_camera.getUp();
                float fovY        = m_camera.getFovYRadians();
                if (locCamPosA  >= 0) glUniform3f(locCamPosA,  camPos.x,  camPos.y,  camPos.z);
                if (locCamLookA >= 0) glUniform3f(locCamLookA, camLook.x, camLook.y, camLook.z);
                if (locCamUpA   >= 0) glUniform3f(locCamUpA,   camUp.x,   camUp.y,   camUp.z);
                if (locFovYA    >= 0) glUniform1f(locFovYA,    fovY);
            }
            glDrawArrays(GL_TRIANGLES, 0, 6);
            m_frameCount++;

            // 3) Composite portal quad
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glUseProgram(m_portalProg);
            GLint locSamp = glGetUniformLocation(m_portalProg, "u_portalTex");
            GLint locAlpha = glGetUniformLocation(m_portalProg, "u_alpha");
            if (locSamp >= 0) glUniform1i(locSamp, 0);
            if (locAlpha >= 0) glUniform1f(locAlpha, 1.0f);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_portalColorTex);
            glBindVertexArray(m_portalVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glDisable(GL_BLEND);
            return;
        }
	}

    if (!m_prog || m_vertexCount == 0) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }

    // Save currently bound draw framebuffer into prevFBO
    GLint prevFBO = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO);

    // Pass 1: render scene into offscreen FBO
    glBindFramebuffer(GL_FRAMEBUFFER, m_sceneFBO);
    glViewport(0, 0, m_fbWidth, m_fbHeight);
    // Clear color and velocity attachments separately
    {
        const float colorClear[4] = {1.f, 1.f, 1.f, 1.f};
        const float velocityClear[2] = {0.f, 0.f};
        const float depthClear = 1.f;
        glClearBufferfv(GL_COLOR, 0, colorClear);   // clear color
        glClearBufferfv(GL_COLOR, 1, velocityClear); //clear velocity buffer
        glClearBufferfv(GL_DEPTH, 0, &depthClear); // clear depth buffer
    }
    glEnable(GL_DEPTH_TEST); // enable depth test

    // disable culling for terrain 
    if (settings.sceneFilePath.empty()) {
        glDisable(GL_CULL_FACE);
    } else {
        glEnable(GL_CULL_FACE);
    }

    glUseProgram(m_prog);
    glBindVertexArray(m_vao);

    glm::mat4 V(1.f), P(1.f);
    V = m_camera.getViewMatrix();
    P = m_camera.getProjectionMatrix();

    GLint uM = glGetUniformLocation(m_prog, "u_M");
    GLint uV = glGetUniformLocation(m_prog, "u_V");
    GLint uP = glGetUniformLocation(m_prog, "u_P");
    GLint uN = glGetUniformLocation(m_prog, "u_N");

    // previous frame's model matrix for motion blur
    GLint uPrevM = glGetUniformLocation(m_prog, "u_prevM");
    GLint uPrevV = glGetUniformLocation(m_prog, "u_prevV");
    GLint uPrevP = glGetUniformLocation(m_prog, "u_prevP");

    GLint uKa = glGetUniformLocation(m_prog, "u_ka");
    GLint uKd = glGetUniformLocation(m_prog, "u_kd");
    GLint uKs = glGetUniformLocation(m_prog, "u_ks");
    GLint uShininess = glGetUniformLocation(m_prog, "u_shininess");
    GLint uCamPos = glGetUniformLocation(m_prog, "u_camPos");
    GLint uGlobal = glGetUniformLocation(m_prog, "u_global");

    // Texture uniforms
    GLint uHasTex = glGetUniformLocation(m_prog, "u_hasTexture");
    GLint uTexRepeat = glGetUniformLocation(m_prog, "u_texRepeat");
    GLint uBlend = glGetUniformLocation(m_prog, "u_blend");
    GLint uTex = glGetUniformLocation(m_prog, "u_tex");

    // Fog uniforms
    GLint uFogColor = glGetUniformLocation(m_prog, "u_fogColor");
    GLint uFogDensity = glGetUniformLocation(m_prog, "u_fogDensity");
    GLint uFogEnable = glGetUniformLocation(m_prog, "u_fogEnable");

    glUniformMatrix4fv(uV, 1, GL_FALSE, glm::value_ptr(V));
    glUniformMatrix4fv(uP, 1, GL_FALSE, glm::value_ptr(P));

    if (uPrevV >= 0) glUniformMatrix4fv(uPrevV, 1, GL_FALSE, glm::value_ptr(m_prevV));
    if (uPrevP >= 0) glUniformMatrix4fv(uPrevP, 1, GL_FALSE, glm::value_ptr(m_prevP));

    glUniform3fv(uCamPos, 1, glm::value_ptr(m_camera.getPosition()));
    glm::vec3 globals(m_render.globalData.ka, m_render.globalData.kd, m_render.globalData.ks);
    glUniform3fv(uGlobal, 1, glm::value_ptr(globals));

    if (uTex >= 0) glUniform1i(uTex, 0); // read texture from texture unit 0

    // Fog parameters
    float nearZ = settings.nearPlane;
    float farZ = settings.farPlane;
    glm::vec3 fogColor(0.85f, 0.9f, 1.0f); // blue-white fog color
    float target = 0.02f;
    float density = (farZ > nearZ) ? (std::sqrt(std::max(0.0f, -std::log(target))) / farZ) : 0.0f;
    if (uFogColor >= 0)   glUniform3fv(uFogColor, 1, glm::value_ptr(fogColor));
    if (uFogDensity >= 0) glUniform1f(uFogDensity, density);
    if (uFogEnable >= 0)  glUniform1i(uFogEnable, settings.fogEnabled ? 1 : 0);

    // Upload lights (up to 8)
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
        types[i] = (L.type == LightType::LIGHT_DIRECTIONAL) ? 0 : (L.type == LightType::LIGHT_POINT ? 1 : 2);
        colors[i] = glm::vec3(L.color);
        poss[i] = glm::vec3(L.pos);
        dirs[i] = glm::normalize(glm::vec3(L.dir));
        funcs[i] = L.function;
        angles[i] = L.angle;
        pens[i] = L.penumbra;
    }
    glUniform1iv(uLightType0, n, types);
    glUniform3fv(uLightColor0, n, glm::value_ptr(colors[0]));
    glUniform3fv(uLightPos0, n, glm::value_ptr(poss[0]));
    glUniform3fv(uLightDir0, n, glm::value_ptr(dirs[0]));
    glUniform3fv(uLightFunc0, n, glm::value_ptr(funcs[0]));
    glUniform1fv(uLightAngle0, n, angles);
    glUniform1fv(uLightPenumbra0, n, pens);

    for (const auto &d : m_draws) {
        glUniformMatrix4fv(uM, 1, GL_FALSE, glm::value_ptr(d.model));
        glUniformMatrix3fv(uN, 1, GL_FALSE, glm::value_ptr(d.normalMat));
        if (uPrevM >= 0) glUniformMatrix4fv(uPrevM, 1, GL_FALSE, glm::value_ptr(d.prevModel));
        glUniform3fv(uKa, 1, glm::value_ptr(d.ka));
        glUniform3fv(uKd, 1, glm::value_ptr(d.kd));
        glUniform3fv(uKs, 1, glm::value_ptr(d.ks));
        glUniform1f(uShininess, d.shininess);
        if (uHasTex >= 0) glUniform1i(uHasTex, d.hasTexture ? 1 : 0);
        if (d.hasTexture) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, d.texture);
            if (uTexRepeat >= 0) glUniform2fv(uTexRepeat, 1, glm::value_ptr(d.texRepeat));
            if (uBlend >= 0) glUniform1f(uBlend, d.blend);
        }
        glDrawArrays(GL_TRIANGLES, d.first, d.count);
    }

    // Pass 2: post-process to previously bound framebuffer 
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(prevFBO));
    int outW = size().width() * m_devicePixelRatio;
    int outH = size().height() * m_devicePixelRatio;
    if (prevFBO != 0) {
        // custom FBO, assume caller set a viewport already
    } else {
        // screen
        glViewport(0, 0, outW, outH);
    }
    glDisable(GL_DEPTH_TEST); // disable depth test for post-processing

    // Select post program
    bool useMotion = settings.extraCredit4 && !m_debugDepth;
    if (m_debugDepth && m_postProgDepth) {
        glUseProgram(m_postProgDepth);
    } else if (useMotion && m_postProgMotion) {
        glUseProgram(m_postProgMotion);
    } else {
        glUseProgram(m_postProg);
    }
    glBindVertexArray(m_screenVAO);

    if (m_debugDepth && m_postProgDepth) {
        // depth debug: sample depth texture and show on screen
        GLint locDepth = glGetUniformLocation(m_postProgDepth, "u_depthTex");
        if (locDepth >= 0) glUniform1i(locDepth, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_sceneDepthTex);
    } else if (useMotion && m_postProgMotion) {
        // motion blur
        GLint locColor = glGetUniformLocation(m_postProgMotion, "u_colorTex");
        GLint locVel = glGetUniformLocation(m_postProgMotion, "u_velocityTex");
        GLint locTexel = glGetUniformLocation(m_postProgMotion, "u_texelSize");
        GLint locMaxPx = glGetUniformLocation(m_postProgMotion, "u_maxBlurPixels");
        GLint locSamples = glGetUniformLocation(m_postProgMotion, "u_numSamples");
        if (locColor >= 0) glUniform1i(locColor, 0);
        if (locVel >= 0) glUniform1i(locVel, 1);
        if (locTexel >= 0) glUniform2f(locTexel, 1.0f / float(m_fbWidth), 1.0f / float(m_fbHeight));
        if (locMaxPx >= 0) glUniform1f(locMaxPx, 16.0f);
        if (locSamples >= 0) glUniform1i(locSamples, 12);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_sceneColorTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_sceneVelocityTex);
    } else {
        // dof and fog
        GLint locColor = glGetUniformLocation(m_postProg, "u_colorTex");
        GLint locDepth = glGetUniformLocation(m_postProg, "u_depthTex");
        if (locColor >= 0) glUniform1i(locColor, 0);
        if (locDepth >= 0) glUniform1i(locDepth, 1);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_sceneColorTex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, m_sceneDepthTex);

        GLint locNear = glGetUniformLocation(m_postProg, "u_near");
        GLint locFar = glGetUniformLocation(m_postProg, "u_far");
        GLint locFD = glGetUniformLocation(m_postProg, "u_focusDist");
        GLint locFR = glGetUniformLocation(m_postProg, "u_focusRange");
        GLint locMaxR = glGetUniformLocation(m_postProg, "u_maxBlurRadius");
        GLint locEnable = glGetUniformLocation(m_postProg, "u_enable");
        GLint locTexel = glGetUniformLocation(m_postProg, "u_texelSize");

        float nearZ = settings.nearPlane;
        float farZ = settings.farPlane;
        if (locNear >= 0) glUniform1f(locNear, nearZ);
        if (locFar >= 0) glUniform1f(locFar, farZ);
        
        int enable = settings.extraCredit3 ? 1 : 0;
        if (locEnable >= 0) glUniform1i(locEnable, enable);
        if (locFD >= 0) glUniform1f(locFD, settings.focusDist);
        if (locFR >= 0) glUniform1f(locFR, settings.focusRange);
        if (locMaxR >= 0) glUniform1f(locMaxR, settings.maxBlurRadius);
        if (locTexel >= 0) glUniform2f(locTexel, 1.0f / float(m_fbWidth), 1.0f / float(m_fbHeight));
    }

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Update previous matrices for next frame
    m_prevV = V;
    m_prevP = P;
    for (auto &d : m_draws) {
        d.prevModel = d.model;
    }
}

void Realtime::resizeGL(int w, int h) {
    // Tells OpenGL how big the screen is
    glViewport(0, 0, size().width() * m_devicePixelRatio, size().height() * m_devicePixelRatio);

    // Students: anything requiring OpenGL calls when the program starts should be done here
	float aspect = float(size().width() * m_devicePixelRatio) / float(size().height() * m_devicePixelRatio);
	m_camera.setAspectRatio(aspect);

    int fbw = size().width() * m_devicePixelRatio;
    int fbh = size().height() * m_devicePixelRatio;
    createOrResizeSceneFBO(fbw, fbh);
    createOrResizePortalFBO(fbw, fbh);
}

void Realtime::sceneChanged(bool preserveCamera) {
    if (settings.sceneFilePath.empty()) {
        // generate procedural terrain when no scene file is provided

        m_draws.clear();
        // m_vertexCount = 0;
        m_render.shapes.clear();
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

        // Set up camera parameters (near/far/aspect) and keep current position/orientation
        m_camera.setClipPlanes(settings.nearPlane, settings.farPlane);
        float aspect = float(size().width() * m_devicePixelRatio) / float(size().height() * m_devicePixelRatio);
        m_camera.setAspectRatio(aspect);

        // Generate terrain
        TerrainGenerator tg;
        std::vector<float> pnc = tg.generateTerrain(); // [ px,py,pz, nx,ny,nz, r,g,b ] per vertex
        std::vector<float> cpuData;
        cpuData.reserve((pnc.size() / 9) * 8);
        for (size_t i = 0; i + 8 < pnc.size(); i += 9) {
            float px = pnc[i + 0];
            float py = pnc[i + 1];
            float pz = pnc[i + 2];
            float nx = pnc[i + 3];
            float ny = pnc[i + 4];
            float nz = pnc[i + 5];

            // uv derived from normalized x,y
            float u = px;
            float v = py;

            // repack to (P,N,UV) for our VAO layout
            cpuData.push_back(px);
            cpuData.push_back(py);
            cpuData.push_back(pz);
            cpuData.push_back(nx);
            cpuData.push_back(ny);
            cpuData.push_back(nz);
            cpuData.push_back(u);
            cpuData.push_back(v);
        }

        int vertexCount = static_cast<int>(cpuData.size()) / 8;
        m_vertexCount = vertexCount;

        // Create a single draw item with a ground-like material
        DrawItem item{};
        item.first = 0;
        item.count = vertexCount;

        item.model = glm::mat4(1.f);
        item.invModel = glm::mat4(1.f);
        item.normalMat = glm::mat3(1.f);
        item.prevModel = item.model;
        // Center the unit [0,1]^2 terrain around the origin and scale up
        // const float scaleXY = 50.f;
        // const float scaleZ  = 10.f;
        // glm::mat4 T = glm::translate(glm::mat4(1.f), glm::vec3(-0.5f, -0.5f, 0.f));
        // glm::mat4 S = glm::scale(glm::mat4(1.f), glm::vec3(scaleXY, scaleXY, scaleZ));
        // item.model = S * T;
        // item.invModel = glm::inverse(item.model);
        // item.normalMat = glm::mat3(glm::transpose(glm::inverse(item.model)));
        item.kd = glm::vec3(0.5f, 0.5f, 0.5f);
        item.ka = glm::vec3(0.2f, 0.2f, 0.2f);
        item.ks = glm::vec3(0.1f, 0.1f, 0.1f);
        item.shininess = 8.f;
        item.hasTexture = false;
        m_draws.push_back(item);

        // upload to GPU
        glBindVertexArray(m_vao);
        glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        if (!cpuData.empty()) {
            glBufferData(GL_ARRAY_BUFFER, cpuData.size() * sizeof(float), cpuData.data(), GL_STATIC_DRAW);
        } else {
            glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_STATIC_DRAW);
        }

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

void Realtime::settingsChanged() {
    if (!settings.sceneFilePath.empty()) {
		// Update camera parameters that depend on settings without rebuilding geometry unless scene path changed
		m_camera.setClipPlanes(settings.nearPlane, settings.farPlane);
		float aspect = float(size().width() * m_devicePixelRatio) / float(size().height() * m_devicePixelRatio);
		m_camera.setAspectRatio(aspect);
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
    // Toggle portal on 'O'
    if (event->key() == Qt::Key_O) {
        m_portalEnabled = !m_portalEnabled;
        update();
        return;
    }
    // Toggle fullscreen scene on 'P' (swap IQ <-> Water)
    if (event->key() == Qt::Key_P) {
        if (settings.fullscreenScene == FullscreenScene::IQ) {
            settings.fullscreenScene = FullscreenScene::Water;
        } else {
            settings.fullscreenScene = FullscreenScene::IQ;
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

        glm::vec3 look = m_camera.getLook();
        glm::vec3 up   = m_camera.getUp();
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

        m_camera.setLook(look);
        m_camera.setUp(up);

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
    const float moveSpeed = 5.0f; // world-space units per second
    glm::vec3 displacement(0.f);

    const glm::vec3 lookDir = glm::normalize(m_camera.getLook());
    const glm::vec3 upDir   = glm::normalize(m_camera.getUp());
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
        displacement = glm::normalize(displacement) * (moveSpeed * deltaTime);
        m_camera.setPosition(m_camera.getPosition() + displacement);
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

    // Depth
    if (m_sceneDepthTex == 0) glGenTextures(1, &m_sceneDepthTex);
    glBindTexture(GL_TEXTURE_2D, m_sceneDepthTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_sceneDepthTex, 0);

    GLenum drawBuffers[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, drawBuffers);

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
    const float w = 0.6f;
    const float h = 0.8f;
    const float x0 = -w * 0.5f, x1 =  w * 0.5f;
    const float y0 = -h * 0.5f, y1 =  h * 0.5f;
    float verts[] = {
        // pos          // uv
        x0, y0,         0.f, 0.f,
        x1, y0,         1.f, 0.f,
        x1, y1,         1.f, 1.f,
        x0, y0,         0.f, 0.f,
        x1, y1,         1.f, 1.f,
        x0, y1,         0.f, 1.f
    };
    glGenVertexArrays(1, &m_portalVAO);
    glGenBuffers(1, &m_portalVBO);
    glBindVertexArray(m_portalVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_portalVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Realtime::releasePortalQuad() {
    if (m_portalVBO) { glDeleteBuffers(1, &m_portalVBO); m_portalVBO = 0; }
    if (m_portalVAO) { glDeleteVertexArrays(1, &m_portalVAO); m_portalVAO = 0; }
}
