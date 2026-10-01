#ifndef VIEWER_RENDERER_H
#define VIEWER_RENDERER_H

#include "Camera.h"
#include "Mesh.h"

struct SceneObject
{
    std::string name;
    MeshData mesh;
    MeshData originalMesh;
    GpuMesh gpu;
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};
    glm::vec3 scale{1.0f};
    glm::vec4 color{0.15f, 0.45f, 0.85f, 1.0f};
    bool visible = true;
    int fmmSource = -1;
    int fmmPart = -1;
    bool fmmEnabled = false;
};

struct RenderSettings
{
    bool wireframe = false;
    bool showEdges = false;
    bool depthTest = true;
    glm::vec4 clearColor{0.06f, 0.08f, 0.12f, 1.0f};
};

class ViewerRenderer
{
public:
    bool initialize();
    void destroy();
    void render(const std::vector<SceneObject> &objects, const ViewerCamera &camera,
                const RenderSettings &settings, int width, int height);

private:
    unsigned int shaderProgram_ = 0;
    int modelLocation_ = -1;
    int viewLocation_ = -1;
    int projectionLocation_ = -1;
    int colorLocation_ = -1;
    int cameraLocation_ = -1;
};

#endif