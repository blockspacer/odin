#include "net_msgs.h"

#include "player.h"



namespace Net
{


uint32 client_msg_join_write(uint8* buffer)
{
	buffer[0] = (uint8)Client_Message::Join;
	return 1;
}

uint32 client_msg_leave_write(uint8* buffer, uint32 slot)
{
	buffer[0] = (uint8)Client_Message::Leave;
	memcpy(&buffer[1], &slot, 2);

	return 3;
}
void client_msg_leave_read(uint8* buffer, uint32* out_slot)
{
	assert(buffer[0] == (uint8)Client_Message::Leave);

	*out_slot = 0;
	memcpy(out_slot, &buffer[1], 2);
}

uint32 client_msg_input_write(uint8* buffer, uint32 slot, Player_Input* input, uint32 timestamp, uint32 tick_number)
{
	// if up/down/left/right are non-zero they're not necessarily 1
	uint8 packed_input = (uint8)(input->up ? 1 : 0) |
		(uint8)(input->down ? 1 << 1 : 0) |
		(uint8)(input->left ? 1 << 2 : 0) |
		(uint8)(input->right ? 1 << 3 : 0);

	buffer[0] = (uint8)Client_Message::Input;
	memcpy(&buffer[1], &slot, 2);
	buffer[3] = packed_input;
	memcpy(&buffer[4], &timestamp, 4);
	memcpy(&buffer[8], &tick_number, 4);

	return 12;
}
void client_msg_input_read(uint8* buffer, uint32* slot, Player_Input* input, uint32* timestamp, uint32* tick_number)
{
	assert(buffer[0] == (uint8)Client_Message::Input);

	*slot = 0;
	memcpy(slot, &buffer[1], 2);

	input->up = buffer[3] & 1;
	input->down = buffer[3] & 1 << 1;
	input->left = buffer[3] & 1 << 2;
	input->right = buffer[3] & 1 << 3;

	memcpy(timestamp, &buffer[4], 4);
	memcpy(tick_number, &buffer[8], 4);
}



uint32 server_msg_join_result_write(uint8* buffer, bool32 success, uint32 slot)
{
	buffer[0] = (uint8)Server_Message::Join_Result;
	buffer[1] = success ? 1 : 0;

	if (success)
	{
		memcpy(&buffer[2], &slot, 2);
		return 4;
	}

	return 2;
}
void server_msg_join_result_read(uint8* buffer, bool32* out_success, uint32* out_slot)
{
	assert(buffer[0] == (uint8)Server_Message::Join_Result);

	*out_success = buffer[1];
	if (*out_success)
	{
		*out_slot = 0;
		memcpy(out_slot, &buffer[2], 2);
	}
}

uint32 server_msg_state_write(
	uint8* buffer, 
	uint32 tick_number, 
	uint32 client_timestamp, 
	Player_Nonvisual_State* local_player_nonvisual_state,
	IP_Endpoint* player_endpoints,
	Player_Visual_State* player_visual_states,
	uint32 max_players)
{
	buffer[0] = (uint8)Server_Message::State;

	memcpy(&buffer[1], &tick_number, 4);
	memcpy(&buffer[5], &target_player_client_timestamp, 4);
	uint32 bytes_written = 9;

	// todo(jbr) just update this one part of the packet when sending to multiple clients
	memcpy(&buffer[bytes_written], &local_player_nonvisual_state->speed, sizeof(local_player_nonvisual_state->speed));
	bytes_written += sizeof(local_player_nonvisual_state->speed);

	uint8* p_num_players = &buffer[bytes_written++]; // written later

	uint8 num_players = 0;
	for (uint8 i = 0; i < max_players; ++i)
	{
		if (player_endpoints[i].address)
		{
			++num_players;

			buffer[bytes_written++] = i;
			memcpy(&buffer[bytes_written], &player_visual_states[i].x, sizeof(player_visual_states[i].x));
			bytes_written += sizeof(player_visual_states[i].x);
			memcpy(&buffer[bytes_written], &player_visual_states[i].y, sizeof(player_visual_states[i].y));
			bytes_written += sizeof(player_visual_states[i].y);
			memcpy(&buffer[bytes_written], &player_visual_states[i].facing, sizeof(player_visual_states[i].facing));
			bytes_written += sizeof(player_visual_states[i].facing);
		}
	}

	*p_num_players = num_players;

	return bytes_written;
}
void server_msg_state_read(
	uint8* buffer,
	uint32* tick_number,
	uint32* client_timestamp, // most recent time stamp server had from client at the time of writing this packet
	Player_Nonvisual_State* local_player_nonvisual_state,
	Player_Visual_State* player_visual_states, // visual state of all players
	bool32* players_present, // a 1 will be written to every slot actually used
	uint32 max_players) // max number of players the client can handle
{
	assert(buffer[0] == (uint8)Server_Message::State);

	memcpy(tick_number, &buffer[1], 4);
	memcpy(client_timestamp, &buffer[5], 4);
	uint32 bytes_read = 9;

	memcpy(&local_player_nonvisual_state->speed, &buffer[bytes_read], sizeof(local_player_nonvisual_state->speed));
	bytes_read += sizeof(local_player_nonvisual_state->speed);

	uint8 num_players = buffer[bytes_read++];
	for (uint8 i = 0; i < num_players; ++i)
	{
		uint8 slot = buffer[bytes_read++];
		assert(slot < max_players);
		memcpy(&player_visual_states[slot].x, &buffer[bytes_read], sizeof(player_visual_states[slot].x));
		bytes_read += sizeof(player_visual_states[slot].x);
		memcpy(&player_visual_states[slot].y, &buffer[bytes_read], sizeof(player_visual_states[slot].y));
		bytes_read += sizeof(player_visual_states[slot].y);
		memcpy(&player_visual_states[slot].facing, &buffer[bytes_read], sizeof(player_visual_states[slot].facing));
		bytes_read += sizeof(player_visual_states[slot].facing);
	}

	*num_players = i;
}


} // namespace Net