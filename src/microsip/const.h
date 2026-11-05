/*
/*
 * Copyright (C) 2011-2026 MicroSIP (http://www.microsip.org)
 *
 * This program is free software; you can redistribute it and/or modify
 * it unbodr the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

#pragma once
#define _GLOBAL_VERSION "3.22.16"
#define _GLOBAL_VERSION_COMMA 3,22,16,0
#define _GLOBAL_TIMESTAMP 1789417397
// [fork] chave de criptografia das senhas no ini e URL de provisionamento:
// definidas em fork_config.h (gerado pelo CI a partir dos secrets do GitHub)
#include "fork_config.h"
#ifndef _GLOBAL_KEY
// [fork] literal wide (L"..."): o codigo faz (LPCTSTR)_GLOBAL_KEY; com literal narrow a chave
// efetiva viraria lixo de memoria e poderia mudar entre compilacoes
#define _GLOBAL_KEY L"*********"
#endif
// [fork] video desativado: dispensa FFmpeg/SDL/libvpx na compilacao
//#define _GLOBAL_VIDEO
#define _GLOBAL_NAME "MicroSIP"
#define _GLOBAL_COMPANY "www.microsip.org"
