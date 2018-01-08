include(../Common/Common.pri)
CONFIG-= console

win32 {
	#put your SDL2 paths here
	SDL_INCLUDES_DIR = ../../SDL2-2.0.3/include
	SDL_LIBS_DIR = ../../SDL2-2.0.3/lib/x86

	LIBS+= $$SDL_LIBS_DIR/SDL2main.lib
	LIBS+= $$SDL_LIBS_DIR/SDL2.lib
	LIBS+= libopengl32

	# Windows sockets 2 (ws2_32.lib).
	LIBS+= libws2_32
}
else {
	SDL_INCLUDES_DIR = /usr/include/SDL2/

	LIBS+= -lSDL2
	LIBS+= -lGL
}

CONFIG( debug, debug|release ) {
	DEFINES+= DEBUG
}

#MMX instructions here.
# remove compiler option and define, if you do not need mmx, or if build target is not x86.
QMAKE_CXXFLAGS += -mmmx
DEFINES+= PC_MMX_INSTRUCTIONS


win32: RC_FILE= PanzerChasm.rc

INCLUDEPATH+= ../panzer_ogl_lib
INCLUDEPATH+= $$SDL_INCLUDES_DIR

SOURCES+= \
	client/client.cpp \
	client/cutscene_player.cpp \
	client/cutscene_script.cpp \
	client/demo_recorder.cpp \
	client/hud_drawer_base.cpp \
	client/hud_drawer_gl.cpp \
	client/hud_drawer_soft.cpp \
	client/map_drawers_common.cpp \
	client/map_drawer_gl.cpp \
	client/map_drawer_soft.cpp \
	client/minimap_drawer_gl.cpp \
	client/minimap_drawer_soft.cpp \
	client/minimap_state.cpp \
	client/map_state.cpp \
	client/movement_controller.cpp \
	client/opengl_renderer/animations_buffer.cpp \
	client/opengl_renderer/map_light.cpp \
	client/opengl_renderer/models_textures_corrector.cpp \
	client/software_renderer/map_bsp_tree.cpp \
	client/software_renderer/rasterizer.cpp \
	client/software_renderer/surfaces_cache.cpp \
	client/weapon_state.cpp \
	commands_processor.cpp \
	connection_info.cpp \
	console.cpp \
	drawers_factory_gl.cpp \
	drawers_factory_soft.cpp \
	game_resources.cpp \
	host.cpp \
	images.cpp \
	log.cpp \
	loopback_buffer.cpp \
	main.cpp \
	map_loader.cpp \
	math_utils.cpp \
	menu.cpp \
	menu_drawers_common.cpp \
	menu_drawer_gl.cpp \
	menu_drawer_soft.cpp \
	messages.cpp \
	messages_extractor.cpp \
	messages_sender.cpp \
	model.cpp \
	net/net.cpp \
	obj.cpp \
	program_arguments.cpp \
	rand.cpp \
	save_load.cpp \
	save_load_streams.cpp \
	server/collisions.cpp \
	server/collision_index.cpp \
	server/map.cpp \
	server/map_save_load.cpp \
	server/monster.cpp \
	server/monster_base.cpp \
	server/movement_restriction.cpp \
	server/player.cpp \
	server/server.cpp \
	settings.cpp \
	shared_drawers.cpp \
	sound/ambient_sound_processor.cpp \
	sound/driver.cpp \
	sound/objects_sounds_processor.cpp \
	sound/sound_engine.cpp \
	sound/sounds_loader.cpp \
	system_event.cpp \
	system_window.cpp \
	ticks_counter.cpp \
	time.cpp \
	text_drawers_common.cpp \
	text_drawer_gl.cpp \
	text_drawer_soft.cpp \
	vfs.cpp \

HEADERS+= \
	assert.hpp \
	client/client.hpp \
	client/cutscene_player.hpp \
	client/cutscene_script.hpp \
	client/demo_recorder.hpp \
	client/fwd.hpp \
	client/i_hud_drawer.hpp \
	client/i_map_drawer.hpp \
	client/i_minimap_drawer.hpp \
	client/hud_drawer_base.hpp \
	client/hud_drawer_gl.hpp \
	client/hud_drawer_soft.hpp \
	client/map_drawers_common.hpp \
	client/map_drawer_gl.hpp \
	client/map_drawer_soft.hpp \
	client/map_state.hpp \
	client/minimap_drawers_common.hpp \
	client/minimap_drawer_gl.hpp \
	client/minimap_drawer_soft.hpp \
	client/minimap_state.hpp \
	client/movement_controller.hpp \
	client/opengl_renderer/animations_buffer.hpp \
	client/opengl_renderer/map_light.hpp \
	client/opengl_renderer/models_textures_corrector.hpp \
	client/software_renderer/fixed.hpp \
	client/software_renderer/map_bsp_tree.hpp \
	client/software_renderer/map_bsp_tree.inl \
	client/software_renderer/rasterizer.hpp \
	client/software_renderer/rasterizer.inl \
	client/software_renderer/surfaces_cache.hpp \
	client/weapon_state.hpp \
	commands_processor.hpp \
	connection_info.hpp \
	console.hpp \
	drawers_factory_gl.hpp \
	drawers_factory_soft.hpp \
	fwd.hpp \
	game_constants.hpp \
	game_resources.hpp \
	host.hpp \
	host_commands.hpp \
	i_connection.hpp \
	i_drawers_factory.hpp \
	i_menu_drawer.hpp \
	i_text_drawer.hpp \
	images.hpp \
	log.hpp \
	loopback_buffer.hpp \
	map_loader.hpp \
	math_utils.hpp \
	menu.hpp \
	menu_drawers_common.hpp \
	menu_drawer_gl.hpp \
	menu_drawer_soft.hpp \
	messages.hpp \
	messages_extractor.hpp \
	messages_extractor.inl \
	messages_list.h \
	messages_sender.hpp \
	model.hpp \
	net/net.hpp \
	obj.hpp \
	particles.hpp \
	program_arguments.hpp \
	rand.hpp \
	rendering_context.hpp \
	save_load.hpp \
	save_load_streams.hpp \
	server/a_code.hpp \
	server/backpack.hpp \
	server/collisions.hpp \
	server/collision_index.hpp \
	server/collision_index.inl \
	server/fwd.hpp \
	server/map.hpp \
	server/monster.hpp \
	server/monster_base.hpp \
	server/movement_restriction.hpp \
	server/player.hpp \
	server/server.hpp \
	settings.hpp \
	shared_drawers.hpp \
	shared_settings_keys.hpp \
	size.hpp \
	sound/ambient_sound_processor.hpp \
	sound/channel.hpp \
	sound/driver.hpp \
	sound/objects_sounds_processor.hpp \
	sound/sound_engine.hpp \
	sound/sound_id.hpp \
	sound/sounds_loader.hpp \
	system_event.hpp \
	system_window.hpp \
	ticks_counter.hpp \
	text_drawers_common.hpp \
	text_drawer_gl.hpp \
	text_drawer_soft.hpp \
	time.hpp \
	vfs.hpp \

SOURCES+= \
	../Common/files.cpp \
	../panzer_ogl_lib/polygon_buffer.cpp \
	../panzer_ogl_lib/shaders_loading.cpp \
	../panzer_ogl_lib/texture.cpp \
	../panzer_ogl_lib/framebuffer.cpp \
	../panzer_ogl_lib/func_addresses.cpp \
	../panzer_ogl_lib/glsl_program.cpp \
	../panzer_ogl_lib/matrix.cpp \
	../panzer_ogl_lib/ogl_state_manager.cpp \
	../panzer_ogl_lib/buffer_texture.cpp \

HEADERS+= \
	../Common/files.hpp \
	../panzer_ogl_lib/plane.hpp \
	../panzer_ogl_lib/ogl_state_manager.hpp \
	../panzer_ogl_lib/panzer_ogl_lib.hpp \
	../panzer_ogl_lib/polygon_buffer.hpp \
	../panzer_ogl_lib/shaders_loading.hpp \
	../panzer_ogl_lib/texture.hpp \
	../panzer_ogl_lib/vec.hpp \
	../panzer_ogl_lib/framebuffer.hpp \
	../panzer_ogl_lib/func_declarations.hpp \
	../panzer_ogl_lib/glsl_program.hpp \
	../panzer_ogl_lib/matrix.hpp \
	../panzer_ogl_lib/bbox.hpp \
	../panzer_ogl_lib/buffer_texture.hpp \
