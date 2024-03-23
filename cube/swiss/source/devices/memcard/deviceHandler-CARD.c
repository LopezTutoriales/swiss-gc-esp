/* deviceHandler-CARD.c
	- device implementation for CARD (NGC Memory Cards)
	by emu_kidid
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <malloc.h>
#include <ogc/card.h>
#include "deviceHandler.h"
#include "gui/FrameBufferMagic.h"
#include "gui/IPLFontWrite.h"
#include "swiss.h"
#include "main.h"
#include "images/gamecube_rgb.h"


file_handle initial_CARDA =
	{ "carda:/",       // directory
	  0,
	  0,          // offset
	  0,          // size
	  IS_DIR,
	  0
};

file_handle initial_CARDB =
	{ "cardb:/",       // directory
	  0	,
	  0,          // offset
	  0,          // size
	  IS_DIR,
	  0
};

static device_info initial_CARD_info[2];

static unsigned char *sys_area[2] = {NULL, NULL};
static int card_init[2] = {0,0};
static u32 card_sectorsize[2] = {8192, 8192};
static GCI *gciInfo;
static bool isCopyGCIMode = 0;

void card_removed_cb(s32 chn, s32 result) {  
  card_init[chn] = 0; 
  CARD_Unmount(chn); 
}

char *cardError(int error_code) {
  switch(error_code) {
    case CARD_ERROR_BUSY:
		return "Memory Card Ocupada";
    case CARD_ERROR_WRONGDEVICE:
		return "El dispositivo no es una Memory Card";
    case CARD_ERROR_NOCARD:
		return "No hay Memory Card insertada";
    case CARD_ERROR_BROKEN:
		return "Memory Card corrupta(?)";
	case CARD_ERROR_NOFILE:
		return "Archivo no existe";
	case CARD_ERROR_FATAL_ERROR:
		return "Error Fatal";
    default:
		return "Error desconocido";
  }
}

#define CARD_SetGameAndCompany() \
	CARD_SetGamecode("SWIS"); \
	CARD_SetCompany("S0");


int initialize_card(int slot) {
	int slot_error = CARD_ERROR_READY, i = 0;
  
	if(!card_init[slot]) {
		/* Pass company identifier and number */
		CARD_Init ("SWIS", "S0");
		if(!sys_area[slot]) sys_area[slot] = memalign(32,CARD_WORKAREA);
		  
		/* Lets try 50 times to mount it. Sometimes it takes a while */
		for(i = 0; i<50; i++) {
			slot_error = CARD_Mount (slot, sys_area[slot], card_removed_cb);
			if(slot_error == CARD_ERROR_READY) {
				CARD_GetSectorSize (slot, &card_sectorsize[slot]);
				break;
			}
		}
	}
	card_init[slot] = slot_error == CARD_ERROR_READY ? 1 : 0;
	return slot_error;
}

device_info* deviceHandler_CARD_info(file_handle* file) {
	int slot = (!strncmp((const char*)initial_CARDB.name, file->name, 7));
	return &initial_CARD_info[slot];
}

s32 deviceHandler_CARD_readDir(file_handle* ffile, file_handle** dir, u32 type){	

	int num_entries = 1, ret = 0, i = 1, slot = (!strncmp((const char*)initial_CARDB.name, ffile->name, 7));
	card_dir *memcard_dir = NULL;
  
	if(!card_init[slot]) { //if some error
		ret = initialize_card(slot);
		if(ret != CARD_ERROR_READY) {
			return -1; //fail
		}
	}
	CARD_SetGameAndCompany();
  
	memcard_dir = (card_dir*)memalign(32,sizeof(card_dir));
	memset(memcard_dir, 0, sizeof(card_dir));
 	
	/* Convert the Memory Card "file" data to fileBrowser_files */
	*dir = calloc(num_entries, sizeof(file_handle));
	concat_path((*dir)[0].name, ffile->name, "..");
	(*dir)[0].fileAttrib = IS_SPECIAL;

	int usedSpace = 0;
	ret = CARD_FindFirst (slot, memcard_dir, true);
	while (CARD_ERROR_NOFILE != ret) {
		// Make sure we have room for this one
		if(i == num_entries){
			++num_entries;
			*dir = reallocarray(*dir, num_entries, sizeof(file_handle));
		}
		memset(&(*dir)[i], 0, sizeof(file_handle));
		concatf_path((*dir)[i].name, ffile->name, "%.*s", CARD_FILENAMELEN, memcard_dir->filename);
		(*dir)[i].fileBase   = memcard_dir->fileno;
		(*dir)[i].size       = memcard_dir->filelen;
		(*dir)[i].fileAttrib = IS_FILE;
		memcpy( (*dir)[i].other, memcard_dir, sizeof(card_dir));
		usedSpace += (*dir)[i].size;
		ret = CARD_FindNext (memcard_dir);
		++i;
	}
	free(memcard_dir);
	
	initial_CARD_info[slot].freeSpace = initial_CARD_info[slot].totalSpace - usedSpace;

	return i;
}

// Finds a file based on file->name and populates file->size if found.
bool findFile(file_handle* file) {
	unsigned int slot = (!strncmp((const char*)initial_CARDB.name, file->name, 7)), ret = 0;
	char *filename = getRelativeName(file->name);
	bool found = false;
	CARD_SetGameAndCompany();
  
	card_dir* memcard_dir = (card_dir*)memalign(32,sizeof(card_dir));
	memset(memcard_dir, 0, sizeof(card_dir));
 	
	ret = CARD_FindFirst (slot, memcard_dir, true);
	while (CARD_ERROR_NOFILE != ret && !found) {
		if(!strncmp(filename, memcard_dir->filename, CARD_FILENAMELEN)) {
			file->size = memcard_dir->filelen;
			memcpy( file->other, memcard_dir, sizeof(card_dir));
			found = true;
 		}
		ret = CARD_FindNext (memcard_dir);
	}
	free(memcard_dir);
	return found;
}

s64 deviceHandler_CARD_seekFile(file_handle* file, s64 where, u32 type){
	if(type == DEVICE_HANDLER_SEEK_SET) file->offset = where;
	else if(type == DEVICE_HANDLER_SEEK_CUR) file->offset = file->offset + where;
	else if(type == DEVICE_HANDLER_SEEK_END) file->offset = file->size + where;
	return file->offset;
}

int CARD_ReadUnaligned(card_file *cardfile, void *buffer, unsigned int length, unsigned int offset, int slot) {
	void *dst = buffer;
	u8 *read_buffer = (u8*)memalign(32,card_sectorsize[slot]);
	int ret = CARD_ERROR_READY;
	print_gecko("Lectura no alineada dst %08X offset %08X long. %i\r\n", dst, offset, length);
	// Unaligned because we're in the middle of a sector, read partial
	ret = CARD_Read(cardfile, read_buffer, card_sectorsize[slot], offset-(offset&0x1ff));
	print_gecko("CARD_Read offset %08X long. %i\r\n", offset-(offset&0x1ff), card_sectorsize[slot]);
	if(ret != CARD_ERROR_READY) {
		free(read_buffer);
		return ret;
	}

	int amountRead = card_sectorsize[slot]-(offset&0x1ff);
	int amountToCopy = length > amountRead ? amountRead : length;
	memcpy(dst, read_buffer+(offset&0x1ff), amountToCopy);
	dst += amountToCopy;
	length -= amountToCopy;
	offset += amountToCopy;
	if(length != 0) {
		print_gecko("Offset restos de lectura no alineada %08X length %i\r\n", offset, length);
		// At least this will be aligned
		ret = CARD_Read(cardfile, read_buffer, card_sectorsize[slot], offset);
		print_gecko("CARD_Read offset %08X long. %i\r\n", offset, card_sectorsize[slot]);
		if(ret != CARD_ERROR_READY) {
			free(read_buffer);
			return ret;
		}
		memcpy(dst, read_buffer, length);
	}

	free(read_buffer);
	return ret;
}


s32 deviceHandler_CARD_readFile(file_handle* file, void* buffer, u32 length){
	card_file cardfile;
	void *dst = buffer;
	card_dir* cd = (card_dir*)&file->other;
	char *filename = getRelativeName(file->name);
	unsigned int slot = (!strncmp((const char*)initial_CARDB.name, file->name, 7)), ret = 0;

	if(cd->company[0] == '\0' && cd->gamecode[0] == '\0') {
		// Find the file we don't know about and populate this file_handle if we find it.
		if(!findFile(file)) {
			return CARD_ERROR_NOFILE;
		}
		CARD_SetCompany((const char*)cd->company);
		CARD_SetGamecode((const char*)cd->gamecode);
	} 
	else {
		CARD_SetCompany((const char*)cd->company);
		CARD_SetGamecode((const char*)cd->gamecode);
	}
	int swissFile = !strncmp((const char*)cd->gamecode, "SWIS", 4)
				 && !strncmp((const char*)cd->company, "S0", 2);
	
	// Open the file based on the slot & file name
	ret = CARD_Open(slot,filename, &cardfile);

	print_gecko("Intento de abrir: [%s] en ranura %s obtenida res: %i\r\n",filename, slot ? "B":"A", ret);

	if(ret != CARD_ERROR_READY)	return ret;

	u8 *read_buffer = (u8*)memalign(32,card_sectorsize[slot]);
	if(!read_buffer) {
		return -1;
	}

	/* Read from the file */
	u32 amountRead = 0;
	// If this file was put here by swiss, then skip the first 8192 bytes
	if(swissFile && !isCopyGCIMode) {
		print_gecko("Archivo copiado de Swiss detectado, saltando icono\r\n");
		file->offset += 8192;
	}
	if(isCopyGCIMode) {
		// Write out a .GCI
		card_stat cardstat;
		GCI gci;
		CARD_GetStatus(slot, cardfile.filenum, &cardstat);
		memset(&gci, 0, sizeof(GCI));
		memcpy(&gci.gamecode,cardstat.gamecode,4);
		memcpy(&gci.company,cardstat.company,2);
		memcpy(&gci.filename,cardstat.filename,CARD_FILENAMELEN);
		gci.reserved01 = 0xFF;
		gci.banner_fmt = cardstat.banner_fmt;
		gci.time = cardstat.time;
		gci.icon_addr = cardstat.icon_addr;
		gci.icon_fmt = cardstat.icon_fmt;
		gci.icon_speed = cardstat.icon_speed;
		gci.unknown1 = cd->permissions;
		gci.filesize8 = cardstat.len / 8192;
		gci.reserved02 = 0xFFFF;
		gci.comment_addr = cardstat.comment_addr;
		memcpy(dst, &gci, sizeof(GCI));
		dst+=sizeof(GCI);
		length-=sizeof(GCI);
		amountRead += sizeof(GCI);
	}
	while(length > 0 && file->offset < file->size) {
		int readsize = length > card_sectorsize[slot] ? card_sectorsize[slot] : length;
		print_gecko("Necesita leer: [%i] bytes mas\r\n", length);
		
		if(file->offset&0x1ff) {
			ret = CARD_ReadUnaligned(&cardfile, read_buffer, card_sectorsize[slot], file->offset, slot);
		}
		else {
			ret = CARD_Read(&cardfile, read_buffer, card_sectorsize[slot], file->offset);
			// Sometimes reads fail the first time stupidly, retry at least once.
			print_gecko("Lectura: [%i] bytes ret [%i] desde offset [%i]\r\n",card_sectorsize[slot],ret, file->offset);
			if(ret == CARD_ERROR_BUSY) {
				print_gecko("Reintentar lectura\r\n");
				usleep(2000);
				ret = CARD_Read(&cardfile, read_buffer, card_sectorsize[slot], file->offset);
			}
		}
		
		if(ret == CARD_ERROR_READY)
			memcpy(dst,read_buffer,readsize);
		else 
			return ret;
		dst+=readsize;
		file->offset += readsize;
		length -= readsize;
		amountRead += readsize;
	}
	// For swiss adjusted files, don't trail off the end.
	if(swissFile && length != 0) {
		amountRead+= length;
	}
	DCFlushRange(dst, amountRead);
	CARD_Close(&cardfile);
	free(read_buffer);   

	return amountRead;
}

// This function should always be called for the FULL length cause CARD is lame like that.
s32 deviceHandler_CARD_writeFile(file_handle* file, const void* data, u32 length) {
	if(isCopyGCIMode) {
		setGCIInfo(data);
		data+=sizeof(GCI);
		length-=sizeof(GCI);
	}
	if(gciInfo == NULL) {	// Swiss ID for this
		CARD_SetGameAndCompany();
	} else {	// Game specific
		char company[4], gamecode[8];
		memset(company, 0, 4);
		memset(gamecode, 0, 8);
		memcpy(gamecode, gciInfo->gamecode, 4);
		memcpy(company, gciInfo->company, 2);
		CARD_SetCompany(company);
		CARD_SetGamecode(gamecode);
	}
	
	card_file cardfile;
	unsigned int slot = (!strncmp((const char*)initial_CARDB.name, file->name, 7)), ret = 0;
	unsigned int adj_length = (length % 8192 == 0 ? length : (length + (8192-length%8192)));
	char *filename = NULL;
	char fname[CARD_FILENAMELEN+1];
	if(gciInfo != NULL) {
		memset(fname, 0, CARD_FILENAMELEN+1);
		memcpy(fname, gciInfo->filename, CARD_FILENAMELEN);
		filename = &fname[0];
	} else {
		filename = getRelativeName(file->name);
		adj_length += 8192;
	}
	
	// Open the file based on the slot & file name
	ret = CARD_Open(slot,filename, &cardfile);

	print_gecko("Intento de abrir: [%s] en ranura %s obtenida res: %i\r\n",filename, slot ? "B":"A", ret);
	
	if(ret == CARD_ERROR_NOFILE) {
		// If the file doesn't exist, create it.
		ret = CARD_Create(slot, filename,adj_length,&cardfile);
		print_gecko("Intento de crear: [%s] en ranura %s obtenida res: %i\r\n",filename, slot ? "B":"A", ret);
		if(ret != CARD_ERROR_READY) {
			return ret;
		}
		ret = CARD_Open(slot,filename, &cardfile);
		print_gecko("Intento de abrir despues de crear: [%s] en ranura %s obtenida res: %i\r\n",filename, slot ? "B":"A", ret);
	}
	
	if(ret != CARD_ERROR_READY) {
		// Something went wrong
		return -1;
	}

	// Write the icon, comment, etc.
	time_t gc_time = time (NULL);
	char *tmpBuffer = memalign(32,adj_length);
	memset(tmpBuffer,0,adj_length);
	if(gciInfo == NULL) {
		strcpy(tmpBuffer, filename);
		strcpy(tmpBuffer+0x20, ctime (&gc_time));
		memcpy(tmpBuffer+0x40, gamecube_rgb, sizeof(gamecube_rgb));	// Copy icon
		memcpy(tmpBuffer+0x2000, data, length);	// Copy data
	}
	else {
		memcpy(tmpBuffer, data, length);	// Copy data
	}
	
	// The file exists by now, write at offset.
	int amount_written = 0;
	while(amount_written != adj_length) {
		ret = CARD_Write(&cardfile, tmpBuffer+amount_written, card_sectorsize[slot], file->offset);
		file->offset += card_sectorsize[slot];
		amount_written += card_sectorsize[slot];
		print_gecko("Intento de escritura: [%s] en ranura %s obtenida res: %i\r\n",filename, slot ? "B":"A", ret);
		if(ret != CARD_ERROR_READY) break;
	}
	
	card_stat cardstat;
	if(gciInfo == NULL) {	// Swiss
		memset(&cardstat, 0, sizeof(card_stat));
		memcpy(&cardstat.filename, filename, CARD_FILENAMELEN-1);
		cardstat.time = gc_time;
		cardstat.comment_addr = 0;
		cardstat.icon_addr = 0x40;
		cardstat.icon_speed = CARD_SPEED_FAST;
		cardstat.banner_fmt = CARD_BANNER_NONE;
		cardstat.icon_fmt = CARD_ICON_RGB;
	}
	else {	// Info is coming from a GCI
		cardstat.banner_fmt = gciInfo->banner_fmt;
		cardstat.time = gciInfo->time;
		cardstat.icon_addr = gciInfo->icon_addr;
		cardstat.icon_fmt = gciInfo->icon_fmt;
		cardstat.icon_speed = gciInfo->icon_speed;
		cardstat.comment_addr = gciInfo->comment_addr;
	}
	CARD_SetStatus (slot, cardfile.filenum, &cardstat);
	
	free(tmpBuffer);
	CARD_Close(&cardfile);
	return ret == CARD_ERROR_READY ? length : ret;
}

void setCopyGCIMode(bool _isCopyGCIMode) {
	isCopyGCIMode = _isCopyGCIMode;
}

void setGCIInfo(const void *buffer) {
	if(buffer == NULL) {
		if(gciInfo != NULL) {
			free(gciInfo);
			gciInfo = NULL;
		}
	}
	else {
		gciInfo = (GCI*)memalign(32, sizeof(GCI));
		memcpy(gciInfo, buffer, sizeof(GCI));
	}
}

char getGCIRegion(const char *gameID)
{
	if (!strncmp(gameID, "DOLX00", 6) || !strncmp(gameID, "SWISS0", 6))
		return 'A';

	switch (gameID[3]) {
		case 'J':
		case 'K':
		case 'W':
			return 'J';
		case 'E':
			return 'E';
		case 'D':
		case 'F':
		case 'H':
		case 'I':
		case 'P':
		case 'S':
		case 'U':
		case 'X':
		case 'Y':
			return 'P';
		case 'A':
			return 'A';
		default:
			return '?';
	}
}

s32 deviceHandler_CARD_init(file_handle* file){
	int slot = (!strncmp((const char*)initial_CARDB.name, file->name, 7));
	file->status = initialize_card(slot);
	s32 memSize = 0, sectSize = 0;
	if(file->status == CARD_ERROR_READY) {
		int ret = CARD_ProbeEx(slot,&memSize,&sectSize);
		if(ret == CARD_ERROR_READY) {
			initial_CARD_info[slot].totalSpace = (u64)(memSize<<17);
		}
		else {
			print_gecko("CARD_ProbeEx fallo %i\r\n", ret);
		}
	}
	initial_CARD_info[slot].freeSpace = 0LL;
	return file->status == CARD_ERROR_READY ? 0 : EIO;
}

s32 deviceHandler_CARD_deinit(file_handle* file) {
	if(file) {
		int slot = (!strncmp((const char*)initial_CARDB.name, file->name, 7));
		card_init[slot] = 0;
		CARD_Unmount(slot);
	}
	return 0;
}

s32 deviceHandler_CARD_deleteFile(file_handle* file) {
	int slot = (!strncmp((const char*)initial_CARDB.name, file->name, 7));
	char *filename = getRelativeName(file->name);
	card_dir* cd = (card_dir*)&file->other;
	
	if(cd->company[0] == '\0' && cd->gamecode[0] == '\0') {
		// Find the file we don't know about and populate this file_handle if we find it.
		if(!findFile(file)) {
			return CARD_ERROR_NOFILE;
		}
		CARD_SetCompany((const char*)cd->company);
		CARD_SetGamecode((const char*)cd->gamecode);
	}
	else {
		CARD_SetCompany((const char*)cd->company);
		CARD_SetGamecode((const char*)cd->gamecode);
	}
	print_gecko("Borrando: %s de ranura %i\r\n", filename, slot);
	
	int ret = CARD_Delete(slot, filename);
	if(ret != CARD_ERROR_READY) {
		uiDrawObj_t *msgBox = DrawMessageBox(D_FAIL,cardError(ret));
		DrawPublish(msgBox);
		wait_press_A();
		DrawDispose(msgBox);
	}
	return ret;
}

s32 deviceHandler_CARD_renameFile(file_handle* file, char* name) {
	return 0;	// TODO Implement
}

s32 deviceHandler_CARD_closeFile(file_handle* file) {
    return 0;
}

bool deviceHandler_CARD_test_a() {
	s32 memSize = 0, sectSize = 0;
	return ((initialize_card(0)==CARD_ERROR_READY) && (CARD_ProbeEx(0, &memSize,&sectSize)==CARD_ERROR_READY));
}

bool deviceHandler_CARD_test_b() {
	s32 memSize = 0, sectSize = 0;
	return ((initialize_card(1)==CARD_ERROR_READY) && (CARD_ProbeEx(1, &memSize,&sectSize)==CARD_ERROR_READY));
}

char* deviceHandler_CARD_status(file_handle* file) {
	if(file->status) {
		return cardError(file->status);
	}
	return NULL;
}

DEVICEHANDLER_INTERFACE __device_card_a = {
	.deviceUniqueId = DEVICE_ID_5,
	.hwName = "Memory Card",
	.deviceName = "Memory Card - R. A",
	.deviceDescription = "Guarda & Restaura partidas",
	.deviceTexture = {TEX_MEMCARD, 76, 84, 80, 92},
	.features = FEAT_READ|FEAT_WRITE|FEAT_BOOT_DEVICE,
	.location = LOC_MEMCARD_SLOT_A,
	.initial = &initial_CARDA,
	.test = deviceHandler_CARD_test_a,
	.info = deviceHandler_CARD_info,
	.init = deviceHandler_CARD_init,
	.readDir = deviceHandler_CARD_readDir,
	.seekFile = deviceHandler_CARD_seekFile,
	.readFile = deviceHandler_CARD_readFile,
	.writeFile = deviceHandler_CARD_writeFile,
	.closeFile = deviceHandler_CARD_closeFile,
	.deleteFile = deviceHandler_CARD_deleteFile,
	.renameFile = deviceHandler_CARD_renameFile,
	.deinit = deviceHandler_CARD_deinit,
	.status = deviceHandler_CARD_status,
};

DEVICEHANDLER_INTERFACE __device_card_b = {
	.deviceUniqueId = DEVICE_ID_6,
	.hwName = "Memory Card",
	.deviceName = "Memory Card - R. B",
	.deviceDescription = "Guarda & Restaura partidas",
	.deviceTexture = {TEX_MEMCARD, 76, 84, 80, 92},
	.features = FEAT_READ|FEAT_WRITE|FEAT_BOOT_DEVICE,
	.location = LOC_MEMCARD_SLOT_B,
	.initial = &initial_CARDB,
	.test = deviceHandler_CARD_test_b,
	.info = deviceHandler_CARD_info,
	.init = deviceHandler_CARD_init,
	.readDir = deviceHandler_CARD_readDir,
	.seekFile = deviceHandler_CARD_seekFile,
	.readFile = deviceHandler_CARD_readFile,
	.writeFile = deviceHandler_CARD_writeFile,
	.closeFile = deviceHandler_CARD_closeFile,
	.deleteFile = deviceHandler_CARD_deleteFile,
	.renameFile = deviceHandler_CARD_renameFile,
	.deinit = deviceHandler_CARD_deinit,
	.status = deviceHandler_CARD_status,
};
