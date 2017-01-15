#include <glsl_program.hpp>
#include <shaders_loading.hpp>

#include "drawers.hpp"
#include "game_resources.hpp"
#include "log.hpp"
#include "map_loader.hpp"
#include "sound/sound_engine.hpp"

#include "host.hpp"

namespace PanzerChasm
{

// Proxy for connections listener.
// TODO - add support for many connection listeners together (loopback, net, etc.).
class Host::ConnectionsListenerProxy final : public IConnectionsListener
{
public:
	ConnectionsListenerProxy(){}
	virtual ~ConnectionsListenerProxy() override {}

	void SetConnectionsListener( IConnectionsListenerPtr connections_listener )
	{
		connections_listener_= std::move( connections_listener );
	}

public: // IConnectionsListener
	virtual IConnectionPtr GetNewConnection()
	{
		if( connections_listener_ == nullptr )
			return nullptr;

		return connections_listener_->GetNewConnection();
	}

private:
	IConnectionsListenerPtr connections_listener_;
};

static DifficultyType DifficultyNumberToDifficulty( const unsigned int n )
{
	switch( n )
	{
	case 0: return Difficulty::Easy;
	case 1: return Difficulty::Normal;
	case 2: return Difficulty::Hard;
	case 3: return Difficulty::Deathmatch;
	default: return Difficulty::Normal;
	};
}

Host::Host()
{
	{ // Register host commands
		CommandsMapPtr commands= std::make_shared<CommandsMap>();

		commands->emplace( "quit", std::bind( &Host::Quit, this ) );
		commands->emplace( "new", std::bind( &Host::NewGameCommand, this, std::placeholders::_1 ) );
		commands->emplace( "go", std::bind( &Host::RunLevel, this, std::placeholders::_1 ) );

		host_commands_= std::move( commands );
		commands_processor_.RegisterCommands( host_commands_ );
	}

	Log::Info( "Read game archive" );
	vfs_= std::make_shared<Vfs>( "CSM.BIN" );

	Log::Info( "Loading game resources" );
	game_resources_= LoadGameResources( vfs_ );

	net_.reset( new Net() );

	system_window_.reset( new SystemWindow() );

	rSetShadersDir( "shaders" );
	{
		const auto shaders_log_callback=
		[]( const char* const log_data )
		{
			Log::Warning( log_data );
		};

		rSetShaderLoadingLogCallback( shaders_log_callback );
		r_GLSLProgram::SetProgramBuildLogOutCallback( shaders_log_callback );
	}
	r_Framebuffer::SetScreenFramebufferSize( system_window_->GetViewportSize().Width(), system_window_->GetViewportSize().Height() );

	RenderingContext rendering_context;
	CreateRenderingContext( rendering_context );

	drawers_= std::make_shared<Drawers>( rendering_context, *game_resources_ );

	Log::Info( "Initialize console" );
	console_.reset( new Console( commands_processor_, drawers_ ) );

	Log::Info( "Initialize sound subsystem" );
	sound_engine_= std::make_shared<Sound::SoundEngine>( game_resources_ );

	Log::Info( "Initialize menu" );
	menu_.reset(
		new Menu(
			*this,
			drawers_,
			sound_engine_ ) );

	map_loader_= std::make_shared<MapLoader>( vfs_ );

	// TODO - do not start new game here. Just show main menu.
	NewGame( Difficulty::Normal );
}

Host::~Host()
{
}

bool Host::Loop()
{
	// Events processing
	if( system_window_ != nullptr )
	{
		events_.clear();
		system_window_->GetInput( events_ );
	}

	for( const SystemEvent& event : events_ )
	{
		if( event.type == SystemEvent::Type::Quit )
			quit_requested_= true;

		if( event.type == SystemEvent::Type::Key &&
			event.event.key.pressed &&
			event.event.key.key_code == SystemEvent::KeyEvent::KeyCode::BackQuote &&
			console_ != nullptr )
			console_->Toggle();
	}

	const bool input_goes_to_console= console_ != nullptr && console_->IsActive();
	const bool input_goes_to_menu= !input_goes_to_console && menu_ != nullptr && menu_->IsActive();

	system_window_->CaptureMouse( !input_goes_to_console && !input_goes_to_menu );

	if( input_goes_to_console )
		console_->ProcessEvents( events_ );

	if( menu_ != nullptr && !input_goes_to_console )
		menu_->ProcessEvents( events_ );

	if( client_ != nullptr && !input_goes_to_console && !input_goes_to_menu )
		client_->ProcessEvents( events_ );

	if( sound_engine_ != nullptr )
		sound_engine_->Tick();

	// Loop operations
	if( local_server_ != nullptr )
		local_server_->Loop();

	if( client_ != nullptr )
		client_->Loop();

	// Draw operations
	if( system_window_ )
	{
		// TODO - remove draww stuff from here
		glClearColor( 0.1f, 0.1f, 0.1f, 0.5f );
		glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

		if( client_ != nullptr )
			client_->Draw();

		if( menu_ != nullptr )
			menu_->Draw();

		if( console_ != nullptr )
			console_->Draw();

		system_window_->SwapBuffers();
	}

	return !quit_requested_;
}

void Host::Quit()
{
	quit_requested_= true;
}

void Host::NewGame( const DifficultyType difficulty )
{
	DoRunLevel( 1u, difficulty );
}

void Host::NewGameCommand( const CommandsArguments& args )
{
	DifficultyType difficulty= Difficulty::Normal;
	if( args.size() >= 1u )
		difficulty= DifficultyNumberToDifficulty( std::atoi( args[0].c_str() ) );

	NewGame( difficulty );
}

void Host::RunLevel( const CommandsArguments& args )
{
	if( args.empty() )
	{
		Log::Info( "Expected map number" );
		return;
	}

	unsigned int map_number= std::atoi( args.front().c_str() );

	DifficultyType difficulty= Difficulty::Normal;
	if( args.size() >= 2u )
		difficulty= DifficultyNumberToDifficulty( std::atoi( args[1].c_str() ) );

	DoRunLevel( map_number, difficulty );
}

void Host::DoRunLevel( const unsigned int map_number, const DifficultyType difficulty )
{
	EnsureClient();
	EnsureServer();
	EnsureLoopbackBuffer();

	loopback_buffer_->RequestDisconnect(); // Kill old connection.

	local_server_->ChangeMap( map_number, difficulty );

	// Making server listen connections from loopback buffer.
	connections_listener_proxy_->SetConnectionsListener( loopback_buffer_ );
	loopback_buffer_->RequestConnect();

	// Make client working with loopback buffer connection.
	client_->SetConnection( loopback_buffer_->GetClientSideConnection() );
}

void Host::DrawLoadingFrame( const float progress, const char* const caption )
{
	// TODO - use this.
	PC_UNUSED( caption );

	if( system_window_ != nullptr && drawers_ != nullptr )
	{
		drawers_->menu.DrawLoading( progress );
		system_window_->SwapBuffers();
	}
}

void Host::CreateRenderingContext( RenderingContext& out_context )
{
	out_context.glsl_version= r_GLSLVersion( r_GLSLVersion::v330, r_GLSLVersion::Profile::Core );
	out_context.viewport_size= system_window_->GetViewportSize();
}

void Host::EnsureClient()
{
	if( client_ != nullptr )
		return;

	Log::Info( "Create client" );

	const DrawLoadingCallback draw_loading_callback=
		std::bind( &Host::DrawLoadingFrame, this, std::placeholders::_1, std::placeholders::_2 );

	RenderingContext rendering_context;
	CreateRenderingContext( rendering_context );

	client_.reset(
		new Client(
			game_resources_,
			map_loader_,
			rendering_context,
			drawers_,
			sound_engine_,
			draw_loading_callback ) );
}

void Host::EnsureServer()
{
	if( local_server_ != nullptr )
		return;

	Log::Info( "Create local server" );

	const DrawLoadingCallback draw_loading_callback=
		std::bind( &Host::DrawLoadingFrame, this, std::placeholders::_1, std::placeholders::_2 );

	PC_ASSERT( connections_listener_proxy_ == nullptr );
	connections_listener_proxy_= std::make_shared<ConnectionsListenerProxy>();

	local_server_.reset(
		new Server(
			commands_processor_,
			game_resources_,
			map_loader_,
			connections_listener_proxy_,
			draw_loading_callback ) );
}

void Host::EnsureLoopbackBuffer()
{
	if( loopback_buffer_ != nullptr )
		return;

	Log::Info( "Create loopback buffer" );
	loopback_buffer_= std::make_shared<LoopbackBuffer>();
}

} // namespace PanzerChasm
