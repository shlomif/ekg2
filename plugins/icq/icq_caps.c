/*
 *  (C) Copyright 2006-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *                     2008 Wies�aw Ochmi�ski <wiechu@wiechu.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "ekg2.h"

#include <string.h>

#include "icq_caps.h"

typedef unsigned char _capability_t[16];
static const _capability_t _caps[CAP_UNKNOWN+1] =
{
	{0x01, 0x38, 0xca, 0x7b, 0x76, 0x9a, 0x49, 0x15, 0x88, 0xf2, 0x13, 0xfc, 0x00, 0x97, 0x9e, 0xa8},	// CAP_HTML
	{0x09, 0x46, 0x00, 0x00, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_NEWCAPS
	{0x09, 0x46, 0x13, 0x41, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_VOICE
	{0x09, 0x46, 0x13, 0x42, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_AIMDIRPLAY
	{0x09, 0x46, 0x13, 0x43, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_SENDFILE
	{0x09, 0x46, 0x13, 0x44, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_ICQDIRECT
	{0x09, 0x46, 0x13, 0x45, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_IMIMAGE
	{0x09, 0x46, 0x13, 0x46, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_BUDDYICON
	{0x09, 0x46, 0x13, 0x47, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_SAVESTOCKS
	{0x09, 0x46, 0x13, 0x48, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_GETFILE
	{0x09, 0x46, 0x13, 0x49, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_SRV_RELAY
	{0x09, 0x46, 0x13, 0x4a, 0x4c, 0x7f, 0x11, 0xd1, 0x22, 0x82, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_GAMES2
	{0x09, 0x46, 0x13, 0x4a, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_GAMES
	{0x09, 0x46, 0x13, 0x4b, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_CONTACTS
	{0x09, 0x46, 0x13, 0x4c, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_DEVILS
	{0x09, 0x46, 0x13, 0x4d, 0x4d, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_INTEROPERATE
	{0x09, 0x46, 0x13, 0x4e, 0x4c, 0x7f, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_UTF
	{0x1a, 0x09, 0x3c, 0x6c, 0xd7, 0xfd, 0x4e, 0xc5, 0x9d, 0x51, 0xa6, 0x47, 0x4e, 0x34, 0xf5, 0xa0},	// CAP_XTRAZ
	{0x56, 0x3f, 0xc8, 0x09, 0x0b, 0x6f, 0x41, 0xbd, 0x9f, 0x79, 0x42, 0x26, 0x09, 0xdf, 0xa2, 0xf3},	// CAP_TYPING
	{0x74, 0x8f, 0x24, 0x20, 0x62, 0x87, 0x11, 0xd1, 0x82, 0x22, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00},	// CAP_CHAT
	{0x97, 0xb1, 0x27, 0x51, 0x24, 0x3c, 0x43, 0x34, 0xad, 0x22, 0xd6, 0xab, 0xf7, 0x3f, 0x14, 0x92},	// CAP_RTF
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}	// CAP_UNKNOWN
};

int icq_cap_id(unsigned char *buf) {
	int i;
	if (!buf)
		return CAP_UNKNOWN;
	for (i=0; i<CAP_UNKNOWN; i++) {
		if (!memcmp(buf, _caps[i], 0x10))
			return i;
	}
	return CAP_UNKNOWN;
}

int icq_short_cap_id(unsigned char *buf) {
	_capability_t cap;
	memcpy(&cap, _caps[CAP_VOICE], 0x10);
	cap[2] = buf[0];
	cap[3] = buf[1];
	return icq_cap_id(cap);
}

const unsigned char *icq_cap_str(int id) {
	if ( (id>0) && (id<CAP_UNKNOWN) )
		return _caps[id];

	return _caps[CAP_UNKNOWN];
}

const char *icq_capability_name(int id) {
	static const char *names[CAP_UNKNOWN] = {
				"HTML",
				"New format of caps",
				"Voice chat",
				"Direct play service",
				"File transfer (can send files)",
				"Something called \"route finder\" (ICQ2K only)",
				"DirectIM/IMImage",
				"Avatar service.",
				"Stocks (add-ins)",
				"Filetransfers (can receive files)",
				"Channel 2 extended, TLV(0x2711) based messages",
				"Games",
				"Games",
				"Buddy lists transfer",
				"Devils",
				"AIM & ICQ messages",
				"UTF-8 messages",
				"XTRAZ",
				"Mini typing notifications",
				"Chat service",
				"RTF messages"
				};

	if ( (id>0) && (id<CAP_UNKNOWN) )
		return names[id];

	return "Unknown";
}

void icq_pack_append_cap(GString *pkt, int cap_id) {
	if ((cap_id >=CAP_UNKNOWN) || (cap_id < 0)) {
		debug_error("icq_pack_append_cap() - unknown cap id: 0x%x\n", cap_id);
		return;
	}

	g_string_append_len(pkt, (char *) _caps[cap_id], 0x10);

}

/*
 * xStatuses
 *
 */

static const _capability_t _capXStatus[XSTATUS_COUNT] = {
  {0x63, 0x62, 0x73, 0x37, 0xa0, 0x3f, 0x49, 0xff, 0x80, 0xe5, 0xf7, 0x09, 0xcd, 0xe0, 0xa4, 0xee},
  {0x5a, 0x58, 0x1e, 0xa1, 0xe5, 0x80, 0x43, 0x0c, 0xa0, 0x6f, 0x61, 0x22, 0x98, 0xb7, 0xe4, 0xc7},
  {0x83, 0xc9, 0xb7, 0x8e, 0x77, 0xe7, 0x43, 0x78, 0xb2, 0xc5, 0xfb, 0x6c, 0xfc, 0xc3, 0x5b, 0xec},
  {0xe6, 0x01, 0xe4, 0x1c, 0x33, 0x73, 0x4b, 0xd1, 0xbc, 0x06, 0x81, 0x1d, 0x6c, 0x32, 0x3d, 0x81},
  {0x8c, 0x50, 0xdb, 0xae, 0x81, 0xed, 0x47, 0x86, 0xac, 0xca, 0x16, 0xcc, 0x32, 0x13, 0xc7, 0xb7},
  {0x3f, 0xb0, 0xbd, 0x36, 0xaf, 0x3b, 0x4a, 0x60, 0x9e, 0xef, 0xcf, 0x19, 0x0f, 0x6a, 0x5a, 0x7f},
  {0xf8, 0xe8, 0xd7, 0xb2, 0x82, 0xc4, 0x41, 0x42, 0x90, 0xf8, 0x10, 0xc6, 0xce, 0x0a, 0x89, 0xa6},
  {0x80, 0x53, 0x7d, 0xe2, 0xa4, 0x67, 0x4a, 0x76, 0xb3, 0x54, 0x6d, 0xfd, 0x07, 0x5f, 0x5e, 0xc6},
  {0xf1, 0x8a, 0xb5, 0x2e, 0xdc, 0x57, 0x49, 0x1d, 0x99, 0xdc, 0x64, 0x44, 0x50, 0x24, 0x57, 0xaf},
  {0x1b, 0x78, 0xae, 0x31, 0xfa, 0x0b, 0x4d, 0x38, 0x93, 0xd1, 0x99, 0x7e, 0xee, 0xaf, 0xb2, 0x18},
  {0x61, 0xbe, 0xe0, 0xdd, 0x8b, 0xdd, 0x47, 0x5d, 0x8d, 0xee, 0x5f, 0x4b, 0xaa, 0xcf, 0x19, 0xa7},
  {0x48, 0x8e, 0x14, 0x89, 0x8a, 0xca, 0x4a, 0x08, 0x82, 0xaa, 0x77, 0xce, 0x7a, 0x16, 0x52, 0x08},
  {0x10, 0x7a, 0x9a, 0x18, 0x12, 0x32, 0x4d, 0xa4, 0xb6, 0xcd, 0x08, 0x79, 0xdb, 0x78, 0x0f, 0x09},
  {0x6f, 0x49, 0x30, 0x98, 0x4f, 0x7c, 0x4a, 0xff, 0xa2, 0x76, 0x34, 0xa0, 0x3b, 0xce, 0xae, 0xa7},
  {0x12, 0x92, 0xe5, 0x50, 0x1b, 0x64, 0x4f, 0x66, 0xb2, 0x06, 0xb2, 0x9a, 0xf3, 0x78, 0xe4, 0x8d},
  {0xd4, 0xa6, 0x11, 0xd0, 0x8f, 0x01, 0x4e, 0xc0, 0x92, 0x23, 0xc5, 0xb6, 0xbe, 0xc6, 0xcc, 0xf0},
  {0x60, 0x9d, 0x52, 0xf8, 0xa2, 0x9a, 0x49, 0xa6, 0xb2, 0xa0, 0x25, 0x24, 0xc5, 0xe9, 0xd2, 0x60},
  {0x1f, 0x7a, 0x40, 0x71, 0xbf, 0x3b, 0x4e, 0x60, 0xbc, 0x32, 0x4c, 0x57, 0x87, 0xb0, 0x4c, 0xf1},
  {0x78, 0x5e, 0x8c, 0x48, 0x40, 0xd3, 0x4c, 0x65, 0x88, 0x6f, 0x04, 0xcf, 0x3f, 0x3f, 0x43, 0xdf},
  {0xa6, 0xed, 0x55, 0x7e, 0x6b, 0xf7, 0x44, 0xd4, 0xa5, 0xd4, 0xd2, 0xe7, 0xd9, 0x5c, 0xe8, 0x1f},
  {0x12, 0xd0, 0x7e, 0x3e, 0xf8, 0x85, 0x48, 0x9e, 0x8e, 0x97, 0xa7, 0x2a, 0x65, 0x51, 0xe5, 0x8d},
  {0xba, 0x74, 0xdb, 0x3e, 0x9e, 0x24, 0x43, 0x4b, 0x87, 0xb6, 0x2f, 0x6b, 0x8d, 0xfe, 0xe5, 0x0f},
  {0x63, 0x4f, 0x6b, 0xd8, 0xad, 0xd2, 0x4a, 0xa1, 0xaa, 0xb9, 0x11, 0x5b, 0xc2, 0x6d, 0x05, 0xa1},
  {0x01, 0xd8, 0xd7, 0xee, 0xac, 0x3b, 0x49, 0x2a, 0xa5, 0x8d, 0xd3, 0xd8, 0x77, 0xe6, 0x6b, 0x92},
  {0x2c, 0xe0, 0xe4, 0xe5, 0x7c, 0x64, 0x43, 0x70, 0x9c, 0x3a, 0x7a, 0x1c, 0xe8, 0x78, 0xa7, 0xdc},
  {0x10, 0x11, 0x17, 0xc9, 0xa3, 0xb0, 0x40, 0xf9, 0x81, 0xac, 0x49, 0xe1, 0x59, 0xfb, 0xd5, 0xd4},
  {0x16, 0x0c, 0x60, 0xbb, 0xdd, 0x44, 0x43, 0xf3, 0x91, 0x40, 0x05, 0x0f, 0x00, 0xe6, 0xc0, 0x09},
  {0x64, 0x43, 0xc6, 0xaf, 0x22, 0x60, 0x45, 0x17, 0xb5, 0x8c, 0xd7, 0xdf, 0x8e, 0x29, 0x03, 0x52},
  {0x16, 0xf5, 0xb7, 0x6f, 0xa9, 0xd2, 0x40, 0x35, 0x8c, 0xc5, 0xc0, 0x84, 0x70, 0x3c, 0x98, 0xfa},
  {0x63, 0x14, 0x36, 0xff, 0x3f, 0x8a, 0x40, 0xd0, 0xa5, 0xcb, 0x7b, 0x66, 0xe0, 0x51, 0xb3, 0x64},
  {0xb7, 0x08, 0x67, 0xf5, 0x38, 0x25, 0x43, 0x27, 0xa1, 0xff, 0xcf, 0x4c, 0xc1, 0x93, 0x97, 0x97},
  {0xdd, 0xcf, 0x0e, 0xa9, 0x71, 0x95, 0x40, 0x48, 0xa9, 0xc6, 0x41, 0x32, 0x06, 0xd6, 0xf2, 0x80}
};

const char* _nameXStatus[XSTATUS_COUNT] = {
	("Shopping"),
	"Taking a bath",
	"Tired",
	"Party",
	"Drinking beer",
	"Thinking",
	"Eating",
	"Watching TV",
	"Meeting",
	"Coffee",
	"Listening to music",
	"Business",
	"Shooting",
	"Having fun",
	"On the phone",
	"Gaming",
	"Studying",
	"Feeling sick",
	"Sleeping",
	"Surfing",
	"Browsing",
	"Working",
	"Typing",
	"Angry",
	"Picnic",
	"Cooking",
	"Smoking",
	"I'm high",
	"On WC",
	"To be or not to be",
	"Watching pro7 on TV",
	"Love"};


const char *icq_xstatus_name(int id) {
	if ((id <=0) || (id > XSTATUS_COUNT))
		return "unknown";
	return _nameXStatus[id-1];
}

int icq_xstatus_id(unsigned char *buf) {
	int i;
	if (!buf)
		return 0;
	for (i=0; i<XSTATUS_COUNT; i++) {
		if (!memcmp(buf, _capXStatus[i], 0x10))
			return i+1;
	}
	return 0;
}

void icq_pack_append_xstatus(GString *pkt, int x_id) {
	if ((x_id <=0) || (x_id > XSTATUS_COUNT))
		return;

	g_string_append_len(pkt, (char *) _capXStatus[x_id-1], 0x10);

}


static const _capability_t _plugins[PLUGIN_UNKNOWN] = {
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},	// PSIG_MESSAGE
	{0xd1, 0x40, 0xcf, 0x10, 0xe9, 0x4f, 0x11, 0xd3, 0xbc, 0xd2, 0x00, 0x04, 0xac, 0x96, 0xdd, 0x96},	// PSIG_STATUS_PLUGIN - Status manager plugin
	{0x37, 0x3f, 0xe9, 0xa0, 0xe9, 0x4f, 0x11, 0xd3, 0xbc, 0xd2, 0x00, 0x04, 0xac, 0x96, 0xdd, 0x96},	// PSIG_INFO_PLUGIN - Info manager plugin

	{0x05, 0x73, 0x6b, 0xbe, 0xc2, 0x0f, 0x4f, 0x10, 0xa6, 0xde, 0x4d, 0xb1, 0xe3, 0x56, 0x4b, 0x0e},	// MGTYPE_MESSAGE - Message plugin
	{0xd9, 0x12, 0x2d, 0xf0, 0x91, 0x30, 0x11, 0xd3, 0x8d, 0xd7, 0x00, 0x10, 0x4b, 0x06, 0x46, 0x2e},	// MGTYPE_FILE - File transfer plugin
	{0x72, 0x58, 0x1c, 0x37, 0x87, 0xe9, 0x11, 0xd4, 0xa4, 0xc1, 0x00, 0xd0, 0xb7, 0x59, 0xb1, 0xd9},	// MGTYPE_WEBURL - URL plugin
	{0xb2, 0x20, 0xf7, 0xbf, 0x8e, 0x37, 0x11, 0xd4, 0xbd, 0x28, 0x00, 0x04, 0xac, 0x96, 0xd9, 0x05},	// MGTYPE_CHAT - Chat plugin
	{0x46, 0x7d, 0x0e, 0x2a, 0x76, 0x76, 0x11, 0xd4, 0xbc, 0xe6, 0x00, 0x04, 0xac, 0x96, 0x1e, 0xa6},	// MGTYPE_CONTACTS - Send contact list plugin
	{0x00, 0xf6, 0x28, 0x0e, 0xe7, 0x11, 0x11, 0xd3, 0xbc, 0xf3, 0x00, 0x04, 0xac, 0x96, 0x9d, 0xc2},	// MGTYPE_SMS_MESSAGE - SMS plugin
	{0x48, 0x3b, 0xe5, 0x01, 0x11, 0xd1, 0xe4, 0x2a, 0x60, 0x00, 0x79, 0xb6, 0x94, 0xe2, 0xe1, 0x97},	// MGTYPE_GREETING_CARD

	{0xed, 0x2d, 0xed, 0x47, 0x1f, 0xf2, 0x11, 0xd4, 0xbc, 0xfd, 0x00, 0x06, 0x29, 0xee, 0x4d, 0xa1},	// User info plugin
	{0x2c, 0x21, 0x7c, 0x90, 0x4d, 0x91, 0x11, 0xd3, 0xad, 0xeb, 0x00, 0x04, 0xac, 0x96, 0xaa, 0xb2},	// Phone info plugin
	{0x5c, 0x1e, 0x1e, 0x50, 0xdd, 0x9e, 0x11, 0xd3, 0xab, 0x1f, 0x00, 0x50, 0x04, 0x8e, 0xbc, 0x8d},	// White search plugin
	{0x2c, 0xf8, 0x6d, 0x50, 0xde, 0x72, 0x11, 0xd3, 0xab, 0x21, 0x00, 0x50, 0x04, 0x8e, 0xbc, 0x8d},	// Search plugin
	{0xfd, 0xf5, 0xa1, 0x3b, 0x6e, 0xd2, 0x40, 0x3f, 0x86, 0xe0, 0xb4, 0x84, 0x6b, 0x77, 0xdf, 0xa7},	// Features list plugin
	{0xd6, 0x8e, 0x4b, 0x1c, 0x02, 0xe7, 0x11, 0xd4, 0xbc, 0xe8, 0x00, 0x04, 0xac, 0x96, 0xd9, 0x05},	// Ext contacts plugin
	{0x3d, 0xa8, 0xf1, 0x60, 0x49, 0x91, 0x11, 0xd3, 0x8d, 0xbe, 0x00, 0x10, 0x4b, 0x06, 0x46, 0x2e},	// Random users service
	{0xfb, 0x9f, 0x72, 0xc0, 0x56, 0x78, 0x11, 0xd3, 0x8d, 0xc2, 0x00, 0x10, 0x4b, 0x06, 0x46, 0x2e},	// Random plugin
	{0x5a, 0x88, 0x1d, 0x65, 0x2a, 0x73, 0x11, 0xd4, 0xbd, 0x0a, 0x00, 0x06, 0x29, 0xee, 0x4d, 0xa1},	// Wireless pager plugin
	{0x39, 0x34, 0x36, 0xbc, 0x07, 0xa4, 0x40, 0xa2, 0x90, 0x0c, 0x35, 0xa9, 0xf0, 0x03, 0xbe, 0x09},	// External plugin
	{0xfa, 0xda, 0x17, 0x86, 0x7e, 0x36, 0x4e, 0xa0, 0x8e, 0x1b, 0xc6, 0xb3, 0xbd, 0x0e, 0x51, 0x5c},	// Add user wizard plugin
	{0xc0, 0x31, 0xd0, 0xd1, 0x31, 0x2c, 0x11, 0xd2, 0x8a, 0x09, 0x00, 0x10, 0x4b, 0x9b, 0x48, 0xab},	// Voice message plugin
	{0x1c, 0xc9, 0x13, 0xa1, 0x7e, 0x1e, 0x11, 0xd2, 0xac, 0x9f, 0x00, 0x10, 0x4b, 0xbc, 0x2b, 0x53},	// IRCQ plugin
};

int icq_plugin_id(unsigned char *buf) {
	int i;
	if (!buf)
		return PLUGIN_UNKNOWN;
	for (i=0; i<PLUGIN_UNKNOWN; i++) {
		if (!memcmp(buf, _plugins[i], 0x10))
			return i;
	}
	return i;
}
