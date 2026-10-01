#ifndef VIEWER_UI_H
#define VIEWER_UI_H

#include "Renderer.h"

#include <filesystem>

struct ViewerUiState
{
    int selectedObject = 0;
    char plyPath[512] = "";
    char selectedPlyPath[512] = "";
    std::filesystem::path plyDirectory = ".";
    bool showPlyBrowser = false;
    std::filesystem::path sceneDirectory = ".";
    bool showSceneBrowser = false;
    bool fastMarchingEnabled = false;
    float fastMarchingSlowness = 1.0f;
    char status[512] = "Listo.";
    int targetFaces = 1000;
    bool showDemo = false;
};

class ViewerUi
{
public:
    bool draw(std::vector<SceneObject> &objects, ViewerUiState &ui, RenderSettings &settings,
              ViewerCamera &camera);
};

bool selectFastMarchingVertex(std::vector<SceneObject> &objects, ViewerUiState &ui,
                              const ViewerCamera &camera, int width, int height,
                              double cursorX, double cursorY);

#endif