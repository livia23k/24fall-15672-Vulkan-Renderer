#include "Source/Tools/SceneMgr.hpp"
#include <glm/gtc/matrix_transform.hpp>

SceneMgr::SceneMgr()
{
    sceneObject = nullptr;
    nodeObjectMap.clear();
    meshObjectMap.clear();
    cameraObjectMap.clear();
    driverObjectMap.clear();
    materialObjectMap.clear();
    lightObjectMap.clear();

    meshVerticesIndexMap.clear();
    textureIndexMap.clear();
    materialTextureIndexMap.clear();
    materialPropertiesMap.clear();
    nodeMatrixMap.clear();

    LambertianMaterial lambertian;
    lambertian.albedo = glm::vec3(0.8f);
    defaultMaterial = new MaterialObject();
    defaultMaterial->name = "default"; // WARNING: make sure "default" is not the name of all material in .s72
    defaultMaterial->type = MaterialType::LAMBERTIAN;
    defaultMaterial->material = lambertian;

    materialObjectMap[defaultMaterial->name] = defaultMaterial;
}

SceneMgr::~SceneMgr()
{
    this->clean_all();
}

void SceneMgr::clean_all()
{
    // delete defaultMaterial; // defaultMaterial is removed during the cleaning of materialObjectMap

    if (sceneObject)
    {
        delete sceneObject;
        sceneObject = nullptr;
    }

    for (auto &pair : nodeObjectMap)
    {
        delete pair.second;
    }
    nodeObjectMap.clear();

    for (auto &pair : meshObjectMap)
    {
        delete pair.second;
    }
    meshObjectMap.clear();

    for (auto &pair : cameraObjectMap)
    {
        delete pair.second;
    }
    cameraObjectMap.clear();

    for (auto &pair : driverObjectMap)
    {
        delete pair.second;
    }
    driverObjectMap.clear();

    for (auto &pair : materialObjectMap)
    {
        delete pair.second;
    }
    materialObjectMap.clear();

    for (auto &pair : lightObjectMap)
    {
        delete pair.second;
    }
    lightObjectMap.clear();

    for (auto &pair : materialPropertiesMap)
    {
        delete pair.second;
    }
    materialPropertiesMap.clear();

    meshVerticesIndexMap.clear();
    textureIndexMap.clear();
    materialTextureIndexMap.clear();
    nodeMatrixMap.clear();
}


// Function functions ========================================================================================================================

float SceneMgr::get_animation_duration()
{
    float maxDuration = 0.f;
    for (auto &pair : driverObjectMap) 
    {
        DriverObject *driver = pair.second;

        float tmax = driver->times.back();
        maxDuration = std::max(maxDuration, tmax);
    }
    return maxDuration;
}

void SceneMgr::update_nodes_from_animation_drivers(float targetTime)
{
    for (auto &pair : driverObjectMap) 
    {
        DriverObject *driver = pair.second;

        auto it = std::lower_bound(driver->times.begin(), driver->times.end(), targetTime);

        // animation finished, no action needed
        if (it == driver->times.end()) 
        {
            continue;
        }

        // find target node
        auto findNodeResult = nodeObjectMap.find(driver->refObjectName);
        if (findNodeResult == nodeObjectMap.end())
        {
            std::cerr << "[SceneMgr] (update_nodes_from_animation_drivers) Node not found: " << driver->refObjectName << std::endl;
            continue;
        }
        NodeObject *nodeObject = findNodeResult->second;

        // executing animation, need to update the node 
        size_t sizeTimes = driver->times.size();

        size_t prev = std::distance(driver->times.begin(), it);
        float prevTime = driver->times[prev];

        if (driver->interpolation == DriverInterpolation::STEP)
        {
            if (driver->channel == DriverChannleType::TRANSLATION)
            {
                glm::vec3 new_translation = extract_vec3(driver->values, prev);
                nodeObject->translation = new_translation;
            }
            else if (driver->channel == DriverChannleType::SCALE)
            {
                glm::vec3 new_scale = extract_vec3(driver->values, prev);
                nodeObject->scale = new_scale;
            }
            else if (driver->channel == DriverChannleType::ROTATION)
            {
                glm::quat new_rotation = extract_quat(driver->values, prev);
                nodeObject->rotation = new_rotation;
            }
        }

        size_t next = (prev == sizeTimes - 1) ?  prev : prev + 1;
        float nextTime = driver->times[next];

        float w = (targetTime - prevTime) / (nextTime - prevTime);
        w = glm::clamp(w, 0.0f, 1.0f); // avoid jittering issue

        if (driver->interpolation == DriverInterpolation::LINEAR)
        {
            if (driver->channel == DriverChannleType::TRANSLATION)
            {
                glm::vec3 prev_translation = extract_vec3(driver->values, prev);
                glm::vec3 next_translation = extract_vec3(driver->values, next);
                glm::vec3 new_translation = linear_interpolation_vec3(prev_translation, next_translation, w);
                nodeObject->translation = new_translation;
            }
            else if (driver->channel == DriverChannleType::SCALE)
            {
                glm::vec3 prev_scale = extract_vec3(driver->values, prev);
                glm::vec3 next_scale = extract_vec3(driver->values, next);
                glm::vec3 new_scale = linear_interpolation_vec3(prev_scale, next_scale, w);
                nodeObject->scale = new_scale;
            }
            else if (driver->channel == DriverChannleType::ROTATION)
            {
                glm::quat prev_rotation = extract_quat(driver->values, prev);
                glm::quat next_rotation = extract_quat(driver->values, next);
                glm::quat new_rotation = slerp_interpolation_quat(prev_rotation, next_rotation, w);
                nodeObject->rotation = new_rotation;
            }
        }
        else if (driver->interpolation == DriverInterpolation::SLERP)
        {
            if (driver->channel == DriverChannleType::ROTATION)
            {
                glm::quat prev_rotation = extract_quat(driver->values, prev);
                glm::quat next_rotation = extract_quat(driver->values, next);
                glm::quat new_rotation = slerp_interpolation_quat(prev_rotation, next_rotation, w);
                nodeObject->rotation = new_rotation;
            }
        }
    }
}

glm::vec3 SceneMgr::extract_vec3(const std::vector<float>& values, size_t idx)
{
    return glm::vec3(values[3 * idx], values[3 * idx + 1], values[3 * idx + 2]);
}

glm::quat SceneMgr::extract_quat(const std::vector<float>& values, size_t idx)
{
    return glm::quat(values[4 * idx + 3], values[4 * idx + 0], values[4 * idx + 1], values[4 * idx + 2]); // w, x, y, z in s72; x, y, z, w in glm::quat
}

glm::vec3 SceneMgr::linear_interpolation_vec3(const glm::vec3 &prev, const glm::vec3 &next, float w)
{
    return prev * w + next * (1.f - w);
}

glm::quat SceneMgr::slerp_interpolation_quat(const glm::quat &prev, const glm::quat &next, float w)
{
    return glm::slerp(prev, next, w);
}


glm::mat4 SceneMgr::calculate_model_matrix(glm::vec3 translation, glm::quat rotation, glm::vec3 scale)
{
    glm::mat4 identity = glm::mat4(1.0f);

    glm::mat4 T = glm::translate(identity, translation);
    glm::mat4 R = glm::mat4_cast(rotation); /* learned from https://stackoverflow.com/questions/38145042/quaternion-to-matrix-using-glm */
    glm::mat4 S = glm::scale(identity, scale);

    return T * R * S; // S first, R later, T last
}

// Printer functions (single object) =========================================================================================================

void SceneMgr::print_glm_mat4(const glm::mat4& matrix) 
{
    std::cout << "mat4 (" << std::endl;
    for (int i = 0; i < 4; ++i) {
        std::cout << "  ";
        for (int j = 0; j < 4; ++j) {
            std::cout << matrix[j][i];
            if (j < 3) std::cout << ", ";
        }
        std::cout << std::endl;
    }
    std::cout << ")" << std::endl;
}


void SceneMgr::print_single_node_object(NodeObject *nodeObject)
{
    if (!nodeObject)
        return;

    std::cout << "[NodeObject]" << std::endl;
    std::cout << "  Name: " << nodeObject->name << std::endl;

    std::cout << "  Translation: ";
    for (int i = 0; i < 3; ++i)
    {
        std::cout << nodeObject->translation[i] << ", ";
    }
    std::cout << std::endl;

    std::cout << "  Scale: ";
    for (int i = 0; i < 3; ++i)
    {
        std::cout << nodeObject->scale[i] << ", ";
    }
    std::cout << std::endl;

    std::cout << "  Rotation: ";
    for (int i = 0; i < 4; ++i)
    {
        std::cout << nodeObject->rotation[i] << ", ";
    }
    std::cout << std::endl;

    std::cout << "  Child Names: ";
    for (const auto &child : nodeObject->childName)
    {
        std::cout << child << " ";
    }
    std::cout << std::endl;

    std::cout << "  Camera Name: " << nodeObject->refCameraName << std::endl;
    std::cout << "  Mesh Name: " << nodeObject->refMeshName << std::endl;
    std::cout << "  Environment Name: " << nodeObject->refEnvironmentName << std::endl;
    std::cout << "  Light Name: " << nodeObject->refLightName << std::endl;

    std::cout << std::endl;
}

void SceneMgr::print_single_mesh_object(MeshObject *meshObject)
{
    if (!meshObject)
        return;

    std::cout << "[MeshObject]" << std::endl;

    std::cout << "  Name: " << meshObject->name << std::endl;
    std::cout << "  VkPrimitiveTopology: " << meshObject->topology << std::endl;
    std::cout << "  Count: " << meshObject->count << std::endl;

    std::cout << "  Indices: " << std::endl;
    std::cout << "    src: " << meshObject->indices.src << std::endl;
    std::cout << "    offset: " << meshObject->indices.offset << std::endl;
    std::cout << "    VkIndexType: " << meshObject->indices.format << std::endl;

    std::cout << "  Attributes (Position): " << std::endl;
    std::cout << "    src: " << meshObject->attrPosition.src << std::endl;
    std::cout << "    offset: " << meshObject->attrPosition.offset << std::endl;
    std::cout << "    stride: " << meshObject->attrPosition.stride << std::endl;
    std::cout << "    VkFormat: " << meshObject->attrPosition.format << std::endl;

    std::cout << "  Attributes (Normal): " << std::endl;
    std::cout << "    src: " << meshObject->attrNormal.src << std::endl;
    std::cout << "    offset: " << meshObject->attrNormal.offset << std::endl;
    std::cout << "    stride: " << meshObject->attrNormal.stride << std::endl;
    std::cout << "    VkFormat: " << meshObject->attrNormal.format << std::endl;

    std::cout << "  Attributes (Tangent): " << std::endl;
    std::cout << "    src: " << meshObject->attrTangent.src << std::endl;
    std::cout << "    offset: " << meshObject->attrTangent.offset << std::endl;
    std::cout << "    stride: " << meshObject->attrTangent.stride << std::endl;
    std::cout << "    VkFormat: " << meshObject->attrTangent.format << std::endl;

    std::cout << "  Attributes (Texcoord): " << std::endl;
    std::cout << "    src: " << meshObject->attrTexcoord.src << std::endl;
    std::cout << "    offset: " << meshObject->attrTexcoord.offset << std::endl;
    std::cout << "    stride: " << meshObject->attrTexcoord.stride << std::endl;
    std::cout << "    VkFormat: " << meshObject->attrTexcoord.format << std::endl;

    std::cout << "  Material Name: " << meshObject->refMaterialName << std::endl;

    std::cout << std::endl;
}

void SceneMgr::print_single_camera_object(CameraObject *cameraObject)
{
    if (!cameraObject)
        return;

    std::cout << "[CameraObject]" << std::endl;

    std::cout << "  Name: " << cameraObject->name << std::endl;
    std::cout << "  ProjectionType: " << cameraObject->projectionType << std::endl;

    std::cout << "  ProjectionParameters: ";
    if (cameraObject->projectionType == ProjectionType::Perspective)
    {
        std::cout << "(Perspective)" << std::endl;
        PerspectiveParameters perspectiveParameters = std::get<PerspectiveParameters>(cameraObject->projectionParameters);
        std::cout << "    Aspect: " << perspectiveParameters.aspect << std::endl;
        std::cout << "    Fov: " << perspectiveParameters.vfov << std::endl;
        std::cout << "    Near: " << perspectiveParameters.nearZ << std::endl;
        std::cout << "    Far: " << perspectiveParameters.farZ << std::endl;
    }
    else
    {
        std::cout << "(Orthographic)" << std::endl;
        OrthographicParameters orthographicParameters = std::get<OrthographicParameters>(cameraObject->projectionParameters);
        std::cout << "    Left: " << orthographicParameters.left << std::endl;
        std::cout << "    Right: " << orthographicParameters.right << std::endl;
        std::cout << "    Bottom: " << orthographicParameters.bottom << std::endl;
        std::cout << "    Top: " << orthographicParameters.top << std::endl;
        std::cout << "    Near: " << orthographicParameters.nearZ << std::endl;
        std::cout << "    Far: " << orthographicParameters.farZ << std::endl;
    }

    std::cout << std::endl;
}

void SceneMgr::print_single_driver_object(DriverObject *driverObject)
{
    if (!driverObject)
        return;

    std::cout << "[DriverObject]" << std::endl;

    std::cout << "  Name: " << driverObject->name << std::endl;
    std::cout << "  RefObjectName: " << driverObject->refObjectName << std::endl;
    std::cout << "  Channel: " << driverObject->channel << std::endl;
    std::cout << "  Channel Dimension: " << driverObject->channelDim << std::endl;

    std::cout << "  Times: ";
    for (const auto &time : driverObject->times)
    {
        std::cout << time << ", ";
    }
    std::cout << std::endl;

    std::cout << "  Values: ";
    int valuesSize = driverObject->values.size();
    for (int i = 0; i < valuesSize; ++i)
    {
        int count = i % driverObject->channelDim;
        if (count == 0)
            std::cout << "; ";
        std::cout << driverObject->values[i] << ", ";
    }
    std::cout << std::endl;

    std::cout << "  Interpolation: ";
    if (driverObject->interpolation == DriverInterpolation::STEP)
        std::cout << "STEP" << std::endl;
    else if (driverObject->interpolation == DriverInterpolation::LINEAR)
        std::cout << "LINEAR" << std::endl;
    else if (driverObject->interpolation == DriverInterpolation::SLERP)
        std::cout << "SLERP" << std::endl;

    std::cout << std::endl;
}

void SceneMgr::print_single_material_object(MaterialObject *materialObject)
{
    if (!materialObject)
        return;

    std::cout << "[MaterialObject]" << std::endl;

    std::cout << "  Name: " << materialObject->name << std::endl;

    if (materialObject->normalmap != std::nullopt)
    {
        std::cout << "  Normalmap: " << std::endl;
        std::cout << "    src: " << materialObject->normalmap->src << std::endl;
        std::cout << "    numChannels: " << materialObject->normalmap->numChannels << std::endl;
    }

    if (materialObject->displacementmap != std::nullopt)
    {
        std::cout << "  Displacementmap: " << std::endl;
        std::cout << "    src: " << materialObject->displacementmap->src << std::endl;
        std::cout << "    numChannels: " << materialObject->displacementmap->numChannels << std::endl;
    }

    std::cout << "  Material Type: ";
    if (materialObject->type == MaterialType::PBR)
        std::cout << "PBR" << std::endl;
    else if (materialObject->type == MaterialType::LAMBERTIAN)
        std::cout << "LAMBERTIAN" << std::endl;
    else if (materialObject->type == MaterialType::MIRROR)
        std::cout << "MIRROR" << std::endl;
    else if (materialObject->type == MaterialType::ENVIRONMENT)
        std::cout << "ENVIRONMENT" << std::endl;

    std::cout << "  Material Properties: ";
    if (materialObject->type == MaterialType::PBR)
    {
        PBRMaterial pbrMaterial = std::get<PBRMaterial>(materialObject->material);
        std::cout << "(PBR)" << std::endl;

        std::cout << "    Albedo: ";
        if (std::holds_alternative<glm::vec3>(pbrMaterial.albedo))
        {
            glm::vec3 albedoValue = std::get<glm::vec3>(pbrMaterial.albedo);
            std::cout << "(vec3) " << albedoValue.x << ", " << albedoValue.y << ", " << albedoValue.z << std::endl;
        }
        else if (std::holds_alternative<Texture>(pbrMaterial.albedo))
        {
            Texture albedoTexture = std::get<Texture>(pbrMaterial.albedo);
            std::cout << "(Texture) " << albedoTexture.src << std::endl;
        }

        std::cout << "    Roughness: ";
        if (std::holds_alternative<float>(pbrMaterial.roughness))
        {
            float roughnessValue = std::get<float>(pbrMaterial.roughness);
            std::cout << "(float) " << roughnessValue << std::endl;
        }
        else if (std::holds_alternative<Texture>(pbrMaterial.roughness))
        {
            Texture roughnessTexture = std::get<Texture>(pbrMaterial.roughness);
            std::cout << "(Texture) " << roughnessTexture.src << std::endl;
        }

        std::cout << "    Metalness: ";
        if (std::holds_alternative<float>(pbrMaterial.metalness))
        {
            float metalnessValue = std::get<float>(pbrMaterial.metalness);
            std::cout << "(float) " << metalnessValue << std::endl;
        }
        else if (std::holds_alternative<Texture>(pbrMaterial.metalness))
        {
            Texture metalnessTexture = std::get<Texture>(pbrMaterial.metalness);
            std::cout << "(Texture) " << metalnessTexture.src << std::endl;
        }

        std::cout << std::endl;
    }
    else if (materialObject->type == MaterialType::LAMBERTIAN)
    {
        LambertianMaterial lambMaterial = std::get<LambertianMaterial>(materialObject->material);
        std::cout << "(LAMBERTIAN)" << std::endl;

        std::cout << "    Albedo: ";
        if (std::holds_alternative<glm::vec3>(lambMaterial.albedo))
        {
            glm::vec3 albedoValue = std::get<glm::vec3>(lambMaterial.albedo);
            std::cout << "(vec3) " << albedoValue.x << ", " << albedoValue.y << ", " << albedoValue.z << std::endl;
        }
        else if (std::holds_alternative<Texture>(lambMaterial.albedo))
        {
            Texture albedoTexture = std::get<Texture>(lambMaterial.albedo);
            std::cout << "(Texture) " << albedoTexture.src << std::endl;
        }

        std::cout << std::endl;
    }
    else if (materialObject->type == MaterialType::MIRROR)
    {
        std::cout << "(MIRROR)" << std::endl;
    }
    else if (materialObject->type == MaterialType::ENVIRONMENT)
    {
        std::cout << "(ENVIRONMENT)" << std::endl;
    }

    std::cout << std::endl;
}

void SceneMgr::print_single_environment_object(EnvironmentObject* environmentObject)
{
    if (!environmentObject)
        return;

    std::cout << "[EnvironmentObject]" << std::endl;

    std::cout << "  Name: " << environmentObject->name << std::endl;

    std::cout << "  Radiance Texture: " << std::endl;
    std::cout << "    src: " << environmentObject->radiance.src << std::endl;
    std::cout << "    numChannels: " << environmentObject->radiance.numChannels << std::endl;

    std::cout << std::endl;
}

void SceneMgr::print_single_light_object(LightObject* lightObject)
{
    if (!lightObject)
        return;
    
    std::cout << "[LightObject]" << std::endl;

    std::cout << "  Name: " << lightObject->name << std::endl;
    std::cout << "  Tint: " << lightObject->tint.x << ", " << lightObject->tint.y << ", " << lightObject->tint.z << std::endl;

    std::cout << "  Light: ";
    if (std::holds_alternative<SunLight>(lightObject->light))
    {
        SunLight sunLight = std::get<SunLight>(lightObject->light);
        std::cout << "(SunLight)" << std::endl;
        std::cout << "    Angle: " << sunLight.angle << std::endl;
        std::cout << "    Strength: " << sunLight.strength << std::endl;
    }
    else if (std::holds_alternative<SphereLight>(lightObject->light))
    {
        SphereLight sphereLight = std::get<SphereLight>(lightObject->light);
        std::cout << "(SphereLight)" << std::endl;
        std::cout << "    Radius: " << sphereLight.radius << std::endl;
        std::cout << "    Power: " << sphereLight.power << std::endl;
        std::cout << "    Limit: " << sphereLight.limit << std::endl;

    }
    else if (std::holds_alternative<SpotLight>(lightObject->light))
    {
        SpotLight spotLight = std::get<SpotLight>(lightObject->light);
        std::cout << "(SpotLight)" << std::endl;
        std::cout << "    Radius: " << spotLight.radius << std::endl;
        std::cout << "    Power: " << spotLight.power << std::endl;
        std::cout << "    Fov: " << spotLight.fov << std::endl;
        std::cout << "    Blend: " << spotLight.blend << std::endl;
        std::cout << "    Limit: " << spotLight.limit << std::endl;
    }

    std::cout << "  Shadow: " << lightObject->shadow << std::endl;

    std::cout << std::endl;
}

// Printer functions (all objects) =========================================================================================================

void SceneMgr::print_node_object_map()
{
    for (auto &pair : nodeObjectMap)
    {
        print_single_node_object(pair.second);
    }
}

void SceneMgr::print_mesh_object_map()
{
    for (auto &pair : meshObjectMap)
    {
        print_single_mesh_object(pair.second);
    }
}

void SceneMgr::print_camera_object_map()
{
    for (auto &pair : cameraObjectMap)
    {
        print_single_camera_object(pair.second);
    }
}

void SceneMgr::print_driver_object_map()
{
    for (auto &pair : driverObjectMap)
    {
        print_single_driver_object(pair.second);
    }
}

void SceneMgr::print_material_object_map()
{
    for (auto &pair : materialObjectMap)
    {
        print_single_material_object(pair.second);
    }
}

void SceneMgr::print_light_object_map()
{
    for (auto &pair : lightObjectMap)
    {
        print_single_light_object(pair.second);
    }
}
