#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_EXTERNAL_IMAGE
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NOEXCEPTION
#include "tiny_gltf.h"

#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include "tiny_obj_loader.h"

#include "meshoptimizer.h"

#include "EntityLoader.h"
#include "Components.h"
#include "Meshlet.h"
#include "Graphics/GfxDevice.h"
#include "Graphics/GfxLinearDynamicAllocator.h"
#include "Logging/Logger.h"
#include "Math/BoundingVolumeHelpers.h"
#include "Math/ComputeTangentFrame.h"
#include "Core/CoreTypes.h"
#include "Utilities/StringUtil.h"
#include "Utilities/FilesUtil.h"
#include "Utilities/HashMap.h"
#include "Utilities/Heightmap.h"


using namespace DirectX;


namespace adria
{

	std::vector<entt::entity> EntityLoader::LoadGrid(GridParameters const& params)
	{
		if (params.heightmap)
		{
			ADRIA_ASSERT(params.heightmap->Depth() == params.tile_count_z + 1);
			ADRIA_ASSERT(params.heightmap->Width() == params.tile_count_x + 1);
		}

		std::vector<entt::entity> chunks;
		std::vector<TexturedNormalVertex> vertices{};
		for (uint64 j = 0; j <= params.tile_count_z; j++)
		{
			for (uint64 i = 0; i <= params.tile_count_x; i++)
			{
				TexturedNormalVertex vertex{};

				float height = params.heightmap ? params.heightmap->HeightAt(i, j) : 0.0f;

				vertex.position = XMFLOAT3(i * params.tile_size_x, height, j * params.tile_size_z);
				vertex.uv = XMFLOAT2(i * 1.0f * params.texture_scale_x / (params.tile_count_x - 1), j * 1.0f * params.texture_scale_z / (params.tile_count_z - 1));
				vertex.normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
				vertices.push_back(vertex);
			}
		}

		if (!params.split_to_chunks)
		{
			std::vector<uint32> indices{};
			uint32 i1 = 0;
			uint32 i2 = 1;
			uint32 i3 = static_cast<uint32>(i1 + params.tile_count_x + 1);
			uint32 i4 = static_cast<uint32>(i2 + params.tile_count_x + 1);
			for (uint64 i = 0; i < params.tile_count_x * params.tile_count_z; ++i)
			{
				indices.push_back(i1);
				indices.push_back(i3);
				indices.push_back(i2);


				indices.push_back(i2);
				indices.push_back(i3);
				indices.push_back(i4);


				++i1;
				++i2;
				++i3;
				++i4;

				if (i1 % (params.tile_count_x + 1) == params.tile_count_x)
				{
					++i1;
					++i2;
					++i3;
					++i4;
				}
			}

			ComputeNormals(params.normal_type, vertices, indices);

			entt::entity grid = reg.create();

			GfxBufferDesc vb_desc{
			.size = vertices.size() * sizeof(TexturedNormalVertex),
			.bind_flags = GfxBindFlag::None,
			.stride = sizeof(TexturedNormalVertex)
			};

			GfxBufferDesc ib_desc{
				.size = indices.size() * sizeof(uint32),
				.bind_flags = GfxBindFlag::None,
				.stride = sizeof(uint32),
				.format = GfxFormat::R32_UINT
			};

			Mesh submesh{};
			submesh.indices_count = (uint32)indices.size();
			submesh.vertex_buffer = std::make_shared<GfxBuffer>(gfx, vb_desc, vertices.data());
			submesh.index_buffer = std::make_shared<GfxBuffer>(gfx, ib_desc, indices.data());

			reg.emplace<Mesh>(grid, submesh);
			reg.emplace<Transform>(grid);

			BoundingBox bounding_box = AABBFromRange(vertices.begin(), vertices.end());
			AABB aabb{};
			aabb.bounding_box = bounding_box;
			aabb.light_visible = true;
			aabb.camera_visible = true;
			reg.emplace<AABB>(grid, aabb);
			chunks.push_back(grid);
		}
		else
		{
			std::vector<uint32> indices{};
			for (size_t j = 0; j < params.tile_count_z; j += params.chunk_count_z)
			{
				for (size_t i = 0; i < params.tile_count_x; i += params.chunk_count_x)
				{
					entt::entity chunk = reg.create();

					uint32 const indices_count = static_cast<uint32>(params.chunk_count_z * params.chunk_count_x * 3 * 2);
					uint32 const indices_offset = static_cast<uint32>(indices.size());


					std::vector<TexturedNormalVertex> chunk_vertices_aabb{};
					for (size_t k = j; k < j + params.chunk_count_z; ++k)
					{
						for (size_t m = i; m < i + params.chunk_count_x; ++m)
						{

							uint32 i1 = static_cast<uint32>(k * (params.tile_count_x + 1) + m);
							uint32 i2 = static_cast<uint32>(i1 + 1);
							uint32 i3 = static_cast<uint32>((k + 1) * (params.tile_count_x + 1) + m);
							uint32 i4 = static_cast<uint32>(i3 + 1);

							indices.push_back(i1);
							indices.push_back(i3);
							indices.push_back(i2);

							indices.push_back(i2);
							indices.push_back(i3);
							indices.push_back(i4);


							chunk_vertices_aabb.push_back(vertices[i1]);
							chunk_vertices_aabb.push_back(vertices[i2]);
							chunk_vertices_aabb.push_back(vertices[i3]);
							chunk_vertices_aabb.push_back(vertices[i4]);
						}
					}

					Mesh submesh{};
					submesh.indices_count = indices_count;
					submesh.start_index_location = indices_offset;

					reg.emplace<Mesh>(chunk, submesh);
					reg.emplace<Transform>(chunk);

					BoundingBox bounding_box = AABBFromRange(chunk_vertices_aabb.begin(), chunk_vertices_aabb.end());
					AABB aabb{};
					aabb.bounding_box = bounding_box;
					aabb.light_visible = true;
					aabb.camera_visible = true;
					reg.emplace<AABB>(chunk, aabb);
					chunks.push_back(chunk);
				}
			}
			ComputeNormals(params.normal_type, vertices, indices);

			GfxBufferDesc vb_desc{
			.size = vertices.size() * sizeof(TexturedNormalVertex),
			.bind_flags = GfxBindFlag::None,
			.stride = sizeof(TexturedNormalVertex)
			};

			GfxBufferDesc ib_desc{
				.size = indices.size() * sizeof(uint32),
				.bind_flags = GfxBindFlag::None,
				.stride = sizeof(uint32),
				.format = GfxFormat::R32_UINT
			};

			std::shared_ptr<GfxBuffer> vb = std::make_shared<GfxBuffer>(gfx, vb_desc, vertices.data());
			std::shared_ptr<GfxBuffer> ib = std::make_shared<GfxBuffer>(gfx, ib_desc, indices.data());

			for (entt::entity chunk : chunks)
			{
				auto& submesh = reg.get<Mesh>(chunk);
				submesh.vertex_buffer = vb;
				submesh.index_buffer = ib;
			}
		}
		return chunks;
	}

	std::vector<entt::entity> EntityLoader::LoadObjMesh(std::string const& model_path)
	{
		tinyobj::ObjReaderConfig reader_config{};
		tinyobj::ObjReader reader;
		std::string model_name = GetFilename(model_path);
		if (!reader.ParseFromFile(model_path, reader_config))
		{
			if (!reader.Error().empty())
			{
				ADRIA_LOG(ERROR, reader.Error().c_str());
			}
			return {};
		}
		if (!reader.Warning().empty())
		{
			ADRIA_LOG(WARNING, reader.Warning().c_str());
		}

		tinyobj::attrib_t const& attrib = reader.GetAttrib();
		std::vector<tinyobj::shape_t> const& shapes = reader.GetShapes();

		// Loop over shapes
		std::vector<TexturedNormalVertex> vertices{};
		std::vector<uint32> indices{};
		std::vector<entt::entity> entities{};

		for (size_t s = 0; s < shapes.size(); s++)
		{
			entt::entity e = reg.create();
			entities.push_back(e);

			Mesh mesh_component{};
			mesh_component.start_index_location = static_cast<uint32>(indices.size());
			mesh_component.base_vertex_location = static_cast<uint32>(vertices.size());

			// Loop over faces(polygon)
			size_t index_offset = 0;
			for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
			{
				size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);

				// Loop over vertices in the face.
				for (size_t v = 0; v < fv; v++)
				{
					// access to vertex
					tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
					indices.push_back((uint32)(index_offset + v));

					TexturedNormalVertex vertex{};
					tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
					tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
					tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

					vertex.position.x = vx;
					vertex.position.y = vy;
					vertex.position.z = vz;

					// Check if `normal_index` is zero or positive. negative = no normal data
					if (idx.normal_index >= 0)
					{
						tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
						tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
						tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

						vertex.normal.x = nx;
						vertex.normal.y = ny;
						vertex.normal.z = nz;
					}

					// Check if `texcoord_index` is zero or positive. negative = no texcoord data
					if (idx.texcoord_index >= 0)
					{
						tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
						tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

						vertex.uv.x = tx;
						vertex.uv.y = ty;
					}

					vertices.push_back(vertex);
				}
				index_offset += fv;

				// per-face material
				//shapes[s].mesh.material_ids[f];
			}
			mesh_component.indices_count = static_cast<uint32>(index_offset);
		}

		GfxBufferDesc vb_desc{
			.size = vertices.size() * sizeof(CompleteVertex),
			.bind_flags = GfxBindFlag::None,
			.stride = sizeof(CompleteVertex)
		};

		GfxBufferDesc ib_desc{
			.size = indices.size() * sizeof(uint32),
			.bind_flags = GfxBindFlag::None,
			.stride = sizeof(uint32),
			.format = GfxFormat::R32_UINT
		};

		std::shared_ptr<GfxBuffer> vb = std::make_shared<GfxBuffer>(gfx, vb_desc, vertices.data());
		std::shared_ptr<GfxBuffer> ib = std::make_shared<GfxBuffer>(gfx, ib_desc, indices.data());

		for (entt::entity e : entities)
		{
			auto& mesh = reg.get<Mesh>(e);
			mesh.vertex_buffer = vb;
			mesh.index_buffer = ib;
			reg.emplace<Tag>(e, model_name + " mesh" + std::to_string(entt::to_integral(e)));
		}

		ADRIA_LOG(INFO, "OBJ Mesh %s successfully loaded!", model_path.c_str());
		return entities;
	}

	EntityLoader::EntityLoader(entt::registry& reg, GfxDevice* gfx)
        : reg(reg), gfx(gfx)
    {
		g_GeometryBufferCache.Initialize(gfx);
    }

	EntityLoader::~EntityLoader()
	{
		g_GeometryBufferCache.Destroy();
	}

	entt::entity EntityLoader::LoadSkybox(SkyboxParameters const& params)
    {
        entt::entity skybox = reg.create();

        Skybox sky{};
        sky.active = true;
		sky.used_in_rt = params.used_in_rt;

        if (params.cubemap.has_value()) sky.cubemap_texture = g_TextureManager.LoadCubemap(params.cubemap.value());
        else sky.cubemap_texture = g_TextureManager.LoadCubemap(params.cubemap_textures);

        reg.emplace<Skybox>(skybox, sky);
        reg.emplace<Tag>(skybox, "Skybox");
        return skybox;

    }

	entt::entity EntityLoader::LoadLight(LightParameters const& params)
    {
        entt::entity light = reg.create();

        if (params.light_data.type == LightType::Directional)
            const_cast<LightParameters&>(params).light_data.position = XMVectorScale(-params.light_data.direction, 1e3);

        reg.emplace<Light>(light, params.light_data);
        if (params.mesh_type == LightMesh::Quad)
        {
            uint32 const size = params.mesh_size;
            std::vector<TexturedVertex> const vertices =
            {
                { {-0.5f * size, -0.5f * size, 0.0f}, {0.0f, 0.0f}},
                { { 0.5f * size, -0.5f * size, 0.0f}, {1.0f, 0.0f}},
                { { 0.5f * size,  0.5f * size, 0.0f}, {1.0f, 1.0f}},
                { {-0.5f * size,  0.5f * size, 0.0f}, {0.0f, 1.0f}}
            };

            std::vector<uint16> const indices =
            {
                    0, 2, 1, 2, 0, 3
            };

			GfxBufferDesc vb_desc{
			.size = vertices.size() * sizeof(TexturedVertex),
			.bind_flags = GfxBindFlag::None,
			.stride = sizeof(TexturedVertex)
			};

			GfxBufferDesc ib_desc{
				.size = indices.size() * sizeof(uint16),
				.bind_flags = GfxBindFlag::None,
				.stride = sizeof(uint16),
				.format = GfxFormat::R16_UINT };

            Mesh submesh{};
            submesh.vertex_buffer = std::make_shared<GfxBuffer>(gfx, vb_desc, vertices.data());
			submesh.index_buffer = std::make_shared<GfxBuffer>(gfx, ib_desc, indices.data());
            submesh.indices_count = static_cast<uint32>(indices.size());

            reg.emplace<Mesh>(light, submesh);

            Material material{};
			XMFLOAT3 base_color;
			XMStoreFloat3(&base_color, params.light_data.color);
			material.base_color[0] = base_color.x;
			material.base_color[1] = base_color.y;
			material.base_color[2] = base_color.z;

            if (params.light_texture.has_value())
                material.albedo_texture = g_TextureManager.LoadTexture(params.light_texture.value()); //
            else if (params.light_data.type == LightType::Directional)
                material.albedo_texture = g_TextureManager.LoadTexture(L"Resources/Textures/sun.png");

            reg.emplace<Material>(light, material);
			auto translation_matrix = XMMatrixTranslationFromVector(params.light_data.position);
            reg.emplace<Transform>(light, translation_matrix);
            if(params.light_data.type != LightType::Directional) reg.emplace<Forward>(light, true);
        }
        else if (params.mesh_type == LightMesh::Sphere)
        {
            //load sphere mesh and mesh component
           //Mesh sphere_mesh{};
           //
           //
           //Material material{};
           //XMStoreFloat3(&material.diffuse, params.light_data.color);
           //
           //if (params.light_texture.has_value())
           //    material.diffuse_texture = texture_manager.LoadTexture(params.light_texture.value()); //
           //else if (params.light_data.type == LightType::eDirectional)
           //    material.diffuse_texture = texture_manager.LoadTexture(L"Resources/Textures/sun.png");
           //
           //if (params.light_data.type == LightType::eDirectional)
           //    material.shader = StandardShader::eSun;
           //else if (material.diffuse_texture == INVALID_TEXTURE_HANDLE)
           //    material.shader = StandardShader::eSolid;
           //else material.shader = StandardShader::eTexture;
           //
           //reg.emplace<Material>(light, material);
        }

        switch (params.light_data.type)
        {
        case LightType::Directional:
            reg.emplace<Tag>(light, "Directional Light");
            break;
        case LightType::Spot:
            reg.emplace<Tag>(light, "Spot Light");
            break;
        case LightType::Point:
            reg.emplace<Tag>(light, "Point Light");
            break;
        }
        return light;
    }

	std::vector<entt::entity> EntityLoader::LoadOcean(OceanParameters const& params)
	{
		std::vector<entt::entity> ocean_chunks = EntityLoader::LoadGrid(params.ocean_grid);

		Material ocean_material{};
		static float default_ocean_color[] = { 0.0123f, 0.3613f, 0.6867f };
		memcpy(ocean_material.base_color, default_ocean_color, 3 * sizeof(float));
		Ocean ocean_component{};

		for (auto ocean_chunk : ocean_chunks)
		{
			reg.emplace<Material>(ocean_chunk, ocean_material);
			reg.emplace<Ocean>(ocean_chunk, ocean_component);
			reg.emplace<Tag>(ocean_chunk, "Ocean Chunk" + std::to_string(entt::to_integral(ocean_chunk)));
		}

		return ocean_chunks;
	}

	entt::entity EntityLoader::LoadDecal(DecalParameters const& params)
	{
		Decal decal{};
		g_TextureManager.EnableMipMaps(false);
		if (!params.albedo_texture_path.empty()) decal.albedo_decal_texture = g_TextureManager.LoadTexture(ToWideString(params.albedo_texture_path));
		if (!params.normal_texture_path.empty()) decal.normal_decal_texture = g_TextureManager.LoadTexture(ToWideString(params.normal_texture_path));
		g_TextureManager.EnableMipMaps(true);

		XMVECTOR P = XMLoadFloat4(&params.position);
		XMVECTOR N = XMLoadFloat4(&params.normal);

		XMVECTOR ProjectorDirection = XMVectorNegate(N);
		XMMATRIX RotationMatrix = XMMatrixRotationAxis(ProjectorDirection, params.rotation);
		XMMATRIX ModelMatrix = XMMatrixScaling(params.size, params.size, params.size) * RotationMatrix * XMMatrixTranslationFromVector(P);

		decal.decal_model_matrix = ModelMatrix;
		decal.modify_gbuffer_normals = params.modify_gbuffer_normals;

		XMFLOAT3 abs_normal;
		XMStoreFloat3(&abs_normal, XMVectorAbs(N));
		if (abs_normal.x >= abs_normal.y && abs_normal.x >= abs_normal.z)
		{
			decal.decal_type = DecalType::Project_YZ;
		}
		else if (abs_normal.y >= abs_normal.x && abs_normal.y >= abs_normal.z)
		{
			decal.decal_type = DecalType::Project_XZ;
		}
		else
		{
			decal.decal_type = DecalType::Project_XY;
		}

		entt::entity decal_entity = reg.create();
		reg.emplace<Decal>(decal_entity, decal);
		if (params.name.empty()) reg.emplace<Tag>(decal_entity, "decal");
		else reg.emplace<Tag>(decal_entity, params.name);

		return decal_entity;
	}

	std::vector<entt::entity> EntityLoader::ImportModel_GLTF(ModelParameters const& params)
	{
		tinygltf::TinyGLTF loader;
		tinygltf::Model model;
		std::string err;
		std::string warn;
		bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, params.model_path);
		std::string model_name = GetFilename(params.model_path);
		if (!warn.empty())
		{
			ADRIA_LOG(WARNING, warn.c_str());
		}
		if (!err.empty())
		{
			ADRIA_LOG(ERROR, err.c_str());
			return {};
		}
		if (!ret)
		{
			ADRIA_LOG(ERROR, "Failed to load model %s", model_name.c_str());
			return {};
		}
		std::vector<entt::entity> entities{};
		HashMap<std::string, std::vector<entt::entity>> mesh_name_to_entities_map;

		//process the materials
		std::vector<Material> materials;
		materials.reserve(model.materials.size());
		for (auto const& gltf_material : model.materials)
		{
			Material& material = materials.emplace_back();
			material.alpha_cutoff = (float)gltf_material.alphaCutoff;
			material.double_sided = gltf_material.doubleSided;
			if (gltf_material.alphaMode == "OPAQUE")
			{
				material.alpha_mode = MaterialAlphaMode::Opaque;
			}
			else if (gltf_material.alphaMode == "BLEND")
			{
				material.alpha_mode = MaterialAlphaMode::Blend;
			}
			else if (gltf_material.alphaMode == "MASK")
			{
				material.alpha_mode = MaterialAlphaMode::Mask;
			}
			tinygltf::PbrMetallicRoughness pbr_metallic_roughness = gltf_material.pbrMetallicRoughness;
			if (pbr_metallic_roughness.baseColorTexture.index >= 0)
			{
				tinygltf::Texture const& base_texture = model.textures[pbr_metallic_roughness.baseColorTexture.index];
				tinygltf::Image const& base_image = model.images[base_texture.source];
				std::string texbase = params.textures_path + base_image.uri;
				material.albedo_texture = g_TextureManager.LoadTexture(ToWideString(texbase));
				material.base_color[0] = (float)pbr_metallic_roughness.baseColorFactor[0];
				material.base_color[1] = (float)pbr_metallic_roughness.baseColorFactor[1];
				material.base_color[2] = (float)pbr_metallic_roughness.baseColorFactor[2];
			}
			if (pbr_metallic_roughness.metallicRoughnessTexture.index >= 0)
			{
				tinygltf::Texture const& metallic_roughness_texture = model.textures[pbr_metallic_roughness.metallicRoughnessTexture.index];
				tinygltf::Image const& metallic_roughness_image = model.images[metallic_roughness_texture.source];
				std::string texmetallicroughness = params.textures_path + metallic_roughness_image.uri;
				material.metallic_roughness_texture = g_TextureManager.LoadTexture(ToWideString(texmetallicroughness));
				material.metallic_factor = (float)pbr_metallic_roughness.metallicFactor;
				material.roughness_factor = (float)pbr_metallic_roughness.roughnessFactor;
			}
			if (gltf_material.normalTexture.index >= 0)
			{
				tinygltf::Texture const& normal_texture = model.textures[gltf_material.normalTexture.index];
				tinygltf::Image const& normal_image = model.images[normal_texture.source];
				std::string texnormal = params.textures_path + normal_image.uri;
				material.normal_texture = g_TextureManager.LoadTexture(ToWideString(texnormal));
			}
			if (gltf_material.emissiveTexture.index >= 0)
			{
				tinygltf::Texture const& emissive_texture = model.textures[gltf_material.emissiveTexture.index];
				tinygltf::Image const& emissive_image = model.images[emissive_texture.source];
				std::string texemissive = params.textures_path + emissive_image.uri;
				material.emissive_texture = g_TextureManager.LoadTexture(ToWideString(texemissive));
				material.emissive_factor = (float)gltf_material.emissiveFactor[0];
			}
		}

		struct MeshData
		{
			std::vector<XMFLOAT3> positions_stream;
			std::vector<XMFLOAT3> normals_stream;
			std::vector<XMFLOAT4> tangents_stream;
			std::vector<XMFLOAT3> bitangents_stream;
			std::vector<XMFLOAT2> uvs_stream;
			std::vector<uint32>   indices;
		};
		MeshData mesh_data{};

		for (auto const& gltf_mesh : model.meshes)
		{
			std::vector<entt::entity>& mesh_entities = mesh_name_to_entities_map[gltf_mesh.name];
			for (auto const& gltf_primitive : gltf_mesh.primitives)
			{
				ADRIA_ASSERT(gltf_primitive.indices >= 0);
				tinygltf::Accessor const& index_accessor = model.accessors[gltf_primitive.indices];
				tinygltf::BufferView const& buffer_view = model.bufferViews[index_accessor.bufferView];
				tinygltf::Buffer const& buffer = model.buffers[buffer_view.buffer];

				entt::entity e = reg.create();
				entities.push_back(e);
				mesh_entities.push_back(e);

				Material material = gltf_primitive.material < 0 ? Material{} : materials[gltf_primitive.material];
				reg.emplace<Material>(e, material);
				reg.emplace<Deferred>(e);

				Mesh submesh{};
				submesh.indices_count = static_cast<uint32>(index_accessor.count);
				submesh.start_index_location = static_cast<uint32>(mesh_data.indices.size());
				submesh.base_vertex_location = static_cast<uint32>(mesh_data.positions_stream.size());

				mesh_data.indices.reserve(mesh_data.indices.size() + index_accessor.count);
				auto AddIndices =[&]<typename T>()
				{
					T* data = (T*)(buffer.data.data() + index_accessor.byteOffset + buffer_view.byteOffset);
					for (size_t i = 0; i < index_accessor.count; i += 3)
					{
						//change later to meshData.indices
						mesh_data.indices.push_back(data[i + 0]);
						mesh_data.indices.push_back(data[i + 1]);
						mesh_data.indices.push_back(data[i + 2]);
					}
				};
				int stride = index_accessor.ByteStride(buffer_view);
				switch (stride)
				{
				case 1:
					AddIndices.template operator()<uint8>();
					break;
				case 2:
					AddIndices.template operator()<uint16>();
					break;
				case 4:
					AddIndices.template operator()<uint32>();
					break;
				default:
					ADRIA_ASSERT(false);
				}
				switch (gltf_primitive.mode)
				{
				case TINYGLTF_MODE_POINTS:
					submesh.topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
					break;
				case TINYGLTF_MODE_LINE:
					submesh.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
					break;
				case TINYGLTF_MODE_LINE_STRIP:
					submesh.topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
					break;
				case TINYGLTF_MODE_TRIANGLES:
					submesh.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
					break;
				case TINYGLTF_MODE_TRIANGLE_STRIP:
					submesh.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
					break;
				default:
					ADRIA_ASSERT(false);
				}

				for (auto const& attr : gltf_primitive.attributes)
				{
					std::string const& attr_name = attr.first;
					int attr_data = attr.second;

					const tinygltf::Accessor& accessor = model.accessors[attr_data];
					const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
					const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

					int stride = accessor.ByteStride(buffer_view);
					size_t count = accessor.count;
					const unsigned char* data = buffer.data.data() + accessor.byteOffset + buffer_view.byteOffset;

					auto ReadAttributeData = [&]<typename T>(std::vector<T>& stream, const char* stream_name)
					{
						if (!attr_name.compare(stream_name))
						{
							stream.reserve(count);
							for (size_t i = 0; i < count; ++i)
							{
								stream.push_back(*(T*)((size_t)data + i * stride));
							}
						}
					};
					ReadAttributeData(mesh_data.positions_stream, "POSITION");
					ReadAttributeData(mesh_data.normals_stream, "NORMAL");
					ReadAttributeData(mesh_data.tangents_stream, "TANGENT");
					ReadAttributeData(mesh_data.uvs_stream, "TEXCOORD_0");
				}
				submesh.vertex_count = (uint32)(mesh_data.positions_stream.size() - submesh.base_vertex_location);
				reg.emplace<Mesh>(e, submesh);
			}
		}

		std::vector<CompleteVertex> vertices;
		std::vector<uint32> const& indices = mesh_data.indices;

		bool has_tangents = !mesh_data.tangents_stream.empty();
		size_t vertex_count = mesh_data.positions_stream.size();
		if (mesh_data.normals_stream.size() != vertex_count) mesh_data.normals_stream.resize(vertex_count);
		if (mesh_data.uvs_stream.size() != vertex_count) mesh_data.uvs_stream.resize(vertex_count);
		if (mesh_data.tangents_stream.size() != vertex_count) mesh_data.tangents_stream.resize(vertex_count);
		if (mesh_data.bitangents_stream.size() != vertex_count) mesh_data.bitangents_stream.resize(vertex_count);
		if (has_tangents)
		{
			for (size_t i = 0; i < vertex_count; ++i)
			{
				float tangent_handness = mesh_data.tangents_stream[i].w;
				XMVECTOR bitangent = XMVectorScale(XMVector3Cross(XMLoadFloat3(&mesh_data.normals_stream[i]), XMLoadFloat4(&mesh_data.tangents_stream[i])), tangent_handness);
				XMStoreFloat3(&mesh_data.bitangents_stream[i], XMVector3Normalize(bitangent));
			}
		}

		vertices.reserve(vertex_count);
		for (size_t i = 0; i < vertex_count; ++i)
		{
			vertices.emplace_back(
				mesh_data.positions_stream[i],
				XMFLOAT2(mesh_data.uvs_stream[i].x, 1.0f - mesh_data.uvs_stream[i].y),
				mesh_data.normals_stream[i],
				XMFLOAT3(mesh_data.tangents_stream[i].x, mesh_data.tangents_stream[i].y, mesh_data.tangents_stream[i].z),
				mesh_data.bitangents_stream[i]
			);
		}

		std::function<void(int, XMMATRIX)> LoadNode;
		LoadNode = [&](int node_index, XMMATRIX parent_transform)
		{
			if (node_index < 0) return;
			auto& node = model.nodes[node_index];
			struct Transforms
			{
				XMFLOAT4 rotation_local = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
				XMFLOAT3 scale_local = XMFLOAT3(1.0f, 1.0f, 1.0f);
				XMFLOAT3 translation_local = XMFLOAT3(0.0f, 0.0f, 0.0f);
				XMFLOAT4X4 world;
				bool update = true;
				void Update()
				{
					if (update)
					{
						XMVECTOR S_local = XMLoadFloat3(&scale_local);
						XMVECTOR R_local = XMLoadFloat4(&rotation_local);
						XMVECTOR T_local = XMLoadFloat3(&translation_local);
						XMMATRIX WORLD = XMMatrixScalingFromVector(S_local) *
							XMMatrixRotationQuaternion(R_local) *
							XMMatrixTranslationFromVector(T_local);
						XMStoreFloat4x4(&world, WORLD);
					}
				}
			} transforms;

			if (!node.scale.empty())
			{
				transforms.scale_local = XMFLOAT3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]);
			}
			if (!node.rotation.empty())
			{
				transforms.rotation_local = XMFLOAT4((float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2], (float)node.rotation[3]);
			}
			if (!node.translation.empty())
			{
				transforms.translation_local = XMFLOAT3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]);
			}
			if (!node.matrix.empty())
			{
				transforms.world._11 = (float)node.matrix[0];
				transforms.world._12 = (float)node.matrix[1];
				transforms.world._13 = (float)node.matrix[2];
				transforms.world._14 = (float)node.matrix[3];
				transforms.world._21 = (float)node.matrix[4];
				transforms.world._22 = (float)node.matrix[5];
				transforms.world._23 = (float)node.matrix[6];
				transforms.world._24 = (float)node.matrix[7];
				transforms.world._31 = (float)node.matrix[8];
				transforms.world._32 = (float)node.matrix[9];
				transforms.world._33 = (float)node.matrix[10];
				transforms.world._34 = (float)node.matrix[11];
				transforms.world._41 = (float)node.matrix[12];
				transforms.world._42 = (float)node.matrix[13];
				transforms.world._43 = (float)node.matrix[14];
				transforms.world._44 = (float)node.matrix[15];
				transforms.update = false;
			}
			transforms.Update();

			if (node.mesh >= 0)
			{
				auto const& mesh = model.meshes[node.mesh];
				std::vector<entt::entity> const& mesh_entities = mesh_name_to_entities_map[mesh.name];
				for (entt::entity e : mesh_entities)
				{
					Mesh const& mesh = reg.get<Mesh>(e);
					XMMATRIX model = XMLoadFloat4x4(&transforms.world) * parent_transform;
					BoundingBox bounding_box = AABBFromRange(vertices.begin() + mesh.base_vertex_location, vertices.begin() + mesh.base_vertex_location + mesh.vertex_count);
					bounding_box.Transform(bounding_box, model);

					AABB aabb{};
					aabb.bounding_box = bounding_box;
					aabb.light_visible = true;
					aabb.camera_visible = true;
					reg.emplace<AABB>(e, aabb);
					reg.emplace<Transform>(e, model);
				}
			}

			for (int child : node.children) LoadNode(child, XMLoadFloat4x4(&transforms.world) * parent_transform);
		};
		tinygltf::Scene const& scene = model.scenes[std::max(0, model.defaultScene)];
		for (size_t i = 0; i < scene.nodes.size(); ++i)
		{
			LoadNode(scene.nodes[i], params.model_matrix);
		}

		GfxBufferDesc vb_desc = VertexBufferDesc(vertices.size(), sizeof(CompleteVertex), params.used_in_raytracing);
		GfxBufferDesc ib_desc = IndexBufferDesc(indices.size(), false, params.used_in_raytracing);

		std::shared_ptr<GfxBuffer> vb = std::make_shared<GfxBuffer>(gfx, vb_desc, vertices.data());
		std::shared_ptr<GfxBuffer> ib = std::make_shared<GfxBuffer>(gfx, ib_desc, indices.data());

		size_t rt_vertices_size = RayTracing::rt_vertices.size();
		size_t rt_indices_size = RayTracing::rt_indices.size();
		if (params.used_in_raytracing)
		{
			RayTracing::rt_vertices.insert(std::end(RayTracing::rt_vertices), std::begin(vertices), std::end(vertices));
			RayTracing::rt_indices.insert(std::end(RayTracing::rt_indices), std::begin(indices), std::end(indices));
		}

		entt::entity root = reg.create();
		reg.emplace<Transform>(root);
		reg.emplace<Tag>(root, model_name);
		Relationship relationship{};
		relationship.children_count = entities.size();
		relationship.first = entities.front();
		reg.emplace<Relationship>(root, relationship);
		for (size_t i = 0; i < entities.size(); ++i)
		{
			entt::entity e = entities[i];
			auto& submesh = reg.get<Mesh>(e);
			submesh.vertex_buffer = vb;
			submesh.index_buffer = ib;
			reg.emplace<Tag>(e, model_name + " mesh" + std::to_string(entt::to_integral(e)));
			if (params.used_in_raytracing)
			{
				RayTracing rt_component{
					.vertex_offset = (uint32)rt_vertices_size + submesh.base_vertex_location,
					.index_offset = (uint32)rt_indices_size + submesh.start_index_location
				};
				reg.emplace<RayTracing>(e, rt_component);
			}
			Relationship relationship{};
			relationship.parent = root;
			relationship.children_count = 0;
			if (i != entities.size() - 1) relationship.next = entities[i + 1];
			if (i != 0) relationship.prev = entities[i - 1];
			reg.emplace<Relationship>(e, relationship);
		}
		ADRIA_LOG(INFO, "GLTF Mesh %s successfully loaded!", params.model_path.c_str());
		return entities;
	}

	entt::entity EntityLoader::ImportModel_GLTF_Optimized(ModelParameters const& params)
	{
		tinygltf::TinyGLTF loader;
		tinygltf::Model model;
		std::string err;
		std::string warn;
		bool ret = loader.LoadASCIIFromFile(&model, &err, &warn, params.model_path);
		std::string model_name = GetFilename(params.model_path);
		if (!warn.empty())
		{
			ADRIA_LOG(WARNING, warn.c_str());
		}
		if (!err.empty())
		{
			ADRIA_LOG(ERROR, err.c_str());
			return {};
		}
		if (!ret)
		{
			ADRIA_LOG(ERROR, "Failed to load model %s", model_name.c_str());
			return {};
		}
		entt::entity mesh = reg.create();
		NewMesh new_mesh{};

		//process the materials
		new_mesh.materials.reserve(model.materials.size());
		for (auto const& gltf_material : model.materials)
		{
			Material& material = new_mesh.materials.emplace_back();
			material.alpha_cutoff = (float)gltf_material.alphaCutoff;
			material.double_sided = gltf_material.doubleSided;
			if (gltf_material.alphaMode == "OPAQUE")
			{
				material.alpha_mode = MaterialAlphaMode::Opaque;
			}
			else if (gltf_material.alphaMode == "BLEND")
			{
				material.alpha_mode = MaterialAlphaMode::Blend;
			}
			else if (gltf_material.alphaMode == "MASK")
			{
				material.alpha_mode = MaterialAlphaMode::Mask;
			}
			tinygltf::PbrMetallicRoughness pbr_metallic_roughness = gltf_material.pbrMetallicRoughness;
			if (pbr_metallic_roughness.baseColorTexture.index >= 0)
			{
				tinygltf::Texture const& base_texture = model.textures[pbr_metallic_roughness.baseColorTexture.index];
				tinygltf::Image const& base_image = model.images[base_texture.source];
				std::string texbase = params.textures_path + base_image.uri;
				material.albedo_texture = g_TextureManager.LoadTexture(ToWideString(texbase));
				material.base_color[0] = (float)pbr_metallic_roughness.baseColorFactor[0];
				material.base_color[1] = (float)pbr_metallic_roughness.baseColorFactor[1];
				material.base_color[2] = (float)pbr_metallic_roughness.baseColorFactor[2];
			}
			if (pbr_metallic_roughness.metallicRoughnessTexture.index >= 0)
			{
				tinygltf::Texture const& metallic_roughness_texture = model.textures[pbr_metallic_roughness.metallicRoughnessTexture.index];
				tinygltf::Image const& metallic_roughness_image = model.images[metallic_roughness_texture.source];
				std::string texmetallicroughness = params.textures_path + metallic_roughness_image.uri;
				material.metallic_roughness_texture = g_TextureManager.LoadTexture(ToWideString(texmetallicroughness));
				material.metallic_factor = (float)pbr_metallic_roughness.metallicFactor;
				material.roughness_factor = (float)pbr_metallic_roughness.roughnessFactor;
			}
			if (gltf_material.normalTexture.index >= 0)
			{
				tinygltf::Texture const& normal_texture = model.textures[gltf_material.normalTexture.index];
				tinygltf::Image const& normal_image = model.images[normal_texture.source];
				std::string texnormal = params.textures_path + normal_image.uri;
				material.normal_texture = g_TextureManager.LoadTexture(ToWideString(texnormal));
			}
			if (gltf_material.emissiveTexture.index >= 0)
			{
				tinygltf::Texture const& emissive_texture = model.textures[gltf_material.emissiveTexture.index];
				tinygltf::Image const& emissive_image = model.images[emissive_texture.source];
				std::string texemissive = params.textures_path + emissive_image.uri;
				material.emissive_texture = g_TextureManager.LoadTexture(ToWideString(texemissive));
				material.emissive_factor = (float)gltf_material.emissiveFactor[0];
			}
		}

		HashMap<int32, std::vector<int32>> mesh_primitives_map; //mesh index -> vector of primitive indices
		int32 primitive_count = 0;

		struct MeshData
		{
			DirectX::BoundingBox bounding_box;
			int32 material_index = -1;
			D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

			std::vector<XMFLOAT3> positions_stream;
			std::vector<XMFLOAT3> normals_stream;
			std::vector<XMFLOAT4> tangents_stream;
			std::vector<XMFLOAT3> bitangents_stream;
			std::vector<XMFLOAT2> uvs_stream;
			std::vector<uint32>   indices;

			std::vector<Meshlet>		 meshlets;
			std::vector<uint32>			 meshlet_vertices;
			std::vector<MeshletTriangle> meshlet_triangles;
		};
		std::vector<MeshData> mesh_datas{};
		for (int32 i = 0; i < model.meshes.size(); ++i)
		{
			std::vector<int32>& primitives = mesh_primitives_map[i];
			auto const& gltf_mesh = model.meshes[i];
			for (auto const& gltf_primitive : gltf_mesh.primitives)
			{
				ADRIA_ASSERT(gltf_primitive.indices >= 0);
				tinygltf::Accessor const& index_accessor = model.accessors[gltf_primitive.indices];
				tinygltf::BufferView const& buffer_view = model.bufferViews[index_accessor.bufferView];
				tinygltf::Buffer const& buffer = model.buffers[buffer_view.buffer];

				MeshData& mesh_data = mesh_datas.emplace_back();
				mesh_data.material_index = gltf_primitive.material;

				mesh_data.indices.reserve(index_accessor.count);
				auto AddIndices = [&]<typename T>()
				{
					T* data = (T*)(buffer.data.data() + index_accessor.byteOffset + buffer_view.byteOffset);
					for (size_t i = 0; i < index_accessor.count; i += 3)
					{
						mesh_data.indices.push_back(data[i + 0]);
						mesh_data.indices.push_back(data[i + 1]);
						mesh_data.indices.push_back(data[i + 2]);
					}
				};
				int stride = index_accessor.ByteStride(buffer_view);
				switch (stride)
				{
				case 1:
					AddIndices.template operator() < uint8 > ();
					break;
				case 2:
					AddIndices.template operator() < uint16 > ();
					break;
				case 4:
					AddIndices.template operator() < uint32 > ();
					break;
				default:
					ADRIA_ASSERT(false);
				}
				switch (gltf_primitive.mode)
				{
				case TINYGLTF_MODE_POINTS:
					mesh_data.topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
					break;
				case TINYGLTF_MODE_LINE:
					mesh_data.topology = D3D_PRIMITIVE_TOPOLOGY_LINELIST;
					break;
				case TINYGLTF_MODE_LINE_STRIP:
					mesh_data.topology = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
					break;
				case TINYGLTF_MODE_TRIANGLES:
					mesh_data.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
					break;
				case TINYGLTF_MODE_TRIANGLE_STRIP:
					mesh_data.topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
					break;
				default:
					ADRIA_ASSERT(false);
				}

				for (auto const& attr : gltf_primitive.attributes)
				{
					std::string const& attr_name = attr.first;
					int attr_data = attr.second;

					const tinygltf::Accessor& accessor = model.accessors[attr_data];
					const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
					const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

					int stride = accessor.ByteStride(buffer_view);
					size_t count = accessor.count;
					const unsigned char* data = buffer.data.data() + accessor.byteOffset + buffer_view.byteOffset;

					auto ReadAttributeData = [&]<typename T>(std::vector<T>&stream, const char* stream_name)
					{
						if (!attr_name.compare(stream_name))
						{
							stream.reserve(count);
							for (size_t i = 0; i < count; ++i)
							{
								stream.push_back(*(T*)((size_t)data + i * stride));
							}
						}
					};
					ReadAttributeData(mesh_data.positions_stream, "POSITION");
					ReadAttributeData(mesh_data.normals_stream, "NORMAL");
					ReadAttributeData(mesh_data.tangents_stream, "TANGENT");
					ReadAttributeData(mesh_data.uvs_stream, "TEXCOORD_0");
				}
				primitives.push_back(primitive_count++);
			}
		}

		uint64 total_buffer_size = 0;
		for (auto& mesh_data : mesh_datas)
		{
			std::vector<uint32> const& indices = mesh_data.indices;
			size_t vertex_count = mesh_data.positions_stream.size();

			bool has_tangents = !mesh_data.tangents_stream.empty();
			if (mesh_data.normals_stream.size() != vertex_count) mesh_data.normals_stream.resize(vertex_count);
			if (mesh_data.uvs_stream.size() != vertex_count) mesh_data.uvs_stream.resize(vertex_count);
			if (mesh_data.tangents_stream.size() != vertex_count) mesh_data.tangents_stream.resize(vertex_count);
			if (mesh_data.bitangents_stream.size() != vertex_count) mesh_data.bitangents_stream.resize(vertex_count);
			if (has_tangents)
			{
				for (size_t i = 0; i < vertex_count; ++i)
				{
					float tangent_handness = mesh_data.tangents_stream[i].w;
					XMVECTOR bitangent = XMVectorScale(XMVector3Cross(XMLoadFloat3(&mesh_data.normals_stream[i]), XMLoadFloat4(&mesh_data.tangents_stream[i])), tangent_handness);
					XMStoreFloat3(&mesh_data.bitangents_stream[i], XMVector3Normalize(bitangent));
				}
			}

			meshopt_optimizeVertexCache(mesh_data.indices.data(), mesh_data.indices.data(), mesh_data.indices.size(), vertex_count);
			meshopt_optimizeOverdraw(mesh_data.indices.data(), mesh_data.indices.data(), mesh_data.indices.size(), &mesh_data.positions_stream[0].x, vertex_count, sizeof(XMFLOAT3), 1.05f);
			std::vector<uint32> remap(vertex_count);
			meshopt_optimizeVertexFetchRemap(&remap[0], mesh_data.indices.data(), mesh_data.indices.size(), vertex_count);
			meshopt_remapIndexBuffer(mesh_data.indices.data(), mesh_data.indices.data(), mesh_data.indices.size(), &remap[0]);
			meshopt_remapVertexBuffer(mesh_data.positions_stream.data(), mesh_data.positions_stream.data(), vertex_count, sizeof(XMFLOAT3), &remap[0]);
			meshopt_remapVertexBuffer(mesh_data.normals_stream.data(), mesh_data.normals_stream.data(), mesh_data.normals_stream.size(), sizeof(XMFLOAT3), &remap[0]);
			meshopt_remapVertexBuffer(mesh_data.tangents_stream.data(), mesh_data.tangents_stream.data(), mesh_data.tangents_stream.size(), sizeof(XMFLOAT4), &remap[0]);
			meshopt_remapVertexBuffer(mesh_data.bitangents_stream.data(), mesh_data.bitangents_stream.data(), mesh_data.bitangents_stream.size(), sizeof(XMFLOAT3), &remap[0]);
			meshopt_remapVertexBuffer(mesh_data.uvs_stream.data(), mesh_data.uvs_stream.data(), mesh_data.uvs_stream.size(), sizeof(XMFLOAT2), &remap[0]);

			size_t const max_meshlets = meshopt_buildMeshletsBound(mesh_data.indices.size(), MESHLET_MAX_VERTICES, MESHLET_MAX_TRIANGLES);
			mesh_data.meshlets.resize(max_meshlets);
			mesh_data.meshlet_vertices.resize(max_meshlets * MESHLET_MAX_VERTICES);

			std::vector<unsigned char> meshlet_triangles(max_meshlets * MESHLET_MAX_TRIANGLES * 3);
			std::vector<meshopt_Meshlet> meshlets(max_meshlets);

			size_t meshlet_count = meshopt_buildMeshlets(meshlets.data(), mesh_data.meshlet_vertices.data(), meshlet_triangles.data(),
				mesh_data.indices.data(), mesh_data.indices.size(), &mesh_data.positions_stream[0].x, mesh_data.positions_stream.size(), sizeof(XMFLOAT3),
				MESHLET_MAX_VERTICES, MESHLET_MAX_TRIANGLES, 0);

			meshopt_Meshlet const& last = meshlets[meshlet_count - 1];
			meshlet_triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
			meshlets.resize(meshlet_count);

			mesh_data.meshlets.resize(meshlet_count);
			mesh_data.meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
			mesh_data.meshlet_triangles.resize(meshlet_triangles.size() / 3);

			uint32 triangle_offset = 0;
			for (size_t i = 0; i < meshlet_count; ++i)
			{
				meshopt_Meshlet const& m = meshlets[i];

				meshopt_Bounds meshopt_bounds = meshopt_computeMeshletBounds(&mesh_data.meshlet_vertices[m.vertex_offset], &meshlet_triangles[m.triangle_offset],
					m.triangle_count, reinterpret_cast<float const*>(mesh_data.positions_stream.data()), vertex_count, sizeof(XMFLOAT3));

				Meshlet& meshlet = mesh_data.meshlets[i];
				std::memcpy(meshlet.center, meshopt_bounds.center, sizeof(float) * 3);
				meshlet.radius = meshopt_bounds.radius;
				meshlet.vertex_count = m.vertex_count;
				meshlet.triangle_count = m.triangle_count;
				meshlet.vertex_offset = m.vertex_offset;
				meshlet.triangle_offset = m.triangle_offset;

				unsigned char* src_triangles = meshlet_triangles.data() + meshlet.triangle_offset;
				for (uint32 triangle_idx = 0; triangle_idx < meshlet.triangle_count; ++triangle_idx)
				{
					MeshletTriangle& tri = mesh_data.meshlet_triangles[triangle_idx + triangle_offset];
					tri.V0 = *src_triangles++;
					tri.V1 = *src_triangles++;
					tri.V2 = *src_triangles++;
				}
				triangle_offset += meshlet.triangle_count;
			}
			mesh_data.meshlet_triangles.resize(triangle_offset);

			total_buffer_size += Align(mesh_data.indices.size() * sizeof(uint32), 16);
			total_buffer_size += Align(mesh_data.positions_stream.size() * sizeof(XMFLOAT3), 16);
			total_buffer_size += Align(mesh_data.uvs_stream.size() * sizeof(XMFLOAT2), 16);
			total_buffer_size += Align(mesh_data.normals_stream.size() * sizeof(XMFLOAT3), 16);
			total_buffer_size += Align(mesh_data.tangents_stream.size() * sizeof(XMFLOAT4), 16);
			total_buffer_size += Align(mesh_data.bitangents_stream.size() * sizeof(XMFLOAT3), 16);
			total_buffer_size += Align(mesh_data.meshlets.size() * sizeof(Meshlet), 16);
			total_buffer_size += Align(mesh_data.meshlet_vertices.size() * sizeof(uint32), 16);
			total_buffer_size += Align(mesh_data.meshlet_triangles.size() * sizeof(MeshletTriangle), 16);

			mesh_data.bounding_box = AABBFromPositions(mesh_data.positions_stream);
		}

		GfxBufferDesc desc{};
		desc.size = total_buffer_size;
		desc.bind_flags = GfxBindFlag::ShaderResource;
		desc.misc_flags = GfxBufferMiscFlag::BufferRaw;
		desc.resource_usage = GfxResourceUsage::Default;
		std::shared_ptr<GfxBuffer> geometry_buffer = std::make_shared<GfxBuffer>(gfx, desc);

		GfxDynamicAllocation staging_buffer = gfx->GetDynamicAllocator()->Allocate(total_buffer_size, 16);

		uint32 current_offset = 0;
		auto CopyData = [&staging_buffer, &current_offset]<typename T>(std::vector<T> const& _data)
		{
			uint64 current_copy_size = _data.size() * sizeof(T);
			staging_buffer.Update(_data.data(), current_copy_size, current_offset);
			current_offset += (uint32)Align(current_copy_size, 16);
		};

		new_mesh.submeshes.reserve(mesh_datas.size());
		for (size_t i = 0; i < mesh_datas.size(); ++i)
		{
			auto const& mesh_data = mesh_datas[i];

			SubMesh& submesh = new_mesh.submeshes.emplace_back();

			submesh.indices_offset = current_offset;
			submesh.indices_count = (uint32)mesh_data.indices.size();
			CopyData(mesh_data.indices);

			submesh.positions_offset = current_offset;
			CopyData(mesh_data.positions_stream);

			submesh.uvs_offset = current_offset;
			CopyData(mesh_data.uvs_stream);

			submesh.normals_offset = current_offset;
			CopyData(mesh_data.normals_stream);

			submesh.tangents_offset = current_offset;
			CopyData(mesh_data.tangents_stream);

			submesh.bitangents_offset = current_offset;
			CopyData(mesh_data.bitangents_stream);

			submesh.meshlet_offset = current_offset;
			CopyData(mesh_data.meshlets);

			submesh.meshlet_vertices_offset = current_offset;
			CopyData(mesh_data.meshlet_vertices);

			submesh.meshlet_triangles_offset = current_offset;
			CopyData(mesh_data.meshlet_triangles);

			submesh.meshlet_count = (uint32)mesh_data.meshlets.size();
		}
		new_mesh.geometry_buffer_handle = g_GeometryBufferCache.CreateAndInitializeGeometryBuffer(staging_buffer.buffer, total_buffer_size, staging_buffer.offset);

		std::function<void(int, XMMATRIX)> LoadNode;
		LoadNode = [&](int node_index, XMMATRIX parent_transform)
		{
			if (node_index < 0) return;
			auto& node = model.nodes[node_index];
			struct Transforms
			{
				XMFLOAT4 rotation_local = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
				XMFLOAT3 scale_local = XMFLOAT3(1.0f, 1.0f, 1.0f);
				XMFLOAT3 translation_local = XMFLOAT3(0.0f, 0.0f, 0.0f);
				XMFLOAT4X4 world;
				bool update = true;
				void Update()
				{
					if (update)
					{
						XMVECTOR S_local = XMLoadFloat3(&scale_local);
						XMVECTOR R_local = XMLoadFloat4(&rotation_local);
						XMVECTOR T_local = XMLoadFloat3(&translation_local);
						XMMATRIX WORLD = XMMatrixScalingFromVector(S_local) *
							XMMatrixRotationQuaternion(R_local) *
							XMMatrixTranslationFromVector(T_local);
						XMStoreFloat4x4(&world, WORLD);
					}
				}
			} transforms;

			if (!node.scale.empty())
			{
				transforms.scale_local = XMFLOAT3((float)node.scale[0], (float)node.scale[1], (float)node.scale[2]);
			}
			if (!node.rotation.empty())
			{
				transforms.rotation_local = XMFLOAT4((float)node.rotation[0], (float)node.rotation[1], (float)node.rotation[2], (float)node.rotation[3]);
			}
			if (!node.translation.empty())
			{
				transforms.translation_local = XMFLOAT3((float)node.translation[0], (float)node.translation[1], (float)node.translation[2]);
			}
			if (!node.matrix.empty())
			{
				transforms.world._11 = (float)node.matrix[0];
				transforms.world._12 = (float)node.matrix[1];
				transforms.world._13 = (float)node.matrix[2];
				transforms.world._14 = (float)node.matrix[3];
				transforms.world._21 = (float)node.matrix[4];
				transforms.world._22 = (float)node.matrix[5];
				transforms.world._23 = (float)node.matrix[6];
				transforms.world._24 = (float)node.matrix[7];
				transforms.world._31 = (float)node.matrix[8];
				transforms.world._32 = (float)node.matrix[9];
				transforms.world._33 = (float)node.matrix[10];
				transforms.world._34 = (float)node.matrix[11];
				transforms.world._41 = (float)node.matrix[12];
				transforms.world._42 = (float)node.matrix[13];
				transforms.world._43 = (float)node.matrix[14];
				transforms.world._44 = (float)node.matrix[15];
				transforms.update = false;
			}
			transforms.Update();

			if (node.mesh >= 0)
			{
				XMMATRIX model_matrix = XMLoadFloat4x4(&transforms.world) * parent_transform;
				for (auto primitive : mesh_primitives_map[node.mesh])
				{
					SubMeshInstance& instance = new_mesh.instances.emplace_back();
					instance.submesh_index = primitive;
					instance.world_transform = model_matrix;
					instance.parent = mesh;
				}
			}

			for (int child : node.children) LoadNode(child, XMLoadFloat4x4(&transforms.world) * parent_transform);
		};

		tinygltf::Scene const& scene = model.scenes[std::max(0, model.defaultScene)];
		for (size_t i = 0; i < scene.nodes.size(); ++i)
		{
			LoadNode(scene.nodes[i], params.model_matrix);
		}

		reg.emplace<NewMesh>(mesh, new_mesh);
		reg.emplace<Tag>(mesh, model_name + " mesh");

		ADRIA_LOG(INFO, "GLTF Mesh %s successfully loaded!", params.model_path.c_str());
		return mesh;
	}
}