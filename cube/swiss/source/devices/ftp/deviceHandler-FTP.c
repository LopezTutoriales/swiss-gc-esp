/**
 * Swiss - deviceHandler-FTP.c
 * Copyright (C) 2017 emu_kidid
 * 
 * deviceHandler module for FTP
 *
 * email address:  emukidid@gmail.com
 *
 *
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence, or any later version.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
**/

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <network.h>
#include <ogcsys.h>
#include <sys/dir.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include "swiss.h"
#include "main.h"
#include "gui/FrameBufferMagic.h"
#include "gui/IPLFontWrite.h"
#include "deviceHandler.h"
#include "deviceHandler-FAT.h"
#include "exi.h"
#include "ftp_devoptab.h"
#include "bba.h"

/* FTP Globals */
extern int net_initialized;
int ftp_initialized = 0;

file_handle initial_FTP =
	{ "ftp:/", // file name
	  0ULL,      // discoffset (u64)
	  0,         // offset
	  0,         // size
	  IS_DIR,
	  0,
	  0
	};

device_info initial_FTP_info = {
	0LL,
	0LL,
	true
};
	
device_info* deviceHandler_FTP_info(file_handle* file) {
	return &initial_FTP_info;
}

void readDeviceInfoFTP() {
	struct statvfs buf;
	memset(&buf, 0, sizeof(statvfs));
	uiDrawObj_t *msgBox = DrawPublish(DrawProgressBar(true, 0, "Leyendo informacion de ftp:/"));
	int res = statvfs("ftp:/", &buf);
	initial_FTP_info.freeSpace = !res ? (u64)((u64)buf.f_bfree*(u64)buf.f_bsize):0LL;
	initial_FTP_info.totalSpace = !res ? (u64)((u64)buf.f_blocks*(u64)buf.f_bsize):0LL;
	DrawDispose(msgBox);
}
	
// Connect to the ftp specified in swiss.ini
void init_ftp() {
  	int res = 0;
	if(ftp_initialized) {
		return;
	}
	res = ftpInitDevice("ftp", swissSettings.ftpUserName, swissSettings.ftpPassword, "/", swissSettings.ftpHostIp, swissSettings.ftpPort, swissSettings.ftpUsePasv);
	print_debug("ftpInitDevice %i\n",res);
	if(res) {
		ftp_initialized = 1;
	}
	else {
		ftp_initialized = 0;
	}
}

s32 deviceHandler_FTP_readDir(file_handle* ffile, file_handle** dir, u32 type){	
   	
	DIR* dp = opendir( ffile->name );
	if(!dp) return -1;
	struct dirent *entry;
	#ifdef _DIRENT_HAVE_D_STAT
	#define fstat entry->d_stat
	#else
	struct stat fstat;
	#endif
	
	// Set everything up to read
	int num_entries = 1, i = 1;
	*dir = calloc(num_entries, sizeof(file_handle));
	concat_path((*dir)[0].name, ffile->name, "..");
	(*dir)[0].fileType = IS_SPECIAL;
	
	// Read each entry of the directory
	do {
		errno = 0;
		entry = readdir(dp);
		if(!entry || !strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
			continue;
		}
		// Do we want this one?
		if((type == -1 || ((entry->d_type == DT_DIR) ? (type==IS_DIR) : (type==IS_FILE)))) {
			// Make sure we have room for this one
			if(i == num_entries){
				++num_entries;
				*dir = reallocarray(*dir, num_entries, sizeof(file_handle));
			}
			memset(&(*dir)[i], 0, sizeof(file_handle));
			if(concat_path((*dir)[i].name, ffile->name, entry->d_name) < PATHNAME_MAX
				#ifndef _DIRENT_HAVE_D_STAT
				&& !stat((*dir)[i].name, &fstat)
				#endif
				&& fstat.st_size <= UINT32_MAX) {
				(*dir)[i].fileBase = fstat.st_ino;
				(*dir)[i].size     = fstat.st_size;
				(*dir)[i].fileType = S_ISDIR(fstat.st_mode) ? IS_DIR : IS_FILE;
				++i;
			}
		}
	} while(entry || errno == EOVERFLOW);
	
	#undef fstat
	closedir(dp);
	return i;
}

s32 deviceHandler_FTP_statFile(file_handle* file) {
	struct stat fstat;
	int ret = stat(file->name, &fstat);
	if(ret == 0) {
		file->fileBase = fstat.st_ino;
		file->size     = fstat.st_size;
		file->fileType = S_ISDIR(fstat.st_mode) ? IS_DIR : IS_FILE;
	}
	return ret;
}

s64 deviceHandler_FTP_seekFile(file_handle* file, s64 where, u32 type){
	if(type == DEVICE_HANDLER_SEEK_SET) file->offset = where;
	else if(type == DEVICE_HANDLER_SEEK_CUR) file->offset = file->offset + where;
	else if(type == DEVICE_HANDLER_SEEK_END) file->offset = file->size + where;
	return file->offset;
}

s32 deviceHandler_FTP_readFile(file_handle* file, void* buffer, u32 length){
	if(!file->fp) {
		file->fp = fopen(file->name, "rb");
		if(!file->fp) return -1;
		setbuf(file->fp, NULL);
		fseek(file->fp, 0, SEEK_END);
		file->size = ftell(file->fp);
		file->fileType = IS_FILE;
	}
	
	fseek(file->fp, file->offset, SEEK_SET);
	size_t bytes_read = fread(buffer, 1, length, file->fp);
	file->offset = ftell(file->fp);
	return bytes_read;
}

s32 deviceHandler_FTP_writeFile(file_handle* file, const void* buffer, u32 length){
	if(!file->fp) {
		file->fp = fopen(file->name, "wb");
		if(!file->fp) return -1;
		setbuf(file->fp, NULL);
		file->size = 0;
		file->fileType = IS_FILE;
	}
	
	fseek(file->fp, file->offset, SEEK_SET);
	size_t bytes_written = fwrite(buffer, 1, length, file->fp);
	file->offset = ftell(file->fp);
	return bytes_written;
}

s32 deviceHandler_FTP_init(file_handle* file){
	// We need at least a share name and ip addr in the settings filled out
	if(!strlen(&swissSettings.ftpHostIp[0])) {
		file->status = E_CHECKCONFIG;
		return EFAULT;
	}

	if(!init_network()) {        //Init if we have to
		file->status = E_NONET;
		return EFAULT;
	} 

	if(!ftp_initialized) {       //Connect to the FTP
		init_ftp();
		if(!ftp_initialized) {
			file->status = E_CONNECTFAIL;
			return EIO;
		}
		readDeviceInfoFTP();
	}
	return 0;
}

s32 deviceHandler_FTP_closeFile(file_handle* file) {
	int ret = 0;
	if(file && file->fp) {
		ret = fclose(file->fp);
		file->fp = NULL;
	}
	return ret;
}

s32 deviceHandler_FTP_deinit(file_handle* file) {
	deviceHandler_FTP_closeFile(file);
	initial_FTP_info.freeSpace = 0LL;
	initial_FTP_info.totalSpace = 0LL;
	if(ftp_initialized) {
		ftpClose("ftp");
		ftp_initialized = 0;
	}
	return 0;
}

s32 deviceHandler_FTP_deleteFile(file_handle* file) {
	deviceHandler_FTP_closeFile(file);
	if(file->fileType == IS_DIR)
		return rmdir(file->name);
	else
		return unlink(file->name);
}

s32 deviceHandler_FTP_renameFile(file_handle* file, char* name) {
	deviceHandler_FTP_closeFile(file);
	int ret = rename(file->name, name);
	if(ret == 0)
		strcpy(file->name, name);
	return ret;
}

s32 deviceHandler_FTP_makeDir(file_handle* dir) {
	return mkdir(dir->name, 0777);
}

bool deviceHandler_FTP_test() {
	__device_ftp.hwName = bba_device_str;
	__device_ftp.location = bba_location;

	switch (bba_exists(LOC_ANY)) {
		case LOC_MEMCARD_SLOT_A:
			__device_ftp.deviceTexture = (textureImage){TEX_GCNET, 65, 84, 72, 88};
			return true;
		case LOC_MEMCARD_SLOT_B:
			if (sdgecko_getDevice(1) == EXI_DEVICE_0)
				__device_ftp.deviceTexture = (textureImage){TEX_GCNET, 65, 84, 72, 88};
			else
				__device_ftp.deviceTexture = (textureImage){TEX_ETH2GC, 64, 80, 64, 80};
			return true;
		case LOC_SERIAL_PORT_1:
			__device_ftp.deviceTexture = (textureImage){TEX_BBA, 140, 64, 140, 64};
			return true;
		case LOC_SERIAL_PORT_2:
			__device_ftp.deviceTexture = (textureImage){TEX_ETH2GC, 64, 80, 64, 80};
			return true;
	}
	return net_initialized;
}

char* deviceHandler_FTP_status(file_handle* file) {
	switch(file->status) {
		case E_NONET:
			return "Red no iniciada";
		case E_CHECKCONFIG:
			return "Chequea Configuracion de FTP";
		case E_CONNECTFAIL:
			return "Error al conectar al servidor FTP";
	}
	return NULL;
}

DEVICEHANDLER_INTERFACE __device_ftp = {
	.deviceUniqueId = DEVICE_ID_D,
	.hwName = "Adaptador de Red",
	.deviceName = "FTP",
	.deviceDescription = "Configurable a traves de los ajustes",
	.deviceTexture = {TEX_BBA, 140, 64, 140, 64},
	.features = FEAT_READ|FEAT_WRITE|FEAT_THREAD_SAFE,
	.initial = &initial_FTP,
	.test = deviceHandler_FTP_test,
	.info = deviceHandler_FTP_info,
	.init = deviceHandler_FTP_init,
	.makeDir = deviceHandler_FTP_makeDir,
	.readDir = deviceHandler_FTP_readDir,
	.statFile = deviceHandler_FTP_statFile,
	.seekFile = deviceHandler_FTP_seekFile,
	.readFile = deviceHandler_FTP_readFile,
	.writeFile = deviceHandler_FTP_writeFile,
	.closeFile = deviceHandler_FTP_closeFile,
	.deleteFile = deviceHandler_FTP_deleteFile,
	.renameFile = deviceHandler_FTP_renameFile,
	.deinit = deviceHandler_FTP_deinit,
	.status = deviceHandler_FTP_status,
};
