#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <vector>
#include <map>
#include <iterator>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "Source/DataType/Mat4.hpp"
#include "Source/DataType/BBox.hpp"

struct SceneMgr
{
    SceneMgr();
	~SceneMgr();
	SceneMgr(SceneMgr const &) = delete;

    // Enums

    enum DriverChannleType
    {
        TRANSLATION,
        SCALE,
        ROTATION,
    };

    enum DriverInterpolation
    {
        STEP,
        LINEAR,
        SLERP,
    };

    enum ProjectionType {
        Perspective,
        Orthographic
    };

    enum MaterialType {
        PBR,
        LAMBERTIAN,
        MIRROR,
        ENVIRONMENT,
    };

    // Basic types

    struct Texture
    {
        std::string src;
        std::string type;
        std::string format;
    };

    struct IndiceStream
    {
        std::string src;
        uint32_t offset;
        VkIndexType format;
    };

    struct AttributeStream
    {
        std::string src;
        uint32_t offset;
        uint32_t stride;
        VkFormat format;
    };

    struct PerspectiveParameters {
        float aspect;
        float vfov;
        float nearZ;
        float farZ;
    };

    struct OrthographicParameters {
        float left;
        float right;
        float bottom;
        float top;
        float nearZ;
        float farZ;
    };

    using AlbedoParam = std::variant<glm::vec3, Texture>;
    using RoughnessParam = std::variant<float, Texture>;
    using MetalnessParam = std::variant<float, Texture>;

    struct PBRMaterial {
        AlbedoParam albedo;
        RoughnessParam roughness;
        MetalnessParam metalness;

        PBRMaterial() :
            albedo(glm::vec3(1.0f)),
            roughness(0.5f),
            metalness(0.5f) {}

        PBRMaterial(glm::vec3 albedo, float roughness, float metalness) :
            albedo(albedo),
            roughness(roughness),
            metalness(metalness) {}
    };

    struct LambertianMaterial {
        AlbedoParam albedo;

        LambertianMaterial() :
            albedo(glm::vec3(1.0f)) {}
        
        LambertianMaterial(glm::vec3 albedo) :
            albedo(albedo) {}
    };

    struct SunLight {
        float angle;
        float strength;
    };

    struct SphereLight {
        float radius;
        float power;
        float limit;
    };

    struct SpotLight {
        float radius;
        float power;
        float fov;
        float blend;
        float limit;
    };

    // Object types

    struct SceneObject 
    {
        std::string name;
        std::vector<std::string> rootName;
    };

    struct NodeObject
    {
        std::string name;

        glm::vec3 translation = glm::vec3(0.f, 0.f, 0.f);
        glm::vec3 scale = glm::vec3(1.f, 1.f, 1.f);
        glm::quat rotation = glm::quat(0.f, 0.f, 0.f, 1.f);

        std::vector<std::string> childName;

        std::string refCameraName;
        std::string refMeshName;
        std::string refEnvironmentName;
        std::string refLightName;
    };

    struct MeshObject
    {
        std::string name;
        VkPrimitiveTopology topology;
        uint32_t count;
        IndiceStream indices;
        AttributeStream attrPosition;
        AttributeStream attrNormal;
        AttributeStream attrTangent;
        AttributeStream attrTexcoord;

        std::string refMaterialName;   

        BBox bbox;
    };

    struct CameraObject
    {
        std::string name;
        ProjectionType projectionType;
        std::variant<PerspectiveParameters, OrthographicParameters> projectionParameters;
    };

    struct DriverObject
    {
        std::string name;
        std::string refObjectName; // target object
        DriverChannleType channel;
        uint32_t channelDim;
        std::vector<float> times;
        std::vector<float> values;
        DriverInterpolation interpolation = DriverInterpolation::LINEAR;

    };

    struct MaterialObject {
        std::string name;
        std::optional<Texture> normalmap; // std::nullopt
        std::optional<Texture> displacementmap; // std::nullopt
        MaterialType type;
        std::variant<std::monostate, PBRMaterial, LambertianMaterial> material;// std::monostate
    };

    struct EnvironmentObject
    {
        std::string name;
        Texture radiance;
    };

    struct LightObject
    {
        std::string name;
        glm::vec3 tint;
        std::variant<SunLight, SphereLight, SpotLight> light;
        uint32_t shadow;
    };

    // object map
    SceneObject* sceneObject;
    std::unordered_map<std::string, NodeObject*> nodeObjectMap;
    std::unordered_map<std::string, MeshObject*> meshObjectMap;
    std::unordered_map<std::string, CameraObject*> cameraObjectMap;
    std::unordered_map<std::string, DriverObject*> driverObjectMap;
    std::unordered_map<std::string, MaterialObject*> materialObjectMap;
    std::unordered_map<std::string, EnvironmentObject*> environmentObjectMap;
    std::unordered_map<std::string, LightObject*> lightObjectMap;

    // object - application buffer map
    std::unordered_map<std::string, uint32_t> meshVerticesIndexMap;
    std::unordered_map<std::string, glm::mat4> nodeMatrixMap;

    // status variables
    std::unordered_map<std::string, CameraObject*>::iterator currentSceneCameraItr; // [WARNING] the cameraObjectMap should not change after the initialization
    uint32_t sceneCameraCount;


    // methods

    void clean_all();

    void update_nodes_from_animation_drivers(float targetTime);
    inline glm::vec3 extract_vec3(const std::vector<float>& values, size_t idx);
    inline glm::quat extract_quat(const std::vector<float>& values, size_t idx);
    inline glm::vec3 linear_interpolation_vec3(const glm::vec3 &prev, const glm::vec3 &next, float w);
    inline glm::quat slerp_interpolation_quat(const glm::quat &prev, const glm::quat &next, float w);

    static glm::mat4 calculate_model_matrix(glm::vec3 translation, glm::quat rotation, glm::vec3 scale);

    static void print_glm_mat4(const glm::mat4& matrix);

    void print_single_node_object(NodeObject* nodeObject);
    void print_single_mesh_object(MeshObject* meshObject);
    void print_single_camera_object(CameraObject* cameraObject);
    void print_single_driver_object(DriverObject* driverObject);
    void print_single_material_object(MaterialObject* materialObject);
    void print_single_environment_object(EnvironmentObject* environmentObject);
    void print_single_light_object(LightObject* lightObject);

    void print_node_object_map();
    void print_mesh_object_map();
    void print_camera_object_map();
    void print_driver_object_map();
    void print_material_object_map();
    void print_environment_object_map();
    void print_light_object_map();
};