/*******************************************************************************
 *                                O P E N  T S
 *******************************************************************************
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright 2025 Electronic Arts Inc.
 * Copyright 2026 OpenTS contributors
 *
 * Contains material derived from Electronic Arts source code.
 * Modified by OpenTS contributors, 2026.
 * EA's GPLv3 Section 7 additional terms and supplemental warranty
 * disclaimers apply; see LICENSE.md.
 ******************************************************************************/

#pragma once

#include "win.h"

#include "netdlg.h"

struct GlobalPacketType;
class IPXAddressClass;

/*
**	The lobby's own state, shared by the game list, host and guest screens.
*/
extern int CurGame;
extern JoinStateType JoinState;
extern bool Net2IsGameListActive;

void Send_Join_Queries(int gamenow, int playernow, int chatnow, int init = 0);
void Net2ServiceGameList(void);

// One pass of the lobby's own maintenance: service the transport, answer the join protocol,
// broadcast the host's options and age out what has gone quiet. Both of the lobby's drivers
// run this once per pass, through the presenter's Service.
void Net2ServiceLobby(void);

// Which of the lobby's three screens is up, as its dialog identifier, or 0 when none is.
// A document has no window, so this answers for both presentations.
int Net2LobbyScreenID(void);

int Net2FirstFreeColor(int reqcolor, int index);
bool Net2Callback(void);
void Net2DisplayUsers(void);
bool Net2Init_Network(void);
void Net2EncodeGameopt(char *out);
void Net2SetAccept(char *who, int status);
int Net2GetAccept(char *who);
int Net2SetHouseAndColor(char *who, int house, int color);
bool Decrypt_Serial(char *buffer);
bool Net2Remote_Connect(void);
bool Process_Global_Packet(GlobalPacketType *packet, IPXAddressClass *address);
void Net2DisplayGameList(void);
