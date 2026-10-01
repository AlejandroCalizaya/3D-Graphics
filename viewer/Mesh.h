#ifndef VIEWER_MESH_H
#define VIEWER_MESH_H

#include "../shared.h"

#include <string>

struct MeshMaterial
{
    glm::vec3 ambient{0.2f};
    glm::vec3 diffuse{0.8f};
    glm::vec3 specular{0.0f};
    float shininess = 32.0f;
    std::string diffuseTexture;
};

struct MeshPart
{
    size_t firstIndex = 0;
    size_t indexCount = 0;
    int materialIndex = -1;
};

struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<CHE> halfEdges;
    std::vector<unsigned int> indices;
    std::vector<MeshMaterial> materials;
    std::vector<MeshPart> parts;
};

struct GpuMesh
{
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    unsigned int fmmColorVbo = 0;
    GLsizei indexCount = 0;
    std::vector<unsigned int> materialTextures;

    void upload(const MeshData &mesh);
    void updateFmmColors(const std::vector<glm::vec3> &colors);
    void destroy();
};

bool loadMeshFromPly(const std::string &path, MeshData &mesh, std::string &error);
bool loadMeshFromObjDirectory(const std::string &directory, MeshData &mesh, std::string &error);
void buildCubeMesh(MeshData &mesh);
void buildSphereMesh(MeshData &mesh, int slices = 32, int stacks = 24, float radius = 1.0f);
bool simplifyMeshData(MeshData &mesh, int targetFaces, std::string &error);

#endif