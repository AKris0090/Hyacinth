#include <iostream>
#include <filesystem>
#include <windows.h>
#include <iostream>
#include <fstream>

#include "xatlas.h"
#include "light_loader.h"

#pragma comment(lib, "Hyacinth-Physics.lib")

class Stopwatch {
public:
    Stopwatch() { reset(); }
    void reset() { m_start = clock(); }
    double elapsed() const { return (clock() - m_start) * 1000.0 / CLOCKS_PER_SEC; }
private:
    clock_t m_start;
};

static int Print(const char* format, ...)
{
    va_list arg;
    va_start(arg, format);
    printf("\r"); // Clear progress text.
    const int result = vprintf(format, arg);
    va_end(arg);
    return result;
}

static bool ProgressCallback(xatlas::ProgressCategory category, int progress, void* userData)
{
    // Don't interupt verbose printing.
    if (true)
        return true;
    Stopwatch* stopwatch = (Stopwatch*)userData;
    static std::mutex progressMutex;
    std::unique_lock<std::mutex> lock(progressMutex);
    if (progress == 0)
        stopwatch->reset();
    printf("\r   %s [", xatlas::StringForEnum(category));
    for (int i = 0; i < 10; i++)
        printf(progress / ((i + 1) * 10) ? "*" : " ");
    printf("] %d%%", progress);
    fflush(stdout);
    if (progress == 100)
        printf("\n      %.2f seconds (%g ms) elapsed\n", stopwatch->elapsed() / 1000.0, stopwatch->elapsed());
    return true;
}

static std::filesystem::path getExeDir() {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return std::filesystem::path(buffer).parent_path();
}

int addToAtlas(LightNode* node, std::vector<LightPrimitive*>& primitives, xatlas::Atlas* atlas) {
    for (auto& p : node->primitives) {
        xatlas::MeshDecl meshDecl;
        meshDecl.vertexCount = (uint32_t) p->vertices.size();
        meshDecl.vertexPositionData = p->vertices.data();
        meshDecl.vertexPositionStride = sizeof(glm::vec3);
        meshDecl.vertexNormalData = p->normals.data();
        meshDecl.vertexNormalStride = sizeof(glm::vec3);
        meshDecl.vertexUvData = p->uvs.data();
        meshDecl.vertexUvStride = sizeof(glm::vec2);
        meshDecl.indexCount = (uint32_t)p->indices.size();
        meshDecl.indexData = p->indices.data();
        meshDecl.indexFormat = xatlas::IndexFormat::UInt32;

        xatlas::AddMeshError error = xatlas::AddMesh(atlas, meshDecl, (uint32_t)node->primitives.size());
        if (error != xatlas::AddMeshError::Success) {
            xatlas::Destroy(atlas);
            std::cout << "[BAKERY] Error adding primitive to atlas: " << xatlas::StringForEnum(error) << std::endl;
            return 1;
        }
        primitives.push_back(p);
    }
    for (auto& n : node->children) {
        int res = addToAtlas(n, primitives, atlas);
        if (res > 0) {
            return res;
        }
    }
    return 0;
}

int main(int argc, char* argv[])
{
    // load the static geometry
    LightLoader lightLoader;
    auto path = getExeDir() / "objects" / "test_scene.glb";
    LightObject* obj = lightLoader.loadFromFile(path.string(), false);
 
    xatlas::SetPrint(Print, true);
    xatlas::Atlas* atlas = xatlas::Create();

    Stopwatch globalStopwatch, stopwatch;
    xatlas::SetProgressCallback(atlas, ProgressCallback, &stopwatch);

    int res = 0;
    std::vector<LightPrimitive*> primitives;
    for (auto& n : obj->parentNodes) {
        res = addToAtlas(n, primitives, atlas);
    }
    if (res > 0) {
        std::cout << "[BAKERY] Error with atlas" << std::endl;
        return 1;
    }

    std::cout << std::endl << "[BAKERY] Generating atlas..." << std::endl;
    xatlas::PackOptions packOptions;
    packOptions.padding = 2;
    packOptions.resolution = 4096;
    xatlas::ChartOptions chartOptions;
    xatlas::Generate(atlas, chartOptions, packOptions);

    for (uint32_t ind = 0; ind < atlas->meshCount; ind++) {
        xatlas::Mesh& m = atlas->meshes[ind];
        LightPrimitive* pOrig = primitives[ind];

        std::vector<glm::vec3> newPositions(m.vertexCount);
        std::vector<glm::vec3> newNormals(m.vertexCount);
        std::vector<glm::vec2> newUVs(m.vertexCount);
        std::vector<glm::vec2> newLightMapUVs(m.vertexCount);
        std::vector<glm::vec4> newTangents(m.vertexCount);

        for (uint32_t i = 0; i < m.vertexCount; i++) {
            uint32_t orig = m.vertexArray[i].xref;
            newPositions[i] = pOrig->vertices[orig];
            newNormals[i] = pOrig->normals[orig];
            newUVs[i] = pOrig->uvs[orig];
            newLightMapUVs[i] = { m.vertexArray[i].uv[0] / atlas->width, m.vertexArray[i].uv[1] / atlas->height };
            newTangents[i] = pOrig->tangents[orig];
        }

        pOrig->vertices.clear();
        pOrig->normals.clear();
        pOrig->uvs.clear();
        pOrig->tangents.clear();

        pOrig->vertices = std::move(newPositions);
        pOrig->normals = std::move(newNormals);
        pOrig->uvs = std::move(newUVs);
        pOrig->lmUVs = std::move(newLightMapUVs);
        pOrig->tangents = std::move(newTangents);

        pOrig->indices.assign(m.indexArray, m.indexArray + m.indexCount);
    }

    auto writeDir = getExeDir() / "object_breakdowns";
    std::filesystem::create_directories(writeDir);

    auto writePath = writeDir / "sponza_breakdown.txt";
    std::cout << "[BAKERY] Writing to: " << writePath.string() << std::endl;
    std::ofstream file (writePath.string());

    if (!file.is_open()) {
        std::cerr << "[BAKERY] Failed to open file for writing: " << writePath << std::endl;
        return 1;
    }

    for (auto& p : primitives) {
        file << "p::" << std::endl;
        for (int i = 0; i < p->vertices.size(); i++) {
            glm::vec3& v = p->vertices[i];
            glm::vec3& n = p->normals[i];
            glm::vec2& uv = p->uvs[i];
            glm::vec2& uv1 = p->lmUVs[i];
            glm::vec4& t = p->tangents[i];

            file << "v " << v.x << " " << v.y << " " << v.z << " " << n.x << " " << n.y << " " << n.z << " " << uv.x << " " << uv.y << " " << uv1.x << " " << uv1.y << " " << t.x << " " << t.y << " " << t.z << " " << t.w << std::endl;
        }
        file << "i ";
        for (int i = 0; i < p->indices.size(); i++) {
            file << p->indices[i] << " ";
        }
        file << std::endl;
    }
    file.close();

    return 0;
}