/*
 *  ALSA lib header file include/version.h
 *  Copyright (c) 2022 Huawei Device Co., Ltd.
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#define SND_LIB_MAJOR		1 /**< major number of library version */
#define SND_LIB_MINOR		2 /**< minor number of library version */
#define SND_LIB_SUBMINOR	11 /**< subminor number of library version */
#define SND_LIB_EXTRAVER	1000000 /**< extra version number, used mainly for betas */
/** library version */
#define SND_LIB_VER(maj, min, sub) (((maj)<<16)|((min)<<8)|(sub))
#define SND_LIB_VERSION SND_LIB_VER(SND_LIB_MAJOR, SND_LIB_MINOR, SND_LIB_SUBMINOR)
/** library version (string) */
#define SND_LIB_VERSION_STR	"1.2.11"

