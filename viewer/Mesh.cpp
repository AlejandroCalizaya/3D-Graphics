#define STB_IMAGE_IMPLEMENTATION
#include "Mesh.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace
{
int resolveObjIndex(int index, size_t count)
{
    if (index > 0)
        return index - 1;
    if (index < 0)
        return static_cast<int>(count) + index;
    return -1;
}

int parseObjPositionIndex(const std::string &token)
{
    const size_t separator = token.find('/');
    return std::stoi(token.substr(0, separator));
}

int parseObjTextureIndex(const std::string &token)
{
    const size_t firstSeparator = token.find('/');
    if (firstSeparator == std::string::npos)
        return 0;
    const size_t secondSeparator = token.find('/', firstSeparator + 1);
    const std::string value = token.substr(firstSeparator + 1, secondSeparator - firstSeparator - 1);
    return value.empty() ? 0 : std::stoi(value);
}

std::string normalizeTexturePath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

bool loadMtlFile(const std::filesystem::path &path, MeshData &mesh,
                 std::unordered_map<std::string, int> &materialIndices)
{
    std::ifstream input(path);
    if (!input)
        return false;

    MeshMaterial *current = nullptr;
    std::string line;
    while (std::getline(input, line))
    {
        std::istringstream stream(line);
        std::string directive;
        stream >> directive;
        if (directive == "newmtl")
        {
            std::string name;
            stream >> name;
            materialIndices[name] = static_cast<int>(mesh.materials.size());
            mesh.materials.push_back({});
            current = &mesh.materials.back();
        }
        else if (current != nullptr && directive == "Ka")
            stream >> current->ambient.r >> current->ambient.g >> current->ambient.b;
        else if (current != nullptr && directive == "Kd")
            stream >> current->diffuse.r >> current->diffuse.g >> current->diffuse.b;
        else if (current != nullptr && directive == "Ks")
            stream >> current->specular.r >> current->specular.g >> current->specular.b;
        else if (current != nullptr && directive == "Ns")
            stream >> current->shininess;
        else if (current != nullptr && directive == "map_Kd")
        {
            std::string texturePath;
            std::getline(stream, texturePath);
            texturePath.erase(0, texturePath.find_first_not_of(" \t"));
            current->diffuseTexture = (path.parent_path() / normalizeTexturePath(texturePath)).string();
        }
    }
    return true;
}

unsigned int loadTexture(const std::string &path)
{
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(true);
    unsigned char *data = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (data == nullptr)
        return 0;

    unsigned int texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    stbi_image_free(data);
    return texture;
}
}

void GpuMesh::upload(const MeshData &mesh)
{
    destroy();
    if (mesh.vertices.empty() || mesh.indices.empty())
        return;

    createMeshBuffers(mesh.vertices, mesh.indices, vao, vbo, ebo);
    indexCount = static_cast<GLsizei>(mesh.indices.size());
    std::vector<glm::vec3> defaultColors(mesh.vertices.size(), glm::vec3(1.0f));
    glBindVertexArray(vao);
    glGenBuffers(1, &fmmColorVbo);
    glBindBuffer(GL_ARRAY_BUFFER, fmmColorVbo);
    glBufferData(GL_ARRAY_BUFFER, defaultColors.size() * sizeof(glm::vec3), defaultColors.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    materialTextures.reserve(mesh.materials.size());
    for (const MeshMaterial &material : mesh.materials)
        materialTextures.push_back(loadTexture(material.diffuseTexture));
}

void GpuMesh::updateFmmColors(const std::vector<glm::vec3> &colors)
{
    if (fmmColorVbo == 0)
        return;
    glBindBuffer(GL_ARRAY_BUFFER, fmmColorVbo);
    glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(glm::vec3), colors.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void GpuMesh::destroy()
{
    if (vao != 0)
        glDeleteVertexArrays(1, &vao);
    if (vbo != 0)
        glDeleteBuffers(1, &vbo);
    if (ebo != 0)
        glDeleteBuffers(1, &ebo);
    if (fmmColorVbo != 0)
        glDeleteBuffers(1, &fmmColorVbo);
    vao = 0;
    vbo = 0;
    ebo = 0;
    fmmColorVbo = 0;
    indexCount = 0;
    for (unsigned int texture : materialTextures)
        if (texture != 0)
            glDeleteTextures(1, &texture);
    materialTextures.clear();
}

bool loadMeshFromPly(const std::string &path, MeshData &mesh, std::string &error)
{
    MeshData loaded;
    if (!loadPLY(path, loaded.vertices, loaded.halfEdges, loaded.indices))
    {
        error = "No se pudo cargar el archivo PLY: " + path;
        return false;
    }

    mesh = std::move(loaded);
    return true;
}

bool loadMeshFromObjDirectory(const std::string &directory, MeshData &mesh, std::string &error)
{
    namespace fs = std::filesystem;
    const fs::path directoryPath(directory);
    std::error_code filesystemError;
    if (!fs::is_directory(directoryPath, filesystemError))
    {
        error = "La carpeta de escena no existe: " + directory;
        return false;
    }

    fs::path objPath;
    for (const fs::directory_entry &entry : fs::directory_iterator(directoryPath, filesystemError))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".obj")
        {
            objPath = entry.path();
            break;
        }
    }
    if (objPath.empty())
    {
        error = "La carpeta no contiene un archivo OBJ de escena.";
        return false;
    }

    std::ifstream input(objPath);
    if (!input)
    {
        error = "No se pudo abrir la escena: " + objPath.string();
        return false;
    }

    std::vector<glm::vec3> positions;
    std::vector<glm::vec2> texCoords;
    std::unordered_map<std::string, int> materialIndices;
    std::unordered_map<std::uint64_t, unsigned int> partVertexMap;
    int currentMaterialIndex = -1;
    MeshData loaded;
    std::string line;
    while (std::getline(input, line))
    {
        std::istringstream stream(line);
        std::string directive;
        stream >> directive;
        if (directive == "mtllib")
        {
            std::string materialFile;
            stream >> materialFile;
            loadMtlFile(objPath.parent_path() / normalizeTexturePath(materialFile), loaded, materialIndices);
        }
        else if (directive == "v")
        {
            glm::vec3 position;
            if (stream >> position.x >> position.y >> position.z)
                positions.push_back(position);
        }
        else if (directive == "vt")
        {
            glm::vec2 texCoord;
            if (stream >> texCoord.x >> texCoord.y)
                texCoords.push_back(texCoord);
        }
        else if (directive == "usemtl")
        {
            std::string materialName;
            stream >> materialName;
            const auto material = materialIndices.find(materialName);
            currentMaterialIndex = material == materialIndices.end() ? -1 : material->second;
        }
        else if (directive == "f")
        {
            struct ObjCorner
            {
                int position = 0;
                int texture = 0;
            };
            std::vector<ObjCorner> face;
            std::string token;
            while (stream >> token)
            {
                try
                {
                    const int index = resolveObjIndex(parseObjPositionIndex(token), positions.size());
                    if (index < 0 || index >= static_cast<int>(positions.size()))
                    {
                        error = "La escena contiene una referencia de vertice invalida.";
                        return false;
                    }
                    const int textureIndex = resolveObjIndex(parseObjTextureIndex(token), texCoords.size());
                    face.push_back({index, textureIndex});
                }
                catch (const std::exception &)
                {
                    error = "La escena contiene una cara OBJ invalida.";
                    return false;
                }
            }
            for (size_t index = 1; index + 1 < face.size(); ++index)
            {
                if (loaded.parts.empty() || loaded.parts.back().materialIndex != currentMaterialIndex)
                {
                    loaded.parts.push_back({loaded.indices.size(), 0, currentMaterialIndex});
                    partVertexMap.clear();
                }
                const ObjCorner triangle[] = {face[0], face[index], face[index + 1]};
                for (const ObjCorner &corner : triangle)
                {
                    const std::uint64_t key = (static_cast<std::uint64_t>(corner.position + 1) << 32) |
                                              static_cast<std::uint32_t>(corner.texture + 1);
                    auto [vertexIt, inserted] = partVertexMap.emplace(key, static_cast<unsigned int>(loaded.vertices.size()));
                    if (inserted)
                    {
                        const glm::vec2 texture = corner.texture >= 0 &&
                                                          corner.texture < static_cast<int>(texCoords.size())
                                                      ? texCoords[corner.texture]
                                                      : glm::vec2(0.0f);
                        const glm::vec3 &position = positions[corner.position];
                        loaded.vertices.push_back({position.x, position.y, position.z, texture.x, texture.y});
                    }
                    loaded.indices.push_back(vertexIt->second);
                    ++loaded.parts.back().indexCount;
                }
            }
        }
    }

    if (loaded.vertices.empty() || loaded.indices.empty())
    {
        error = "La escena no contiene una malla renderizable.";
        return false;
    }

    glm::vec3 minBound(1e9f);
    glm::vec3 maxBound(-1e9f);
    for (const Vertex &vertex : loaded.vertices)
    {
        minBound = glm::min(minBound, glm::vec3(vertex.x, vertex.y, vertex.z));
        maxBound = glm::max(maxBound, glm::vec3(vertex.x, vertex.y, vertex.z));
    }
    const glm::vec3 center = (minBound + maxBound) * 0.5f;
    const glm::vec3 dimensions = maxBound - minBound;
    const float extent = std::max({dimensions.x, dimensions.y, dimensions.z});
    if (extent <= 0.0f)
    {
        error = "La escena tiene dimensiones invalidas.";
        return false;
    }
    for (Vertex &vertex : loaded.vertices)
    {
        const glm::vec3 normalized = (glm::vec3(vertex.x, vertex.y, vertex.z) - center) * (3.0f / extent);
        vertex.x = normalized.x;
        vertex.y = normalized.y;
        vertex.z = normalized.z;
    }

    rebuildHalfEdges(loaded.halfEdges, loaded.indices);
    mesh = std::move(loaded);
    return true;
}

void buildCubeMesh(MeshData &mesh)
{
    mesh.vertices.clear();
    mesh.halfEdges.clear();
    mesh.indices.clear();
    std::vector<unsigned int> edgeIndices;
    buildCube(mesh.vertices, mesh.halfEdges, mesh.indices, edgeIndices);
}

void buildSphereMesh(MeshData &mesh, int slices, int stacks, float radius)
{
    mesh.vertices.clear();
    mesh.halfEdges.clear();
    mesh.indices.clear();
    std::vector<unsigned int> edgeIndices;
    buildSphere(slices, stacks, radius, mesh.vertices, mesh.halfEdges, mesh.indices, edgeIndices);
}

bool simplifyMeshData(MeshData &mesh, int targetFaces, std::string &error)
{
    const int currentFaces = static_cast<int>(mesh.indices.size() / 3);
    if (currentFaces == 0)
    {
        error = "La malla no tiene caras triangulares.";
        return false;
    }
    if (targetFaces < 1 || targetFaces >= currentFaces)
    {
        error = "El objetivo debe ser menor que el numero actual de caras.";
        return false;
    }

    simplifyMesh(mesh.vertices, mesh.halfEdges, mesh.indices, std::max(1, targetFaces));
    return true;
}