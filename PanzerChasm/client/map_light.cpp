#include <cstring>

#include <ogl_state_manager.hpp>

#include "../game_resources.hpp"
#include "../map_loader.hpp"
#include "../math_utils.hpp"
#include "map_state.hpp"

#include "map_light.hpp"

namespace PanzerChasm
{

namespace
{

const float g_floor_light_scale= 0.8f / 144.0f;
const float g_walls_light_scale= 1.2f / 144.0f;

const unsigned int g_wall_lightmap_size= 32u;
const Size2 g_walls_lightmap_atlas_size(
	MapData::c_map_size * g_wall_lightmap_size,
	MapData::c_map_size );

const float g_lightmap_clear_color[4u]= { 0.0f, 0.0f, 0.0f, 0.0f };
const GLenum g_gl_state_blend_func[2]= { GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA };

const r_OGLState g_lightmap_clear_state(
	false, false, false, false,
	g_gl_state_blend_func,
	g_lightmap_clear_color );

const GLenum g_light_pass_blend_func[2]= { GL_ONE, GL_ONE };
const r_OGLState g_light_pass_state(
	true, false, false, false,
	g_light_pass_blend_func );

} // namespace

MapLight::MapLight(
	const GameResourcesConstPtr& game_resources,
	const RenderingContext& rendering_context )
	: game_resources_(game_resources)
{
	PC_ASSERT( game_resources_ != nullptr );

	const unsigned int c_hd_lightmap_scale= 4u;

	// Floor
	base_floor_lightmap_=
		r_Framebuffer(
			{ r_Texture::PixelFormat::R8 }, r_Texture::PixelFormat::Unknown,
			c_hd_lightmap_scale * MapData::c_lightmap_size, c_hd_lightmap_scale * MapData::c_lightmap_size );
	final_floor_lightmap_=
		r_Framebuffer(
			{ r_Texture::PixelFormat::R8 }, r_Texture::PixelFormat::Unknown,
			c_hd_lightmap_scale * MapData::c_lightmap_size, c_hd_lightmap_scale * MapData::c_lightmap_size );

	// Walls
	base_walls_lightmap_=
		r_Framebuffer(
			{ r_Texture::PixelFormat::R8 }, r_Texture::PixelFormat::Unknown,
			g_walls_lightmap_atlas_size.Width(), g_walls_lightmap_atlas_size.Height() );
	final_walls_lightmap_=
		r_Framebuffer(
			{ r_Texture::PixelFormat::R8 }, r_Texture::PixelFormat::Unknown,
			g_walls_lightmap_atlas_size.Width(), g_walls_lightmap_atlas_size.Height() );

	// Shadowmap
	shadowmap_=
		r_Framebuffer(
			{ r_Texture::PixelFormat::R8 }, r_Texture::PixelFormat::Unknown,
			c_hd_lightmap_scale * MapData::c_lightmap_size, c_hd_lightmap_scale * MapData::c_lightmap_size );

	// Shaders
	floor_light_pass_shader_.ShaderSource(
		rLoadShader( "light_f.glsl", rendering_context.glsl_version ),
		rLoadShader( "light_v.glsl", rendering_context.glsl_version ) );
	floor_light_pass_shader_.Create();

	floor_ambient_light_pass_shader_.ShaderSource(
		rLoadShader( "ambient_light_f.glsl", rendering_context.glsl_version ),
		rLoadShader( "ambient_light_v.glsl", rendering_context.glsl_version ) );
	floor_ambient_light_pass_shader_.Create();

	walls_light_pass_shader_.ShaderSource(
		rLoadShader( "walls_light_f.glsl", rendering_context.glsl_version ),
		rLoadShader( "walls_light_v.glsl", rendering_context.glsl_version ) );
	walls_light_pass_shader_.SetAttribLocation( "pos", 0 );
	walls_light_pass_shader_.SetAttribLocation( "lightmap_coord", 1 );
	walls_light_pass_shader_.SetAttribLocation( "normal", 2 );
	walls_light_pass_shader_.Create();

	walls_ambient_light_pass_shader_.ShaderSource(
		rLoadShader( "walls_ambient_light_f.glsl", rendering_context.glsl_version ),
		rLoadShader( "walls_light_v.glsl", rendering_context.glsl_version ) );
	walls_ambient_light_pass_shader_.SetAttribLocation( "pos", 0 );
	walls_ambient_light_pass_shader_.SetAttribLocation( "lightmap_coord", 1 );
	walls_ambient_light_pass_shader_.SetAttribLocation( "normal", 2 );
	walls_ambient_light_pass_shader_.Create();

	shadowmap_shader_.ShaderSource(
		rLoadShader( "shadowmap_f.glsl", rendering_context.glsl_version ),
		rLoadShader( "shadowmap_v.glsl", rendering_context.glsl_version ),
		rLoadShader( "shadowmap_g.glsl", rendering_context.glsl_version ) );
	shadowmap_shader_.SetAttribLocation( "pos", 0u );
	shadowmap_shader_.Create();
}

MapLight::~MapLight()
{}

void MapLight::SetMap( const MapDataConstPtr& map_data )
{
	map_data_= map_data;
	PrepareMapWalls( *map_data );

	// Occludders buffer.
	r_PolygonBuffer walls_buffer;
	{
		std::vector<short> vertices;

		for( const MapData::Wall& wall : map_data_->static_walls )
		{
			if( wall.texture_id >= 87u )
				continue;
			if( map_data_->walls_textures[ wall.texture_id ].file_path[0] == '\0' )
				continue;

			vertices.resize( vertices.size() + 4u );
			short* const v= vertices.data() + vertices.size() - 4u;

			v[0]= static_cast<short>( wall.vert_pos[0].x * 256.0f );
			v[1]= static_cast<short>( wall.vert_pos[0].y * 256.0f );
			v[2]= static_cast<short>( wall.vert_pos[1].x * 256.0f );
			v[3]= static_cast<short>( wall.vert_pos[1].y * 256.0f );
		}

		walls_buffer.VertexData( vertices.data(), vertices.size() * sizeof(short), sizeof(short) * 2u );
		walls_buffer.VertexAttribPointer( 0, 2, GL_SHORT, false, 0 );
		walls_buffer.SetPrimitiveType( GL_LINES );
	}

	base_floor_lightmap_.Bind();
	r_OGLStateManager::UpdateState( g_lightmap_clear_state );
	glClear( GL_COLOR_BUFFER_BIT );

	base_walls_lightmap_.Bind();
	r_OGLStateManager::UpdateState( g_lightmap_clear_state );
	glClear( GL_COLOR_BUFFER_BIT );

	for( const MapData::Light& light : map_data_->lights )
	{
		if( light.power <= 1.0f )
			continue;

		// Bind and clear shadowmam.
		shadowmap_.Bind();
		r_OGLStateManager::UpdateState( g_lightmap_clear_state );
		glClear( GL_COLOR_BUFFER_BIT );

		// Draw walls into shadowmap.
		shadowmap_shader_.Bind();

		m_Mat4 shadow_scale_mat, shadow_shift_mat, shadow_view_mat, shadow_vertices_scale_mat;
		shadow_scale_mat.Scale( 2.0f / float(MapData::c_map_size) );
		shadow_shift_mat.Translate( m_Vec3( -1.0f, -1.0f, 0.0f ) );
		shadow_vertices_scale_mat.Scale( 1.0f / 256.0f );

		shadow_view_mat= shadow_scale_mat * shadow_shift_mat;

		shadowmap_shader_.Uniform( "view_matrix", shadow_vertices_scale_mat * shadow_view_mat );
		shadowmap_shader_.Uniform( "light_pos", ( m_Vec3( light.pos, 0.0f ) * shadow_view_mat ).xy() );

		const float shadowmap_texel_size= 2.0f / float( shadowmap_.GetTextures().front().Width() );
		shadowmap_shader_.Uniform( "offset", shadowmap_texel_size * 1.5f );

		// TODO - use scissor test for speed.
		walls_buffer.Draw();

		r_OGLStateManager::UpdateState( g_light_pass_state );

		// Add light to floor lightmap.
		base_floor_lightmap_.Bind();
		floor_light_pass_shader_.Bind();
		DrawFloorLight( light );

		// Add light to walls lightmap.
		base_walls_lightmap_.Bind();
		walls_light_pass_shader_.Bind();
		DrawWallsLight( light );
	}

	r_Framebuffer::BindScreenFramebuffer();

	// Update ambient light texture.
	ambient_lightmap_texture_=
		r_Texture(
			r_Texture::PixelFormat::R8,
			MapData::c_map_size, MapData::c_map_size,
			map_data->ambient_lightmap );
	ambient_lightmap_texture_.SetFiltration( r_Texture::Filtration::Nearest, r_Texture::Filtration::Nearest );
}

void MapLight::Update( const MapState& map_state )
{
	UpdateLightOnDynamicWalls( map_state );

	const auto gen_light_for_rocket=
	[&]( const MapState::Rocket& rocket, MapData::Light& out_light ) -> bool
	{
		if( rocket.rocket_id >= game_resources_->rockets_description.size() )
			return false;
		if( !game_resources_->rockets_description[ rocket.rocket_id ].Light )
			return false;

		out_light.inner_radius= 0.5f;
		out_light.outer_radius= 1.0f;
		out_light.power= 64.0f;
		out_light.max_light_level= 128.0f;
		out_light.pos= rocket.pos.xy();

		return true;
	};

	// Clear shadowmam.
	shadowmap_.Bind();
	r_OGLStateManager::UpdateState( g_lightmap_clear_state );
	glClear( GL_COLOR_BUFFER_BIT );

	// Draw to floor lightmap.
	final_floor_lightmap_.Bind();

	{ // Copy base floor lightmap.
		r_OGLStateManager::UpdateState( g_lightmap_clear_state );
		floor_ambient_light_pass_shader_.Bind();
		base_floor_lightmap_.GetTextures().front().Bind(0);
		floor_ambient_light_pass_shader_.Uniform( "tex", 0 );
		glDrawArrays( GL_TRIANGLES, 0, 6 );
	}

	{ // Mix with ambient light texture.
		r_OGLStateManager::UpdateState( g_light_pass_state );
		floor_ambient_light_pass_shader_.Bind();
		ambient_lightmap_texture_.Bind(0);
		floor_ambient_light_pass_shader_.Uniform( "tex", 0 );

		glBlendEquation( GL_MAX );
		glDrawArrays( GL_TRIANGLES, 0, 6 );
		glBlendEquation( GL_FUNC_ADD );
	}

	{ // Dynamic lights.
		r_OGLStateManager::UpdateState( g_light_pass_state );
		floor_light_pass_shader_.Bind();

		for( const MapState::RocketsContainer::value_type& rocket_value : map_state.GetRockets() )
		{
			MapData::Light light;
			if( gen_light_for_rocket( rocket_value.second, light ) )
				DrawFloorLight( light );
		}
	}

	// Draw to walls lightmap.
	final_walls_lightmap_.Bind();

	{ // Copy base walls lightmap.
		r_OGLStateManager::UpdateState( g_lightmap_clear_state );
		floor_ambient_light_pass_shader_.Bind();
		base_walls_lightmap_.GetTextures().front().Bind(0);
		floor_ambient_light_pass_shader_.Uniform( "tex", 0 );
		glDrawArrays( GL_TRIANGLES, 0, 6 );
	}

	{ // Mix with ambient light texture.
		r_OGLStateManager::UpdateState( g_light_pass_state );
		walls_ambient_light_pass_shader_.Bind();
		ambient_lightmap_texture_.Bind(0);
		walls_ambient_light_pass_shader_.Uniform( "tex", 0 );

		glBlendEquation( GL_MAX );
		walls_vertex_buffer_.Draw();
		glBlendEquation( GL_FUNC_ADD );
	}

	{ // Dynamic lights.
		r_OGLStateManager::UpdateState( g_light_pass_state );
		walls_light_pass_shader_.Bind();

		for( const MapState::RocketsContainer::value_type& rocket_value : map_state.GetRockets() )
		{
			MapData::Light light;
			if( gen_light_for_rocket( rocket_value.second, light ) )
				DrawWallsLight( light );
		}
	}

	r_Framebuffer::BindScreenFramebuffer();
}

void MapLight::GetStaticWallLightmapCoord(
	const unsigned int static_wall_index,
	unsigned char* out_coord_xy ) const
{
	PC_ASSERT( static_wall_index < map_data_->static_walls.size() );
	std::memcpy(
		out_coord_xy,
		walls_vertices_[ static_walls_first_vertex_ + 2u * static_wall_index ].lightmap_coord_xy,
		2u );
}

void MapLight::GetDynamicWallLightmapCoord(
	const unsigned int dynamic_wall_index,
	unsigned char* out_coord_xy ) const
{
	PC_ASSERT( dynamic_wall_index < map_data_->dynamic_walls.size() );
	std::memcpy(
		out_coord_xy,
		walls_vertices_[ dynamic_walls_first_vertex_ + 2u * dynamic_wall_index ].lightmap_coord_xy,
		2u );
}

const r_Texture& MapLight::GetFloorLightmap() const
{
	return final_floor_lightmap_.GetTextures().front();
}

const r_Texture& MapLight::GetWallsLightmap() const
{
	return final_walls_lightmap_.GetTextures().front();
}

void MapLight::PrepareMapWalls( const MapData& map_data )
{
	walls_vertices_.resize( 2u * ( map_data.static_walls.size() + map_data.dynamic_walls.size() ) );
	static_walls_first_vertex_= 0u;
	dynamic_walls_first_vertex_= map_data.static_walls.size() * 2u;

	// Place walls in lightmap atlas.
	unsigned int x= 0u, y= 0u;

	const auto build_wall=
	[&]( const MapData::Wall& in_wall, WallVertex* const v )
	{
		v[0].pos[0]= short( in_wall.vert_pos[0].x * 256.0f );
		v[0].pos[1]= short( in_wall.vert_pos[0].y * 256.0f );
		v[1].pos[0]= short( in_wall.vert_pos[1].x * 256.0f );
		v[1].pos[1]= short( in_wall.vert_pos[1].y * 256.0f );

		v[0].lightmap_coord_xy[0]= x;
		v[0].lightmap_coord_xy[1]= y;
		v[1].lightmap_coord_xy[0]= x + 1u;
		v[1].lightmap_coord_xy[1]= y;

		m_Vec2 normal( in_wall.vert_pos[0].y - in_wall.vert_pos[1].y, in_wall.vert_pos[1].x - in_wall.vert_pos[0].x );
		normal.Normalize();

		v[0].normal[0]= v[1].normal[0]= static_cast<unsigned char>( normal.x * 126.5f );
		v[0].normal[1]= v[1].normal[1]= static_cast<unsigned char>( normal.y * 126.5f );

		x++;
		if( x >= MapData::c_map_size )
		{
			x= 0u;
			y++;
		}
	};

	for( unsigned int w= 0u; w < map_data.static_walls.size(); w++ )
		build_wall(
			map_data.static_walls[w],
			&walls_vertices_[ static_walls_first_vertex_ + w * 2u ] );

	for( unsigned int w= 0u; w < map_data.dynamic_walls.size(); w++ )
		build_wall(
			map_data.dynamic_walls[w],
			&walls_vertices_[ dynamic_walls_first_vertex_ + w * 2u ] );

	// Send data to GPU.
	walls_vertex_buffer_.VertexData(
		walls_vertices_.data(),
		sizeof(WallVertex) * walls_vertices_.size(),
		sizeof(WallVertex) );

	walls_vertex_buffer_.SetPrimitiveType( GL_LINES );

	WallVertex v;
	walls_vertex_buffer_.VertexAttribPointer( 0, 2, GL_SHORT, false, ((char*)v.pos) - (char*)&v );
	walls_vertex_buffer_.VertexAttribPointer( 1, 2, GL_UNSIGNED_BYTE, false, ((char*)v.lightmap_coord_xy) - (char*)&v );
	walls_vertex_buffer_.VertexAttribPointer( 2, 2, GL_BYTE, true, ((char*)v.normal) - (char*)&v );
}

void MapLight::UpdateLightOnDynamicWalls( const MapState& map_state )
{
	const MapState::DynamicWalls& dynamic_walls= map_state.GetDynamicWalls();

	updated_dynamic_walls_flags_.clear();
	updated_dynamic_walls_flags_.resize( dynamic_walls.size(), false );

	unsigned int first_updated_wall= ~0u;
	unsigned int last_updated_wall= ~0u;

	for( unsigned int w= 0u; w < dynamic_walls.size(); w++ )
	{
		const MapState::DynamicWall& wall= dynamic_walls[w];
		WallVertex* const v= &walls_vertices_[ dynamic_walls_first_vertex_ + w * 2u ];

		short pos[2][2];
		for( unsigned int j= 0u; j < 2; j++ )
		{
			pos[j][0]= short( wall.vert_pos[j].x * 256.0f );
			pos[j][1]= short( wall.vert_pos[j].y * 256.0f );
		}

		if( std::memcmp( pos[0], v[0].pos, sizeof(short) * 2u ) != 0 || std::memcmp( pos[1], v[1].pos, sizeof(short) * 2u ) != 0 )
		{
			// Update flags
			updated_dynamic_walls_flags_[w]= true;
			if( first_updated_wall == ~0u )
				first_updated_wall= w;
			last_updated_wall= w;

			// Update position and normal.
			v[0].pos[0]= pos[0][0]; v[0].pos[1]= pos[0][1];
			v[1].pos[0]= pos[1][0]; v[1].pos[1]= pos[1][1];

			m_Vec2 normal( wall.vert_pos[0].y - wall.vert_pos[1].y, wall.vert_pos[1].x - wall.vert_pos[0].x );
			normal.Normalize();

			v[0].normal[0]= v[1].normal[0]= static_cast<unsigned char>( normal.x * 126.5f );
			v[0].normal[1]= v[1].normal[1]= static_cast<unsigned char>( normal.y * 126.5f );
		}
	}

	if( first_updated_wall == ~0u || last_updated_wall == ~0u )
		return; // Nothing to update.

	// Update GPU data.
	walls_vertex_buffer_.VertexSubData(
		walls_vertices_.data() + dynamic_walls_first_vertex_ + first_updated_wall * 2u,
		( 1u + last_updated_wall - first_updated_wall ) * 2u * sizeof(WallVertex),
		( dynamic_walls_first_vertex_ + first_updated_wall * 2u ) * sizeof(WallVertex) );

	// Recalculate light for dynamic walls.

	base_walls_lightmap_.Bind();
	walls_light_pass_shader_.Bind();
	walls_vertex_buffer_.Bind();

	for( unsigned int w= first_updated_wall; w <= last_updated_wall; w++ )
	{
		if( !updated_dynamic_walls_flags_[w] ) continue;
		const MapState::DynamicWall& wall= dynamic_walls[w];

		// Clear individual wall lightmap with "black" light.
		r_OGLStateManager::UpdateState( g_lightmap_clear_state );

		shadowmap_.GetTextures().front().Bind(0);

		walls_light_pass_shader_.Uniform( "shadowmap", int(0) );
		walls_light_pass_shader_.Uniform( "light_pos", m_Vec2( 0.0f, 0.0f ));
		walls_light_pass_shader_.Uniform( "light_power", 0.0f );
		walls_light_pass_shader_.Uniform( "max_light_level", 0.0f );
		walls_light_pass_shader_.Uniform( "min_radius", 0.5f );
		walls_light_pass_shader_.Uniform( "max_radius", 1.0f );

		glDrawArrays( GL_LINES, dynamic_walls_first_vertex_ + w * 2u, 2u );

		// Add light from nearest sources.
		r_OGLStateManager::UpdateState( g_light_pass_state );
		for( const MapData::Light& light : map_data_->lights )
		{
			if( DistanceToLineSegment( light.pos, wall.vert_pos[0], wall.vert_pos[1] ) > light.outer_radius )
				continue;

			walls_light_pass_shader_.Uniform( "shadowmap", int(0) );
			walls_light_pass_shader_.Uniform( "light_pos", light.pos );
			walls_light_pass_shader_.Uniform( "light_power", light.power * g_walls_light_scale );
			walls_light_pass_shader_.Uniform( "max_light_level", light.max_light_level * g_walls_light_scale );
			walls_light_pass_shader_.Uniform( "min_radius", light.inner_radius );
			walls_light_pass_shader_.Uniform( "max_radius", light.outer_radius );

			glDrawArrays( GL_LINES, dynamic_walls_first_vertex_ + w * 2u, 2u );
		}
	}

	r_Framebuffer::BindScreenFramebuffer();
}

void MapLight::DrawFloorLight( const MapData::Light& light )
{
	shadowmap_.GetTextures().front().Bind(0);

	const float lightmap_texel_size= float( MapData::c_map_size ) / float( base_floor_lightmap_.Width() );
	const float half_map_size= 0.5f * float(MapData::c_map_size);
	const float extended_radius= light.outer_radius + lightmap_texel_size;

	m_Mat4 world_scale_mat, viewport_scale_mat, world_shift_mat, viewport_shift_mat, world_mat, viewport_mat;
	world_scale_mat.Scale( extended_radius );
	viewport_scale_mat.Scale( extended_radius / half_map_size );

	world_shift_mat.Translate( m_Vec3( light.pos, 0.0f ) );
	viewport_shift_mat.Translate(
		m_Vec3(
			( light.pos - m_Vec2( half_map_size, half_map_size ) ) / half_map_size,
			0.0f ) );

	world_mat= world_scale_mat * world_shift_mat;
	viewport_mat= viewport_scale_mat * viewport_shift_mat;

	floor_light_pass_shader_.Uniform( "shadowmap", int(0) );
	floor_light_pass_shader_.Uniform( "view_matrix", viewport_mat );
	floor_light_pass_shader_.Uniform( "world_matrix", world_mat );
	floor_light_pass_shader_.Uniform( "light_pos", light.pos );
	floor_light_pass_shader_.Uniform( "light_power", light.power * g_floor_light_scale );
	floor_light_pass_shader_.Uniform( "max_light_level", light.max_light_level * g_floor_light_scale );
	floor_light_pass_shader_.Uniform( "min_radius", light.inner_radius );
	floor_light_pass_shader_.Uniform( "max_radius", light.outer_radius );

	glDrawArrays( GL_TRIANGLES, 0, 6 );
}

void MapLight::DrawWallsLight( const MapData::Light& light )
{
	shadowmap_.GetTextures().front().Bind(0);

	walls_light_pass_shader_.Uniform( "shadowmap", int(0) );
	walls_light_pass_shader_.Uniform( "light_pos", light.pos );
	walls_light_pass_shader_.Uniform( "light_power", light.power * g_walls_light_scale );
	walls_light_pass_shader_.Uniform( "max_light_level", light.max_light_level * g_walls_light_scale );
	walls_light_pass_shader_.Uniform( "min_radius", light.inner_radius );
	walls_light_pass_shader_.Uniform( "max_radius", light.outer_radius );

	walls_vertex_buffer_.Draw();
}

} // namespace PanzerChasm