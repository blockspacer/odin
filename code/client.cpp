#include <time.h>

#include "common.cpp"
#define FAKE_LAG
#include "common_net.cpp"
#include "client_graphics.cpp"


struct Input
{
	bool32 up, down, left, right;
};

static bool32 g_is_running;
static Input g_input;


static void log_warning(const char* msg)
{
	OutputDebugStringA(msg);
}

static void update_input(WPARAM keycode, bool32 value)
{
	switch (keycode)
	{
		case 'A':
			g_input.left = value;
		break;

		case 'D':
			g_input.right = value;
		break;

		case 'W':
			g_input.up = value;
		break;

		case 'S':
			g_input.down = value;
		break;
	}
}

LRESULT CALLBACK WindowProc( HWND window_handle, UINT message, WPARAM w_param, LPARAM l_param )
{
	switch (message)
	{
		case WM_QUIT:
		case WM_DESTROY:
			g_is_running = 0;
		break;

		case WM_KEYDOWN:
			update_input(w_param, 1);
		break;

		case WM_KEYUP:
			update_input(w_param, 0);
		break;
	}

	return DefWindowProc( window_handle, message, w_param, l_param );
}

int CALLBACK WinMain( HINSTANCE instance, HINSTANCE /*prev_instance*/, LPSTR /*cmd_line*/, int cmd_show )
{
	WNDCLASS window_class;
	window_class.style = 0;
	window_class.lpfnWndProc = WindowProc;
	window_class.cbClsExtra = 0;
	window_class.cbWndExtra = 0;
	window_class.hInstance = instance;
	window_class.hIcon = 0;
	window_class.hCursor = 0;
	window_class.hbrBackground = 0;
	window_class.lpszMenuName = 0;
	window_class.lpszClassName = "app_window_class";

	ATOM window_class_atom = RegisterClass( &window_class );

	assert( window_class_atom );


	constexpr uint32 c_window_width = 1280;
	constexpr uint32 c_window_height = 720;

	HWND window_handle;
	{
		LPCSTR 	window_name 	= "";
		DWORD 	style 			= WS_OVERLAPPED;
		int 	x 				= CW_USEDEFAULT;
		int 	y 				= 0;
		HWND 	parent_window 	= 0;
		HMENU 	menu 			= 0;
		LPVOID 	param 			= 0;

		window_handle 			= CreateWindow( window_class.lpszClassName, window_name, style, x, y, c_window_width, c_window_height, parent_window, menu, instance, param );

		assert( window_handle );
	}
	ShowWindow( window_handle, cmd_show );

	// init graphics
	constexpr uint32 c_num_vertices = 4 * c_max_clients;
	Graphics::Vertex vertices[c_num_vertices];

	srand((unsigned int)time(0));
	for (uint32 i = 0; i < c_max_clients; ++i)
	{
		// generate colour for client
		float32 rgb[3] = {0.0f, 0.0f, 0.0f};

		// for a fully saturated colour, need 1 component at 1, and a second
		// component at <= 1, and the third must be zero
		int32 component = rand() % 3;
		rgb[component] = 1.0f;

		component += rand() % 2 ? 1 : -1;
		if(component < 0) component += 3;
		else if(component > 2) component -= 3;
		rgb[component] = (float32)(rand() % 100) / 100.0f;

		// assign colour to all 4 verts, and zero positions to begin with
		uint32 start_verts = i * 4;
		for (uint32 j = 0; j < 4; ++j)
		{
			vertices[start_verts + j].pos_x = 0.0f;
			vertices[start_verts + j].pos_y = 0.0f;
			vertices[start_verts + j].col_r = rgb[0];
			vertices[start_verts + j].col_g = rgb[1];
			vertices[start_verts + j].col_b = rgb[2];
		}
	}

	constexpr uint32 c_num_indices = 6 * c_max_clients;
	uint16 indices[c_num_indices];
	
	for(uint16 index = 0, vertex = 0; index < c_num_indices; index += 6, vertex += 4)
	{
		// quads will be top left, bottom left, bottom right, top right
		indices[index] = vertex;				// 0
		indices[index + 1] = vertex + 2;		// 2
		indices[index + 2] = vertex + 1;		// 1
		indices[index + 3] = vertex;			// 0
		indices[index + 4] = vertex + 3;		// 3
		indices[index + 5] = vertex + 2;		// 2
	}

	Graphics::State graphics_state;
	Graphics::init(window_handle, instance, c_window_width, c_window_height, c_num_vertices, indices, c_num_indices, &graphics_state);

	// init winsock
	if (!Net::init())
	{
		log_warning("Net::init failed\n");
		return 0;
	}
	Net::Socket sock;
	if (!Net::socket_create(&sock))
	{
		log_warning("socket_create failed\n");
		return 0;
	}

	uint8 buffer[c_socket_buffer_size];
	Net::IP_Endpoint server_endpoint = Net::ip_endpoint_create(127, 0, 0, 1, c_port);

	buffer[0] = (uint8)Client_Message::Join;
	if (!Net::socket_send(&sock, buffer, 1, &server_endpoint))
	{
		log_warning("join message failed to send\n");
		return 0;
	}


	struct Player_State
	{
		float32 x, y, facing;
	};
	Player_State objects[c_max_clients];
	uint32 num_objects = 0;
	uint16 slot = 0xFFFF;

	Timing_Info timing_info = timing_info_create();

	
	// main loop
	g_is_running = 1;
	while (g_is_running)
	{
		LARGE_INTEGER tick_start_time;
		QueryPerformanceCounter(&tick_start_time);

		// Windows messages
		MSG message;
		UINT filter_min = 0;
		UINT filter_max = 0;
		UINT remove_message = PM_REMOVE;
		while (PeekMessage(&message, window_handle, filter_min, filter_max, remove_message))
		{
			TranslateMessage( &message );
			DispatchMessage( &message );
		}


		// Process Packets
		uint32 bytes_received;
		Net::IP_Endpoint from;
		while (Net::socket_receive(&sock, buffer, &bytes_received, &from))
		{
			switch (buffer[0])
			{
				case Server_Message::Join_Result:
				{
					if (buffer[1])
					{
						memcpy(&slot, &buffer[2], sizeof(slot));
					}
					else
					{
						log_warning("server didn't let us in\n");
					}
				}
				break;

				case Server_Message::State:
				{
					num_objects = 0;
					uint32 bytes_read = 1;
					while (bytes_read < bytes_received)
					{
						uint16 id; // unused
						memcpy(&id, &buffer[bytes_read], sizeof(id));
						bytes_read += sizeof(id);

						memcpy(&objects[num_objects].x, &buffer[bytes_read], sizeof(objects[num_objects].x));
						bytes_read += sizeof(objects[num_objects].x);

						memcpy(&objects[num_objects].y, &buffer[bytes_read], sizeof(objects[num_objects].y));
						bytes_read += sizeof(objects[num_objects].y);

						memcpy(&objects[num_objects].facing, &buffer[bytes_read], sizeof(objects[num_objects].facing));
						bytes_read += sizeof(objects[num_objects].facing);

						++num_objects;
					}
				}
				break;
			}
		}

		// Send input
		if (slot != 0xFFFF)
		{
			buffer[0] = (uint8)Client_Message::Input;
			int bytes_written = 1;

			memcpy(&buffer[bytes_written], &slot, sizeof(slot));
			bytes_written += sizeof(slot);

			uint8 input = 	(uint8)g_input.up | 
							((uint8)g_input.down << 1) | 
							((uint8)g_input.left << 2) | 
							((uint8)g_input.right << 3);
			buffer[bytes_written] = input;
			++bytes_written;

			if (!Net::socket_send(&sock, buffer, bytes_written, &server_endpoint))
			{
				log_warning("socket_send failed\n");
			}			
		}


		// Draw
		for (uint32 i = 0; i < num_objects; ++i)
		{
			constexpr float32 size = 0.05f;
			float32 x = objects[i].x * 0.01f;
			float32 y = objects[i].y * -0.01f;

			uint32 verts_start = i * 4;
			vertices[verts_start].pos_x = x - size; // TL (hdc y is +ve down screen)
			vertices[verts_start].pos_y = y - size;
			vertices[verts_start + 1].pos_x = x - size; // BL
			vertices[verts_start + 1].pos_y = y + size;
			vertices[verts_start + 2].pos_x = x + size; // BR
			vertices[verts_start + 2].pos_y = y + size;
			vertices[verts_start + 3].pos_x = x + size; // TR
			vertices[verts_start + 3].pos_y = y - size;
		}
		for (uint32 i = num_objects; i < c_max_clients; ++i)
		{
			uint32 verts_start = i * 4;
			for (uint32 j = 0; j < 4; ++j)
			{
				vertices[verts_start + j].pos_x = vertices[verts_start + j].pos_y = 0.0f;
			}
		}
		Graphics::update_and_draw(vertices, c_num_vertices, &graphics_state);


		wait_for_tick_end(tick_start_time, &timing_info);
	}

	buffer[0] = (uint8)Client_Message::Leave;
	int bytes_written = 1;
	memcpy(&buffer[bytes_written], &slot, sizeof(slot));
	Net::socket_send(&sock, buffer, bytes_written, &server_endpoint);
	Net::socket_close(&sock);

	// todo( jbr ) return wParam of WM_QUIT
	return 0;
}