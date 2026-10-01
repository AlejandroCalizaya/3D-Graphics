#include "Ui.h"
#include "../S3/fast_marching/fast_marching.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <limits>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>

#include "imgui.h"

namespace
{
void setStatus(ViewerUiState &ui, const std::string &message)
{
    std::snprintf(ui.status, sizeof(ui.status), "%s", message.c_str());
}

void createObject(std::vector<SceneObject> &objects, const std::string &name, MeshData mesh, const glm::vec4 &color)
{
    SceneObject object;
    object.name = name;
    object.mesh = std::move(mesh);
    object.originalMesh = object.mesh;
    object.color = color;
    object.gpu.upload(object.mesh);
    objects.push_back(std::move(object));
}

void addExampleScene(std::vector<SceneObject> &objects)
{
    MeshData floorMesh;
    buildCubeMesh(floorMesh);
    createObject(objects, "Escena - suelo", std::move(floorMesh), {0.32f, 0.36f, 0.40f, 1.0f});
    objects.back().position = {0.0f, -0.9f, 0.0f};
    objects.back().scale = {3.0f, 0.1f, 3.0f};

    MeshData centerMesh;
    buildSphereMesh(centerMesh);
    createObject(objects, "Escena - esfera", std::move(centerMesh), {0.15f, 0.55f, 0.85f, 1.0f});
    objects.back().position = {0.0f, 0.15f, 0.0f};
    objects.back().scale = {0.75f, 0.75f, 0.75f};

    MeshData sideMesh;
    buildCubeMesh(sideMesh);
    createObject(objects, "Escena - cubo", std::move(sideMesh), {0.9f, 0.35f, 0.12f, 1.0f});
    objects.back().position = {1.35f, -0.25f, 0.0f};
    objects.back().scale = {0.45f, 0.65f, 0.45f};
}

bool hasPlyExtension(const std::filesystem::path &path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return extension == ".ply";
}

bool isDirectory(const std::filesystem::path &path)
{
    std::error_code error;
    return std::filesystem::is_directory(path, error);
}

bool isPlyFile(const std::filesystem::path &path)
{
    std::error_code error;
    return std::filesystem::is_regular_file(path, error) && hasPlyExtension(path);
}

void loadSelectedPly(std::vector<SceneObject> &objects, ViewerUiState &ui)
{
    MeshData mesh;
    std::string error;
    if (loadMeshFromPly(ui.selectedPlyPath, mesh, error))
    {
        std::snprintf(ui.plyPath, sizeof(ui.plyPath), "%s", ui.selectedPlyPath);
        createObject(objects, ui.selectedPlyPath, std::move(mesh), {0.72f, 0.58f, 0.36f, 1.0f});
        ui.selectedObject = static_cast<int>(objects.size()) - 1;
        setStatus(ui, "PLY anadido correctamente.");
        ui.showPlyBrowser = false;
    }
    else
        setStatus(ui, error);
}

void loadSelectedScene(std::vector<SceneObject> &objects, ViewerUiState &ui)
{
    MeshData mesh;
    std::string error;
    if (loadMeshFromObjDirectory(ui.sceneDirectory.string(), mesh, error))
    {
        createObject(objects, "Escena - " + ui.sceneDirectory.filename().string(), std::move(mesh),
                     {0.72f, 0.58f, 0.36f, 1.0f});
        ui.selectedObject = static_cast<int>(objects.size()) - 1;
        setStatus(ui, "Escena anadida correctamente.");
        ui.showSceneBrowser = false;
    }
    else
        setStatus(ui, error);
}

void updateFastMarching(SceneObject &object, float slowness)
{
    if (object.fmmSource < 0 || object.fmmPart >= static_cast<int>(object.mesh.parts.size()))
        return;

    const MeshPart *part = object.fmmPart >= 0 ? &object.mesh.parts[object.fmmPart] : nullptr;
    const size_t firstIndex = part != nullptr ? part->firstIndex : 0;
    const size_t indexCount = part != nullptr ? part->indexCount : object.mesh.indices.size();
    std::unordered_map<unsigned int, int> localIndices;
    std::vector<Vertex> localVertices;
    std::vector<unsigned int> localFaceIndices;
    for (size_t offset = 0; offset < indexCount; ++offset)
    {
        const unsigned int globalIndex = object.mesh.indices[firstIndex + offset];
        auto [localIt, inserted] = localIndices.emplace(globalIndex, static_cast<int>(localVertices.size()));
        if (inserted)
            localVertices.push_back(object.mesh.vertices[globalIndex]);
        localFaceIndices.push_back(static_cast<unsigned int>(localIt->second));
    }

    std::vector<CHE> localHalfEdges;
    rebuildHalfEdges(localHalfEdges, localFaceIndices);
    std::vector<std::vector<int>> adjacency;
    std::unordered_set<std::uint64_t> edges;
    FMM::buildTopology(localHalfEdges, localFaceIndices, adjacency, edges);
    const int localSource = localIndices[static_cast<unsigned int>(object.fmmSource)];
    const FMM::FMMResult result = FMM::fastMarching(localVertices, adjacency, edges, {localSource}, slowness);
    const float range = std::max(result.maxU - result.minU, 1e-9f);
    std::vector<glm::vec3> colors(object.mesh.vertices.size(), glm::vec3(1.0f));
    for (const auto &[globalIndex, localIndex] : localIndices)
    {
        if (std::isfinite(result.U[localIndex]))
            colors[globalIndex] = FMM::jet((result.U[localIndex] - result.minU) / range);
    }
    object.gpu.updateFmmColors(colors);
    object.fmmEnabled = true;
}

bool intersectRayTriangle(const glm::vec3 &origin, const glm::vec3 &direction,
                          const glm::vec3 &a, const glm::vec3 &b, const glm::vec3 &c,
                          float &distance, glm::vec3 &hit)
{
    const glm::vec3 edge1 = b - a;
    const glm::vec3 edge2 = c - a;
    const glm::vec3 pvec = glm::cross(direction, edge2);
    const float determinant = glm::dot(edge1, pvec);
    if (std::abs(determinant) < 1e-7f)
        return false;

    const float inverseDeterminant = 1.0f / determinant;
    const glm::vec3 tvec = origin - a;
    const float u = glm::dot(tvec, pvec) * inverseDeterminant;
    if (u < 0.0f || u > 1.0f)
        return false;
    const glm::vec3 qvec = glm::cross(tvec, edge1);
    const float v = glm::dot(direction, qvec) * inverseDeterminant;
    if (v < 0.0f || u + v > 1.0f)
        return false;

    distance = glm::dot(edge2, qvec) * inverseDeterminant;
    if (distance <= 0.0f)
        return false;
    hit = origin + direction * distance;
    return true;
}
}

bool ViewerUi::draw(std::vector<SceneObject> &objects, ViewerUiState &ui, RenderSettings &settings,
                   ViewerCamera &camera)
{
    bool shouldClose = false;

    ImGui::Begin("3D Graphics Viewer");
    if (ImGui::Button("Salir"))
        shouldClose = true;
    ImGui::SameLine();
    ImGui::TextUnformatted(ui.status);

    if (ImGui::CollapsingHeader("Escena", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Crear cubo"))
        {
            MeshData mesh;
            buildCubeMesh(mesh);
            createObject(objects, "Cubo " + std::to_string(objects.size() + 1), std::move(mesh), {0.9f, 0.35f, 0.12f, 1.0f});
            ui.selectedObject = static_cast<int>(objects.size()) - 1;
            setStatus(ui, "Cubo creado.");
        }
        ImGui::SameLine();
        if (ImGui::Button("Crear esfera"))
        {
            MeshData mesh;
            buildSphereMesh(mesh);
            createObject(objects, "Esfera " + std::to_string(objects.size() + 1), std::move(mesh), {0.15f, 0.45f, 0.85f, 1.0f});
            ui.selectedObject = static_cast<int>(objects.size()) - 1;
            setStatus(ui, "Esfera creada.");
        }

        ImGui::SameLine();
        if (ImGui::Button("Cargar escena"))
        {
            ui.sceneDirectory = ".";
            ui.showSceneBrowser = true;
        }

        ImGui::InputText("Archivo PLY", ui.plyPath, sizeof(ui.plyPath));
        if (ImGui::Button("Añadir PLY"))
        {
            const std::filesystem::path typedPath(ui.plyPath);
            if (isDirectory(typedPath))
            {
                ui.plyDirectory = typedPath;
                ui.selectedPlyPath[0] = '\0';
            }
            else
            {
                ui.plyDirectory = typedPath.has_parent_path() ? typedPath.parent_path() : ".";
                std::snprintf(ui.selectedPlyPath, sizeof(ui.selectedPlyPath), "%s", ui.plyPath);
            }
            ui.showPlyBrowser = true;
        }

        if (ui.showPlyBrowser && ImGui::Begin("Seleccionar PLY", &ui.showPlyBrowser,
                                              ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Carpeta: %s", ui.plyDirectory.string().c_str());
            if (ui.plyDirectory.has_parent_path() && ImGui::Button(".."))
                ui.plyDirectory = ui.plyDirectory.parent_path();

            std::vector<std::filesystem::directory_entry> entries;
            std::error_code directoryError;
            for (const auto &entry : std::filesystem::directory_iterator(ui.plyDirectory, directoryError))
            {
                if (entry.is_directory() || hasPlyExtension(entry.path()))
                    entries.push_back(entry);
            }
            std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
                const bool leftIsDirectory = isDirectory(left.path());
                const bool rightIsDirectory = isDirectory(right.path());
                if (leftIsDirectory != rightIsDirectory)
                    return leftIsDirectory;
                return left.path().filename().string() < right.path().filename().string();
            });

            for (const auto &entry : entries)
            {
                const std::string name = entry.path().filename().string();
                const bool entryIsDirectory = isDirectory(entry.path());
                const std::string label = (entryIsDirectory ? "[DIR] " : "      ") + name;
                const bool selected = std::filesystem::path(ui.selectedPlyPath) == entry.path();
                if (ImGui::Selectable(label.c_str(), selected))
                {
                    if (entryIsDirectory)
                    {
                        ui.plyDirectory = entry.path();
                        ui.selectedPlyPath[0] = '\0';
                    }
                    else
                        std::snprintf(ui.selectedPlyPath, sizeof(ui.selectedPlyPath), "%s",
                                      entry.path().string().c_str());
                }
            }

            ImGui::InputText("Seleccionado", ui.selectedPlyPath, sizeof(ui.selectedPlyPath));
            if (ImGui::Button("Abrir") && isPlyFile(std::filesystem::path(ui.selectedPlyPath)))
                loadSelectedPly(objects, ui);
            ImGui::SameLine();
            if (ImGui::Button("Cancelar"))
                ui.showPlyBrowser = false;
            ImGui::End();
        }

        if (ui.showSceneBrowser && ImGui::Begin("Seleccionar escena", &ui.showSceneBrowser,
                                                ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Carpeta de escena: %s", ui.sceneDirectory.string().c_str());
            if (ui.sceneDirectory.has_parent_path() && ImGui::Button(".."))
                ui.sceneDirectory = ui.sceneDirectory.parent_path();

            std::vector<std::filesystem::directory_entry> entries;
            std::error_code directoryError;
            for (const auto &entry : std::filesystem::directory_iterator(ui.sceneDirectory, directoryError))
            {
                if (isDirectory(entry.path()) || entry.path().extension() == ".obj")
                    entries.push_back(entry);
            }
            std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
                const bool leftIsDirectory = isDirectory(left.path());
                const bool rightIsDirectory = isDirectory(right.path());
                if (leftIsDirectory != rightIsDirectory)
                    return leftIsDirectory;
                return left.path().filename().string() < right.path().filename().string();
            });

            for (const auto &entry : entries)
            {
                const bool entryIsDirectory = isDirectory(entry.path());
                const std::string name = entry.path().filename().string();
                const std::string label = (entryIsDirectory ? "[DIR] " : "      ") + name;
                if (ImGui::Selectable(label.c_str()))
                {
                    if (entryIsDirectory)
                        ui.sceneDirectory = entry.path();
                }
            }

            if (ImGui::Button("Confirmar carpeta"))
                loadSelectedScene(objects, ui);
            ImGui::SameLine();
            if (ImGui::Button("Cancelar"))
                ui.showSceneBrowser = false;
            ImGui::End();
        }

        if (!objects.empty())
        {
            std::vector<const char *> names;
            names.reserve(objects.size());
            for (const SceneObject &object : objects)
                names.push_back(object.name.c_str());
            ui.selectedObject = glm::clamp(ui.selectedObject, 0, static_cast<int>(objects.size()) - 1);
            ImGui::ListBox("Objetos", &ui.selectedObject, names.data(), static_cast<int>(names.size()), 5);
            if (ImGui::Button("Eliminar seleccionado"))
            {
                objects[ui.selectedObject].gpu.destroy();
                objects.erase(objects.begin() + ui.selectedObject);
                ui.selectedObject = glm::max(0, ui.selectedObject - 1);
                setStatus(ui, "Objeto eliminado.");
            }
        }
    }

    if (!objects.empty())
    {
        SceneObject &object = objects[ui.selectedObject];
        if (ImGui::CollapsingHeader("Transformacion", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::InputFloat3("Posicion", &object.position.x, "%.3f");
            ImGui::InputFloat3("Rotacion", &object.rotation.x, "%.2f");
            ImGui::InputFloat3("Escala", &object.scale.x, "%.3f");
            object.scale = glm::max(object.scale, glm::vec3(0.001f));
            ImGui::ColorEdit3("Color", &object.color.x);
            ImGui::Checkbox("Visible", &object.visible);
        }

        if (ImGui::CollapsingHeader("Fast Marching", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const bool previousEnabled = ui.fastMarchingEnabled;
            ImGui::Checkbox("Visualizar Fast Marching", &ui.fastMarchingEnabled);
            ImGui::SliderFloat("Velocidad", &ui.fastMarchingSlowness, 0.1f, 5.0f);
            if (ui.fastMarchingEnabled != previousEnabled)
            {
                object.fmmEnabled = false;
                if (ui.fastMarchingEnabled)
                    setStatus(ui, "Haz clic sobre la malla para elegir el vertice de inicio.");
            }
            if (ui.fastMarchingEnabled && object.fmmSource >= 0)
            {
                ImGui::Text("Vertice de inicio: %d", object.fmmSource);
                if (ImGui::Button("Recalcular"))
                    updateFastMarching(object, ui.fastMarchingSlowness);
            }
            else
                ImGui::TextUnformatted("Haz clic sobre la malla para elegir el inicio.");
        }

        if (ImGui::CollapsingHeader("Malla", ImGuiTreeNodeFlags_DefaultOpen))
        {
            const int faceCount = static_cast<int>(object.mesh.indices.size() / 3);
            ImGui::Text("Vertices: %d", static_cast<int>(object.mesh.vertices.size()));
            ImGui::Text("Caras: %d", faceCount);
            ImGui::InputInt("Caras objetivo", &ui.targetFaces);
            ui.targetFaces = glm::clamp(ui.targetFaces, 1, glm::max(1, faceCount - 1));
            if (ImGui::Button("Simplificar QEM") && faceCount > 1)
            {
                std::string error;
                if (simplifyMeshData(object.mesh, ui.targetFaces, error))
                {
                    object.gpu.upload(object.mesh);
                    object.fmmSource = -1;
                    object.fmmPart = -1;
                    object.fmmEnabled = false;
                    setStatus(ui, "Malla simplificada.");
                }
                else
                    setStatus(ui, error);
            }
            ImGui::SameLine();
            if (ImGui::Button("Restaurar original"))
            {
                object.mesh = object.originalMesh;
                object.gpu.upload(object.mesh);
                object.fmmSource = -1;
                object.fmmPart = -1;
                object.fmmEnabled = false;
                setStatus(ui, "Malla original restaurada.");
            }
        }
    }

    if (ImGui::CollapsingHeader("Camara", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Perspectiva", &camera.perspective);
        ImGui::SliderFloat("Campo de vision", &camera.fov, 20.0f, 100.0f);
        if (ImGui::Button("Restaurar camara"))
            camera.reset();
    }

    if (ImGui::CollapsingHeader("Visualizacion", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Wireframe", &settings.wireframe);
        ImGui::Checkbox("Depth test", &settings.depthTest);
        ImGui::ColorEdit3("Fondo", &settings.clearColor.x);
        ImGui::Checkbox("Mostrar demo de ImGui", &ui.showDemo);
    }

    ImGui::End();

    if (ui.showDemo)
        ImGui::ShowDemoWindow(&ui.showDemo);
    return shouldClose;
}

bool selectFastMarchingVertex(std::vector<SceneObject> &objects, ViewerUiState &ui,
                              const ViewerCamera &camera, int width, int height,
                              double cursorX, double cursorY)
{
    if (!ui.fastMarchingEnabled || objects.empty() || width <= 0 || height <= 0)
        return false;

    const int objectIndex = glm::clamp(ui.selectedObject, 0, static_cast<int>(objects.size()) - 1);
    SceneObject &object = objects[objectIndex];
    glm::mat4 model(1.0f);
    model = glm::translate(model, object.position);
    model = glm::rotate(model, glm::radians(object.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(object.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(object.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
    model = glm::scale(model, object.scale);
    const glm::mat4 view = camera.viewMatrix();
    const glm::mat4 projection = camera.projectionMatrix(static_cast<float>(width) / height);
    const glm::mat4 inverseViewProjection = glm::inverse(projection * view);
    const glm::vec2 normalizedCursor(static_cast<float>(2.0 * cursorX / width - 1.0),
                                     static_cast<float>(1.0 - 2.0 * cursorY / height));
    const glm::vec4 nearPoint = inverseViewProjection * glm::vec4(normalizedCursor, -1.0f, 1.0f);
    const glm::vec4 farPoint = inverseViewProjection * glm::vec4(normalizedCursor, 1.0f, 1.0f);
    const glm::vec3 rayOrigin = glm::vec3(nearPoint) / nearPoint.w;
    const glm::vec3 rayEnd = glm::vec3(farPoint) / farPoint.w;
    const glm::vec3 rayDirection = glm::normalize(rayEnd - rayOrigin);

    float nearestDistance = std::numeric_limits<float>::max();
    int nearestVertex = -1;
    int nearestPart = -1;
    for (size_t index = 0; index + 2 < object.mesh.indices.size(); index += 3)
    {
        const unsigned int indexA = object.mesh.indices[index];
        const unsigned int indexB = object.mesh.indices[index + 1];
        const unsigned int indexC = object.mesh.indices[index + 2];
        const Vertex &vertexA = object.mesh.vertices[indexA];
        const Vertex &vertexB = object.mesh.vertices[indexB];
        const Vertex &vertexC = object.mesh.vertices[indexC];
        const glm::vec3 pointA = glm::vec3(model * glm::vec4(vertexA.x, vertexA.y, vertexA.z, 1.0f));
        const glm::vec3 pointB = glm::vec3(model * glm::vec4(vertexB.x, vertexB.y, vertexB.z, 1.0f));
        const glm::vec3 pointC = glm::vec3(model * glm::vec4(vertexC.x, vertexC.y, vertexC.z, 1.0f));
        float distance;
        glm::vec3 hit;
        if (!intersectRayTriangle(rayOrigin, rayDirection, pointA, pointB, pointC, distance, hit) ||
            distance >= nearestDistance)
            continue;

        nearestDistance = distance;
        const float distanceA = glm::distance(hit, pointA);
        const float distanceB = glm::distance(hit, pointB);
        const float distanceC = glm::distance(hit, pointC);
        nearestVertex = distanceA < distanceB && distanceA < distanceC ? static_cast<int>(indexA)
                                                                        : distanceB < distanceC
                                                                              ? static_cast<int>(indexB)
                                                                              : static_cast<int>(indexC);
        nearestPart = -1;
        for (size_t partIndex = 0; partIndex < object.mesh.parts.size(); ++partIndex)
        {
            const MeshPart &part = object.mesh.parts[partIndex];
            if (index >= part.firstIndex && index < part.firstIndex + part.indexCount)
            {
                nearestPart = static_cast<int>(partIndex);
                break;
            }
        }
    }

    if (nearestVertex < 0)
        return false;
    object.fmmSource = nearestVertex;
    object.fmmPart = nearestPart;
    updateFastMarching(object, ui.fastMarchingSlowness);
    setStatus(ui, "Vertice de inicio seleccionado: " + std::to_string(nearestVertex));
    return true;
}