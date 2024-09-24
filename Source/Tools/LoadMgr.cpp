#include "Source/Tools/LoadMgr.hpp"
#include "Source/Tools/SceneMgr.hpp"
#include "Source/Tools/VkTypeHelper.hpp"
#include "Source/DataType/ObjStruct.hpp"
#include "Source/DataType/PosColVertex.hpp"
#include "Source/DataType/PosNorTexVertex.hpp"
#include "lib/sejp.hpp"

// Scene Graph Loader functions =================================================================================

void LoadMgr::load_objects_from_s72(const std::string& path, SceneMgr &targetSceneMgr) {

    // Valid Test ----------------------------------------------------------------------------------

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return;
    }

    std::string line;
    getline(file, line);
    
    if (line.substr(0, 9) != "[\"s72-v2\"")
    {
        std::cerr << "Invalid scene graph file: " << path << std::endl;
        return;
    }
    std::cout << "valid scene graph file: " << path << std::endl;

    // Prepare to parse the scene graph -----------------------------------------------------------

    targetSceneMgr.clean_all();

    // Parse the scene graph -----------------------------------------------------------------------

    sejp::value sceneGraphInfo = sejp::load(path);

    // Handle the info by scene objects ------------------------------------------------------------

    parse_scene_graph_info(sceneGraphInfo, targetSceneMgr);
}

void LoadMgr::parse_scene_graph_info(const sejp::value& sceneGraphInfo, SceneMgr& targetSceneMgr)
{
    // parse object ARRAY included by `[]` ------------------------------------------------------

    if (!sceneGraphInfo.as_array().has_value()) return;

    auto &objectMap = sceneGraphInfo.as_array().value();
    for (auto &object : objectMap)
    {
        // parse property OBJECT included by `{}` ------------------------------------------------

        if (!object.as_object().has_value()) continue;

        OptionalPropertyMap propertyMap = object.as_object().value();
        
        // 1. find object type

        std::string objectType = propertyMap->find("type")->second.as_string().value(); 

        // 2. parse `[]` scene object info ARRAY to cpp scene object data structure ---------------
        {
            if (objectType == "SCENE")
            {
                parse_scene_object_info(propertyMap, targetSceneMgr);
            } 
            else if (objectType == "NODE")
            {
                parse_node_object_info(propertyMap, targetSceneMgr);
            }
            else if (objectType == "MESH")
            {
                parse_mesh_object_info(propertyMap, targetSceneMgr);
            }
            else if (objectType == "CAMERA")
            {
                parse_camera_object_info(propertyMap, targetSceneMgr);
            }
            else if (objectType == "DRIVER")
            {
                parse_driver_object_info(propertyMap, targetSceneMgr);
            }
            else if (objectType == "MATERIAL")
            {
                parse_material_object_info(propertyMap, targetSceneMgr);
            }
            else if (objectType == "ENVIRONMENT")
            {
                parse_environment_object_info(propertyMap, targetSceneMgr);
            }
            else if (objectType == "LIGHT")
            {
                parse_light_object_info(propertyMap, targetSceneMgr);
            }
            else
            {
                std::cerr << "Unknown object type: " << objectType << std::endl;
            }
        };
    }
}

void LoadMgr::parse_scene_object_info(OptionalPropertyMap &sceneObjectInfo, SceneMgr &targetSceneMgr)
{
    SceneMgr::SceneObject* sceneObject = new SceneMgr::SceneObject;

    for (auto & [propertyName, propertyInfo] : *sceneObjectInfo)
    {
        if (propertyName == "type")
        {
            continue;
        }
        else if (propertyName == "name")
        {
            if (!propertyInfo.as_string()) continue;

            sceneObject->name = propertyInfo.as_string().value();
        }
        else if (propertyName == "roots")
        {
            if (!propertyInfo.as_array()) continue;

            auto & rootArray = propertyInfo.as_array().value();
            for (const auto &root : rootArray)
            {
                if (!root.as_number()) continue;
                sceneObject->rootIdx.push_back( static_cast<uint32_t>( root.as_number().value() ) );
            }
        }
        else 
        {
            std::cerr << "[parse_scene_object_info] Unknown property name: " << propertyName << std::endl;
            continue;
        }
    }

    targetSceneMgr.sceneObject = sceneObject;
    // std::cout << sceneObject->name << " updated to sceneObject." << std::endl;
}

void LoadMgr::parse_node_object_info(OptionalPropertyMap &nodeObjectInfo, SceneMgr &targetSceneMgr)
{
    SceneMgr::NodeObject *nodeObject = new SceneMgr::NodeObject;

    for (auto & [propertyName, propertyInfo] : *nodeObjectInfo)
    {
        if (propertyName == "type")
        {
            continue;
        }
        else if (propertyName == "name")
        {
            if (!propertyInfo.as_string()) continue;

            nodeObject->name = propertyInfo.as_string().value();
        }
        else if (propertyName == "translation")
        {
            if (!propertyInfo.as_array()) continue;

            auto &translationArray = propertyInfo.as_array().value();
            for (int i = 0; i < 3; ++ i)
            {
                if (!translationArray[i].as_number()) continue;
                nodeObject->translation[i] = translationArray[i].as_number().value();
                // std::cout << nodeObject->translation[i] << ", "; // [PASS]
            }
        }
        else if (propertyName == "rotation")
        {
            if (!propertyInfo.as_array()) continue;

            auto &rotationArray = propertyInfo.as_array().value();
            for (int i = 0; i < 4; ++ i)
            {
                if (!rotationArray[i].as_number()) continue;
                nodeObject->rotation[i] = rotationArray[i].as_number().value();
                // std::cout << nodeObject->rotation[i] << ", "; // [PASS]
            }
        }
        else if (propertyName == "scale")
        {
            if (!propertyInfo.as_array()) continue;

            auto &scaleArray = propertyInfo.as_array().value();
            for (int i = 0; i < 3; ++ i)
            {
                if (!scaleArray[i].as_number()) continue;
                nodeObject->scale[i] = scaleArray[i].as_number().value();
                // std::cout << nodeObject->scale[i] << ", "; // [PASS]
            }
        }
        else if (propertyName == "children")
        {
            if (!propertyInfo.as_array()) continue;

            auto &childrenArray = propertyInfo.as_array().value();
            for (const auto &child : childrenArray)
            {
                if (!child.as_string()) continue;
                nodeObject->childName.push_back(child.as_string().value());
                // std::cout << child.as_string().value() << ", "; // [PASS]
            }
        }
        else if (propertyName == "camera")
        {
            if (!propertyInfo.as_string()) continue;

            nodeObject->refCameraName = propertyInfo.as_string().value();
            // std::cout << nodeObject->refCameraName << std::endl; // [PASS]
        }
         else if (propertyName == "mesh")
        {
            if (!propertyInfo.as_string()) continue;

            nodeObject->refMeshName = propertyInfo.as_string().value();
            // std::cout << nodeObject->refMeshName << std::endl; // [PASS]
        }
        else if (propertyName == "environment")
        {
            if (!propertyInfo.as_string()) continue;

            nodeObject->refEnvironmentName = propertyInfo.as_string().value();
            // std::cout << nodeObject->refEnvironmentName << std::endl; // [PASS]
        }
        else if (propertyName == "light")
        {
            if (!propertyInfo.as_string()) continue;

            nodeObject->refLightName = propertyInfo.as_string().value();
            // std::cout << nodeObject->refLightName << std::endl; // [PASS]
        }
        else
        {
            std::cerr << "[parse_node_object_info] Unknown property name: " << propertyName << std::endl;
            continue;
        }
    }

    targetSceneMgr.nodeObjectMap[nodeObject->name] = nodeObject;
    // std::cout << nodeObject->name << " added to nodeObjectMap." << std::endl; // [PASS]
}

void LoadMgr::parse_mesh_object_info(OptionalPropertyMap &meshObjectInfo, SceneMgr &targetSceneMgr)
{
    SceneMgr::MeshObject *meshObject = new SceneMgr::MeshObject;

    for (auto & [propertyName, propertyInfo] : *meshObjectInfo)
    {
        if (propertyName == "type")
        {
            continue;
        }
        else if (propertyName == "name")
        {
            if (!propertyInfo.as_string()) continue;

            meshObject->name = propertyInfo.as_string().value();
            // std::cout << meshObject->name << std::endl; // [PASS]
        }
        else if (propertyName == "topology")
        {
            if (!propertyInfo.as_string()) continue;

            std::string topologyStr = propertyInfo.as_string().value();
            std::optional<VkPrimitiveTopology>  typeConvertedResult = VkTypeHelper::findVkPrimitiveTopology(topologyStr);
            if (typeConvertedResult == std::nullopt) continue;
            meshObject->topology = typeConvertedResult.value();
            // std::cout << topology_str << ": " << meshObject->topology << std::endl; // [PASS] TRIANGLE_LIST: 3
        }
        else if (propertyName == "count")
        {
            if (!propertyInfo.as_number()) continue;

            meshObject->count = static_cast<uint32_t>(propertyInfo.as_number().value());
            // std::cout << meshObject->count << std::endl; // [PASS]
        }
        else if (propertyName == "indices")
        {
            if (!propertyInfo.as_object()) continue;

            auto &indicesObject = propertyInfo.as_object().value();
            for (auto & [indiceName, indiceInfo] : indicesObject)
            {
                if (indiceName == "src")
                {
                    if (!indiceInfo.as_string()) continue;

                    meshObject->indices.src = indiceInfo.as_string().value();
                    // std::cout << meshObject->indices.src << std::endl; // [PASS]
                }
                else if (indiceName == "offset")
                {
                    if (!indiceInfo.as_number()) continue;

                    meshObject->indices.offset = static_cast<uint32_t>(indiceInfo.as_number().value());
                    // std::cout << meshObject->indices.offset << std::endl; // [PASS]
                }
                else if (indiceName == "format")
                {
                    if (!indiceInfo.as_string()) continue;

                    std::string formatStr = indiceInfo.as_string().value();
                    std::optional<VkIndexType> typeConvertedResult = VkTypeHelper::findVkIndexType(formatStr);
                    if (typeConvertedResult == std::nullopt) continue;
                    meshObject->indices.format = typeConvertedResult.value();
                    // std::cout << formatStr << ": " << meshObject->indices.format << std::endl; // [PASS] UINT32: 7
                }
                else
                {
                    std::cerr << "[parse_mesh_object_info] (indices) Unknown indice name: " << indiceName << std::endl;
                    continue;
                }
            }
        }
        else if (propertyName == "attributes")
        {
            if (!propertyInfo.as_object()) continue;

            auto &attributesObject = propertyInfo.as_object().value();
            for (auto & [attributeName, attributeInfo] : attributesObject)
            {
                if (attributeName == "POSITION")
                {
                    if (!attributeInfo.as_object()) continue;

                    OptionalPropertyMap subAttributePropertyMap = attributeInfo.as_object().value();
                    parse_sub_attribute_info(subAttributePropertyMap, meshObject->attrPosition);
                }
                else if (attributeName == "NORMAL")
                {
                    if (!attributeInfo.as_object()) continue;

                    OptionalPropertyMap subAttributePropertyMap = attributeInfo.as_object().value();
                    parse_sub_attribute_info(subAttributePropertyMap, meshObject->attrNormal);
                }
                else if (attributeName == "TANGENT")
                {
                    if (!attributeInfo.as_object()) continue;

                    OptionalPropertyMap subAttributePropertyMap = attributeInfo.as_object().value();
                    parse_sub_attribute_info(subAttributePropertyMap, meshObject->attrTangent);
                }
                else if (attributeName == "TEXCOORD")
                {
                    if (!attributeInfo.as_object()) continue;

                    OptionalPropertyMap subAttributePropertyMap = attributeInfo.as_object().value();
                    parse_sub_attribute_info(subAttributePropertyMap, meshObject->attrTexcoord);
                }
                else 
                {
                    std::cerr << "[parse_mesh_object_info] (attributes) Unknown attribute name: " << attributeName << std::endl;
                    continue;
                }
            }
        }
        else if (propertyName == "material")
        {
            if (!propertyInfo.as_string()) continue;

            meshObject->refMaterialName = propertyInfo.as_string().value();
            // std::cout << meshObject->refMaterialName << std::endl; // [PASS]
        }
        else
        {
            std::cerr << "[parse_mesh_object_info] Unknown property name: " << propertyName << std::endl;
            continue;
        }
    }

    targetSceneMgr.meshObjectMap[meshObject->name] = meshObject;
    // std::cout << meshObject->name << " added to meshObjectMap." << std::endl;
}

void LoadMgr::parse_camera_object_info(OptionalPropertyMap &cameraObjectInfo, SceneMgr &targetSceneMgr)
{
    SceneMgr::CameraObject *cameraObject = new SceneMgr::CameraObject;

    for (auto & [propertyName, propertyInfo] : *cameraObjectInfo)
    {
        if (propertyName == "type")
        {
            continue;
        }
        else if (propertyName == "name")
        {
            if (!propertyInfo.as_string()) continue;

            cameraObject->name = propertyInfo.as_string().value();
            // std::cout << cameraObjectInfo->name << std::endl; // [PASS]
        }
        else if (propertyName == "perspective")
        {
            if (!propertyInfo.as_object()) continue;
        
            cameraObject->projectionType = SceneMgr::ProjectionType::Perspective;
            SceneMgr::PerspectiveParameters curPerspectiveParameters;

            auto &perspectiveINFO = propertyInfo.as_object().value();
            for (auto & [perspectivePropertyName, perspectivePropertyInfo] : perspectiveINFO)
            {
                if (perspectivePropertyName == "aspect")
                {
                    if (!perspectivePropertyInfo.as_number()) continue;

                    curPerspectiveParameters.aspect = perspectivePropertyInfo.as_number().value();
                    // std::cout << "aspect " << curPerspectiveParameters.aspect << std::endl; // [PASS]
                }
                else if (perspectivePropertyName == "vfov")
                {
                    if (!perspectivePropertyInfo.as_number()) continue;

                    curPerspectiveParameters.vfov = perspectivePropertyInfo.as_number().value();
                    // std::cout << "vfov " << curPerspectiveParameters.vfov << std::endl; // [PASS]
                }
                else if (perspectivePropertyName == "near")
                {
                    if (!perspectivePropertyInfo.as_number()) continue;

                    curPerspectiveParameters.nearZ = perspectivePropertyInfo.as_number().value();
                    // std::cout << "near " << curPerspectiveParameters.nearZ << std::endl; // [PASS]
                }
                else if (perspectivePropertyName == "far")
                {
                    if (!perspectivePropertyInfo.as_number()) continue;

                    curPerspectiveParameters.farZ = perspectivePropertyInfo.as_number().value();
                    // std::cout << "far " << curPerspectiveParameters.farZ << std::endl; // [PASS]
                }
                else
                {
                    std::cerr << "[parse_camera_object_info] (perspective) Unknown perspective property name: " << perspectivePropertyName << std::endl;
                    continue;
                }
            }

            cameraObject->projectionParameters = curPerspectiveParameters;
        }
        else if (propertyName == "orthographic") // [TOCHECK] both .s72 format support and load implementation
        {
            if (!propertyInfo.as_object()) continue;

            cameraObject->projectionType = SceneMgr::ProjectionType::Orthographic;
            SceneMgr::OrthographicParameters curOrthographicParameters;

            auto &orthographicINFO = propertyInfo.as_object().value();
            for (auto & [orthographicPropertyName, orthographicPropertyInfo] : orthographicINFO)
            {
                if (orthographicPropertyName == "left")
                {
                    if (!orthographicPropertyInfo.as_number()) continue;

                    curOrthographicParameters.left = orthographicPropertyInfo.as_number().value();
                    // std::cout << "left " << curOrthographicParameters.left << std::endl;
                }
                else if (orthographicPropertyName == "right")
                {
                    if (!orthographicPropertyInfo.as_number()) continue;

                    curOrthographicParameters.right = orthographicPropertyInfo.as_number().value();
                    // std::cout << "right " << curOrthographicParameters.right << std::endl;
                }
                else if (orthographicPropertyName == "bottom")
                {
                    if (!orthographicPropertyInfo.as_number()) continue;

                    curOrthographicParameters.bottom = orthographicPropertyInfo.as_number().value();
                    // std::cout << "bottom " << curOrthographicParameters.bottom << std::endl;
                }
                else if (orthographicPropertyName == "top")
                {
                    if (!orthographicPropertyInfo.as_number()) continue;

                    curOrthographicParameters.top = orthographicPropertyInfo.as_number().value();
                    // std::cout << "top " << curOrthographicParameters.top << std::endl;
                }
                else if (orthographicPropertyName == "near")
                {
                    if (!orthographicPropertyInfo.as_number()) continue;

                    curOrthographicParameters.nearZ = orthographicPropertyInfo.as_number().value();
                    // std::cout << "near " << curOrthographicParameters.nearZ << std::endl;
                }
                else if (orthographicPropertyName == "far")
                {
                    if (!orthographicPropertyInfo.as_number()) continue;

                    curOrthographicParameters.farZ = orthographicPropertyInfo.as_number().value();
                    // std::cout << "far " << curOrthographicParameters.farZ << std::endl;
                }
                else
                {
                    std::cerr << "[parse_camera_object_info] (orthographic) Unknown orthographic property name: " << orthographicPropertyName << std::endl;
                    continue;
                }
            }

            cameraObject->projectionParameters = curOrthographicParameters;
        }
        else
        {
            std::cerr << "[parse_camera_object_info] Unknown property name: " << propertyName << std::endl;
            continue;
        }
    }
}

void LoadMgr::parse_driver_object_info(OptionalPropertyMap &driverObjectInfo, SceneMgr &targetSceneMgr)
{

}

void LoadMgr::parse_material_object_info(OptionalPropertyMap &materialObjectInfo, SceneMgr &targetSceneMgr)
{

}

void LoadMgr::parse_environment_object_info(OptionalPropertyMap &environmentObjectInfo, SceneMgr &targetSceneMgr)
{

}

void LoadMgr::parse_light_object_info(OptionalPropertyMap &lightObjectInfo, SceneMgr &targetSceneMgr)
{

}


void LoadMgr::parse_sub_attribute_info(OptionalPropertyMap &subAttributeInfo, SceneMgr::AttributeStream &attrStream)
{
    for (auto & [propertyName, propertyInfo] : *subAttributeInfo)
    {
        if (propertyName == "src")
        {
            if (!propertyInfo.as_string()) continue;

            attrStream.src = propertyInfo.as_string().value();
            // std::cout << attrStream.src << std::endl; // [PASS]
        }
        else if (propertyName == "offset")
        {
            if (!propertyInfo.as_number()) continue;

            attrStream.offset = static_cast<uint32_t>(propertyInfo.as_number().value());
            // std::cout << attrStream.offset << std::endl; // [PASS]
        }
        else if (propertyName == "stride")
        {
            if (!propertyInfo.as_number()) continue;

            attrStream.stride = static_cast<uint32_t>(propertyInfo.as_number().value());
            // std::cout << attrStream.stride << std::endl; // [PASS]
        }
        else if (propertyName == "format")
        {
            if (!propertyInfo.as_string()) continue;

            std::string formatStr = propertyInfo.as_string().value();
            std::optional<VkFormat> typeConvertedResult = VkTypeHelper::findVkFormat(formatStr);
            if (typeConvertedResult == std::nullopt) continue;
            attrStream.format = typeConvertedResult.value();
            // std::cout << formatStr << ": " << attrStream.format << std::endl; // [PASS]
        }
        else 
        {
            std::cerr << "[parse_sub_attribute_info] Unknown property name: " << propertyName << std::endl;
            continue;
        }
    }
}



// OBJ Loader functions ================================================================================================

void LoadMgr::load_line_from_OBJ(const std::string& path, std::vector<PosColVertex>& mesh_vertices) {
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

void LoadMgr::load_object_from_OBJ(const std::string& path, std::vector<PosNorTexVertex>& mesh_vertices) {

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

