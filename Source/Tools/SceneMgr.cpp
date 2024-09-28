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

    meshVerticesIndexMap.clear();
}

SceneMgr::~SceneMgr()
{
    this->clean_all();
}

void SceneMgr::clean_all()
{
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
}

// Function functions ========================================================================================================================

glm::mat4 SceneMgr::calculate_model_matrix(glm::vec3 translation, glm::quat rotation, glm::vec3 scale)
{
    glm::mat4 identity = glm::mat4(1.0f);

    glm::mat4 T = glm::translate(identity, translation);
    glm::mat4 R = glm::mat4_cast(rotation); /* learned from https://stackoverflow.com/questions/38145042/quaternion-to-matrix-using-glm */
    glm::mat4 S = glm::scale(identity, scale);

    return S * R * T;
}

// Printer functions (single object) =========================================================================================================

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
        std::cout << "    type: " << materialObject->normalmap->type << std::endl;
        std::cout << "    format: " << materialObject->normalmap->format << std::endl;
    }

    if (materialObject->displacementmap != std::nullopt)
    {
        std::cout << "  Displacementmap: " << std::endl;
        std::cout << "    src: " << materialObject->displacementmap->src << std::endl;
        std::cout << "    type: " << materialObject->displacementmap->type << std::endl;
        std::cout << "    format: " << materialObject->displacementmap->format << std::endl;
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
    std::cout << "    type: " << environmentObject->radiance.type << std::endl;
    std::cout << "    format: " << environmentObject->radiance.format << std::endl;

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

void SceneMgr::print_environment_object_map()
{
    for (auto &pair : environmentObjectMap)
    {
        print_single_environment_object(pair.second);
    }
}

void SceneMgr::print_light_object_map()
{
    for (auto &pair : lightObjectMap)
    {
        print_single_light_object(pair.second);
    }
}
