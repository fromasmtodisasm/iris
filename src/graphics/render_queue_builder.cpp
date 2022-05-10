////////////////////////////////////////////////////////////////////////////////
//         Distributed under the Boost Software License, Version 1.0.         //
//            (See accompanying file LICENSE or copy at                       //
//                 https://www.boost.org/LICENSE_1_0.txt)                     //
////////////////////////////////////////////////////////////////////////////////

#include "graphics/render_queue_builder.h"

#include <deque>
#include <map>
#include <vector>

#include "core/root.h"
#include "graphics/mesh_manager.h"
#include "graphics/render_graph/arithmetic_node.h"
#include "graphics/render_graph/blur_node.h"
#include "graphics/render_graph/component_node.h"
#include "graphics/render_graph/conditional_node.h"
#include "graphics/render_graph/node.h"
#include "graphics/render_graph/post_processing/ambient_occlusion_node.h"
#include "graphics/render_graph/post_processing/anti_aliasing_node.h"
#include "graphics/render_graph/post_processing/colour_adjust_node.h"
#include "graphics/render_graph/render_graph.h"
#include "graphics/render_graph/sky_box_node.h"
#include "graphics/render_graph/texture_node.h"
#include "graphics/render_graph/value_node.h"
#include "graphics/render_target.h"
#include "graphics/renderer.h"
#include "graphics/single_entity.h"
#include "log/log.h"

namespace
{

/**
 * Helper function to create and enqueue all commands for rendering a Scene with
 * a given light type.
 *
 * @param scene
 *   Scene to render.
 *
 * @param light_type
 *   Type of light for the scene.
 *
 * @param cmd
 *   Command object to mutate and enqueue, this is passed in so it can be
 *   "pre-loaded" with the correct state.
 *
 * @param create_material_callback
 *   Callback for creating a Material object.
 *
 * @param render_queue
 *   Queue to add render commands to.
 *
 * @param shadow_maps
 *   Map of directional lights to their associated shadow map render target.
 */
void encode_light_pass_commands(
    const iris::Scene *scene,
    iris::LightType light_type,
    bool has_normal_target,
    bool has_position_target,
    iris::RenderCommand &cmd,
    iris::RenderQueueBuilder::CreateMaterialCallback create_material_callback,
    std::vector<iris::RenderCommand> &render_queue,
    const std::map<iris::DirectionalLight *, iris::RenderTarget *> &shadow_maps)
{
    // create commands for each entity in the scene
    for (const auto &[render_graph, render_entity] : scene->entities())
    {
        auto *material = create_material_callback(
            render_graph,
            render_entity.get(),
            cmd.render_pass()->colour_target,
            light_type,
            has_normal_target && (light_type == iris::LightType::AMBIENT),
            has_position_target && (light_type == iris::LightType::AMBIENT));
        cmd.set_material(material);

        cmd.set_render_entity(render_entity.get());

        // light specific draw commands
        switch (light_type)
        {
            case iris::LightType::AMBIENT:
                cmd.set_type(iris::RenderCommandType::DRAW);
                cmd.set_light(scene->lighting_rig()->ambient_light.get());
                render_queue.push_back(cmd);
                break;
            case iris::LightType::POINT:
                // a draw command for each light
                for (auto &light : scene->lighting_rig()->point_lights)
                {
                    cmd.set_type(iris::RenderCommandType::DRAW);
                    cmd.set_light(light.get());
                    render_queue.push_back(cmd);
                }
                break;
            case iris::LightType::DIRECTIONAL:
                // a draw command for each light
                for (auto &light : scene->lighting_rig()->directional_lights)
                {
                    cmd.set_type(iris::RenderCommandType::DRAW);
                    cmd.set_light(light.get());

                    // set shadow map in render command
                    if (render_entity->receive_shadow())
                    {
                        auto *shadow_map = shadow_maps.count(light.get()) == 0u ? nullptr : shadow_maps.at(light.get());
                        cmd.set_shadow_map(shadow_map);
                    }

                    render_queue.push_back(cmd);
                }
                break;
        }
    }
}
}

namespace iris
{

RenderQueueBuilder::RenderQueueBuilder(
    std::uint32_t width_,
    std::uint32_t height_,
    CreateMaterialCallback create_material_callback_,
    CreateRenderTargetCallback create_render_target_callback_,
    CreateHybriRenderTargetCallback create_hybrid_render_target_callback)
    : width_(width_)
    , height_(height_)
    , create_material_callback_(create_material_callback_)
    , create_render_target_callback_(create_render_target_callback_)
    , create_hybrid_render_target_callback_(create_hybrid_render_target_callback)
    , pass_data_()
{
}

std::vector<RenderCommand> RenderQueueBuilder::build(std::deque<RenderPass> &render_passes)
{
    std::map<DirectionalLight *, RenderTarget *> shadow_maps;
    std::deque<RenderPass> pre_process_passes{};

    auto initial_passes = render_passes;
    render_passes.clear();

    // for each shadow casting light create a render target for the shadow map
    // and enqueue commands so they are rendered
    for (auto &pass : initial_passes)
    {
        for (const auto &light : pass.scene->lighting_rig()->directional_lights)
        {
            if (light->casts_shadows())
            {
                auto *rt = create_render_target_callback_(1024u, 1024u);
                RenderPass shadow_pass = pass;
                shadow_pass.post_processing_description = {};
                shadow_pass.camera = std::addressof(light->shadow_camera());
                shadow_pass.colour_target = rt;
                shadow_pass.normal_target = nullptr;
                shadow_pass.position_target = nullptr;
                shadow_pass.depth_only = true;
                shadow_pass.clear_colour = true;
                shadow_pass.clear_depth = true;

                pre_process_passes.emplace_back(shadow_pass);

                shadow_maps[light.get()] = rt;
            }
        }

        // SSAO is a bit messy to integrate, we first need to create a pass to output all the required information such
        // as screen space normals and positions and then add the pass to combine all this data into our occlusion
        // texture
        // later on we will then wire this into the normal lighting passes, due to how the render is setup it's not easy
        // to just add the occlusion texture into the ambient light pass, so instead we do that whole calculation in the
        // SSAO pass and use that instead of recalculating the ambient pass
        if (const auto ssao = pass.post_processing_description.ambient_occlusion; ssao)
        {
            // add a pass to output the data we need
            RenderPass ao_data_pass = pass;
            ao_data_pass.post_processing_description = {};
            ao_data_pass.colour_target = nullptr;
            ao_data_pass.normal_target = create_render_target_callback_(width_, height_);
            ao_data_pass.position_target = create_render_target_callback_(width_, height_);
            ao_data_pass.depth_only = true;
            ao_data_pass.clear_colour = true;

            pre_process_passes.emplace_back(ao_data_pass);
            auto *prev = std::addressof(pre_process_passes.back());

            const auto prev_camera = *prev->camera;

            // add a pass to calculate ssao (combined with the ambient light pass)
            auto *ao_target = add_pass(pre_process_passes, [prev, ssao](RenderGraph *rg, const RenderTarget *target) {
                rg->set_render_node<AmbientOcclusionNode>(
                    rg->create<TextureNode>(target->colour_texture()),
                    rg->create<TextureNode>(prev->normal_target->colour_texture()),
                    rg->create<TextureNode>(prev->position_target->colour_texture()),
                    *ssao);
            });

            // ensure we render with the perspective camera not the orthographic camera that will be created for the
            // new pass
            pass_data_.back().camera = prev_camera;

            prev = std::addressof(pre_process_passes.back());
            prev->colour_target = pass.colour_target;

            // fudge the colour target of the pre pass and the depth target of the ssao pass into one render target
            pass.colour_target = create_hybrid_render_target_callback_(prev->colour_target, ao_target);
            pass.clear_colour = false;
            pass.clear_depth = false;
        }
    }

    // insert shadow passes into the queue
    initial_passes.insert(std::cbegin(initial_passes), std::cbegin(pre_process_passes), std::cend(pre_process_passes));

    add_post_processing_passes(initial_passes, render_passes);

    std::vector<RenderCommand> render_queue;
    RenderCommand cmd{};

    // convert each pass into a series of commands which will render it
    for (auto &pass : render_passes)
    {
        cmd.set_render_pass(std::addressof(pass));

        cmd.set_type(RenderCommandType::PASS_START);
        render_queue.push_back(cmd);

        const auto has_normal_target = pass.normal_target != nullptr;
        const auto has_position_target = pass.position_target != nullptr;

        // encode ambient light pass unless we have used ssao (in which case this gets done by the ssao pass itself)
        if (!pass.post_processing_description.ambient_occlusion)
        {
            encode_light_pass_commands(
                pass.scene,
                LightType::AMBIENT,
                has_normal_target,
                has_position_target,
                cmd,
                create_material_callback_,
                render_queue,
                shadow_maps);
        }

        if (!pass.depth_only)
        {
            // encode point lights if there are any
            if (!pass.scene->lighting_rig()->point_lights.empty())
            {
                encode_light_pass_commands(
                    pass.scene,
                    LightType::POINT,
                    has_normal_target,
                    has_position_target,
                    cmd,
                    create_material_callback_,
                    render_queue,
                    shadow_maps);
            }

            // encode directional lights if there are any
            if (!pass.scene->lighting_rig()->directional_lights.empty())
            {
                encode_light_pass_commands(
                    pass.scene,
                    LightType::DIRECTIONAL,
                    has_normal_target,
                    has_position_target,
                    cmd,
                    create_material_callback_,
                    render_queue,
                    shadow_maps);
            }

            if (pass.sky_box != nullptr)
            {
                auto *scene = pass.scene;

                auto *sky_box_rg = scene->create_render_graph();
                sky_box_rg->set_render_node<SkyBoxNode>(pass.sky_box);

                auto *sky_box_entity = scene->create_entity<SingleEntity>(
                    sky_box_rg, Root::mesh_manager().cube({}), Transform({}, {}, {0.5f}));

                auto *material = create_material_callback_(
                    sky_box_rg, sky_box_entity, cmd.render_pass()->colour_target, LightType::AMBIENT, false, false);
                cmd.set_material(material);

                cmd.set_render_entity(sky_box_entity);

                cmd.set_type(iris::RenderCommandType::DRAW);
                cmd.set_light(scene->lighting_rig()->ambient_light.get());
                render_queue.push_back(cmd);
            }
        }

        cmd.set_type(RenderCommandType::PASS_END);
        render_queue.push_back(cmd);
    }

    cmd.set_type(RenderCommandType::PRESENT);
    render_queue.push_back(cmd);

    return render_queue;
}

const RenderTarget *RenderQueueBuilder::add_pass(
    std::deque<RenderPass> &render_passes,
    std::function<void(RenderGraph *, const RenderTarget *)> create_render_graph_callback)
{
    // create new pass with
    pass_data_.push_back({.scene = {}, .camera = {CameraType::ORTHOGRAPHIC, width_, height_}});
    auto &[scene, camera] = pass_data_.back();
    const auto *target = create_render_target_callback_(width_, height_);

    auto *rg = scene.create_render_graph();
    create_render_graph_callback(rg, target);
    scene.create_entity<SingleEntity>(
        rg,
        Root::mesh_manager().sprite({}),
        Transform({}, {}, {static_cast<float>(width_), static_cast<float>(height_), 1. - 1}));

    // wire up this pass
    render_passes.back().colour_target = target;
    render_passes.push_back({.scene = &scene, .camera = &camera});

    return target;
}

void RenderQueueBuilder::add_post_processing_passes(
    const std::deque<RenderPass> &passes,
    std::deque<RenderPass> &render_passes)
{
    for (auto &render_pass : passes)
    {
        auto &last_pass = render_passes.emplace_back(render_pass);
        const auto *input_target = last_pass.colour_target;

        const auto description = render_pass.post_processing_description;

        if (const auto bloom = description.bloom; bloom)
        {
            const auto *null_target = add_pass(render_passes, [](RenderGraph *rg, const RenderTarget *target) {
                rg->render_node()->set_colour_input(rg->create<TextureNode>(target->colour_texture()));
            });

            add_pass(render_passes, [&bloom](RenderGraph *rg, const RenderTarget *target) {
                rg->render_node()->set_colour_input(rg->create<ConditionalNode>(
                    rg->create<ArithmeticNode>(
                        rg->create<TextureNode>(target->colour_texture()),
                        rg->create<ValueNode<Colour>>(Colour{0.2126f, 0.7152f, 0.0722f, 0.0f}),
                        ArithmeticOperator::DOT),
                    rg->create<ValueNode<float>>(bloom->threshold),
                    rg->create<TextureNode>(target->colour_texture()),
                    rg->create<ValueNode<Colour>>(Colour{0.0f, 0.0f, 0.0f, 1.0f}),
                    ConditionalOperator::GREATER));
            });

            for (auto i = 0u; i < bloom->iterations; ++i)
            {
                add_pass(render_passes, [](RenderGraph *rg, const RenderTarget *target) {
                    rg->render_node()->set_colour_input(
                        rg->create<BlurNode>(rg->create<TextureNode>(target->colour_texture())));
                });
            }

            add_pass(render_passes, [null_target](RenderGraph *rg, const RenderTarget *target) {
                rg->render_node()->set_colour_input(rg->create<ArithmeticNode>(
                    rg->create<TextureNode>(null_target->colour_texture()),
                    rg->create<TextureNode>(target->colour_texture()),
                    ArithmeticOperator::ADD));
            });
        }

        if (const auto colour_adjust = description.colour_adjust; colour_adjust)
        {
            add_pass(render_passes, [&colour_adjust](RenderGraph *rg, const RenderTarget *target) {
                rg->set_render_node<ColourAdjustNode>(
                    rg->create<TextureNode>(target->colour_texture()), *colour_adjust);
            });
        }

        if (description.anti_aliasing)
        {
            add_pass(render_passes, [](RenderGraph *rg, const RenderTarget *target) {
                rg->set_render_node<AntiAliasingNode>(rg->create<TextureNode>(target->colour_texture()));
            });
        }

        render_passes.back().colour_target = input_target;
    }
}

}
