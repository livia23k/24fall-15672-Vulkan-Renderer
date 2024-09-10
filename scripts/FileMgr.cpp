#include "FileMgr.hpp"
#include "datastructures/ObjStruct.hpp"
#include "datastructures/PosColVertex.hpp"
#include "datastructures/PosNorTexVertex.hpp"

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

void FileMgr::load_line_from_object(const std::string& path, std::vector<PosColVertex>& mesh_vertices) {
    std::vector<Vector3> vertices;

    PosColVertex tmp_vertex {
        {0.0f, 0.0f, 0.0f},  // Position initialized to zeros
        {255, 0, 0, 255}     // Color initialized to red
    };

    vertices.clear();
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
                size_t firstSlash = descriptor.find('/', 1);
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

void FileMgr::load_mesh_from_object(const std::string& path, std::vector<PosNorTexVertex>& mesh_vertices) {

    std::vector<Vector3> vertices;
    std::vector<Vector3> normals;
    std::vector<Vector2> texcoords;

    vertices.clear();
    normals.clear();
    texcoords.clear();
    mesh_vertices.clear();

    // padding to match 1-based indexing
    vertices.push_back({0.f, 0.f, 0.f});
    normals.push_back({0.f, 0.f, 0.f});
    texcoords.push_back({0.f, 0.f});

    std::ifstream file(path);
    if (!file) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return;
    }

    std::string line;
    while (getline(file, line)) {
        std::istringstream stream(line);
        std::string type;
        stream >> type;

        if (type == "v") {

            Vector3 vertex;
            stream >> vertex.x >> vertex.y >> vertex.z;
            vertices.push_back(vertex);
            // std::cout << "Vertex: " << vertex.x << ", " << vertex.y << ", " << vertex.z << std::endl;
            
        } else if (type == "vt") {

            Vector2 texcoord;
            stream >> texcoord.x >> texcoord.y;
            texcoords.push_back(texcoord);
            // std::cout << "Texcoord: " << texcoord.x << ", " << texcoord.y << std::endl;

        } else if (type == "vn") {

            Vector3 normal;
            stream >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
            // std::cout << "Normal: " << normal.x << ", " << normal.y << ", " << normal.z << std::endl;

        } else if (type == "f") {

            std::string descriptor;
            std::vector<VertexIndices> face_vertices;

            while (stream >> descriptor) 
            {
                size_t firstSlash = descriptor.find('/');
                size_t secondSlash = descriptor.find('/', firstSlash + 1);

                int vertex_index = std::stoi(descriptor.substr(0, firstSlash));
                int tex_index = std::stoi(descriptor.substr(firstSlash + 1, secondSlash));
                int normal_index = std::stoi(descriptor.substr(secondSlash + 1));

                VertexIndices vertex_info = {vertex_index, tex_index, normal_index};

                face_vertices.push_back(vertex_info);
            }

            int size_vertices = face_vertices.size();
            
            if (size_vertices == 3) { // parse according to the number of vertices in the face
                for (int i = 0; i < size_vertices; ++ i) {
                    int v1 = face_vertices[i].v;
                    int vt1 = face_vertices[i].vt;
                    int vn1 = face_vertices[i].vn;

                    mesh_vertices.push_back({
                        {vertices[v1].x, vertices[v1].y, vertices[v1].z},
                        {normals[vn1].x, normals[vn1].y, normals[vn1].z},
                        {texcoords[vt1].x, texcoords[vt1].y}
                    });
                }
            } else if (size_vertices == 4) {
                const int A = 0;
                const int B = 1;
                const int C = 2;
                const int D = 3;

                int vA = face_vertices[A].v;
                int vtA = face_vertices[A].vt;
                int vnA = face_vertices[A].vn;

                int vB = face_vertices[B].v;
                int vtB = face_vertices[B].vt;
                int vnB = face_vertices[B].vn;
                
                int vC = face_vertices[C].v;
                int vtC = face_vertices[C].vt;
                int vnC = face_vertices[C].vn;

                int vD = face_vertices[D].v;
                int vtD = face_vertices[D].vt;
                int vnD = face_vertices[D].vn;

                // triangle 1
                mesh_vertices.push_back({
                        {vertices[vA].x, vertices[vA].y, vertices[vA].z},
                        {normals[vnA].x, normals[vnA].y, normals[vnA].z},
                        {texcoords[vtA].x, texcoords[vtA].y}
                });
                mesh_vertices.push_back({
                        {vertices[vB].x, vertices[vB].y, vertices[vB].z},
                        {normals[vnB].x, normals[vnB].y, normals[vnB].z},
                        {texcoords[vtB].x, texcoords[vtB].y}
                });
                mesh_vertices.push_back({
                        {vertices[vC].x, vertices[vC].y, vertices[vC].z},
                        {normals[vnC].x, normals[vnC].y, normals[vnC].z},
                        {texcoords[vtC].x, texcoords[vtC].y}
                });

                // triangle 2
                mesh_vertices.push_back({
                        {vertices[vA].x, vertices[vA].y, vertices[vA].z},
                        {normals[vnA].x, normals[vnA].y, normals[vnA].z},
                        {texcoords[vtA].x, texcoords[vtA].y}
                });
                mesh_vertices.push_back({
                        {vertices[vC].x, vertices[vC].y, vertices[vC].z},
                        {normals[vnC].x, normals[vnC].y, normals[vnC].z},
                        {texcoords[vtC].x, texcoords[vtC].y}
                });
                mesh_vertices.push_back({
                        {vertices[vD].x, vertices[vD].y, vertices[vD].z},
                        {normals[vnD].x, normals[vnD].y, normals[vnD].z},
                        {texcoords[vtD].x, texcoords[vtD].y}
                });
            }
        }
    }

    // std::cout << "Loaded " << path << " with " << mesh_vertices.size() << " vertices." << std::endl;
    return;
}

