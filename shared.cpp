#include "shared.h"

// ============================================================
// GEOMETRY UTILITIES
// ============================================================

Point3 midpoint(const Point3 &a, const Point3 &b)
{
    return {
        (a.x + b.x) * 0.5f,
        (a.y + b.y) * 0.5f,
        (a.z + b.z) * 0.5f,
    };
}

Vertex midpoint(const Vertex &a, const Vertex &b)
{
    return {
        (a.x + b.x) * 0.5f,
        (a.y + b.y) * 0.5f,
        (a.z + b.z) * 0.5f,
    };
}

// ============================================================
// SHADER COMPILATION
// ============================================================

unsigned int compileShader(GLenum type, const char *source)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    return shader;
}

unsigned int createShaderProgram(const char *vertexSource, const char *fragmentSource)
{
    unsigned int vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    unsigned int fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "ERROR::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

// ============================================================
// HALF-EDGE DATA STRUCTURE (to/opposite)
// ============================================================

void addTriangle(
    std::vector<unsigned int> &faceIndices,
    std::vector<CHE> &halfEdges,
    int v1, int v2, int v3,
    std::map<std::tuple<int, int>, int> &halfEdgeMap)
{
    int baseHe = static_cast<int>(halfEdges.size());

    halfEdges.push_back({v2, -1});
    halfEdges.push_back({v3, -1});
    halfEdges.push_back({v1, -1});

    halfEdgeMap[{v1, v2}] = baseHe;
    halfEdgeMap[{v2, v3}] = baseHe + 1;
    halfEdgeMap[{v3, v1}] = baseHe + 2;

    faceIndices.push_back(v1);
    faceIndices.push_back(v2);
    faceIndices.push_back(v3);
}

void computeHalfEdgeTwins(
    std::vector<CHE> &halfEdges,
    const std::vector<unsigned int> &faceIndices,
    std::map<std::tuple<int, int>, int> &halfEdgeMap)
{
    for (size_t i = 0; i < halfEdges.size(); ++i)
    {
        int to = halfEdges[i].to;
        int t = static_cast<int>(i / 3);
        int localIdx = static_cast<int>(i % 3);
        int from = faceIndices[3 * t + (localIdx + 2) % 3];

        auto it = halfEdgeMap.find({to, from});
        if (it != halfEdgeMap.end())
        {
            halfEdges[i].opposite = it->second;
            halfEdges[it->second].opposite = static_cast<int>(i);
        }
    }
}

void extractEdgeIndices(
    const std::vector<CHE> &halfEdges,
    const std::vector<unsigned int> &faceIndices,
    std::vector<unsigned int> &edgeIndices)
{
    for (size_t i = 0; i < halfEdges.size(); ++i)
    {
        int t = static_cast<int>(i / 3);
        int localIdx = static_cast<int>(i % 3);
        int from = faceIndices[3 * t + (localIdx + 2) % 3];
        int to = halfEdges[i].to;

        if (from < to || halfEdges[i].opposite == -1)
        {
            edgeIndices.push_back(from);
            edgeIndices.push_back(to);
        }
    }
}

void rebuildHalfEdges(
    std::vector<CHE> &halfEdges,
    const std::vector<unsigned int> &faceIndices)
{
    halfEdges.clear();
    std::map<std::tuple<int, int>, int> edgeMap;
    int numFaces = static_cast<int>(faceIndices.size() / 3);

    for (int t = 0; t < numFaces; ++t)
    {
        int v1 = faceIndices[3 * t];
        int v2 = faceIndices[3 * t + 1];
        int v3 = faceIndices[3 * t + 2];

        int baseHe = static_cast<int>(halfEdges.size());

        halfEdges.push_back({v2, -1});
        halfEdges.push_back({v3, -1});
        halfEdges.push_back({v1, -1});

        edgeMap[{v1, v2}] = baseHe;
        edgeMap[{v2, v3}] = baseHe + 1;
        edgeMap[{v3, v1}] = baseHe + 2;
    }

    computeHalfEdgeTwins(halfEdges, faceIndices, edgeMap);
}

// ============================================================
// MESH GENERATION
// ============================================================

void buildSphere(
    int numSlices, int numStacks,
    std::vector<Vertex> &vertices,
    std::vector<CHE> &halfEdges,
    std::vector<unsigned int> &faceIndices,
    std::vector<unsigned int> &edgeIndices)
{
    vertices.clear();
    halfEdges.clear();
    faceIndices.clear();
    edgeIndices.clear();

    std::map<std::tuple<int, int>, int> halfEdgeMap;

    // North pole
    vertices.push_back({0.0f, 0.0f, 1.0f});

    // Rings
    for (int i = 1; i < numStacks; ++i)
    {
        float phi = static_cast<float>(M_PI * i / numStacks);
        for (int j = 0; j < numSlices; ++j)
        {
            float theta = static_cast<float>(2.0 * M_PI * j / numSlices);
            float x = sin(phi) * cos(theta);
            float y = sin(phi) * sin(theta);
            float z = cos(phi);
            vertices.push_back({x, y, z});
        }
    }

    // South pole
    int southPole = static_cast<int>(vertices.size());
    vertices.push_back({0.0f, 0.0f, -1.0f});

    // Upper triangles
    for (int j = 0; j < numSlices; ++j)
    {
        int current = 1 + j;
        int next = 1 + (j + 1) % numSlices;
        addTriangle(faceIndices, halfEdges, 0, current, next, halfEdgeMap);
    }

    // Middle triangles
    for (int i = 0; i < numStacks - 2; ++i)
    {
        int currentRingStart = 1 + i * numSlices;
        int nextRingStart = currentRingStart + numSlices;
        for (int j = 0; j < numSlices; ++j)
        {
            int current = currentRingStart + j;
            int next = currentRingStart + (j + 1) % numSlices;
            int currentBelow = nextRingStart + j;
            int nextBelow = nextRingStart + (j + 1) % numSlices;

            addTriangle(faceIndices, halfEdges, current, currentBelow, next, halfEdgeMap);
            addTriangle(faceIndices, halfEdges, next, currentBelow, nextBelow, halfEdgeMap);
        }
    }

    // Lower triangles
    int lastRingStart = 1 + (numStacks - 2) * numSlices;
    for (int j = 0; j < numSlices; ++j)
    {
        int current = lastRingStart + j;
        int next = lastRingStart + (j + 1) % numSlices;
        addTriangle(faceIndices, halfEdges, current, southPole, next, halfEdgeMap);
    }

    computeHalfEdgeTwins(halfEdges, faceIndices, halfEdgeMap);
    extractEdgeIndices(halfEdges, faceIndices, edgeIndices);
}

void buildCube(
    std::vector<Vertex> &vertices,
    std::vector<CHE> &halfEdges,
    std::vector<unsigned int> &faceIndices,
    std::vector<unsigned int> &edgeIndices)
{
    vertices = {
        {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, -1.0f}, {-1.0f, 1.0f, -1.0f},
        {-1.0f, -1.0f, 1.0f},  {1.0f, -1.0f, 1.0f},  {1.0f, 1.0f, 1.0f},  {-1.0f, 1.0f, 1.0f}};

    std::map<std::tuple<int, int>, int> halfEdgeMap;

    std::vector<Face> cubeFaces = {
        {0, 1, 2}, {2, 3, 0},
        {4, 5, 6}, {6, 7, 4},
        {4, 5, 1}, {1, 0, 4},
        {7, 6, 2}, {2, 3, 7},
        {4, 7, 3}, {3, 0, 4},
        {5, 6, 2}, {2, 1, 5}};

    for (const auto &face : cubeFaces)
    {
        addTriangle(faceIndices, halfEdges, face.v1, face.v2, face.v3, halfEdgeMap);
    }

    computeHalfEdgeTwins(halfEdges, faceIndices, halfEdgeMap);
    extractEdgeIndices(halfEdges, faceIndices, edgeIndices);
}

// ============================================================
// QEM SIMPLIFICATION
// ============================================================

Edge evaluateEdgeCollapse(
    int u, int v,
    const std::vector<Vertex> &vertices,
    const std::vector<glm::mat4> &vertexQ)
{
    Edge edge;
    edge.u = u;
    edge.v = v;

    glm::mat4 Q_sum = vertexQ[u] + vertexQ[v];

    glm::mat4 sys = Q_sum;
    sys[0][3] = 0.0f;
    sys[1][3] = 0.0f;
    sys[2][3] = 0.0f;
    sys[3][3] = 1.0f;

    glm::vec4 b(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec3 bestPos;

    float det = glm::determinant(sys);

    if (std::abs(det) > 1e-5f)
    {
        glm::vec4 v_opt = glm::inverse(sys) * b;
        bestPos = glm::vec3(v_opt);
    }
    else
    {
        glm::vec3 p_u(vertices[u].x, vertices[u].y, vertices[u].z);
        glm::vec3 p_v(vertices[v].x, vertices[v].y, vertices[v].z);
        glm::vec3 p_mid = (p_u + p_v) * 0.5f;

        auto evalPos = [&](const glm::vec3 &p) {
            glm::vec4 v_h(p, 1.0f);
            return glm::dot(v_h, Q_sum * v_h);
        };

        float cost_u = evalPos(p_u);
        float cost_v = evalPos(p_v);
        float cost_mid = evalPos(p_mid);

        if (cost_mid <= cost_u && cost_mid <= cost_v) bestPos = p_mid;
        else if (cost_u <= cost_v) bestPos = p_u;
        else bestPos = p_v;
    }

    glm::vec4 v_homo(bestPos, 1.0f);
    edge.cost = glm::dot(v_homo, Q_sum * v_homo);
    edge.targetPos = bestPos;

    return edge;
}

void simplifyMesh(
    std::vector<Vertex> &vertices,
    std::vector<CHE> &halfEdges,
    std::vector<unsigned int> &faceIndices,
    int targetFaces)
{
    int numFaces = static_cast<int>(faceIndices.size() / 3);
    std::vector<bool> faceActive(numFaces, true);
    std::vector<bool> vertexActive(vertices.size(), true);

    std::vector<glm::mat4> vertexQ(vertices.size(), glm::mat4(0.0f));
    std::vector<glm::vec3> faceNormals(numFaces);
    std::vector<std::vector<int>> vertexFaces(vertices.size());

    for (int t = 0; t < numFaces; ++t)
    {
        int v1 = faceIndices[3 * t];
        int v2 = faceIndices[3 * t + 1];
        int v3 = faceIndices[3 * t + 2];

        vertexFaces[v1].push_back(t);
        vertexFaces[v2].push_back(t);
        vertexFaces[v3].push_back(t);

        glm::vec3 p1(vertices[v1].x, vertices[v1].y, vertices[v1].z);
        glm::vec3 p2(vertices[v2].x, vertices[v2].y, vertices[v2].z);
        glm::vec3 p3(vertices[v3].x, vertices[v3].y, vertices[v3].z);

        glm::vec3 normal = glm::cross(p2 - p1, p3 - p1);
        float len = glm::length(normal);
        if (len < 1e-8f) continue;
        normal /= len;
        faceNormals[t] = normal;

        float d = -glm::dot(normal, p1);
        glm::vec4 p(normal, d);
        glm::mat4 Kp = glm::outerProduct(p, p);

        vertexQ[v1] += Kp;
        vertexQ[v2] += Kp;
        vertexQ[v3] += Kp;
    }

    std::set<std::pair<int, int>> uniqueEdges;
    for (size_t i = 0; i < halfEdges.size(); ++i)
    {
        int t = static_cast<int>(i / 3);
        int localIdx = static_cast<int>(i % 3);
        int u = faceIndices[3 * t + (localIdx + 2) % 3];
        int v = halfEdges[i].to;

        if (u > v) std::swap(u, v);
        uniqueEdges.insert({u, v});
    }

    std::priority_queue<Edge, std::vector<Edge>, std::greater<Edge>> pq;
    for (const auto &e : uniqueEdges)
    {
        Edge edgeCollapse = evaluateEdgeCollapse(e.first, e.second, vertices, vertexQ);
        pq.push(edgeCollapse);
    }

    int currentFaces = numFaces;

    while (currentFaces > targetFaces && !pq.empty())
    {
        Edge edge = pq.top();
        pq.pop();

        int u = edge.u;
        int v = edge.v;

        if (!vertexActive[u] || !vertexActive[v]) continue;

        bool causesFlip = false;
        std::vector<int> affectedFaces = vertexFaces[u];
        affectedFaces.insert(affectedFaces.end(), vertexFaces[v].begin(), vertexFaces[v].end());

        for (int t : affectedFaces)
        {
            if (!faceActive[t]) continue;

            int i1 = faceIndices[3 * t];
            int i2 = faceIndices[3 * t + 1];
            int i3 = faceIndices[3 * t + 2];

            if ((i1 == u || i2 == u || i3 == u) && (i1 == v || i2 == v || i3 == v)) continue;

            glm::vec3 p1 = (i1 == u || i1 == v) ? edge.targetPos : glm::vec3(vertices[i1].x, vertices[i1].y, vertices[i1].z);
            glm::vec3 p2 = (i2 == u || i2 == v) ? edge.targetPos : glm::vec3(vertices[i2].x, vertices[i2].y, vertices[i2].z);
            glm::vec3 p3 = (i3 == u || i3 == v) ? edge.targetPos : glm::vec3(vertices[i3].x, vertices[i3].y, vertices[i3].z);

            glm::vec3 newNormal = glm::cross(p2 - p1, p3 - p1);
            if (glm::length(newNormal) > 1e-6f)
            {
                newNormal = glm::normalize(newNormal);
                if (glm::dot(newNormal, faceNormals[t]) < 0.2f)
                {
                    causesFlip = true;
                    break;
                }
            }
        }
        if (causesFlip) continue;

        vertices[u].x = edge.targetPos.x;
        vertices[u].y = edge.targetPos.y;
        vertices[u].z = edge.targetPos.z;
        vertexActive[v] = false;

        vertexQ[u] += vertexQ[v];

        std::set<int> neighborsOfU;

        for (int t : affectedFaces)
        {
            if (!faceActive[t]) continue;

            if (faceIndices[3 * t] == v) faceIndices[3 * t] = u;
            if (faceIndices[3 * t + 1] == v) faceIndices[3 * t + 1] = u;
            if (faceIndices[3 * t + 2] == v) faceIndices[3 * t + 2] = u;

            int idx1 = faceIndices[3 * t];
            int idx2 = faceIndices[3 * t + 1];
            int idx3 = faceIndices[3 * t + 2];

            if (idx1 == idx2 || idx2 == idx3 || idx3 == idx1)
            {
                faceActive[t] = false;
                currentFaces--;
            }
            else
            {
                vertexFaces[u].push_back(t);
                if (idx1 != u && vertexActive[idx1]) neighborsOfU.insert(idx1);
                if (idx2 != u && vertexActive[idx2]) neighborsOfU.insert(idx2);
                if (idx3 != u && vertexActive[idx3]) neighborsOfU.insert(idx3);

                glm::vec3 p1(vertices[idx1].x, vertices[idx1].y, vertices[idx1].z);
                glm::vec3 p2(vertices[idx2].x, vertices[idx2].y, vertices[idx2].z);
                glm::vec3 p3(vertices[idx3].x, vertices[idx3].y, vertices[idx3].z);
                faceNormals[t] = glm::normalize(glm::cross(p2 - p1, p3 - p1));
            }
        }

        for (int neighbor : neighborsOfU)
        {
            Edge updatedEdge = evaluateEdgeCollapse(u, neighbor, vertices, vertexQ);
            pq.push(updatedEdge);
        }
    }

    std::vector<unsigned int> newFaceIndices;
    for (int t = 0; t < numFaces; ++t)
    {
        if (faceActive[t])
        {
            newFaceIndices.push_back(faceIndices[3 * t]);
            newFaceIndices.push_back(faceIndices[3 * t + 1]);
            newFaceIndices.push_back(faceIndices[3 * t + 2]);
        }
    }
    faceIndices = newFaceIndices;

    rebuildHalfEdges(halfEdges, faceIndices);
}

// ============================================================
// PLY LOADER
// ============================================================

bool loadPLY(
    const std::string &filename,
    std::vector<Vertex> &vertices,
    std::vector<CHE> &halfEdges,
    std::vector<unsigned int> &faceIndices)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Error: No se pudo abrir el archivo .ply en la ruta: " << filename << std::endl;
        return false;
    }

    std::string line;
    int numVertices = 0;
    int numFaces = 0;
    bool headerEnded = false;

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string token;
        ss >> token;

        if (token == "element")
        {
            ss >> token;
            if (token == "vertex") ss >> numVertices;
            else if (token == "face") ss >> numFaces;
        }
        else if (token == "end_header")
        {
            headerEnded = true;
            break;
        }
    }

    if (!headerEnded) return false;

    vertices.clear();
    faceIndices.clear();

    glm::vec3 minBound(1e9f), maxBound(-1e9f);

    for (int i = 0; i < numVertices; ++i)
    {
        std::getline(file, line);
        std::stringstream ss(line);
        float x, y, z;
        ss >> x >> y >> z;

        vertices.push_back({x, y, z});

        minBound.x = std::min(minBound.x, x);
        minBound.y = std::min(minBound.y, y);
        minBound.z = std::min(minBound.z, z);

        maxBound.x = std::max(maxBound.x, x);
        maxBound.y = std::max(maxBound.y, y);
        maxBound.z = std::max(maxBound.z, z);
    }

    glm::vec3 center = (minBound + maxBound) * 0.5f;
    float maxDim = std::max({maxBound.x - minBound.x, maxBound.y - minBound.y, maxBound.z - minBound.z});
    float scale = 1.8f / maxDim;

    for (auto &v : vertices)
    {
        v.x = (v.x - center.x) * scale;
        v.y = (v.y - center.y) * scale;
        v.z = (v.z - center.z) * scale;
    }

    for (int i = 0; i < numFaces; ++i)
    {
        std::getline(file, line);
        std::stringstream ss(line);
        int nIndices;
        ss >> nIndices;

        if (nIndices == 3)
        {
            int idx0, idx1, idx2;
            ss >> idx0 >> idx1 >> idx2;
            faceIndices.push_back(idx0);
            faceIndices.push_back(idx1);
            faceIndices.push_back(idx2);
        }
    }

    rebuildHalfEdges(halfEdges, faceIndices);
    return true;
}

// ============================================================
// BUFFER MANAGEMENT
// ============================================================

void createMeshBuffers(
    const std::vector<Vertex> &vertices,
    const std::vector<unsigned int> &indices,
    unsigned int &VAO, unsigned int &VBO, unsigned int &EBO)
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void createVertexArray(
    const std::vector<float> &vertices,
    unsigned int &VAO, unsigned int &VBO)
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void createVertexArrayWithIndices(
    const std::vector<float> &vertices,
    const std::vector<unsigned int> &indices,
    unsigned int &VAO, unsigned int &VBO, unsigned int &EBO)
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

// ============================================================
// INPUT HANDLING
// ============================================================

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    glViewport(0, 0, width, height);
}

// ============================================================
// CAMERA UTILITIES
// ============================================================

glm::mat4 createViewMatrix(const glm::vec3 &cameraPos, const glm::vec3 &cameraFront, const glm::vec3 &cameraUp)
{
    return glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
}

glm::mat4 createProjectionMatrix(float fov, float aspect, float nearPlane, float farPlane)
{
    return glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
}

// ============================================================
// MESH CONVERSION
// ============================================================

std::vector<unsigned int> facesToIndices(const std::vector<Face> &faces)
{
    std::vector<unsigned int> indices;
    indices.reserve(faces.size() * 3);

    for (const auto &face : faces)
    {
        indices.push_back(face.v1);
        indices.push_back(face.v2);
        indices.push_back(face.v3);
    }

    return indices;
}
