/* deviceHandler-FAT.c
	- device implementation for FAT (via SD Card Adapters/IDE-EXI)
	by emu_kidid
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <ogc/dvd.h>
#include <ogc/machine/processor.h>
#include <sdcard/card_cmn.h>
#include <sdcard/card_io.h>
#include <sdcard/gcsd.h>
#include "deviceHandler.h"
#include "gui/FrameBufferMagic.h"
#include "gui/IPLFontWrite.h"
#include "swiss.h"
#include "main.h"
#include "ata.h"
#include "patcher.h"
#include "dvd.h"

static FATFS *fs[6] = {NULL, NULL, NULL, NULL, NULL, NULL};
#define SD_COUNT 3
#define IS_SDCARD(file) (file->name[0] == 's' && file->name[1] == 'd')
int GET_SLOT(file_handle* file) {
	if(IS_SDCARD(file)) {
		return file->name[2] - 'a';
	}
	// IDE-EXI
	return file->name[3] - 'a';
}

file_handle initial_SD_A =
	{ "sda:/",       // directory
	  0ULL,     // fileBase (u64)
	  0,        // offset
	  0,        // size
	  IS_DIR,
	  0,
	  0
	};
	
file_handle initial_SD_B =
	{ "sdb:/",       // directory
	  0ULL,     // fileBase (u64)
	  0,        // offset
	  0,        // size
	  IS_DIR,
	  0,
	  0
	};
	
file_handle initial_SD_C =
	{ "sdc:/",       // directory
	  0ULL,     // fileBase (u64)
	  0,        // offset
	  0,        // size
	  IS_DIR,
	  0,
	  0
	};	
	
file_handle initial_ATA_A =
	{ "ataa:/",       // directory
	  0ULL,     // fileBase (u64)
	  0,        // offset
	  0,        // size
	  IS_DIR,
	  0,
	  0
	};
	
file_handle initial_ATA_B =
	{ "atab:/",       // directory
	  0ULL,     // fileBase (u64)
	  0,        // offset
	  0,        // size
	  IS_DIR,
	  0,
	  0
	};
	
file_handle initial_ATA_C =
	{ "atac:/",       // directory
	  0ULL,     // fileBase (u64)
	  0,        // offset
	  0,        // size
	  IS_DIR,
	  0,
	  0
	};

static device_info initial_FAT_info[FF_VOLUMES];

device_info* deviceHandler_FAT_info(file_handle* file) {
	device_info* info = NULL;
	DWORD freeClusters;
	FATFS* fatfs;
	if(f_getfree(file->name, &freeClusters, &fatfs) == FR_OK) {
		info = &initial_FAT_info[fatfs->pdrv];
		info->freeSpace = (u64)(freeClusters) * fatfs->csize * fatfs->ssize;
		info->totalSpace = (u64)(fatfs->n_fatent - 2) * fatfs->csize * fatfs->ssize;
		info->metric = true;
	}
	return info;
}

s32 deviceHandler_FAT_readDir(file_handle* ffile, file_handle** dir, u32 type) {	

	FFDIR* dp = malloc(sizeof(FFDIR));
	memset(dp, 0, sizeof(FFDIR));
	if(f_opendir(dp, ffile->name) != FR_OK) return -1;
	FILINFO entry;
	
	// Set everything up to read
	int num_entries = 1, i = 1;
	*dir = calloc(num_entries, sizeof(file_handle));
	concat_path((*dir)[0].name, ffile->name, "..");
	(*dir)[0].fileType = IS_SPECIAL;

	// Read each entry of the directory
	while( f_readdir(dp, &entry) == FR_OK && entry.fname[0] != '\0') {
		// Do we want this one?
		if((type == -1 || ((entry.fattrib & AM_DIR) ? (type==IS_DIR) : (type==IS_FILE)))) {
			// Make sure we have room for this one
			if(i == num_entries){
				++num_entries;
				*dir = reallocarray(*dir, num_entries, sizeof(file_handle));
			}
			memset(&(*dir)[i], 0, sizeof(file_handle));
			if(concat_path((*dir)[i].name, ffile->name, entry.fname) < PATHNAME_MAX && entry.fsize <= UINT32_MAX) {
				(*dir)[i].fileBase   = entry.fclust;
				(*dir)[i].size       = entry.fsize;
				(*dir)[i].fileAttrib = entry.fattrib;
				(*dir)[i].fileType   = (entry.fattrib & AM_DIR) ? IS_DIR : IS_FILE;
				++i;
			}
		}
	}
	
	f_closedir(dp);
	free(dp);
	return i;
}

s32 deviceHandler_FAT_statFile(file_handle* file) {
	FILINFO entry;
	int ret = f_stat(file->name, &entry);
	if(ret == FR_OK) {
		file->fileBase   = entry.fclust;
		file->size       = entry.fsize;
		file->fileAttrib = entry.fattrib;
		file->fileType   = (entry.fattrib & AM_DIR) ? IS_DIR : IS_FILE;
	}
	return ret;
}

s64 deviceHandler_FAT_seekFile(file_handle* file, s64 where, u32 type){
	if(type == DEVICE_HANDLER_SEEK_SET) file->offset = where;
	else if(type == DEVICE_HANDLER_SEEK_CUR) file->offset = file->offset + where;
	else if(type == DEVICE_HANDLER_SEEK_END) file->offset = file->size + where;
	return file->offset;
}

s32 deviceHandler_FAT_readFile(file_handle* file, void* buffer, u32 length) {
	if(!file->ffsFp) {
		file->ffsFp = malloc(sizeof(FIL));
		if(f_open(file->ffsFp, file->name, FA_READ) != FR_OK) {
			free(file->ffsFp);
			file->ffsFp = NULL;
			return -1;
		}
		file->fileBase = file->ffsFp->obj.sclust;
		file->size     = file->ffsFp->obj.objsize;
		file->fileType = IS_FILE;
	}
	f_lseek(file->ffsFp, file->offset);
	
	UINT bytes_read;
	if(f_read(file->ffsFp, buffer, length, &bytes_read) != FR_OK) {
		return -1;
	}
	file->offset = f_tell(file->ffsFp);
	file->size = f_size(file->ffsFp);
	return bytes_read;
}

s32 deviceHandler_FAT_writeFile(file_handle* file, const void* buffer, u32 length) {
	if(!file->ffsFp) {
		file->ffsFp = malloc(sizeof(FIL));
		if(f_open(file->ffsFp, file->name, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) {
			free(file->ffsFp);
			file->ffsFp = NULL;
			return -1;
		}
		file->fileBase = file->ffsFp->obj.sclust;
		file->size     = file->ffsFp->obj.objsize;
		file->fileType = IS_FILE;
		
		f_expand(file->ffsFp, file->offset + length, 1);
	}
	f_lseek(file->ffsFp, file->offset);
	
	UINT bytes_written;
	if(f_write(file->ffsFp, buffer, length, &bytes_written) != FR_OK) {
		return -1;
	}
	file->offset = f_tell(file->ffsFp);
	file->size = f_size(file->ffsFp);
	return bytes_written;
}

s32 deviceHandler_FAT_setupFile(file_handle* file, file_handle* file2, ExecutableFile* filesToPatch, int numToPatch) {
	int i;
	file_frag *fragList = NULL;
	u32 numFrags = 0;
	
	if(numToPatch < 0) {
		return 0;
	}
	// Look for patch files, if we find some, open them and add them as fragments
	file_handle patchFile;
	for(i = 0; i < numToPatch; i++) {
		if(!filesToPatch[i].patchFile) continue;
		if(!getFragments(DEVICE_CUR, filesToPatch[i].patchFile, &fragList, &numFrags, filesToPatch[i].file == file2, filesToPatch[i].offset, filesToPatch[i].size)) {
			free(fragList);
			return 0;
		}
	}
	
	// If disc 1 is fragmented, make a note of the fragments and their sizes
	if(!getFragments(DEVICE_CUR, file, &fragList, &numFrags, FRAGS_DISC_1, 0, UINT32_MAX)) {
		free(fragList);
		return 0;
	}
	
	// If there is a disc 2 and it's fragmented, make a note of the fragments and their sizes
	if(file2) {
		if(!getFragments(DEVICE_CUR, file2, &fragList, &numFrags, FRAGS_DISC_2, 0, UINT32_MAX)) {
			free(fragList);
			return 0;
		}
	}
	
	if(swissSettings.igrType == IGR_APPLOADER || endsWith(file->name,".tgc")) {
		memset(&patchFile, 0, sizeof(file_handle));
		concat_path(patchFile.name, devices[DEVICE_CUR]->initial->name, "swiss/patches/apploader.img");
		
		getFragments(DEVICE_CUR, &patchFile, &fragList, &numFrags, FRAGS_APPLOADER, 0x2440, 0);
		devices[DEVICE_CUR]->closeFile(&patchFile);
	}
	
	if(swissSettings.emulateMemoryCard) {
		if(devices[DEVICE_CUR] != &__device_sd_a && devices[DEVICE_CUR] != &__device_ata_a && devices[DEVICE_CUR] != &__device_ata_c) {
			memset(&patchFile, 0, sizeof(file_handle));
			concatf_path(patchFile.name, devices[DEVICE_CUR]->initial->name, "swiss/patches/MemoryCardA.%s.raw", wodeRegionToString(GCMDisk.RegionCode));
			concatf_path(txtbuffer, devices[DEVICE_CUR]->initial->name, "swiss/saves/MemoryCardA.%s.raw", wodeRegionToString(GCMDisk.RegionCode));
			ensure_path(DEVICE_CUR, "swiss/saves", NULL, false);
			devices[DEVICE_CUR]->renameFile(&patchFile, txtbuffer);	// TODO remove this in our next major release
			
			if(devices[DEVICE_CUR]->statFile(&patchFile)) {
				devices[DEVICE_CUR]->seekFile(&patchFile, 16*1024*1024, DEVICE_HANDLER_SEEK_SET);
				devices[DEVICE_CUR]->writeFile(&patchFile, NULL, 0);
				devices[DEVICE_CUR]->closeFile(&patchFile);
			}
			
			if(getFragments(DEVICE_CUR, &patchFile, &fragList, &numFrags, FRAGS_CARD_A, 0, 31.5*1024*1024))
				*(vu8*)VAR_CARD_A_ID = (patchFile.size << 3 >> 20) & 0xFC;
			devices[DEVICE_CUR]->closeFile(&patchFile);
		}
		
		if(devices[DEVICE_CUR] != &__device_sd_b && devices[DEVICE_CUR] != &__device_ata_b) {
			memset(&patchFile, 0, sizeof(file_handle));
			concatf_path(patchFile.name, devices[DEVICE_CUR]->initial->name, "swiss/patches/MemoryCardB.%s.raw", wodeRegionToString(GCMDisk.RegionCode));
			concatf_path(txtbuffer, devices[DEVICE_CUR]->initial->name, "swiss/saves/MemoryCardB.%s.raw", wodeRegionToString(GCMDisk.RegionCode));
			ensure_path(DEVICE_CUR, "swiss/saves", NULL, false);
			devices[DEVICE_CUR]->renameFile(&patchFile, txtbuffer);	// TODO remove this in our next major release
			
			if(devices[DEVICE_CUR]->statFile(&patchFile)) {
				devices[DEVICE_CUR]->seekFile(&patchFile, 16*1024*1024, DEVICE_HANDLER_SEEK_SET);
				devices[DEVICE_CUR]->writeFile(&patchFile, NULL, 0);
				devices[DEVICE_CUR]->closeFile(&patchFile);
			}
			
			if(getFragments(DEVICE_CUR, &patchFile, &fragList, &numFrags, FRAGS_CARD_B, 0, 31.5*1024*1024))
				*(vu8*)VAR_CARD_B_ID = (patchFile.size << 3 >> 20) & 0xFC;
			devices[DEVICE_CUR]->closeFile(&patchFile);
		}
	}
	
	if(fragList) {
		print_frag_list(fragList, numFrags);
		*(vu32**)VAR_FRAG_LIST = installPatch2(fragList, (numFrags + 1) * sizeof(file_frag));
		free(fragList);
		fragList = NULL;
	}
	
	s32 exi_channel, exi_device;
	if(getExiDeviceByLocation(devices[DEVICE_CUR]->location, &exi_channel, &exi_device)) {
		if(IS_SDCARD(file)) {
			exi_device = sdgecko_getDevice(exi_channel);
			// Card Type
			*(vu8*)VAR_SD_SHIFT = sdgecko_getAddressingType(exi_channel) ? 0:9;
			// Copy the actual freq
			*(vu8*)VAR_EXI_CPR = (exi_channel << 6) | ((1 << exi_device) << 3) | sdgecko_getSpeed(exi_channel);
		}
		else {
			// Is the HDD in use a 48 bit LBA supported HDD?
			*(vu8*)VAR_ATA_LBA48 = ataDriveInfo.lba48Support;
			// Copy the actual freq
			*(vu8*)VAR_EXI_CPR = (exi_channel << 6) | ((1 << exi_device) << 3) | (swissSettings.exiSpeed ? EXI_SPEED32MHZ : EXI_SPEED16MHZ);
		}
		// Device slot (0, 1 or 2)
		*(vu8*)VAR_EXI_SLOT = (*(vu8*)VAR_EXI_SLOT & 0xF0) | (((exi_device << 2) | exi_channel) & 0x0F);
		*(vu32**)VAR_EXI_REGS = ((vu32(*)[5])0xCC006800)[exi_channel];
	}
	return 1;
}

s32 fatFs_Mount(u8 devNum, char *path) {
	if(fs[devNum] != NULL) {
		print_debug("Desmontar %i devnum, ruta: %s\n", devNum, path);
		f_unmount(path);
		free(fs[devNum]);
		fs[devNum] = NULL;
	}
	fs[devNum] = (FATFS*)malloc(sizeof(FATFS));
	return f_mount(fs[devNum], path, 1);
}

void setSDGeckoSpeed(int slot, bool fast) {
	sdgecko_setSpeed(slot, fast ? EXI_SPEED32MHZ : EXI_SPEED16MHZ);
	print_debug("Velocidad SD fijada en %s\n", (fast ? "32MHz":"16MHz"));
}

s32 deviceHandler_FAT_init(file_handle* file) {
	int isSDCard = IS_SDCARD(file);
	int slot = GET_SLOT(file);
	file->status = 0;
	print_debug("Iniciar %s %i\n", (isSDCard ? "SD":"ATA"), slot);
	// SD Card - Slot A
	if(isSDCard && slot == 0) {
		setSDGeckoSpeed(slot, swissSettings.exiSpeed);
		file->status = fatFs_Mount(DEV_SDA, "sda:/");
		if(sdgecko_getSpeed(slot) < EXI_SPEED32MHZ)
			__device_sd_a.quirks |=  QUIRK_EXI_SPEED;
		else
			__device_sd_a.quirks &= ~QUIRK_EXI_SPEED;
	}
	// SD Card - Slot B
	if(isSDCard && slot == 1) {
		setSDGeckoSpeed(slot, swissSettings.exiSpeed);
		file->status = fatFs_Mount(DEV_SDB, "sdb:/");
		if(sdgecko_getSpeed(slot) < EXI_SPEED32MHZ)
			__device_sd_b.quirks |=  QUIRK_EXI_SPEED;
		else
			__device_sd_b.quirks &= ~QUIRK_EXI_SPEED;
	}
	// SD Card - SD2SP2
	if(isSDCard && slot == 2) {
		setSDGeckoSpeed(slot, swissSettings.exiSpeed);
		file->status = fatFs_Mount(DEV_SDC, "sdc:/");
		if(sdgecko_getSpeed(slot) < EXI_SPEED32MHZ)
			__device_sd_c.quirks |=  QUIRK_EXI_SPEED;
		else
			__device_sd_c.quirks &= ~QUIRK_EXI_SPEED;
	}
	// IDE-EXI - Slot A
	if(!isSDCard && slot == 0) {
		file->status = fatFs_Mount(DEV_ATAA, "ataa:/");
	}
	// IDE-EXI - Slot B
	if(!isSDCard && slot == 1) {
		file->status = fatFs_Mount(DEV_ATAB, "atab:/");
	}
	// M.2 Loader
	if(!isSDCard && slot == 2) {
		file->status = fatFs_Mount(DEV_ATAC, "atac:/");
	}
	return file->status == FR_OK ? 0 : EIO;
}

s32 deviceHandler_FAT_closeFile(file_handle* file) {
	int ret = 0;
	if(file && file->ffsFp) {
		ret = f_close(file->ffsFp);
		free(file->ffsFp);
		file->ffsFp = NULL;
	}
	return ret;
}

s32 deviceHandler_FAT_deinit(file_handle* file) {
	deviceHandler_FAT_closeFile(file);
	if(file) {
		int isSDCard = IS_SDCARD(file);
		int slot = GET_SLOT(file);
		f_unmount(file->name);
		free(fs[isSDCard ? slot : SD_COUNT+slot]);
		fs[isSDCard ? slot : SD_COUNT+slot] = NULL;
	}
	return 0;
}

s32 deviceHandler_FAT_deleteFile(file_handle* file) {
	deviceHandler_FAT_closeFile(file);
	return f_unlink(file->name);
}

s32 deviceHandler_FAT_renameFile(file_handle* file, char* name) {
	deviceHandler_FAT_closeFile(file);
	int ret = f_rename(file->name, name);
	if(ret == FR_OK || ret == FR_NO_FILE || ret == FR_NO_PATH)
		strcpy(file->name, name);
	return ret;
}

s32 deviceHandler_FAT_hideFile(file_handle* file, bool hide) {
	FILINFO entry;
	int ret = f_stat(file->name, &entry);
	if(ret == FR_OK && !!(entry.fattrib & AM_HID) ^ hide)
		ret = f_chmod(file->name, hide ? AM_HID : 0, AM_HID);
	return ret;
}

s32 deviceHandler_FAT_makeDir(file_handle* dir) {
	return f_mkdir(dir->name);
}

bool deviceHandler_FAT_test_sd_a() {
	bool ret = sdgecko_isInitialized(0) || (__io_gcsda.startup(&__io_gcsda) && __io_gcsda.shutdown(&__io_gcsda));

	if (__io_gcsda.features & FEATURE_GAMECUBE_SLOTA) {
		__device_sd_a.deviceName = "SD - Ranura A";
		__device_sd_a.location = LOC_MEMCARD_SLOT_A;
	} else if (__io_gcsda.features & FEATURE_GAMECUBE_PORT1) {
		__device_sd_a.deviceName = "SD - SD2SP1";
		__device_sd_a.location = LOC_SERIAL_PORT_1;
	}
	return ret;
}
bool deviceHandler_FAT_test_sd_b() {
	bool ret = sdgecko_isInitialized(1) || (__io_gcsdb.startup(&__io_gcsdb) && __io_gcsdb.shutdown(&__io_gcsdb));

	if (ret) {
		if (sdgecko_getTransferMode(1) == CARDIO_TRANSFER_DMA)
			__device_sd_b.hwName = "Adaptador SD Semi-Pasivo";
		else if (sdgecko_getDevice(1) == EXI_DEVICE_0)
			__device_sd_b.hwName = "Adaptador SD Pasivo";
		else
			__device_sd_b.hwName = "ETH2GC Netcard+";
	}
	return ret;
}
bool deviceHandler_FAT_test_sd_c() {
	bool ret = sdgecko_isInitialized(2) || (__io_gcsd2.startup(&__io_gcsd2) && __io_gcsd2.shutdown(&__io_gcsd2));

	if (ret) {
		if (sdgecko_getTransferMode(2) == CARDIO_TRANSFER_DMA)
			__device_sd_c.hwName = "Adaptador SD Semi-Pasivo";
		else if (sdgecko_getDevice(2) == EXI_DEVICE_0)
			__device_sd_c.hwName = "Adaptador SD Pasivo";
		else
			__device_sd_c.hwName = "ETH2GC Sidecar+";
	}
	return ret;
}
bool deviceHandler_FAT_test_ata_a() {
	return ide_exi_inserted(0);
}
bool deviceHandler_FAT_test_ata_b() {
	return ide_exi_inserted(1);
}
bool deviceHandler_FAT_test_ata_c() {
	return ide_exi_inserted(2);
}

u32 deviceHandler_FAT_emulated_sd() {
	if ((swissSettings.emulateAudioStream == 1 && swissSettings.audioStreaming) ||
		swissSettings.emulateAudioStream > 1)
		return EMU_READ | EMU_AUDIO_STREAMING | EMU_BUS_ARBITER;
	else if (swissSettings.emulateReadSpeed)
		return EMU_READ | EMU_READ_SPEED | EMU_BUS_ARBITER;
	else if (swissSettings.emulateEthernet && (devices[DEVICE_CUR]->emulable & EMU_ETHERNET))
		return EMU_READ | EMU_ETHERNET | EMU_BUS_ARBITER | EMU_NO_PAUSING;
	else if (swissSettings.emulateMemoryCard)
		return EMU_READ | EMU_MEMCARD | EMU_BUS_ARBITER;
	else
		return EMU_READ | EMU_BUS_ARBITER;
}

u32 deviceHandler_FAT_emulated_ata() {
	if ((swissSettings.emulateAudioStream == 1 && swissSettings.audioStreaming) ||
		swissSettings.emulateAudioStream > 1)
		return EMU_READ | EMU_AUDIO_STREAMING | EMU_BUS_ARBITER;
	else if (swissSettings.emulateEthernet && (devices[DEVICE_CUR]->emulable & EMU_ETHERNET))
		return EMU_READ | EMU_ETHERNET | EMU_BUS_ARBITER | EMU_NO_PAUSING;
	else if (swissSettings.emulateMemoryCard)
		return EMU_READ | EMU_MEMCARD | EMU_BUS_ARBITER;
	else
		return EMU_READ | EMU_BUS_ARBITER;
}

char* deviceHandler_FAT_status(file_handle* file) {
	switch(file->status) {
		case FR_OK:			/* (0) Function succeeded */
			return NULL;
		case FR_DISK_ERR:
			return "Error en la capa de E/S de bajo nivel del disco";
		case FR_INT_ERR:
			return "Asercion fallo";
		case FR_NOT_READY:
			return "La unidad fisica no puede funcionar";
		case FR_NO_FILE:
			return "Imposible encontrar el archivo";
		case FR_NO_PATH:
			return "Imposible encontrar la ruta";
		case FR_INVALID_NAME:
			return "Nombre de la ruta invalido";
		case FR_DENIED:
			return "Acceso denegado por  acceso prohibido o directorio lleno";
		case FR_EXIST:
			return "Acceso denegado por  acceso prohibido";
		case FR_INVALID_OBJECT:
			return "Objeto de archivo/directorio invalido";
		case FR_WRITE_PROTECTED:
			return "Unidad fisica protegida contra escritura";
		case FR_INVALID_DRIVE:
			return "Numero de unidad logica invalido";
		case FR_NOT_ENABLED:
			return "El volumen no tiene area de trabajo";
		case FR_NO_FILESYSTEM:
			return "No hay un volumen FAT valido";
		case FR_MKFS_ABORTED:
			return "f_mkfs() abortado por cualquier problema";
		case FR_TIMEOUT:
			return "Imposible acceder al volumen dentro del periodo definido";
		case FR_LOCKED:
			return "Operacion cancelada por la politica de intercambio de archivos";
		case FR_NOT_ENOUGH_CORE:
			return "Imposible asignar buffer de trabajo LFN";
		case FR_TOO_MANY_OPEN_FILES:
			return "Numero de archivos abiertos > FF_FS_LOCK";
		case FR_INVALID_PARAMETER:
			return "El parametro dado no es valido";
		default:
			return "Ocurrio un error desconocido";
	}
}

static float exiSpeeds[] = {
	27.0f/32.0f,
	27.0f/16.0f,
	27.0f/8.0f,
	27.0f/4.0f,
	27.0f/2.0f,
	27.0f/1.0f
};

char* deviceHandler_FAT_details(file_handle* file) {
	char* deviceDetails = NULL;
	int isSDCard = IS_SDCARD(file);
	int slot = GET_SLOT(file);
	if(isSDCard) {
		off_t c_size;
		char c_size_str[26 + 1];
		if(sdgecko_readCID(slot) != CARDIO_ERROR_READY) return NULL;
		switch(CSD_STRUCTURE(slot)) {
			case 0:
				c_size = ((C_SIZE(slot) + SIZE_OF_PROTECTED_AREA(slot) + 1LL) << (C_SIZE_MULT(slot) + 2)) << READ_BL_LEN(slot);
				break;
			case 1:
				c_size = ((C_SIZE1(slot) + 1LL) << (READ_BL_LEN(slot) + 10)) + SIZE_OF_PROTECTED_AREA(slot);
				break;
			case 2:
				c_size = ((C_SIZE2(slot) + 1LL) << (READ_BL_LEN(slot) + 10)) + SIZE_OF_PROTECTED_AREA(slot);
				break;
			default:
				c_size = 0;
				break;
		}
		asprintf(&deviceDetails,
			"ID Fabricante: %02X\n"
			"ID OEM/Aplicacion: %.2s\n"
			"Nombre Producto: %.5s\n"
			"Revision: %u.%u\n"
			"Numero de Serie: %08X\n"
			"Fecha Fabricacion: %u-%02u\n"
			"\n"
			"Modo Vel. Bus: %s (%.3g MHz)\n"
			"Capacidad: %.*s\n"
			"Clase Velocidad: C%u\n"
			"Clase Velocidad UHS: U%u\n"
			"Clase Velocidad Video: V%u\n"
			"Clase Rendimiento Aplicacion: A%u\n"
			"Las inconsistencias pueden indicar una SD Falsa.",
			MANUFACTURER_ID(slot),
			OEM_APPLICATION_ID(slot),
			PRODUCT_NAME(slot),
			PRODUCT_REVISION(slot) >> 4, PRODUCT_REVISION(slot) & 0xF,
			PRODUCT_SERIAL_NUMBER(slot),
			2000 + (MANUFACTURING_DATE(slot) >> 4), MANUFACTURING_DATE(slot) & 0xF,
			TRAN_SPEED(slot) == 0x5A ? "Alta Velocidad" : "Velocidad Por Def.",
			exiSpeeds[sdgecko_getSpeed(slot)],
			formatBytes(c_size_str, c_size, 0, true), c_size_str,
			SPEED_CLASS(slot) < 4 ? SPEED_CLASS(slot) * 2 : 10,
			UHS_SPEED_GRADE(slot),
			VIDEO_SPEED_CLASS(slot),
			APP_PERF_CLASS(slot)
		);
	}
	return deviceDetails;
}

DEVICEHANDLER_INTERFACE __device_sd_a = {
	.deviceUniqueId = DEVICE_ID_1,
	.hwName = "Adaptador SD",
	.deviceName = "SD - Ranura A",
	.deviceDescription = "SD(HC/XC) - Formato(s) soportado(s): FAT16, FAT32, exFAT",
	.deviceTexture = {TEX_SDSMALL, 59, 78, 64, 80},
	.features = FEAT_READ|FEAT_WRITE|FEAT_BOOT_GCM|FEAT_BOOT_DEVICE|FEAT_CONFIG_DEVICE|FEAT_AUTOLOAD_DOL|FEAT_THREAD_SAFE|FEAT_HYPERVISOR|FEAT_PATCHES|FEAT_AUDIO_STREAMING|FEAT_EXI_SPEED,
	.emulable = EMU_READ|EMU_READ_SPEED|EMU_AUDIO_STREAMING|EMU_MEMCARD,
	.location = LOC_MEMCARD_SLOT_A,
	.initial = &initial_SD_A,
	.test = deviceHandler_FAT_test_sd_a,
	.info = deviceHandler_FAT_info,
	.init = deviceHandler_FAT_init,
	.makeDir = deviceHandler_FAT_makeDir,
	.readDir = deviceHandler_FAT_readDir,
	.statFile = deviceHandler_FAT_statFile,
	.seekFile = deviceHandler_FAT_seekFile,
	.readFile = deviceHandler_FAT_readFile,
	.writeFile = deviceHandler_FAT_writeFile,
	.closeFile = deviceHandler_FAT_closeFile,
	.deleteFile = deviceHandler_FAT_deleteFile,
	.renameFile = deviceHandler_FAT_renameFile,
	.hideFile = deviceHandler_FAT_hideFile,
	.setupFile = deviceHandler_FAT_setupFile,
	.deinit = deviceHandler_FAT_deinit,
	.emulated = deviceHandler_FAT_emulated_sd,
	.status = deviceHandler_FAT_status,
	.details = deviceHandler_FAT_details,
};

DEVICEHANDLER_INTERFACE __device_sd_b = {
	.deviceUniqueId = DEVICE_ID_2,
	.hwName = "Adaptador SD",
	.deviceName = "SD - Ranura B",
	.deviceDescription = "SD(HC/XC) - Formato(s) soportado(s): FAT16, FAT32, exFAT",
	.deviceTexture = {TEX_SDSMALL, 59, 78, 64, 80},
	.features = FEAT_READ|FEAT_WRITE|FEAT_BOOT_GCM|FEAT_BOOT_DEVICE|FEAT_CONFIG_DEVICE|FEAT_AUTOLOAD_DOL|FEAT_THREAD_SAFE|FEAT_HYPERVISOR|FEAT_PATCHES|FEAT_AUDIO_STREAMING|FEAT_EXI_SPEED,
	.emulable = EMU_READ|EMU_READ_SPEED|EMU_AUDIO_STREAMING|EMU_MEMCARD,
	.location = LOC_MEMCARD_SLOT_B,
	.initial = &initial_SD_B,
	.test = deviceHandler_FAT_test_sd_b,
	.info = deviceHandler_FAT_info,
	.init = deviceHandler_FAT_init,
	.makeDir = deviceHandler_FAT_makeDir,
	.readDir = deviceHandler_FAT_readDir,
	.statFile = deviceHandler_FAT_statFile,
	.seekFile = deviceHandler_FAT_seekFile,
	.readFile = deviceHandler_FAT_readFile,
	.writeFile = deviceHandler_FAT_writeFile,
	.closeFile = deviceHandler_FAT_closeFile,
	.deleteFile = deviceHandler_FAT_deleteFile,
	.renameFile = deviceHandler_FAT_renameFile,
	.hideFile = deviceHandler_FAT_hideFile,
	.setupFile = deviceHandler_FAT_setupFile,
	.deinit = deviceHandler_FAT_deinit,
	.emulated = deviceHandler_FAT_emulated_sd,
	.status = deviceHandler_FAT_status,
	.details = deviceHandler_FAT_details,
};

DEVICEHANDLER_INTERFACE __device_ata_a = {
	.deviceUniqueId = DEVICE_ID_3,
	.hwName = "IDE-EXI",
	.deviceName = "IDE - Ranura A",
	.deviceDescription = "IDE/PATA HDD - Formato(s) soportado(s): FAT16, FAT32, exFAT",
	.deviceTexture = {TEX_HDD, 104, 73, 104, 76},
	.features = FEAT_READ|FEAT_WRITE|FEAT_BOOT_GCM|FEAT_BOOT_DEVICE|FEAT_CONFIG_DEVICE|FEAT_AUTOLOAD_DOL|FEAT_THREAD_SAFE|FEAT_HYPERVISOR|FEAT_PATCHES|FEAT_AUDIO_STREAMING|FEAT_EXI_SPEED,
	.emulable = EMU_READ|EMU_AUDIO_STREAMING|EMU_MEMCARD,
	.location = LOC_MEMCARD_SLOT_A,
	.initial = &initial_ATA_A,
	.test = deviceHandler_FAT_test_ata_a,
	.info = deviceHandler_FAT_info,
	.init = deviceHandler_FAT_init,
	.makeDir = deviceHandler_FAT_makeDir,
	.readDir = deviceHandler_FAT_readDir,
	.statFile = deviceHandler_FAT_statFile,
	.seekFile = deviceHandler_FAT_seekFile,
	.readFile = deviceHandler_FAT_readFile,
	.writeFile = deviceHandler_FAT_writeFile,
	.closeFile = deviceHandler_FAT_closeFile,
	.deleteFile = deviceHandler_FAT_deleteFile,
	.renameFile = deviceHandler_FAT_renameFile,
	.hideFile = deviceHandler_FAT_hideFile,
	.setupFile = deviceHandler_FAT_setupFile,
	.deinit = deviceHandler_FAT_deinit,
	.emulated = deviceHandler_FAT_emulated_ata,
	.status = deviceHandler_FAT_status,
};

DEVICEHANDLER_INTERFACE __device_ata_b = {
	.deviceUniqueId = DEVICE_ID_4,
	.hwName = "IDE-EXI",
	.deviceName = "IDE - Ranura B",
	.deviceDescription = "IDE/PATA HDD - Formato(s) soportado(s): FAT16, FAT32, exFAT",
	.deviceTexture = {TEX_HDD, 104, 73, 104, 76},
	.features = FEAT_READ|FEAT_WRITE|FEAT_BOOT_GCM|FEAT_BOOT_DEVICE|FEAT_CONFIG_DEVICE|FEAT_AUTOLOAD_DOL|FEAT_THREAD_SAFE|FEAT_HYPERVISOR|FEAT_PATCHES|FEAT_AUDIO_STREAMING|FEAT_EXI_SPEED,
	.emulable = EMU_READ|EMU_AUDIO_STREAMING|EMU_MEMCARD,
	.location = LOC_MEMCARD_SLOT_B,
	.initial = &initial_ATA_B,
	.test = deviceHandler_FAT_test_ata_b,
	.info = deviceHandler_FAT_info,
	.init = deviceHandler_FAT_init,
	.makeDir = deviceHandler_FAT_makeDir,
	.readDir = deviceHandler_FAT_readDir,
	.statFile = deviceHandler_FAT_statFile,
	.seekFile = deviceHandler_FAT_seekFile,
	.readFile = deviceHandler_FAT_readFile,
	.writeFile = deviceHandler_FAT_writeFile,
	.closeFile = deviceHandler_FAT_closeFile,
	.deleteFile = deviceHandler_FAT_deleteFile,
	.renameFile = deviceHandler_FAT_renameFile,
	.hideFile = deviceHandler_FAT_hideFile,
	.setupFile = deviceHandler_FAT_setupFile,
	.deinit = deviceHandler_FAT_deinit,
	.emulated = deviceHandler_FAT_emulated_ata,
	.status = deviceHandler_FAT_status,
};

DEVICEHANDLER_INTERFACE __device_sd_c = {
	.deviceUniqueId = DEVICE_ID_F,
	.hwName = "Adaptador SD",
	.deviceName = "SD - SD2SP2",
	.deviceDescription = "SD(HC/XC) - Formato(s) soportado(s): FAT16, FAT32, exFAT",
	.deviceTexture = {TEX_SDSMALL, 59, 78, 64, 80},
	.features = FEAT_READ|FEAT_WRITE|FEAT_BOOT_GCM|FEAT_BOOT_DEVICE|FEAT_CONFIG_DEVICE|FEAT_AUTOLOAD_DOL|FEAT_THREAD_SAFE|FEAT_HYPERVISOR|FEAT_PATCHES|FEAT_AUDIO_STREAMING|FEAT_EXI_SPEED,
	.emulable = EMU_READ|EMU_READ_SPEED|EMU_AUDIO_STREAMING|EMU_MEMCARD,
	.location = LOC_SERIAL_PORT_2,
	.initial = &initial_SD_C,
	.test = deviceHandler_FAT_test_sd_c,
	.info = deviceHandler_FAT_info,
	.init = deviceHandler_FAT_init,
	.makeDir = deviceHandler_FAT_makeDir,
	.readDir = deviceHandler_FAT_readDir,
	.statFile = deviceHandler_FAT_statFile,
	.seekFile = deviceHandler_FAT_seekFile,
	.readFile = deviceHandler_FAT_readFile,
	.writeFile = deviceHandler_FAT_writeFile,
	.closeFile = deviceHandler_FAT_closeFile,
	.deleteFile = deviceHandler_FAT_deleteFile,
	.renameFile = deviceHandler_FAT_renameFile,
	.hideFile = deviceHandler_FAT_hideFile,
	.setupFile = deviceHandler_FAT_setupFile,
	.deinit = deviceHandler_FAT_deinit,
	.emulated = deviceHandler_FAT_emulated_sd,
	.status = deviceHandler_FAT_status,
	.details = deviceHandler_FAT_details,
};

DEVICEHANDLER_INTERFACE __device_ata_c = {
	.deviceUniqueId = DEVICE_ID_H,
	.hwName = "M.2 Loader",
	.deviceName = "M.2 Loader",
	.deviceDescription = "M.2 SATA SSD - Formato(s) soportado(s): FAT16, FAT32, exFAT",
	.deviceTexture = {TEX_M2LOADER, 112, 54, 112, 56},
	.features = FEAT_READ|FEAT_WRITE|FEAT_BOOT_GCM|FEAT_BOOT_DEVICE|FEAT_CONFIG_DEVICE|FEAT_AUTOLOAD_DOL|FEAT_THREAD_SAFE|FEAT_HYPERVISOR|FEAT_PATCHES|FEAT_AUDIO_STREAMING|FEAT_EXI_SPEED,
	.emulable = EMU_READ|EMU_READ_SPEED|EMU_AUDIO_STREAMING|EMU_MEMCARD,
	.location = LOC_SERIAL_PORT_1,
	.initial = &initial_ATA_C,
	.test = deviceHandler_FAT_test_ata_c,
	.info = deviceHandler_FAT_info,
	.init = deviceHandler_FAT_init,
	.makeDir = deviceHandler_FAT_makeDir,
	.readDir = deviceHandler_FAT_readDir,
	.statFile = deviceHandler_FAT_statFile,
	.seekFile = deviceHandler_FAT_seekFile,
	.readFile = deviceHandler_FAT_readFile,
	.writeFile = deviceHandler_FAT_writeFile,
	.closeFile = deviceHandler_FAT_closeFile,
	.deleteFile = deviceHandler_FAT_deleteFile,
	.renameFile = deviceHandler_FAT_renameFile,
	.hideFile = deviceHandler_FAT_hideFile,
	.setupFile = deviceHandler_FAT_setupFile,
	.deinit = deviceHandler_FAT_deinit,
	.emulated = deviceHandler_FAT_emulated_sd,
	.status = deviceHandler_FAT_status,
};
