#pragma once

void vending(ENetEvent &event, const ::hPipe &hPipe);
void open_vending_dialog(ENetPeer *connection, ::peer &player, ::world &world, const ::pos &pos, short block_id);
