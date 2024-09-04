#include "FileMgr.hpp"
#include "datastructures/ObjStruct.hpp"
#include "datastructures/PosColVertex.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

void FileMgr::loadOBJ(const std::string& path, std::vector<PosColVertex>& mesh_vertices) {
    std::vector<Vector3> vertices;
    std::vector<Face> faces;

    PosColVertex tmp_vertex {
        {0.0f, 0.0f, 0.0f},  // Position initialized to zeros
        {255, 0, 0, 255}     // Color initialized to red
    };

    vertices.clear();
    faces.clear();
    mesh_vertices.clear();

    vertices.push_back({0, 0, 0}); // padding to match 1-based indexing

    std::ifstream file(path);
    if (!file) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return;
    }

    std::string line;
    while (getline(file, line)) {
        std::istringstream stream(line);
        char type;
        stream >> type;

        if (type == 'v') {
            Vector3 vertex;
            stream >> vertex.x >> vertex.y >> vertex.z;
            vertices.push_back(vertex);
            // std::cout << "Vertex: " << vertex.x << ", " << vertex.y << ", " << vertex.z << std::endl;
            
        } else if (type == 'f') {
            std::string descriptor;
            std::vector<int> indices; 
            while (stream >> descriptor) {
                size_t firstSlash = descriptor.find('/');
                int v1 = std::stoi(descriptor.substr(0, firstSlash));
                indices.push_back(v1);
            }

            int size_indices = indices.size();
            for (int i = 0; i < size_indices; ++ i) {
                int v1 = indices[i];
                int v2 = indices[(i + 1) % size_indices];
                
                mesh_vertices.push_back({
                    {vertices[v1].x, vertices[v1].y, vertices[v1].z},
                    tmp_vertex.Color
                });
                mesh_vertices.push_back({
                    {vertices[v2].x, vertices[v2].y, vertices[v2].z},
                    tmp_vertex.Color
                });
                // std::cout << "Line: " << v1 << ", " << v2 << std::endl;
            }
        }
    }

    // std::cout << "Loaded " << path << " with " << mesh_vertices.size() << " vertices." << std::endl;
    return;
}