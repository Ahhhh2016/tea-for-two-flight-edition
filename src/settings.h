#ifndef SETTINGS_H
#define SETTINGS_H

#include <string>
#include "realtime.h"

enum class FullscreenScene {
	None = 0,
	IQ = 1,
    Water = 2,
    Planet = 3
};

struct Settings {
    std::string sceneFilePath;
    int shapeParameter1 = 1;
    int shapeParameter2 = 1;
    float nearPlane = 1;
    float farPlane = 1;
	// Fullscreen scene selection (used when no scene file is loaded)
	FullscreenScene fullscreenScene = FullscreenScene::IQ;
    // Depth of Field settings
    float focusDist = 3.0f;       // u_focusDist
    float focusRange = 0.75f;     // u_focusRange
    float maxBlurRadius = 8.0f;   // u_maxBlurRadius (pixels)
    bool perPixelFilter = false;
    bool kernelBasedFilter = false;
    bool extraCredit1 = false;
    bool extraCredit2 = false;
    bool extraCredit3 = false;
    bool extraCredit4 = false;
    bool fogEnabled = false;
};

// The global Settings object, will be initialized by MainWindow
extern Settings settings;

#endif // SETTINGS_H
