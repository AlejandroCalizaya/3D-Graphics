#ifndef SHARED_H
#define SHARED_H

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <iostream>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
#include <vector>
#include <queue>
#include <functional>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// ============================================================
// ESTRUCTURAS COMUNES
// ============================================================

struct Vertex
{
    float x, y, z;
};

struct Face
{
    int v1, v2, v3;
};

// Half-Edge: solo almacena 'to' y 'opposite'.
// 'from' se deriva de faceIndices: faceIndices[3*(i/3) + ((i%3)+2)%3]
struct CHE
{
    int to;         // Índice del vértice de destino
    int opposite;   // Índice de la media arista opuesta (twin)
};

// Punto 3D para operaciones geométricas
struct Point3
{
    float x, y, z;
};

// Estructura para QEM (Quadric Error Metrics)
struct Edge
{
    int u, v;            // Vértices involucrados
    float cost;          // Error E(v̄)
    glm::vec3 targetPos; // Posición óptima v̄

    bool operator>(const Edge &other) const
    {
        return cost > other.cost;
    }
};

// ============================================================
// GEOMETRY UTILITIES
// ============================================================

Point3 midpoint(const Point3 &a, const Point3 &b);
Vertex midpoint(const Vertex &a, const Vertex &b);

// ============================================================
// SHADER COMPILATION
// ============================================================

unsigned int compileShader(GLenum type, const char *source);
unsigned int createShaderProgram(const char *vertexSource, const char *fragmentSource);

// ============================================================
// HALF-EDGE DATA STRUCTURE (to/opposite)
// ============================================================

void addTriangle(
    std::vector<unsigned int> &faceIndices,
    std::vector<CHE> &halfEdges,
    int v1, int v2, int v3,
    std::map<std::tuple<int, int>, int> &halfEdgeMap);

void computeHalfEdgeTwins(
    std::vector<CHE> &halfEdges,
    const std::vector<unsigned int> &faceIndices,
    std::map<std::tuple<int, int>, int> &halfEdgeMap);

void extractEdgeIndices(
    const std::vector<CHE> &halfEdges,
    const std::vector<unsigned int> &faceIndices,
    std::vector<unsigned int> &edgeIndices);

void rebuildHalfEdges(
    std::vector<CHE> &halfEdges,
    const std::vector<unsigned int> &faceIndices);

// ============================================================
// MESH GENERATION
// ============================================================

void buildSphere(
    int numSlices, int numStacks,
    std::vector<Vertex> &vertices,
    std::vector<CHE> &halfEdges,
    std::vector<unsigned int> &faceIndices,
    std::vector<unsigned int> &edgeIndices);

void buildCube(
    std::vector<Vertex> &vertices,
    std::vector<CHE> &halfEdges,
    std::vector<unsigned int> &faceIndices,
    std::vector<unsigned int> &edgeIndices);

// ============================================================
// QEM SIMPLIFICATION
// ============================================================

Edge evaluateEdgeCollapse(
    int u, int v,
    const std::vector<Vertex> &vertices,
    const std::vector<glm::mat4> &vertexQ);

void simplifyMesh(
    std::vector<Vertex> &vertices,
    std::vector<CHE> &halfEdges,
    std::vector<unsigned int> &faceIndices,
    int targetFaces);

// ============================================================
// PLY LOADER
// ============================================================

bool loadPLY(
    const std::string &filename,
    std::vector<Vertex> &vertices,
    std::vector<CHE> &halfEdges,
    std::vector<unsigned int> &faceIndices);

// ============================================================
// BUFFER MANAGEMENT
// ============================================================

void createMeshBuffers(
    const std::vector<Vertex> &vertices,
    const std::vector<unsigned int> &indices,
    unsigned int &VAO, unsigned int &VBO, unsigned int &EBO);

void createVertexArray(
    const std::vector<float> &vertices,
    unsigned int &VAO, unsigned int &VBO);

void createVertexArrayWithIndices(
    const std::vector<float> &vertices,
    const std::vector<unsigned int> &indices,
    unsigned int &VAO, unsigned int &VBO, unsigned int &EBO);

// ============================================================
// INPUT HANDLING
// ============================================================

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

// ============================================================
// CAMERA UTILITIES
// ============================================================

glm::mat4 createViewMatrix(const glm::vec3 &cameraPos, const glm::vec3 &cameraFront, const glm::vec3 &cameraUp);
glm::mat4 createProjectionMatrix(float fov, float aspect, float nearPlane, float farPlane);

// ============================================================
// MESH CONVERSION
// ============================================================

std::vector<unsigned int> facesToIndices(const std::vector<Face> &faces);

#endif // SHARED_H
