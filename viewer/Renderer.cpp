#include "Renderer.h"

namespace
{
const char *vertexShader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aFmmColor;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec3 FragPos;
out vec2 TexCoord;
out vec3 FmmColor;
void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    TexCoord = aTexCoord;
    FmmColor = aFmmColor;
    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
)";

const char *fragmentShader = R"(
#version 330 core
in vec3 FragPos;
in vec2 TexCoord;
in vec3 FmmColor;
out vec4 FragColor;
uniform vec3 color;
uniform vec3 cameraPos;
uniform vec3 materialAmbient;
uniform vec3 materialDiffuse;
uniform vec3 materialSpecular;
uniform float materialShininess;
uniform sampler2D diffuseTexture;
uniform bool hasDiffuseTexture;
uniform bool hasFmmColor;
uniform bool markerPass;
void main()
{
    if (markerPass)
    {
        FragColor = vec4(1.0);
        return;
    }
    vec3 normal = normalize(cross(dFdx(FragPos), dFdy(FragPos)));
    vec3 lightDir = normalize(cameraPos - FragPos);
    float diffuse = max(dot(normal, lightDir), 0.0);
    if (hasFmmColor)
    {
        FragColor = vec4(FmmColor, 1.0);
        return;
    }
    vec3 baseColor = hasDiffuseTexture ? texture(diffuseTexture, TexCoord).rgb : materialDiffuse;
    vec3 ambient = baseColor * 0.22 + materialAmbient * 0.18;
    float specular = pow(max(dot(normal, lightDir), 0.0), max(materialShininess, 1.0));
    vec3 tint = hasFmmColor ? vec3(1.0) : color;
    FragColor = vec4((ambient + baseColor * diffuse * 0.78 + materialSpecular * specular * 0.1) * tint, 1.0);
}
)";
}

bool ViewerRenderer::initialize()
{
    shaderProgram_ = createShaderProgram(vertexShader, fragmentShader);
    if (shaderProgram_ == 0)
        return false;

    modelLocation_ = glGetUniformLocation(shaderProgram_, "model");
    viewLocation_ = glGetUniformLocation(shaderProgram_, "view");
    projectionLocation_ = glGetUniformLocation(shaderProgram_, "projection");
    colorLocation_ = glGetUniformLocation(shaderProgram_, "color");
    cameraLocation_ = glGetUniformLocation(shaderProgram_, "cameraPos");
    return true;
}

void ViewerRenderer::destroy()
{
    if (shaderProgram_ != 0)
        glDeleteProgram(shaderProgram_);
    shaderProgram_ = 0;
}

void ViewerRenderer::render(const std::vector<SceneObject> &objects, const ViewerCamera &camera,
                            const RenderSettings &settings, int width, int height)
{
    glViewport(0, 0, width, height);
    glClearColor(settings.clearColor.r, settings.clearColor.g, settings.clearColor.b, settings.clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (settings.depthTest)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);

    glPolygonMode(GL_FRONT_AND_BACK, settings.wireframe ? GL_LINE : GL_FILL);
    glUseProgram(shaderProgram_);

    const float aspect = height > 0 ? static_cast<float>(width) / static_cast<float>(height) : 1.0f;
    const glm::mat4 view = camera.viewMatrix();
    const glm::mat4 projection = camera.projectionMatrix(aspect);
    const glm::vec3 cameraPosition = camera.position();
    glUniformMatrix4fv(viewLocation_, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projectionLocation_, 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(cameraLocation_, 1, glm::value_ptr(cameraPosition));

    for (const SceneObject &object : objects)
    {
        if (!object.visible || object.gpu.vao == 0)
            continue;

        glm::mat4 model(1.0f);
        model = glm::translate(model, object.position);
        model = glm::rotate(model, glm::radians(object.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, glm::radians(object.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, glm::radians(object.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::scale(model, object.scale);

        glUniformMatrix4fv(modelLocation_, 1, GL_FALSE, glm::value_ptr(model));
        glBindVertexArray(object.gpu.vao);
        const bool hasFmmColor = object.fmmEnabled && object.fmmSource >= 0;
        const auto drawPart = [&](size_t firstIndex, size_t partIndexCount, int materialIndex, int partIndex) {
            glm::vec3 ambient(0.2f);
            glm::vec3 diffuse(0.8f);
            glm::vec3 specular(0.0f);
            float shininess = 32.0f;
            bool hasTexture = false;
            unsigned int texture = 0;
            const bool drawFmmColor = hasFmmColor &&
                                      (object.mesh.parts.empty() || partIndex == object.fmmPart);
            if (materialIndex >= 0 && materialIndex < static_cast<int>(object.mesh.materials.size()) &&
                materialIndex < static_cast<int>(object.gpu.materialTextures.size()))
            {
                const MeshMaterial &material = object.mesh.materials[materialIndex];
                ambient = material.ambient;
                diffuse = material.diffuse;
                specular = material.specular;
                shininess = material.shininess;
                texture = object.gpu.materialTextures[materialIndex];
                hasTexture = texture != 0;
            }
            glUniform3fv(colorLocation_, 1, glm::value_ptr(glm::vec3(object.color)));
            glUniform3fv(glGetUniformLocation(shaderProgram_, "materialAmbient"), 1, glm::value_ptr(ambient));
            glUniform3fv(glGetUniformLocation(shaderProgram_, "materialDiffuse"), 1, glm::value_ptr(diffuse));
            glUniform3fv(glGetUniformLocation(shaderProgram_, "materialSpecular"), 1, glm::value_ptr(specular));
            glUniform1f(glGetUniformLocation(shaderProgram_, "materialShininess"), shininess);
            glUniform1i(glGetUniformLocation(shaderProgram_, "hasDiffuseTexture"), hasTexture);
            glUniform1i(glGetUniformLocation(shaderProgram_, "hasFmmColor"), drawFmmColor);
            glUniform1i(glGetUniformLocation(shaderProgram_, "markerPass"), false);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);
            glUniform1i(glGetUniformLocation(shaderProgram_, "diffuseTexture"), 0);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(partIndexCount), GL_UNSIGNED_INT,
                           reinterpret_cast<const void *>(firstIndex * sizeof(unsigned int)));
        };
        if (object.mesh.parts.empty())
            drawPart(0, object.gpu.indexCount, -1, -1);
        else
            for (size_t partIndex = 0; partIndex < object.mesh.parts.size(); ++partIndex)
            {
                const MeshPart &part = object.mesh.parts[partIndex];
                drawPart(part.firstIndex, part.indexCount, part.materialIndex, static_cast<int>(partIndex));
            }

        if (hasFmmColor && object.fmmSource < static_cast<int>(object.mesh.vertices.size()))
        {
            glUniform1i(glGetUniformLocation(shaderProgram_, "markerPass"), true);
            glPointSize(12.0f);
            glDrawArrays(GL_POINTS, object.fmmSource, 1);
            glUniform1i(glGetUniformLocation(shaderProgram_, "markerPass"), false);
        }
    }

    glBindVertexArray(0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}