// =============================================================================
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2019 projectchrono.org
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be found
// in the LICENSE file at the top level of the distribution and at
// http://projectchrono.org/license-chrono.txt.
//
// =============================================================================
// Authors: Nic Olsen, Conlain Kelly, Ruochun Zhang
// =============================================================================
// Set of simple tests for validating low-level behavior of a Chrono::GPU
// system.
// =============================================================================

#include <cmath>
#include <iostream>
#include <string>

#include "chrono/core/ChGlobal.h"
#include "chrono/utils/ChUtilsSamplers.h"
#include "chrono_gpu/ChGpuData.h"
#include "chrono_gpu/physics/ChSystemGpu.h"
#include "chrono_gpu/utils/ChGpuJsonParser.h"
#include "chrono_thirdparty/filesystem/path.h"

using namespace chrono;
using namespace chrono::gpu;

std::string output_dir = "../test_results";
std::string delim = "--------------------------------";

// Default values
constexpr float sphere_radius = 1.f;
constexpr float sphere_density = 2.50f;
constexpr float grav_Z = -980.f;
constexpr float normalStiffness_S2S = 1e8;
constexpr float normalStiffness_S2W = 1e8;
constexpr float normalStiffness_S2M = 1e8;
constexpr float normalDampS2S = 10000;
constexpr float normalDampS2W = 10000;
constexpr float normalDampS2M = 10000;

constexpr float tangentStiffness_S2S = 3e7;
constexpr float tangentStiffness_S2W = 3e7;
constexpr float tangentStiffness_S2M = 3e7;
constexpr float tangentDampS2S = 500;
constexpr float tangentDampS2W = 500;
constexpr float tangentDampS2M = 500;

constexpr float static_friction_coeff = 0.5f;

constexpr float cohes = 0;

constexpr float timestep = 2e-5;

constexpr unsigned int psi_T = 16;
constexpr unsigned int psi_L = 16;

float box_X = 400.f;
float box_Y = 100.f;
float box_Z = 50.f;
float timeEnd = 5.f;

double fill_top;
double step_mass = 1;
double step_height = -1;

constexpr int fps = 100;
constexpr float frame_step = 1.f / fps;

CHGPU_OUTPUT_MODE write_mode = CHGPU_OUTPUT_MODE::CSV;

// assume we run for at least one frame
float curr_time = 0;
int currframe = 0;

// Bowling ball starts on incline to accelerate
enum TEST_TYPE { ROTF = 0, PYRAMID = 1, ROTF_MESH = 2, PYRAMID_MESH = 3, MESH_STEP = 4, MESH_FORCE = 5 };

void ShowUsage(std::string name) {
    std::cout << "usage: " + name + " <TEST_TYPE: 0:ROTF 1:PYRAMID 2:ROTF_MESH 3:PYRAMID_MESH 4:MESH_STEP 5:MESH_FORCE>"
              << std::endl;
}

// Set common set of parameters for all tests
void setCommonParameters(ChSystemGpu& gpu_sys) {

    gpu_sys.SetPsiFactors(psi_T, psi_L);
    gpu_sys.SetKn_SPH2SPH(normalStiffness_S2S);
    gpu_sys.SetKn_SPH2WALL(normalStiffness_S2W);
    gpu_sys.SetGn_SPH2SPH(normalDampS2S);
    gpu_sys.SetGn_SPH2WALL(normalDampS2W);

    gpu_sys.SetKt_SPH2SPH(tangentStiffness_S2S);
    gpu_sys.SetKt_SPH2WALL(tangentStiffness_S2W);
    gpu_sys.SetGt_SPH2SPH(tangentDampS2S);
    gpu_sys.SetGt_SPH2WALL(tangentDampS2W);

    gpu_sys.SetCohesionRatio(cohes);
    gpu_sys.SetAdhesionRatio_SPH2WALL(cohes);
    gpu_sys.SetGravitationalAcceleration(ChVector<>(0, 0, grav_Z));
    gpu_sys.SetOutputMode(write_mode);
    gpu_sys.SetStaticFrictionCoeff_SPH2SPH(static_friction_coeff);
    gpu_sys.SetStaticFrictionCoeff_SPH2WALL(static_friction_coeff);

    gpu_sys.SetRollingCoeff_SPH2SPH(static_friction_coeff / 2.);
    gpu_sys.SetRollingCoeff_SPH2WALL(static_friction_coeff / 2.);

    gpu_sys.SetTimeIntegrator(CHGPU_TIME_INTEGRATOR::CENTERED_DIFFERENCE);
    gpu_sys.SetFixedStepSize(timestep);
    filesystem::create_directory(filesystem::path(output_dir));
    gpu_sys.SetBDFixed(true);
}

void setCommonMeshParameters(ChSystemGpuMesh& gpu_sys) {

    gpu_sys.SetKn_SPH2MESH(normalStiffness_S2M);
    gpu_sys.SetGn_SPH2MESH(normalDampS2M);
    gpu_sys.SetKt_SPH2MESH(tangentStiffness_S2M);
    gpu_sys.SetGt_SPH2MESH(tangentDampS2M);
}

void writeGranFile(ChSystemGpu& gpu_sys) {
    printf("rendering frame %u\n", currframe);
    char filename[100];
    sprintf(filename, "%s/step%06d", output_dir.c_str(), ++currframe);
    gpu_sys.WriteFile(std::string(filename));
}

void advanceGranSim(ChSystemGpu& gpu_sys) {
    gpu_sys.AdvanceSimulation(frame_step);
    curr_time += frame_step;
}

void run_ROTF() {
    ChSystemGpuMesh gpu_sys(sphere_radius, sphere_density, make_float3(box_X, box_Y, box_Z));
    setCommonParameters(gpu_sys);

    float ramp_angle = CH_C_PI / 4;
    // ramp normal is 45 degrees about y
    float nx = std::cos(ramp_angle);
    float nz = std::sin(ramp_angle);

    ChVector<> plane_normal(nx, 0.f, nz);
    printf("Plane normal: (%f, %f, %f)\n", plane_normal.x(), plane_normal.y(), plane_normal.z());
    // place so that plane intersects wall near z = 0
    ChVector<> plane_pos(-box_X / 2.f, 0.f, 0.);

    std::vector<ChVector<float>> points;
    // start at far-x wall, halfway up
    ChVector<float> sphere_pos(-box_X / 2.f + 2.f * sphere_radius, 0, 2 * sphere_radius);
    points.push_back(sphere_pos);
    gpu_sys.SetParticlePositions(points);

    printf("Plane pos: (%f, %f, %f)\n", plane_pos.x(), plane_pos.y(), plane_pos.z());

    size_t slope_plane_id = gpu_sys.CreateBCPlane(plane_pos, plane_normal, true);
    // add bottom plane to capture bottom forces
    ChVector<> bot_plane_pos(0, 0, -box_Z / 2 + 2 * sphere_radius);
    ChVector<float> bot_plane_normal(0, 0, 1);
    size_t bottom_plane_id = gpu_sys.CreateBCPlane(bot_plane_pos, bot_plane_normal, true);
    // Finalize settings and Initialize for runtime

    gpu_sys.SetFrictionMode(CHGPU_FRICTION_MODE::MULTI_STEP);
    gpu_sys.SetRollingMode(CHGPU_ROLLING_MODE::NO_RESISTANCE);
    gpu_sys.Initialize();

    ChVector<float> reaction_forces;

    // total distance traveled parallel to slope
    float total_dist = (1 / std::cos(ramp_angle)) * box_Z / 2;
    float estimated_time_to_bot = std::sqrt(2 * total_dist / std::abs(grav_Z * std::cos(ramp_angle)));
    printf("total dist is %f, estimated time is %f\n", total_dist, estimated_time_to_bot);

    // Run settling experiments
    while (curr_time < timeEnd) {
        bool success = gpu_sys.GetBCReactionForces(slope_plane_id, reaction_forces);
        if (!success) {
            printf("ERROR! Get contact forces for plane failed\n");
        } else {
            printf("curr time is %f, slope plane force is (%f, %f, %f) Newtons\n", curr_time, reaction_forces.x(),
                   reaction_forces.x(), reaction_forces.z());
        }

        success = gpu_sys.GetBCReactionForces(bottom_plane_id, reaction_forces);
        if (!success) {
            printf("ERROR! Get contact forces for plane failed\n");
        } else {
            printf("curr time is %f, bottom plane force is (%f, %f, %f) Newtons\n", curr_time, reaction_forces.x(),
                   reaction_forces.y(), reaction_forces.z());
        }
        writeGranFile(gpu_sys);
        advanceGranSim(gpu_sys);
    }
}

void run_ROTF_MESH() {
    // overwrite olds system
    ChSystemGpuMesh gpu_sys(sphere_radius, sphere_density, make_float3(box_X, box_Y, box_Z));
    setCommonParameters(gpu_sys);

    setCommonMeshParameters(gpu_sys);

    // place so that plane intersects wall near z = 0
    ChVector<> plane_pos(-box_X / 2.f, 0.f, 0.);

    std::vector<ChVector<float>> points;
    // start at far-x wall, halfway up
    ChVector<float> sphere_pos(-box_X / 2.f + 2.f * sphere_radius, 0, 2 * sphere_radius);
    points.push_back(sphere_pos);
    gpu_sys.SetParticlePositions(points);

    printf("Plane pos: (%f, %f, %f)\n", plane_pos.x(), plane_pos.y(), plane_pos.z());

    // add bottom plane to capture bottom forces
    ChVector<> bot_plane_pos(0, 0, -box_Z / 2 + 2 * sphere_radius);
    ChVector<float> bot_plane_normal(0, 0, 1);

    std::vector<string> mesh_filenames;
    std::vector<ChMatrix33<float>> mesh_rotscales;
    std::vector<float3> mesh_translations;
    std::vector<float> mesh_masses;

    ChMatrix33<float> mesh_scaling = ChMatrix33<float>(ChVector<float>(100, 100, 100));

    // make two plane meshes, one for ramp and one for bottom
    mesh_filenames.push_back(GetDataFile("meshes/testsuite/square_plane_fine.obj"));
    mesh_rotscales.push_back(mesh_scaling);
    mesh_translations.push_back(make_float3(0, 0, 0));
    mesh_masses.push_back(10.f);

    mesh_filenames.push_back(GetDataFile("meshes/testsuite/square_plane_fine.obj"));
    mesh_rotscales.push_back(mesh_scaling);
    mesh_translations.push_back(make_float3(0, 0, 0));
    mesh_masses.push_back(10.f);

    gpu_sys.LoadMeshes(mesh_filenames, mesh_rotscales, mesh_translations, mesh_masses);

    // Finalize settings and Initialize for runtime
    gpu_sys.SetFrictionMode(CHGPU_FRICTION_MODE::MULTI_STEP);
    gpu_sys.SetRollingMode(CHGPU_ROLLING_MODE::NO_RESISTANCE);
    gpu_sys.Initialize();

    unsigned int nSoupFamilies = gpu_sys.GetNumMeshes();
    printf("%u soup families\n", nSoupFamilies);

    double* meshSoupLocOri = new double[7 * nSoupFamilies];

    // bottom plane faces upwards
    auto bot_quat = Q_from_AngY(0);

    // this is a quaternion
    auto rot_quat = Q_from_AngY(CH_C_PI / 4);


    // Run settling experiments
    while (curr_time < timeEnd) {
        gpu_sys.ApplyMeshMotion(0, bot_plane_pos, bot_quat, ChVector<>(0,0,0), ChVector<>(0,0,0));
        gpu_sys.ApplyMeshMotion(1, plane_pos, rot_quat, ChVector<>(0,0,0), ChVector<>(0,0,0));
        writeGranFile(gpu_sys);
        advanceGranSim(gpu_sys);
    }
}

void run_PYRAMID() {
    ChSystemGpuMesh gpu_sys(sphere_radius, sphere_density, make_float3(box_X, box_Y, box_Z));

    setCommonParameters(gpu_sys);

    timeEnd = 1;
    // slightly inflated diameter to ensure no penetration
    float diam_delta = 2.01;
    // add plane just below origin
    ChVector<> bot_plane_pos(0, 0, -1.02 * sphere_radius);
    ChVector<> bot_plane_normal(0, 0, 1);
    size_t bottom_plane_id = gpu_sys.CreateBCPlane(bot_plane_pos, bot_plane_normal, true);

    gpu_sys.SetFrictionMode(CHGPU_FRICTION_MODE::MULTI_STEP);
    gpu_sys.SetRollingMode(CHGPU_ROLLING_MODE::NO_RESISTANCE);
    ChVector<> reaction_forces;

    // just above origin
    ChVector<> base_sphere_1(0, 0, 0);
    // down the x a little
    ChVector<> base_sphere_2(diam_delta * sphere_radius, 0, 0);
    // top of the triangle
    ChVector<> base_sphere_3(diam_delta * sphere_radius * std::cos(CH_C_PI / 3),
                             diam_delta * sphere_radius * std::sin(CH_C_PI / 3), 0);
    // top of pyramid in middle (average x, y)
    ChVector<> top_sphere((base_sphere_1.x() + base_sphere_2.x() + base_sphere_3.x()) / 3.,
                          (base_sphere_1.y() + base_sphere_2.y() + base_sphere_3.y()) / 3.,
                          2.0 * sphere_radius * std::sin(CH_C_PI / 3));

    std::vector<ChVector<float>> points;

    points.push_back(base_sphere_1);
    points.push_back(base_sphere_2);
    points.push_back(base_sphere_3);
    points.push_back(top_sphere);
    gpu_sys.SetParticlePositions(points);

    gpu_sys.Initialize();

    while (curr_time < timeEnd) {
        writeGranFile(gpu_sys);
        advanceGranSim(gpu_sys);
    }
}

void run_PYRAMID_MESH() {
    ChSystemGpuMesh gpu_sys(sphere_radius, sphere_density, make_float3(box_X, box_Y, box_Z));
    setCommonParameters(gpu_sys);

    setCommonMeshParameters(gpu_sys);

    timeEnd = 1;
    // slightly inflated diameter to ensure no penetration
    float diam_delta = 2.01;
    // add plane just below origin
    ChVector<> bot_plane_pos(0, 0, -1.02 * sphere_radius);
    ChVector<> bot_plane_normal(0, 0, 1);

    std::vector<string> mesh_filenames;
    std::vector<ChMatrix33<float>> mesh_rotscales;
    std::vector<float3> mesh_translations;
    std::vector<float> mesh_masses;

    ChMatrix33<float> mesh_scaling = ChMatrix33<float>(ChVector<float>(1, 1, 1));
    // make two plane meshes, one for ramp and one for bottom
    mesh_filenames.push_back(GetDataFile("meshes/testsuite/tiny_triangle.obj"));
    mesh_rotscales.push_back(mesh_scaling);
    mesh_translations.push_back(make_float3(0, 0, 0));
    mesh_masses.push_back(10.f);
    gpu_sys.LoadMeshes(mesh_filenames, mesh_rotscales, mesh_translations, mesh_masses);

    gpu_sys.SetFrictionMode(CHGPU_FRICTION_MODE::MULTI_STEP);
    gpu_sys.SetRollingMode(CHGPU_ROLLING_MODE::NO_RESISTANCE);
    ChVector<> reaction_forces;

    // just above origin
    ChVector<> base_sphere_1(0, 0, 0);
    // down the x a little
    ChVector<> base_sphere_2(diam_delta * sphere_radius, 0, 0);
    // top of the triangle
    ChVector<> base_sphere_3(diam_delta * sphere_radius * std::cos(CH_C_PI / 3),
                             diam_delta * sphere_radius * std::sin(CH_C_PI / 3), 0);
    // top of pyramid in middle (average x, y)
    ChVector<> top_sphere((base_sphere_1.x() + base_sphere_2.x() + base_sphere_3.x()) / 3.,
                          (base_sphere_1.y() + base_sphere_2.y() + base_sphere_3.y()) / 3.,
                          2.0 * sphere_radius * std::sin(CH_C_PI / 3));

    std::vector<ChVector<float>> points;

    points.push_back(base_sphere_1);
    points.push_back(base_sphere_2);
    points.push_back(base_sphere_3);
    points.push_back(top_sphere);
    gpu_sys.SetParticlePositions(points);

    gpu_sys.Initialize();

    unsigned int nSoupFamilies = gpu_sys.GetNumMeshes();
    printf("%u soup families\n", nSoupFamilies);

    double* meshSoupLocOri = new double[7 * nSoupFamilies];

    // bottom plane faces upwards
    auto quat = Q_from_AngY(0);

    while (curr_time < timeEnd) {
        gpu_sys.ApplyMeshMotion(0,bot_plane_pos,quat,ChVector<>(0,0,0),ChVector<>(0,0,0));
        writeGranFile(gpu_sys);
        char filename[100];

        sprintf(filename, "%s/step%06d_meshes", output_dir.c_str(), currframe);

        gpu_sys.WriteMeshes(std::string(filename));

        advanceGranSim(gpu_sys);
    }
}

void run_MESH_STEP() {
    ChSystemGpuMesh gpu_sys(sphere_radius, sphere_density, make_float3(box_X, box_Y, box_Z));
    setCommonParameters(gpu_sys);
    setCommonMeshParameters(gpu_sys);

    std::vector<string> mesh_filenames;
    std::string mesh_filename(GetDataFile("meshes/testsuite/step.obj"));
    mesh_filenames.push_back(mesh_filename);

    std::vector<ChMatrix33<float>> mesh_rotscales;
    std::vector<float3> mesh_translations;

    ChMatrix33<float> scaling = ChMatrix33<float>(ChVector<float>(box_X / 2, box_Y / 2, step_height));
    mesh_rotscales.push_back(scaling);
    mesh_translations.push_back(make_float3(0, 0, 0));

    std::vector<float> mesh_masses;
    mesh_masses.push_back(step_mass);

    gpu_sys.LoadMeshes(mesh_filenames, mesh_rotscales, mesh_translations, mesh_masses);

    // Fill domain with particles
    std::vector<ChVector<float>> body_points;
    double epsilon = 0.2 * sphere_radius;
    double spacing = 2 * sphere_radius + epsilon;

    utils::PDSampler<float> sampler(spacing);
    double fill_bottom = -box_Z / 2 + step_height + 2 * spacing;
    fill_top = box_Z / 2 - sphere_radius - epsilon;
    ChVector<> hdims(box_X / 2 - sphere_radius - epsilon, box_Y / 2 - sphere_radius - epsilon, 0);
    for (double z = fill_bottom; z < fill_top; z += spacing) {
        ChVector<> center(0, 0, z);
        auto points = sampler.SampleBox(center, hdims);
        body_points.insert(body_points.end(), points.begin(), points.end());
    }

    std::cout << "Created " << body_points.size() << " spheres" << std::endl;

    gpu_sys.SetParticlePositions(body_points);

    unsigned int nSoupFamilies = gpu_sys.GetNumMeshes();
    std::cout << nSoupFamilies << " soup families" << std::endl;

    ChVector<> meshSoupLoc(0,0,-box_Z / 2 + 2 * sphere_radius);
    auto quat = Q_from_AngY(0);    

    gpu_sys.Initialize();

    for (float t = 0; t < timeEnd; t += frame_step) {
        gpu_sys.ApplyMeshMotion(0,meshSoupLoc,quat,ChVector<>(0,0,0),ChVector<>(0,0,0));
        std::cout << "Rendering frame " << currframe << std::endl;
        char filename[100];
        sprintf(filename, "%s/step%06u", output_dir.c_str(), currframe);
        writeGranFile(gpu_sys);
        gpu_sys.WriteMeshes(std::string(filename));
        advanceGranSim(gpu_sys);
    }
}

void run_MESH_FORCE() {
    // TODO Adapt sizing
    std::cout << "MESH_FORCE not implemented" << std::endl;
    return;

    ChSystemGpuMesh gpu_sys(sphere_radius, sphere_density, make_float3(box_X, box_Y, box_Z));
    setCommonParameters(gpu_sys);
    setCommonMeshParameters(gpu_sys);

    utils::HCPSampler<float> sampler(2.1 * sphere_radius);
    auto pos = sampler.SampleBox(ChVector<>(0, 0, 26), ChVector<>(38, 38, 10));

    unsigned int n_spheres = pos.size();
    std::cout << "Created " << n_spheres << " spheres" << std::endl;
    double sphere_mass = sphere_density * 4.0 * CH_C_PI * sphere_radius * sphere_radius * sphere_radius / 3.0;

    double total_mass = sphere_mass * n_spheres;
    double sphere_weight = sphere_mass * std::abs(grav_Z);
    double total_weight = total_mass * std::abs(grav_Z);

    gpu_sys.SetParticlePositions(pos);

    // Mesh values
    std::vector<string> mesh_filenames;
    std::string mesh_filename(GetDataFile("meshes/testsuite/square_box.obj"));
    mesh_filenames.push_back(mesh_filename);

    std::vector<ChMatrix33<float>> mesh_rotscales;
    std::vector<float3> mesh_translations;
    ChMatrix33<float> scaling = ChMatrix33<float>(ChVector<float>(40, 40, 40));
    mesh_rotscales.push_back(scaling);
    mesh_translations.push_back(make_float3(0, 0, 0));

    std::vector<float> mesh_masses;
    float mass = 1;
    mesh_masses.push_back(mass);

    gpu_sys.LoadMeshes(mesh_filenames, mesh_rotscales, mesh_translations, mesh_masses);

    unsigned int nSoupFamilies = gpu_sys.GetNumMeshes();
    std::cout << nSoupFamilies << " soup families" << std::endl;

    // Triangle remains at the origin
    ChVector<> meshLoc(0,0,0);
    auto quat = Q_from_AngY(0);

    gpu_sys.Initialize();

    // Run a loop that is typical of co-simulation. For instance, the wheeled is moved a bit, which moves the
    // particles. Conversely, the particles impress a force and torque upon the mesh soup
    for (float t = 0; t < timeEnd; t += frame_step) {
        gpu_sys.ApplyMeshMotion(0,meshLoc,quat,ChVector<>(0,0,0),ChVector<>(0,0,0));
        std::cout << "Rendering frame " << currframe << std::endl;
        char filename[100];
        sprintf(filename, "%s/step%06u", output_dir.c_str(), currframe);
        writeGranFile(gpu_sys);
        gpu_sys.WriteMeshes(std::string(filename));
        // gpu_sys.checkSDCounts(std::string(filename) + "SU", true, false);
        ChVector<> force;
        ChVector<> torque;
        gpu_sys.CollectMeshContactForces(0,force,torque);
        std::cout << "force_z: " << force.z() << "; total weight: " << total_weight << "; sphere weight "
                  << sphere_weight << std::endl;
        std::cout << "torque: " << torque.x() << ", " << torque.y() << ", " << torque.z() << std::endl;

        advanceGranSim(gpu_sys);
    }

}

int main(int argc, char* argv[]) {
    TEST_TYPE curr_test = ROTF;
    if (argc != 2) {
        ShowUsage(argv[0]);
        return 1;
    }

    curr_test = static_cast<TEST_TYPE>(std::atoi(argv[1]));

    std::cout << "frame step is " << frame_step << std::endl;

    switch (curr_test) {
        case ROTF: {
            run_ROTF();
            break;
        }
        case ROTF_MESH: {
            run_ROTF_MESH();
            break;
        }
        case PYRAMID: {
            run_PYRAMID();
            break;
        }
        case PYRAMID_MESH: {
            run_PYRAMID_MESH();
            break;
        }
        case MESH_STEP: {
            run_MESH_STEP();
            break;
        }
        case MESH_FORCE: {
            run_MESH_FORCE();
            break;
        }
        default: {
            std::cout << "Invalid test" << std::endl;
            return 1;
        }
    }
    return 0;
}
