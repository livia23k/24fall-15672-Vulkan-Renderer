#include "Source/Tools/LoadMgr.hpp"
#include "Source/Tools/SceneMgr.hpp"
#include "Source/Tools/VkTypeHelper.hpp"
#include "Source/DataType/ObjStruct.hpp"
#include "Source/DataType/PosColVertex.hpp"
#include "Source/DataType/PosNorTexVertex.hpp"
#include "lib/sejp.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "lib/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "lib/stb_image_write.h"

// Scene Graph Loader functions =================================================================================

void LoadMgr::load_scene_graph_info_from_s72(const std::string &path, SceneMgr &targetSceneMgr)
{

    // Valid Test ----------------------------------------------------------------------------------

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
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

void LoadMgr::parse_scene_graph_info(const sejp::value &sceneGraphInfo, SceneMgr &targetSceneMgr)
{
    // parse object ARRAY included by `[]` ------------------------------------------------------

    if (!sceneGraphInfo.as_array().has_value())
        return;

    auto &objectMap = sceneGraphInfo.as_array().value();
    for (auto &object : objectMap)
    {
        // parse property OBJECT included by `{}` ------------------------------------------------

        if (!object.as_object().has_value())
            continue;

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
    if (sceneObjectInfo == std::nullopt)
    {
        std::cerr << "[parse_scene_object_info] sceneObjectInfo is null." << std::endl;
        return;
    }

    SceneMgr::SceneObject *sceneObject = new SceneMgr::SceneObject;

    for (auto &[propertyName, propertyInfo] : sceneObjectInfo.value())
    {
        if (propertyName == "type")
        {
            continue;
        }
        else if (propertyName == "name")
        {
            if (!propertyInfo.as_string())
                continue;

            sceneObject->name = propertyInfo.as_string().value();
        }
        else if (propertyName == "roots")
        {
            if (!propertyInfo.as_array())
                continue;

            auto &rootArray = propertyInfo.as_array().value();
            for (const auto &root : rootArray)
            {
                if (!root.as_string())
                    continue;
                sceneObject->rootName.push_back(root.as_string().value());
                // std::cout << sceneObject->rootName.back() << ", "; // [PASS]
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
    if (nodeObjectInfo == std::nullopt)
    {
        std::cerr << "[parse_node_object_info] nodeObjectInfo is null." << std::endl;
        return;
    }

    SceneMgr::NodeObject *nodeObject = new SceneMgr::NodeObject;

    for (auto &[propertyName, propertyInfo] : nodeObjectInfo.value())
    {
        if (propertyName == "type")
        {
            continue;
        }
        else if (propertyName == "name")
        {
            if (!propertyInfo.as_string())
                continue;

            nodeObject->name = propertyInfo.as_string().value();
        }
        else if (propertyName == "translation")
        {
            if (!propertyInfo.as_array())
                continue;

            auto &translationArray = propertyInfo.as_array().value();
            for (int i = 0; i < 3; ++i)
            {
                if (!translationArray[i].as_number())
                    continue;
                nodeObject->translation[i] = translationArray[i].as_number().value();
                // std::cout << nodeObject->translation[i] << ", "; // [PASS]
            }
        }
        else if (propertyName == "rotation")
        {
            if (!propertyInfo.as_array())
                continue;

            auto &rotationArray = propertyInfo.as_array().value();
            for (int i = 0; i < 4; ++i)
            {
                if (!rotationArray[i].as_number())
                    continue;
                nodeObject->rotation[i] = rotationArray[i].as_number().value();
                // std::cout << nodeObject->rotation[i] << ", "; // [PASS]
            }
        }
        else if (propertyName == "scale")
        {
            if (!propertyInfo.as_array())
                continue;

            auto &scaleArray = propertyInfo.as_array().value();
            for (int i = 0; i < 3; ++i)
            {
                if (!scaleArray[i].as_number())
                    continue;
                nodeObject->scale[i] = scaleArray[i].as_number().value();
                // std::cout << nodeObject->scale[i] << ", "; // [PASS]
            }
        }
        else if (propertyName == "children")
        {
            if (!propertyInfo.as_array())
                continue;

            auto &childrenArray = propertyInfo.as_array().value();
            for (const auto &child : childrenArray)
            {
                if (!child.as_string())
                    continue;
                nodeObject->childName.push_back(child.as_string().value());
                // std::cout << child.as_string().value() << ", "; // [PASS]
            }
        }
        else if (propertyName == "camera")
        {
            if (!propertyInfo.as_string())
                continue;

            nodeObject->refCameraName = propertyInfo.as_string().value();
            // std::cout << nodeObject->refCameraName << std::endl; // [PASS]
        }
        else if (propertyName == "mesh")
        {
            if (!propertyInfo.as_string())
                continue;

            nodeObject->refMeshName = propertyInfo.as_string().value();
            // std::cout << nodeObject->refMeshName << std::endl; // [PASS]
        }
        else if (propertyName == "environment")
        {
            if (!propertyInfo.as_string())
                continue;

            nodeObject->refEnvironmentName = propertyInfo.as_string().value();
            // std::cout << nodeObject->refEnvironmentName << std::endl; // [PASS]
        }
        else if (propertyName == "light")
        {
            if (!propertyInfo.as_string())
                continue;

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
    if (meshObjectInfo == std::nullopt)
    {
        std::cerr << "[parse_mesh_object_info] meshObjectInfo is null." << std::endl;
        return;
    }

    SceneMgr::MeshObject *meshObject = new SceneMgr::MeshObject;

    for (auto &[propertyName, propertyInfo] : meshObjectInfo.value())
    {
        if (propertyName == "type")
        {
            continue;
        }
        else if (propertyName == "name")
        {
            if (!propertyInfo.as_string())
                continue;

            meshObject->name = propertyInfo.as_string().value();
            // std::cout << meshObject->name << std::endl; // [PASS]
        }
        else if (propertyName == "topology")
        {
            if (!propertyInfo.as_string())
                continue;

            std::string topologyStr = propertyInfo.as_string().value();
            std::optional<VkPrimitiveTopology> typeConvertedResult = VkTypeHelper::findVkPrimitiveTopology(topologyStr);
            if (typeConvertedResult == std::nullopt)
                continue;
            meshObject->topology = typeConvertedResult.value();
            // std::cout << topology_str << ": " << meshObject->topology << std::endl; // [PASS] TRIANGLE_LIST: 3
        }
        else if (propertyName == "count")
        {
            if (!propertyInfo.as_number())
                continue;

            meshObject->count = static_cast<uint32_t>(propertyInfo.as_number().value());
            // std::cout << meshObject->count << std::endl; // [PASS]
        }
        else if (propertyName == "indices")
        {
            if (!propertyInfo.as_object())
                continue;

            auto &indicesObject = propertyInfo.as_object().value();
            for (auto &[indiceName, indiceInfo] : indicesObject)
            {
                if (indiceName == "src")
                {
                    if (!indiceInfo.as_string())
                        continue;

                    meshObject->indices.src = indiceInfo.as_string().value();
                    // std::cout << meshObject->indices.src << std::endl; // [PASS]
                }
                else if (indiceName == "offset")
                {
                    if (!indiceInfo.as_number())
                        continue;

                    meshObject->indices.offset = static_cast<uint32_t>(indiceInfo.as_number().value());
                    // std::cout << meshObject->indices.offset << std::endl; // [PASS]
                }
                else if (indiceName == "format")
                {
                    if (!indiceInfo.as_string())
                        continue;

                    std::string formatStr = indiceInfo.as_string().value();
                    std::optional<VkIndexType> typeConvertedResult = VkTypeHelper::findVkIndexType(formatStr);
                    if (typeConvertedResult == std::nullopt)
                        continue;
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
            if (!propertyInfo.as_object())
                continue;

            auto &attributesObject = propertyInfo.as_object().value();
            for (auto &[attributeName, attributeInfo] : attributesObject)
            {
                if (attributeName == "POSITION")
                {
                    if (!attributeInfo.as_object())
                        continue;

                    OptionalPropertyMap subAttributePropertyMap = attributeInfo.as_object().value();
                    parse_sub_attribute_info(subAttributePropertyMap, meshObject->attrPosition);
                }
                else if (attributeName == "NORMAL")
                {
                    if (!attributeInfo.as_object())
                        continue;

                    OptionalPropertyMap subAttributePropertyMap = attributeInfo.as_object().value();
                    parse_sub_attribute_info(subAttributePropertyMap, meshObject->attrNormal);
                }
                else if (attributeName == "TANGENT")
                {
                    if (!attributeInfo.as_object())
                        continue;

                    OptionalPropertyMap subAttributePropertyMap = attributeInfo.as_object().value();
                    parse_sub_attribute_info(subAttributePropertyMap, meshObject->attrTangent);
                }
                else if (attributeName == "TEXCOORD")
                {
                    if (!attributeInfo.as_object())
                        continue;

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
            if (!propertyInfo.as_string())
                continue;

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
    if (cameraObjectInfo == std::nullopt)
    {
        std::cerr << "[parse_camera_object_info] cameraObjectInfo is null." << std::endl;
        return;
    }

    SceneMgr::CameraObject *cameraObject = new SceneMgr::CameraObject;

    for (auto &[propertyName, propertyInfo] : cameraObjectInfo.value())
    {
        if (propertyName == "type")
        {
            continue;
        }
        else if (propertyName == "name")
        {
            if (!propertyInfo.as_string())
                continue;

            cameraObject->name = propertyInfo.as_string().value();
            // std::cout << cameraObjectInfo->name << std::endl; // [PASS]
        }
        else if (propertyName == "perspective")
        {
            if (!propertyInfo.as_object())
                continue;

            cameraObject->projectionType = SceneMgr::ProjectionType::Perspective;
            SceneMgr::PerspectiveParameters curPerspectiveParameters;

            auto &perspectiveINFO = propertyInfo.as_object().value();
            for (auto &[perspectivePropertyName, perspectivePropertyInfo] : perspectiveINFO)
            {
                if (perspectivePropertyName == "aspect")
                {
                    if (!perspectivePropertyInfo.as_number())
                        continue;

                    curPerspectiveParameters.aspect = perspectivePropertyInfo.as_number().value();
                    // std::cout << "aspect " << curPerspectiveParameters.aspect << std::endl; // [PASS]
                }
                else if (perspectivePropertyName == "vfov")
                {
                    if (!perspectivePropertyInfo.as_number())
                        continue;

                    curPerspectiveParameters.vfov = perspectivePropertyInfo.as_number().value();
                    // std::cout << "vfov " << curPerspectiveParameters.vfov << std::endl; // [PASS]
                }
                else if (perspectivePropertyName == "near")
                {
                    if (!perspectivePropertyInfo.as_number())
                        continue;

                    curPerspectiveParameters.nearZ = perspectivePropertyInfo.as_number().value();
                    // std::cout << "near " << curPerspectiveParameters.nearZ << std::endl; // [PASS]
                }
                else if (perspectivePropertyName == "far")
                {
                    if (!perspectivePropertyInfo.as_number())
                        continue;

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
            if (!propertyInfo.as_object())
                continue;

            cameraObject->projectionType = SceneMgr::ProjectionType::Orthographic;
            SceneMgr::OrthographicParameters curOrthographicParameters;

            auto &orthographicINFO = propertyInfo.as_object().value();
            for (auto &[orthographicPropertyName, orthographicPropertyInfo] : orthographicINFO)
            {
                if (orthographicPropertyName == "left")
                {
                    if (!orthographicPropertyInfo.as_number())
                        continue;

                    curOrthographicParameters.left = orthographicPropertyInfo.as_number().value();
                    // std::cout << "left " << curOrthographicParameters.left << std::endl;
                }
                else if (orthographicPropertyName == "right")
                {
                    if (!orthographicPropertyInfo.as_number())
                        continue;

                    curOrthographicParameters.right = orthographicPropertyInfo.as_number().value();
                    // std::cout << "right " << curOrthographicParameters.right << std::endl;
                }
                else if (orthographicPropertyName == "bottom")
                {
                    if (!orthographicPropertyInfo.as_number())
                        continue;

                    curOrthographicParameters.bottom = orthographicPropertyInfo.as_number().value();
                    // std::cout << "bottom " << curOrthographicParameters.bottom << std::endl;
                }
                else if (orthographicPropertyName == "top")
                {
                    if (!orthographicPropertyInfo.as_number())
                        continue;

                    curOrthographicParameters.top = orthographicPropertyInfo.as_number().value();
                    // std::cout << "top " << curOrthographicParameters.top << std::endl;
                }
                else if (orthographicPropertyName == "near")
                {
                    if (!orthographicPropertyInfo.as_number())
                        continue;

                    curOrthographicParameters.nearZ = orthographicPropertyInfo.as_number().value();
                    // std::cout << "near " << curOrthographicParameters.nearZ << std::endl;
                }
                else if (orthographicPropertyName == "far")
                {
                    if (!orthographicPropertyInfo.as_number())
                        continue;

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

    targetSceneMgr.cameraObjectMap[cameraObject->name] = cameraObject;
    // std::cout << cameraObject->name << " added to cameraObjectMap." << std::endl;
}

void LoadMgr::parse_driver_object_info(OptionalPropertyMap &driverObjectInfo, SceneMgr &targetSceneMgr)
{
    if (driverObjectInfo == std::nullopt)
    {
        std::cerr << "[parse_driver_object_info] driverObjectInfo is null." << std::endl;
        return;
    }

    SceneMgr::DriverObject *driverObject = new SceneMgr::DriverObject;

    for (auto &[propertyName, propertyInfo] : driverObjectInfo.value())
    {
        if (propertyName == "type")
        {
            continue;
        }
        else if (propertyName == "name")
        {
            if (!propertyInfo.as_string())
                continue;

            driverObject->name = propertyInfo.as_string().value();
            // std::cout << "name " << driverObject->name << std::endl; // [PASS]
        }
        else if (propertyName == "node")
        {
            if (!propertyInfo.as_string())
                continue;

            driverObject->refObjectName = propertyInfo.as_string().value();
            // std::cout << "target node " << driverObject->refObjectName << std::endl; // [PASS]
        }
        else if (propertyName == "channel")
        {
            if (!propertyInfo.as_string())
                continue;

            std::string channelStr = propertyInfo.as_string().value();
            if (channelStr == "translation")
            {
                driverObject->channel = SceneMgr::DriverChannleType::TRANSLATION;
                driverObject->channelDim = 3;
                // std::cout << "channel " << driverObject->channel << std::endl; // [PASS]
                // std::cout << "channelDim " << driverObject->channelDim << std::endl; // [PASS]
            }
            else if (channelStr == "scale")
            {
                driverObject->channel = SceneMgr::DriverChannleType::SCALE;
                driverObject->channelDim = 3;
                // std::cout << "channel " << driverObject->channel << std::endl; // [PASS]
                // std::cout << "channelDim " << driverObject->channelDim << std::endl; // [PASS]
            }
            else if (channelStr == "rotation")
            {
                driverObject->channel = SceneMgr::DriverChannleType::ROTATION;
                driverObject->channelDim = 4;
                // std::cout << "channel " << driverObject->channel << std::endl; // [PASS]
                // std::cout << "channelDim " << driverObject->channelDim << std::endl; // [PASS]
            }
            else
            {
                std::cerr << "[parse_driver_object_info] (channel) Unknown channel name: " << channelStr << std::endl;
                continue;
            }
        }
        else if (propertyName == "times")
        {
            if (!propertyInfo.as_array())
                continue;

            auto &timesArray = propertyInfo.as_array().value();
            // std::cout << "times ";
            for (const auto &time : timesArray)
            {
                if (!time.as_number())
                    continue;

                driverObject->times.push_back(time.as_number().value());
                // std::cout << time.as_number().value() << ", "; // [PASS]
            }
            // std::cout << std::endl;
        }
        else if (propertyName == "values")
        {
            if (!propertyInfo.as_array())
                continue;

            auto &valuesArray = propertyInfo.as_array().value();
            // std::cout << "values ";
            for (const auto &value : valuesArray)
            {
                if (!value.as_number())
                    continue;

                driverObject->values.push_back(value.as_number().value());
                // std::cout << value.as_number().value() << ", "; // [PASS]
            }
            // std::cout << std::endl;
        }
        else if (propertyName == "interpolation")
        {
            if (!propertyInfo.as_string())
                continue;

            std::string interpolationStr = propertyInfo.as_string().value();
            if (interpolationStr == "STEP")
            {
                driverObject->interpolation = SceneMgr::DriverInterpolation::STEP;
                // std::cout << "interpolation " << driverObject->interpolation << std::endl; // [PASS]
            }
            else if (interpolationStr == "LINEAR")
            {
                driverObject->interpolation = SceneMgr::DriverInterpolation::LINEAR;
                // std::cout << "interpolation " << driverObject->interpolation << std::endl; // [PASS]
            }
            else if (interpolationStr == "SLERP")
            {
                driverObject->interpolation = SceneMgr::DriverInterpolation::SLERP;
                // std::cout << "interpolation " << driverObject->interpolation << std::endl; // [PASS]
            }
            else
            {
                std::cerr << "[parse_driver_object_info] (interpolation) Unknown interpolation name: " << interpolationStr << std::endl;
                continue;
            }
        }
        else
        {
            std::cerr << "[parse_driver_object_info] Unknown property name: " << propertyName << std::endl;
            continue;
        }
    }

    targetSceneMgr.driverObjectMap[driverObject->name] = driverObject;
    // std::cout << driverObject->name << " added to driverObjectMap." << std::endl;
}

void LoadMgr::parse_material_object_info(OptionalPropertyMap &materialObjectInfo, SceneMgr &targetSceneMgr)
{
    if (materialObjectInfo == std::nullopt)
    {
        std::cerr << "[parse_material_object_info] materialObjectInfo is null." << std::endl;
        return;
    }

    SceneMgr::MaterialObject *materialObject = new SceneMgr::MaterialObject;

    for (auto &[propertyName, propertyInfo] : materialObjectInfo.value())
    {
        if (propertyName == "type")
        {
            continue;
        }
        else if (propertyName == "name")
        {
            if (!propertyInfo.as_string())
                continue;

            materialObject->name = propertyInfo.as_string().value();
            // std::cout << materialObject->name << std::endl; // [PASS]
        }
        else if (propertyName == "normalMap")
        {
            if (!propertyInfo.as_object())
                continue;

            PropertyMap normalmapObjects = propertyInfo.as_object().value();
            for (auto &[normalmapPropertyName, normalmapPropertyInfo] : normalmapObjects)
            {
                if (normalmapPropertyName == "src")
                {
                    if (!normalmapPropertyInfo.as_string())
                        continue;

                    materialObject->normalmap = SceneMgr::Texture(); // replacing the std::nullopt
                    materialObject->normalmap->src = normalmapPropertyInfo.as_string().value();
                    materialObject->normalmap->numChannels = 3; // normal vector in tangent space (x, y, z)
                    // std::cout << materialObject->normalmap->src << std::endl; // [PASS]
                }
                else
                {
                    std::cerr << "[parse_material_object_info] (normalmap) (tex_property) Unknown normalmap property name: " << normalmapPropertyName << std::endl;
                    continue;
                }
            }
        }
        else if (propertyName == "displacementMap")
        {
            if (!propertyInfo.as_object())
                continue;

            PropertyMap displacementmapObjects = propertyInfo.as_object().value();
            for (auto &[displacementmapPropertyName, displacementmapPropertyInfo] : displacementmapObjects)
            {
                if (displacementmapPropertyName == "src")
                {
                    if (!displacementmapPropertyInfo.as_string())
                        continue;

                    materialObject->displacementmap = SceneMgr::Texture(); // replacing the std::nullopt
                    materialObject->displacementmap->src = displacementmapPropertyInfo.as_string().value();
                    materialObject->displacementmap->numChannels = 1; // height
                    // std::cout << materialObject->displacementmap->src << std::endl; // [PASS]
                }
                else
                {
                    std::cerr << "[parse_material_object_info] (displacementmap) (tex_property) Unknown displacementmap property name: " << displacementmapPropertyName << std::endl;
                    continue;
                }
            }
        }
        else if (propertyName == "pbr")
        {
            materialObject->type = SceneMgr::MaterialType::PBR;

            SceneMgr::PBRMaterial pbrMaterial;

            PropertyMap pbrObjects = propertyInfo.as_object().value();
            for (auto &[pbrPropertyName, pbrPropertyInfo] : pbrObjects)
            {
                if (pbrPropertyName == "albedo")
                {
                    if (pbrPropertyInfo.as_array())
                    {
                        auto &albedoArray = pbrPropertyInfo.as_array().value();
                        glm::vec3 albedoVec3;
                        for (int i = 0; i < 3; ++i)
                        {
                            if (!albedoArray[i].as_number())
                                continue;
                            albedoVec3[i] = (albedoArray[i].as_number().value());
                        }
                        pbrMaterial.albedo = albedoVec3;

                        // if (std::holds_alternative<glm::vec3>(pbrMaterial.albedo))
                        // { // [PASS]
                        //     glm::vec3 albedoValue = std::get<glm::vec3>(pbrMaterial.albedo);
                        //     std::cout << "pbr albedo (vec3): " << albedoValue.x << ", " << albedoValue.y << ", " << albedoValue.z << std::endl;
                        // }
                    }
                    else if (pbrPropertyInfo.as_object())
                    {
                        PropertyMap albedoObjects = pbrPropertyInfo.as_object().value();
                        for (auto &[albedoPropertyName, albedoPropertyInfo] : albedoObjects)
                        {
                            if (albedoPropertyName == "src")
                            {
                                if (!albedoPropertyInfo.as_string())
                                    continue;

                                SceneMgr::Texture albedo_tex;
                                albedo_tex.src = albedoPropertyInfo.as_string().value();
                                albedo_tex.numChannels = 3; // (r, g, b)
                                pbrMaterial.albedo = albedo_tex;

                                // if (std::holds_alternative<SceneMgr::Texture>(pbrMaterial.albedo))
                                // { // [PASS]
                                //     SceneMgr::Texture albedoTexture = std::get<SceneMgr::Texture>(pbrMaterial.albedo);
                                //     std::cout << "pbr albedo (tex): " << albedoTexture.src << std::endl;
                                // }
                            }
                            else
                            {
                                std::cerr << "[parse_material_object_info] (pbr) (albedo) (tex_property) Unknown albedo property name: " << albedoPropertyName << std::endl;
                                continue;
                            }
                        }
                    }
                    else
                    {
                        std::cerr << "[parse_material_object_info] (pbr) (albedo) Unknown albedo type." << std::endl;
                        continue;
                    }
                }
                else if (pbrPropertyName == "roughness")
                {
                    if (pbrPropertyInfo.as_number())
                    {
                        float roughness_flt = pbrPropertyInfo.as_number().value();
                        pbrMaterial.roughness = roughness_flt;

                        // if (std::holds_alternative<float>(pbrMaterial.roughness))
                        // { // [PASS]
                        //     float roughnessValue = std::get<float>(pbrMaterial.roughness);
                        //     std::cout << "pbr roughness (flt): " << roughnessValue << std::endl;
                        // }
                    }
                    else if (pbrPropertyInfo.as_object())
                    {
                        PropertyMap roughnessObjects = pbrPropertyInfo.as_object().value();
                        for (auto &[roughnessPropertyName, roughnessPropertyInfo] : roughnessObjects)
                        {
                            if (roughnessPropertyName == "src")
                            {
                                if (!roughnessPropertyInfo.as_string())
                                    continue;

                                SceneMgr::Texture roughness_tex;
                                roughness_tex.src = roughnessPropertyInfo.as_string().value();
                                roughness_tex.numChannels = 1; // roughness
                                pbrMaterial.roughness = roughness_tex;

                                // if (std::holds_alternative<SceneMgr::Texture>(pbrMaterial.roughness))
                                // { // [PASS]
                                //     SceneMgr::Texture roughnessTexture = std::get<SceneMgr::Texture>(pbrMaterial.roughness);
                                //     std::cout << "pbr roughness (tex): " << roughnessTexture.src << std::endl;
                                // }
                            }
                            else
                            {
                                std::cerr << "[parse_material_object_info] (pbr) (roughness) (tex_property) Unknown roughness property name: " << roughnessPropertyName << std::endl;
                                continue;
                            }
                        }
                    }
                    else
                    {
                        std::cerr << "[parse_material_object_info] (pbr) (roughness) Unknown roughness type." << std::endl;
                        continue;
                    }
                }
                else if (pbrPropertyName == "metalness")
                {
                    if (pbrPropertyInfo.as_number())
                    {
                        float metalness_flt = pbrPropertyInfo.as_number().value();
                        pbrMaterial.metalness = metalness_flt;

                        // if (std::holds_alternative<float>(pbrMaterial.metalness))
                        // { // [PASS]
                        //     float metalnessValue = std::get<float>(pbrMaterial.metalness);
                        //     std::cout << "pbr metalness (flt): " << metalnessValue << std::endl;
                        // }
                    }
                    else if (pbrPropertyInfo.as_object())
                    {
                        PropertyMap metalnessObjects = pbrPropertyInfo.as_object().value();
                        for (auto &[metalnessPropertyName, metalnessPropertyInfo] : metalnessObjects)
                        {
                            if (metalnessPropertyName == "src")
                            {
                                if (!metalnessPropertyInfo.as_string())
                                    continue;

                                SceneMgr::Texture metalness_tex;
                                metalness_tex.src = metalnessPropertyInfo.as_string().value();
                                metalness_tex.numChannels = 1; // metalness
                                pbrMaterial.metalness = metalness_tex;

                                // if (std::holds_alternative<SceneMgr::Texture>(pbrMaterial.metalness))
                                // { // [PASS]
                                //     SceneMgr::Texture metalnessTexture = std::get<SceneMgr::Texture>(pbrMaterial.metalness);
                                //     std::cout << "pbr metalness (tex): " << metalnessTexture.src << std::endl;
                                // }
                            }
                            else
                            {
                                std::cerr << "[parse_material_object_info] (pbr) (metalness) (tex_property) Unknown metalness property name: " << metalnessPropertyName << std::endl;
                                continue;
                            }
                        }
                    }
                    else
                    {
                        std::cerr << "[parse_material_object_info] (pbr) (metalness) Unknown metalness type." << std::endl;
                        continue;
                    }
                }
                else
                {
                    std::cerr << "[parse_material_object_info] (pbr) Unknown pbr property name: " << pbrPropertyName << std::endl;
                    continue;
                }
            }

            materialObject->material = pbrMaterial;
            // std::cout << "pbr material added." << std::endl;
        }
        else if (propertyName == "lambertian")
        {
            materialObject->type = SceneMgr::MaterialType::LAMBERTIAN;

            SceneMgr::LambertianMaterial lambMaterial;

            PropertyMap lambObjects = propertyInfo.as_object().value();
            for (auto &[lambPropertyName, lambPropertyInfo] : lambObjects)
            {
                if (lambPropertyName == "albedo")
                {
                    if (lambPropertyInfo.as_array())
                    {
                        auto &albedoArray = lambPropertyInfo.as_array().value();
                        glm::vec3 albedoVec3;
                        for (int i = 0; i < 3; ++i)
                        {
                            if (!albedoArray[i].as_number())
                                continue;
                            albedoVec3[i] = (albedoArray[i].as_number().value());
                        }
                        lambMaterial.albedo = albedoVec3;

                        // if (std::holds_alternative<glm::vec3>(lambMaterial.albedo))
                        // { // [PASS]
                        //     glm::vec3 albedoValue = std::get<glm::vec3>(lambMaterial.albedo);
                        //     std::cout << "albedo (vec3): " << albedoValue.x << ", " << albedoValue.y << ", " << albedoValue.z << std::endl;
                        // }
                    }
                    else if (lambPropertyInfo.as_object())
                    {
                        PropertyMap albedoObjects = lambPropertyInfo.as_object().value();
                        for (auto &[albedoPropertyName, albedoPropertyInfo] : albedoObjects)
                        {
                            if (albedoPropertyName == "src")
                            {
                                if (!albedoPropertyInfo.as_string())
                                    continue;

                                SceneMgr::Texture albedo_tex;
                                albedo_tex.src = albedoPropertyInfo.as_string().value();
                                albedo_tex.numChannels = 3; // (r, g, b)
                                lambMaterial.albedo = albedo_tex;

                                // if (std::holds_alternative<SceneMgr::Texture>(lambMaterial.albedo))
                                // { // [PASS]
                                //     SceneMgr::Texture albedoTexture = std::get<SceneMgr::Texture>(lambMaterial.albedo);
                                //     std::cout << "albedo (tex): " << albedoTexture.src << std::endl;
                                // }
                            }
                            else
                            {
                                std::cerr << "[parse_material_object_info] (lamb) (albedo) (tex_property) Unknown albedo property name: " << albedoPropertyName << std::endl;
                                continue;
                            }
                        }
                    }
                    else
                    {
                        std::cerr << "[parse_material_object_info] (lamb) (albedo) Unknown albedo type." << std::endl;
                        continue;
                    }
                }
                else
                {
                    std::cerr << "[parse_material_object_info] (lamb) Unknown lamb property name: " << lambPropertyName << std::endl;
                    continue;
                }
            }

            materialObject->material = lambMaterial;
            // std::cout << "lamb material added." << std::endl;
        }
        else if (propertyName == "mirror")
        {
            materialObject->type = SceneMgr::MaterialType::MIRROR;
            // std::cout << "mirror material added." << std::endl;
        }
        else if (propertyName == "environment")
        {
            materialObject->type = SceneMgr::MaterialType::ENVIRONMENT;
            // std::cout << "environment material added." << std::endl;
        }
        else
        {
            std::cerr << "[parse_material_object_info] Unknown property name: " << propertyName << std::endl;
            continue;
        }
    }

    targetSceneMgr.materialObjectMap[materialObject->name] = materialObject;
    // std::cout << materialObject->name << " added to materialObjectMap." << std::endl;
}

void LoadMgr::parse_environment_object_info(OptionalPropertyMap &environmentObjectInfo, SceneMgr &targetSceneMgr)
{
    if (environmentObjectInfo == std::nullopt)
    {
        std::cerr << "[parse_environment_object_info] environmentObjectInfo is null." << std::endl;
        return;
    }

    SceneMgr::EnvironmentObject *environmentObject = new SceneMgr::EnvironmentObject;

    for (auto & [propertyName, propertyInfo] : environmentObjectInfo.value())
    {
        if (propertyName == "type")
        {
            continue;
        }
        else if (propertyName =="name")
        {
            if (!propertyInfo.as_string())
                continue;

            environmentObject->name = propertyInfo.as_string().value();
            // std::cout << environmentObject->name << std::endl; // [PASS]
        }
        else if (propertyName == "radiance")
        {
            if (!propertyInfo.as_object())
                continue;

            PropertyMap radianceObjects = propertyInfo.as_object().value();
            for (auto & [radiancePropertyName, radiancePropertyInfo] : radianceObjects)
            {
                if (radiancePropertyName == "src")
                {
                    if (!radiancePropertyInfo.as_string())
                        continue;
                    
                    environmentObject->radiance.src = radiancePropertyInfo.as_string().value();
                    // std::cout << "src " << environmentObject->radiance.src << std::endl; // [PASS]
                }
                else if (radiancePropertyName == "type")
                {
                    if (!radiancePropertyInfo.as_string())
                        continue;
                    
                    if (radiancePropertyInfo.as_string().value() == "cube")
                    {
                        environmentObject->radiance.numChannels = 3; // (r, g, b)
                    }
                    // TODO: more environment type
                    else 
                    {
                        environmentObject->radiance.numChannels = 0;
                    }

                    // std::cout << "type " << environmentObject->radiance.type << std::endl; // [PASS]
                }
                else if (radiancePropertyName == "format")
                {
                    continue;
                }
                else
                {
                    std::cerr << "[parse_environment_object_info] (radiance) Unknown radiance property name: " << radiancePropertyName << std::endl;
                    continue;
                }
            }
        }
        else
        {
            std::cerr << "[parse_environment_object_info] Unknown property name: " << propertyName << std::endl;
            continue;
        }
    }

    targetSceneMgr.environmentObject = environmentObject;
    // std::cout << environmentObject->name << " added to sceneMgr." << std::endl;
}

void LoadMgr::parse_light_object_info(OptionalPropertyMap &lightObjectInfo, SceneMgr &targetSceneMgr)
{
    if (lightObjectInfo == std::nullopt)
    {
        std::cerr << "[parse_light_object_info] lightObjectInfo is null." << std::endl;
        return;
    }

    SceneMgr::LightObject *lightObject = new SceneMgr::LightObject;

    for (auto & [propertyName, propertyInfo] : lightObjectInfo.value())
    {
        if (propertyName == "type")
        {
            continue;
        }
        else if (propertyName == "name")
        {
            if (!propertyInfo.as_string())
                continue;

            lightObject->name = propertyInfo.as_string().value();
            // std::cout << "name " << lightObject->name << std::endl;
        }
        else if (propertyName == "tint")
        {
            if (!propertyInfo.as_array())
                continue;

            auto &tintArray = propertyInfo.as_array().value();
            // std::cout << "tint ";
            for (int i = 0; i < 3; ++ i)
            {
                if (!tintArray[i].as_number())
                    continue;

                lightObject->tint[i] = tintArray[i].as_number().value();
                // std::cout << lightObject->tint[i] << ", ";
            }
            // std::cout << std::endl;
        }
        else if (propertyName == "sun")
        {
            SceneMgr::SunLight sunLight;

            if (!propertyInfo.as_object())
                continue;

            PropertyMap sunProperties = propertyInfo.as_object().value();
            for (auto & [sunPropertyName, sunPropertyInfo] : sunProperties)
            {
                if (sunPropertyName == "angle")
                {
                    if (!sunPropertyInfo.as_number())
                        continue;

                    sunLight.angle = sunPropertyInfo.as_number().value();
                    // std::cout << "angle " << sunLight.angle << std::endl;
                }
                else if (sunPropertyName == "strength")
                {
                    if (!sunPropertyInfo.as_number())
                        continue;

                    sunLight.strength = sunPropertyInfo.as_number().value();
                    // std::cout << "strength " << sunLight.strength << std::endl;
                }
                else
                {
                    std::cerr << "[parse_light_object_info] (sun) Unknown sun property name: " << sunPropertyName << std::endl;
                    continue;
                }
            }

            lightObject->light = sunLight;
            // std::cout << "SunLight read." << std::endl;
        }
        else if (propertyName == "sphere")
        {
            SceneMgr::SphereLight sphereLight;

            if (!propertyInfo.as_object())
                continue;

            PropertyMap sphereProperties = propertyInfo.as_object().value();
            for (auto & [spherePropertyName, spherePropertyInfo] : sphereProperties)
            {
                if (spherePropertyName == "radius")
                {
                    if (!spherePropertyInfo.as_number())
                        continue;

                    sphereLight.radius = spherePropertyInfo.as_number().value();
                    // std::cout << "radius " << sphereLight.radius << std::endl;
                }
                else if (spherePropertyName == "power")
                {
                    if (!spherePropertyInfo.as_number())
                        continue;

                    sphereLight.power = spherePropertyInfo.as_number().value();
                    // std::cout << "power " << sphereLight.power << std::endl;
                }
                else if (spherePropertyName == "limit")
                {
                    if (!spherePropertyInfo.as_number())
                        continue;

                    sphereLight.limit = spherePropertyInfo.as_number().value();
                    // std::cout << "limit " << sphereLight.limit << std::endl;
                }
                else
                {
                    std::cerr << "[parse_light_object_info] (sphere) Unknown sphere property name: " << spherePropertyName << std::endl;
                    continue;
                }
            }

            lightObject->light = sphereLight;
            // std::cout << "SphereLight read." << std::endl;
        }
        else if (propertyName == "spot")
        {
            SceneMgr::SpotLight spotLight;

            if (!propertyInfo.as_object())
                continue;

            PropertyMap spotProperties = propertyInfo.as_object().value();
            for (auto & [spotPropertyName, spotPropertyInfo] : spotProperties)
            {
                if (spotPropertyName == "radius")
                {
                    if (!spotPropertyInfo.as_number())
                        continue;
                    
                    spotLight.radius = spotPropertyInfo.as_number().value();
                    // std::cout << "radius " << spotLight.radius << std::endl;
                }
                else if (spotPropertyName == "power")
                {
                    if (!spotPropertyInfo.as_number())
                        continue;
                    
                    spotLight.power = spotPropertyInfo.as_number().value();
                    // std::cout << "power " << spotLight.power << std::endl;
                }
                else if (spotPropertyName == "fov")
                {
                    if (!spotPropertyInfo.as_number())
                        continue;

                    spotLight.fov = spotPropertyInfo.as_number().value();
                    // std::cout << "fov " << spotLight.fov << std::endl;
                }
                else if (spotPropertyName == "blend")
                {
                    if (!spotPropertyInfo.as_number())
                        continue;
                    
                    spotLight.blend = spotPropertyInfo.as_number().value();
                    // std::cout << "blend " << spotLight.blend << std::endl;
                }
                else if (spotPropertyName == "limit")
                {
                    if (!spotPropertyInfo.as_number())
                        continue;

                    spotLight.limit = spotPropertyInfo.as_number().value();
                    // std::cout << "limit " << spotLight.limit << std::endl;
                }
                else
                {
                    std::cerr << "[parse_light_object_info] (spot) Unknown spot property name: " << spotPropertyName << std::endl;
                    continue;
                }
            }

            lightObject->light = spotLight;
            // std::cout << "SpotLight read." << std::endl;
        }
        else if (propertyName == "shadow")
        {
            if (!propertyInfo.as_number())
                continue;

            lightObject->shadow = propertyInfo.as_number().value();
            // std::cout << "shadow " << lightObject->shadow << std::endl;
        }
        else
        {
            std::cerr << "[parse_light_object_info] Unknown property name: " << propertyName << std::endl;
            continue;
        }
    }

    targetSceneMgr.lightObjectMap[lightObject->name] = lightObject;
    // std::cout << lightObject->name << " added to lightObjectMap." << std::endl;
}

void LoadMgr::parse_sub_attribute_info(OptionalPropertyMap &subAttributeInfo, SceneMgr::AttributeStream &attrStream)
{
    if (subAttributeInfo == std::nullopt)
    {
        std::cerr << "[parse_sub_attribute_info] subAttributeInfo is null." << std::endl;
        return;
    }

    for (auto &[propertyName, propertyInfo] : subAttributeInfo.value())
    {
        if (propertyName == "src")
        {
            if (!propertyInfo.as_string())
                continue;

            attrStream.src = propertyInfo.as_string().value();
            // std::cout << attrStream.src << std::endl; // [PASS]
        }
        else if (propertyName == "offset")
        {
            if (!propertyInfo.as_number())
                continue;

            attrStream.offset = static_cast<uint32_t>(propertyInfo.as_number().value());
            // std::cout << attrStream.offset << std::endl; // [PASS]
        }
        else if (propertyName == "stride")
        {
            if (!propertyInfo.as_number())
                continue;

            attrStream.stride = static_cast<uint32_t>(propertyInfo.as_number().value());
            // std::cout << attrStream.stride << std::endl; // [PASS]
        }
        else if (propertyName == "format")
        {
            if (!propertyInfo.as_string())
                continue;

            std::string formatStr = propertyInfo.as_string().value();
            std::optional<VkFormat> typeConvertedResult = VkTypeHelper::findVkFormat(formatStr);
            if (typeConvertedResult == std::nullopt)
                continue;
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


// load mesh ---------------------------------------------------------------------------------------------------------------------

template <typename T>
void LoadMgr::read_s72_mesh_attribute_to_list(std::vector<T> &targetList, SceneMgr::AttributeStream &attrStream, std::string srcFolder)
{
    // safety check
    size_t size_format;
    if (attrStream.format == VK_FORMAT_R32G32B32_SFLOAT)
        size_format = 12;
    else if (attrStream.format == VK_FORMAT_R32G32B32A32_SFLOAT)
        size_format = 16;
    else if (attrStream.format == VK_FORMAT_R32G32_SFLOAT)
        size_format = 8;
    else
        size_format = 0;
    assert(size_format == sizeof(T));

    // get input file

    std::string path = srcFolder + attrStream.src;

    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to open file: " << path << std::endl;
        return;
    }

    // parse specific attribute

    // find the size of file
    //  cr. learnt from https://coniferproductions.com/posts/2022/10/25/reading-binary-files-cpp/
    file.seekg(0, std::ios_base::end);
    uint32_t size_file = file.tellg();

    file.seekg(0, std::ios_base::beg);

    for (uint32_t i = attrStream.offset; i < size_file; i += attrStream.stride)
    {
        T targetAttribute;

        // read the data
        //  cr. learnt from https://cplusplus.com/forum/beginner/224309/
        file.seekg(i, std::ios_base::beg);
        file.read(reinterpret_cast<char*>(&targetAttribute), sizeof(T));

        // if constexpr (std::is_same<T, glm::vec3>::value) { 
        //     std::cout << targetAttribute.x << ", " << targetAttribute.y << ", " << targetAttribute.z << std::endl; // [PASS]
        // }
        // if constexpr (std::is_same<T, glm::vec4>::value) {
        //     std::cout << targetAttribute.x << ", " << targetAttribute.y << ", " << targetAttribute.z << ", " << targetAttribute.w << std::endl; // [PASS]
        // }
        // if constexpr (std::is_same<T, glm::vec2>::value) {
        //     std::cout << targetAttribute.x << ", " << targetAttribute.y << std::endl; // [PASS]
        // }

        targetList.push_back(targetAttribute);
    }
}

// Explicit instantiations
template void LoadMgr::read_s72_mesh_attribute_to_list<glm::vec2>(std::vector<glm::vec2> &, SceneMgr::AttributeStream &, std::string srcFolder);
template void LoadMgr::read_s72_mesh_attribute_to_list<glm::vec3>(std::vector<glm::vec3> &, SceneMgr::AttributeStream &, std::string srcFolder);
template void LoadMgr::read_s72_mesh_attribute_to_list<glm::vec4>(std::vector<glm::vec4> &, SceneMgr::AttributeStream &, std::string srcFolder);


// load matrices -----------------------------------------------------------------------------------------------------------------

void LoadMgr::update_s72_node_matrices(SceneMgr &targetSceneMgr)
{
    using NodeObject = SceneMgr::NodeObject;

    if (targetSceneMgr.sceneObject == nullptr)
		return;

    // clean up
    
    targetSceneMgr.nodeMatrixMap.clear();
    targetSceneMgr.nodeMatrixMap.reserve(targetSceneMgr.nodeObjectMap.size());
    
    // bfs, get the node matrices

    struct NodeMatrix {
        NodeObject *nodeObject;
        glm::mat4 modelMatrix;
    };

	std::queue<NodeMatrix> nodeMatrixQueue;
	for (std::string & nodeName : targetSceneMgr.sceneObject->rootName)
	{
		auto findNodeResult = targetSceneMgr.nodeObjectMap.find(nodeName);
		if (findNodeResult == targetSceneMgr.nodeObjectMap.end())
			continue;


        NodeMatrix nodeMatrix;
        nodeMatrix.nodeObject = findNodeResult->second;
        nodeMatrix.modelMatrix = SceneMgr::calculate_model_matrix(
                                    nodeMatrix.nodeObject->translation, 
                                    nodeMatrix.nodeObject->rotation, 
                                    nodeMatrix.nodeObject->scale);

		nodeMatrixQueue.push(nodeMatrix);
	}

	while (!nodeMatrixQueue.empty())
	{
		// get the top node
		NodeMatrix current_nodeMatrix = nodeMatrixQueue.front();
		nodeMatrixQueue.pop();

        // record the node matrix pair
        targetSceneMgr.nodeMatrixMap[current_nodeMatrix.nodeObject->name] = current_nodeMatrix.modelMatrix;

        // cauculate the parent matrix for child nodes
        glm::mat4 &parentMatrix = current_nodeMatrix.modelMatrix;

		// push children to queue
		for (std::string & childName : current_nodeMatrix.nodeObject->childName)
		{
			auto findNodeResult = targetSceneMgr.nodeObjectMap.find(childName);
			if (findNodeResult == targetSceneMgr.nodeObjectMap.end())
				continue;

            NodeMatrix childNodeMatrix;
            childNodeMatrix.nodeObject = findNodeResult->second;
            glm::mat4 childLocalMatrix = SceneMgr::calculate_model_matrix(
                                            childNodeMatrix.nodeObject->translation, 
                                            childNodeMatrix.nodeObject->rotation, 
                                            childNodeMatrix.nodeObject->scale);
            childNodeMatrix.modelMatrix = parentMatrix * childLocalMatrix;

			nodeMatrixQueue.push(childNodeMatrix);
		}
	}
}


// load materials ---------------------------------------------------------------------------------------------------------------------

void LoadMgr::load_texture_from_file(unsigned char *&dst, const char *src, int &w, int &h, const int &desired_channels)
{
    int org_channels;
    dst = stbi_load(src, &w, &h, &org_channels, desired_channels);

    // std::cout << "[org_channels] " << org_channels << std::endl; // [PASS]

	if (dst == nullptr) 
    {
        std::stringstream ss;
        ss << "Failed to load texture data from " << src << std::endl;
		throw std::runtime_error(ss.str()); 
	} 
    // else // [PASS]
    // {
    //     std::cout << "Successfully loaded texture from " << src << std::endl;
    //     std::cout << "Width: " << w << ", Height: " << h << ", Channels: " << desired_channels << std::endl;
    // }
}

void LoadMgr::load_cubemap_from_file(unsigned char **dst, const char *src, int &w, int &h, int &org_channels, const int &desired_channels, const int &NUM_CUBE_FACES, bool flip)
{
    stbi_set_flip_vertically_on_load(flip); // if true, adapt .s72 (+y up) to vulkan coords (-y down)
	unsigned char *cubemap_data = stbi_load(src, &w, &h, &org_channels, desired_channels); // format: RGBARGBA...

	if (cubemap_data == nullptr)  {
		throw std::runtime_error("falied to load environment cubemap data."); 
	}
    if (h % NUM_CUBE_FACES != 0) {
        std::stringstream ss;
        ss << "Invalid cubemap. The height of cubemap should be divisible by " << NUM_CUBE_FACES << ".";
        throw std::runtime_error(ss.str());
    }

	// std::cout << "[Total texels]: src: " << src << "; channel: " << org_channels << "; size: " 
    //     << w << " * " << h << std::endl; // [PASS]

    int face_w = w;
    int face_h = h / 6;
    int bytes_per_face = face_w * face_h * (desired_channels); // w * h * bytes_per_pixel

    for (int i = 0; i < 6; ++ i)
    {
        dst[i] = new unsigned char[bytes_per_face]; 
        memcpy(dst[i], cubemap_data + i * bytes_per_face, bytes_per_face);
    }

    // verification
    // save_cubemap_faces_as_images(dst, face_w, face_h, desired_channels); // [PASS]

	stbi_image_free(cubemap_data);
}


void LoadMgr::save_cubemap_faces_as_images(unsigned char **dst, int face_w, int face_h, int desired_channels) 
{
    for (int i = 0; i < 6; ++ i) 
    {
        char filename[50];
        snprintf(filename, sizeof(filename), "./Assets/Cubemap/Verification/face_%d.png", i);

        int result = stbi_write_png(filename, face_w, face_h, desired_channels, dst[i], face_w * desired_channels);

        if (result) {
            std::cout << "Saved " << filename << " successfully." << std::endl;
        } else {
            std::cerr << "Failed to save " << filename << std::endl;
        }
    }
}

void LoadMgr::rotate_cubemap_face_by_90_cw(unsigned char *face, const int &w, const int &h, const int &channels)
{
    int face_size = w * h * channels;
    std::vector<char> tmp_face(face_size);

    for (int y = 0; y < h; ++ y)
    {
        for (int x = 0; x < w; ++ x)
        {
            int src_idx_x = x;
            int src_idx_y = y;
            int src_idx = (src_idx_y * w + src_idx_x) * channels;
            
            int dst_idx_x = w - y;
            int dst_idx_y = x;
            int dst_idx = (dst_idx_y * w + dst_idx_x) * channels;

            for (int c = 0; c < channels; ++ c) 
                tmp_face[dst_idx + c] = face[src_idx + c];
        }
    }

    memcpy(face, tmp_face.data(), face_size);
}


void LoadMgr::rotate_cubemap_face_by_90_ccw(unsigned char *face, const int &w, const int &h, const int &channels)
{
    int face_size = w * h * channels;
    std::vector<char> tmp_face(face_size);

    for (int y = 0; y < h; ++ y)
    {
        for (int x = 0; x < w; ++ x)
        {
            int src_idx_x = x;
            int src_idx_y = y;
            int src_idx = (src_idx_y * w + src_idx_x) * channels;
            
            int dst_idx_x = y;
            int dst_idx_y = h - x;
            int dst_idx = (dst_idx_y * w + dst_idx_x) * channels;

            for (int c = 0; c < channels; ++ c) 
                tmp_face[dst_idx + c] = face[src_idx + c];
        }
    }

    memcpy(face, tmp_face.data(), face_size);
}


void LoadMgr::horizontal_flip_cubemap_face(unsigned char *face, const int &w, const int &h, const int &channels)
{
    int face_size = w * h * channels;
    std::vector<char> tmp_face(face_size);

    for (int y = 0; y < h; ++ y)
    {
        for (int x = 0; x < w; ++ x)
        {
            int src_idx_x = x;
            int src_idx_y = y;
            int src_idx = (src_idx_y * w + src_idx_x) * channels;
            
            int dst_idx_x = w - x;
            int dst_idx_y = y;
            int dst_idx = (dst_idx_y * w + dst_idx_x) * channels;

            for (int c = 0; c < channels; ++ c) 
                tmp_face[dst_idx + c] = face[src_idx + c];
        }
    }

    memcpy(face, tmp_face.data(), face_size);
}

void LoadMgr::vertical_flip_cubemap_face(unsigned char *face, const int &w, const int &h, const int &channels)
{
    int face_size = w * h * channels;
    std::vector<char> tmp_face(face_size);

    for (int y = 0; y < h; ++ y)
    {
        for (int x = 0; x < w; ++ x)
        {
            int src_idx_x = x;
            int src_idx_y = y;
            int src_idx = (src_idx_y * w + src_idx_x) * channels;
            
            int dst_idx_x = x;
            int dst_idx_y = h - y;
            int dst_idx = (dst_idx_y * w + dst_idx_x) * channels;

            for (int c = 0; c < channels; ++ c) 
                tmp_face[dst_idx + c] = face[src_idx + c];
        }
    }

    memcpy(face, tmp_face.data(), face_size);
}


// OBJ Loader functions =============================================================================================================================================

void LoadMgr::load_line_from_OBJ(const std::string &path, std::vector<PosColVertex> &mesh_vertices)
{
    std::vector<Vector3> vertices;

    PosColVertex tmp_vertex{
        {0.0f, 0.0f, 0.0f}, // Position initialized to zeros
        {255, 0, 0, 255}    // Color initialized to red
    };

    vertices.clear();
    mesh_vertices.clear();

    vertices.push_back({0, 0, 0}); // padding to match 1-based indexing

    std::ifstream file(path);
    if (!file)
    {
        std::cerr << "Failed to open file: " << path << std::endl;
        return;
    }

    std::string line;
    while (getline(file, line))
    {
        std::istringstream stream(line);
        char type;
        stream >> type;

        if (type == 'v')
        {
            Vector3 vertex;
            stream >> vertex.x >> vertex.y >> vertex.z;
            vertices.push_back(vertex);
            // std::cout << "Vertex: " << vertex.x << ", " << vertex.y << ", " << vertex.z << std::endl;
        }
        else if (type == 'f')
        {
            std::string descriptor;
            std::vector<int> indices;
            while (stream >> descriptor)
            {
                size_t firstSlash = descriptor.find('/', 1);
                int v1 = std::stoi(descriptor.substr(0, firstSlash));
                indices.push_back(v1);
            }

            int size_indices = indices.size();
            for (int i = 0; i < size_indices; ++i)
            {
                int v1 = indices[i];
                int v2 = indices[(i + 1) % size_indices];

                mesh_vertices.push_back({{vertices[v1].x, vertices[v1].y, vertices[v1].z},
                                         tmp_vertex.Color});
                mesh_vertices.push_back({{vertices[v2].x, vertices[v2].y, vertices[v2].z},
                                         tmp_vertex.Color});
                // std::cout << "Line: " << v1 << ", " << v2 << std::endl;
            }
        }
    }

    // std::cout << "Loaded " << path << " with " << mesh_vertices.size() << " vertices." << std::endl;
    return;
}

void LoadMgr::load_object_from_OBJ(const std::string &path, std::vector<MeshAttribute> &mesh_vertices)
{

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
    if (!file)
    {
        std::cerr << "Failed to open file: " << path << std::endl;
        return;
    }

    std::string line;
    while (getline(file, line))
    {
        std::istringstream stream(line);
        std::string type;
        stream >> type;

        if (type == "v")
        {
            Vector3 vertex;
            stream >> vertex.x >> vertex.y >> vertex.z;
            vertices.push_back(vertex);
            // std::cout << "Vertex: " << vertex.x << ", " << vertex.y << ", " << vertex.z << std::endl;
        }
        else if (type == "vt")
        {

            Vector2 texcoord;
            stream >> texcoord.x >> texcoord.y;
            texcoords.push_back(texcoord);
            // std::cout << "Texcoord: " << texcoord.x << ", " << texcoord.y << std::endl;
        }
        else if (type == "vn")
        {

            Vector3 normal;
            stream >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
            // std::cout << "Normal: " << normal.x << ", " << normal.y << ", " << normal.z << std::endl;
        }
        else if (type == "f")
        {

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

            if (size_vertices == 3)
            { // parse according to the number of vertices in the face
                for (int i = 0; i < size_vertices; ++i)
                {
                    int v1 = face_vertices[i].v;
                    int vt1 = face_vertices[i].vt;
                    int vn1 = face_vertices[i].vn;

                    mesh_vertices.push_back({{vertices[v1].x, vertices[v1].y, vertices[v1].z},
                                             {normals[vn1].x, normals[vn1].y, normals[vn1].z},
                                             {0.0, 0.0, 0.0, 1.0},
                                             {texcoords[vt1].x, texcoords[vt1].y}});
                }
            }
            else if (size_vertices == 4)
            {
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
                mesh_vertices.push_back({{vertices[vA].x, vertices[vA].y, vertices[vA].z},
                                         {normals[vnA].x, normals[vnA].y, normals[vnA].z},
                                         {0.0, 0.0, 0.0, 1.0},
                                         {texcoords[vtA].x, texcoords[vtA].y}});
                mesh_vertices.push_back({{vertices[vB].x, vertices[vB].y, vertices[vB].z},
                                         {normals[vnB].x, normals[vnB].y, normals[vnB].z},
                                         {0.0, 0.0, 0.0, 1.0},
                                         {texcoords[vtB].x, texcoords[vtB].y}});
                mesh_vertices.push_back({{vertices[vC].x, vertices[vC].y, vertices[vC].z},
                                         {normals[vnC].x, normals[vnC].y, normals[vnC].z},
                                         {0.0, 0.0, 0.0, 1.0},
                                         {texcoords[vtC].x, texcoords[vtC].y}});

                // triangle 2
                mesh_vertices.push_back({{vertices[vA].x, vertices[vA].y, vertices[vA].z},
                                         {normals[vnA].x, normals[vnA].y, normals[vnA].z},
                                         {0.0, 0.0, 0.0, 1.0},
                                         {texcoords[vtA].x, texcoords[vtA].y}});
                mesh_vertices.push_back({{vertices[vC].x, vertices[vC].y, vertices[vC].z},
                                         {normals[vnC].x, normals[vnC].y, normals[vnC].z},
                                         {0.0, 0.0, 0.0, 1.0},
                                         {texcoords[vtC].x, texcoords[vtC].y}});
                mesh_vertices.push_back({{vertices[vD].x, vertices[vD].y, vertices[vD].z},
                                         {normals[vnD].x, normals[vnD].y, normals[vnD].z},
                                         {0.0, 0.0, 0.0, 1.0},
                                         {texcoords[vtD].x, texcoords[vtD].y}});
            }
        }
    }

    // std::cout << "Loaded " << path << " with " << mesh_vertices.size() << " vertices." << std::endl;
    return;
}
