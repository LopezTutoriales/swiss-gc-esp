#include <argz.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>
#include "deviceHandler.h"
#include "FrameBufferMagic.h"
#include "IPLFontWrite.h"
#include "swiss.h"
#include "main.h"
#include "config.h"
#include "settings.h"

// This is an example Swiss settings entry (sits at the top of global.ini)
//!!Swiss Settings Start!!
//SD/IDE Speed=32MHz
//Swiss Video Mode=Auto
//Enable Debug=No
//Force No DVD Drive Mode=No
//Hide Unknown file types=No
//Stop DVD Motor on startup=Yes
//Enable WiiRD debug=Yes
//Enable File Management=No
//SMBUserName=user
//SMBPassword=password
//SMBShareName=nas
//SMBHostIP=192.168.1.32
//AutoCheats=Yes
//InitNetwork=No
//IGRType=Disabled
//FTPUserName=user
//FTPPassword=password
//FTPHostIP=192.168.1.32
//FTPPort=21
//FTPUsePasv=No
//!!Swiss Settings End!!

// This is an example game entry
//ID=GAFE
//Name=Animal Crossing (NTSC)
//Comment=Playable without issues
//Status=Working
//Force Video Mode=Progressive
//Mute Audio Streaming=Yes
//No Disc Mode=Yes
//Force Widescreen=Yes

#define SWISS_SETTINGS_FILENAME_LEGACY "swiss.ini"
#define SWISS_SETTINGS_FILENAME "global.ini"
#define SWISS_RECENTLIST_FILENAME "recent.ini"
#define SWISS_BASE_DIR "swiss"
#define SWISS_SETTINGS_DIR "swiss/settings"
#define SWISS_GAME_SETTINGS_DIR "swiss/settings/game"

// Tries to init the current config device
bool config_set_device() {
	// Set the current config device to whatever the current configDeviceId is
	DEVICEHANDLER_INTERFACE *configDevice = getDeviceByUniqueId(swissSettings.configDeviceId);
	devices[DEVICE_CONFIG] = NULL;
	if(configDevice != NULL) {
		if(configDevice->test()) {
			deviceHandler_setDeviceAvailable(configDevice, true);
			devices[DEVICE_CONFIG] = configDevice;
		}
	}
	
	// Not available or not a writable device? That's too bad.
	if(devices[DEVICE_CONFIG] == NULL) {
		return false;
	}
	//print_gecko("Save device is %s\r\n", devices[DEVICE_CONFIG]->deviceName);
	deviceHandler_setStatEnabled(0);
	// If we're not using this device already, init it.
	if(devices[DEVICE_CONFIG] != devices[DEVICE_CUR]) {
		print_gecko("Dispositivo de guardado no es el actual, el actual es (%s)\r\n", devices[DEVICE_CUR] == NULL ? "NULO":devices[DEVICE_CUR]->deviceName);
		if(devices[DEVICE_CONFIG]->init(devices[DEVICE_CONFIG]->initial)) {
			print_gecko("Fallo al iniciar dispositivo de guardado\r\n");
			deviceHandler_setStatEnabled(1);
			return false;
		}
	}
	deviceHandler_setStatEnabled(1);
	return true;
}

void config_unset_device() {
	if(devices[DEVICE_CONFIG] != devices[DEVICE_CUR]) {
		devices[DEVICE_CONFIG]->deinit(devices[DEVICE_CONFIG]->initial);
	}
}

// Reads from a file and returns a populated buffer, NULL if anything goes wrong.
char* config_file_read(char* filename) {
	char* readBuffer = NULL;
	file_handle *configFile = (file_handle*)calloc(1, sizeof(file_handle));
	concat_path(configFile->name, devices[DEVICE_CONFIG]->initial->name, filename);
	print_gecko("config_file_read: buscando %s\r\n", configFile->name);
	if(devices[DEVICE_CONFIG]->readFile(configFile, NULL, 0) == 0) {
		readBuffer = (char*)calloc(1, configFile->size + 1);
		if (readBuffer) {
			print_gecko("config_file_read: leyendo archivo de %i byte\r\n", configFile->size);
			devices[DEVICE_CONFIG]->readFile(configFile, readBuffer, configFile->size);
		}
	}
	devices[DEVICE_CONFIG]->closeFile(configFile);
	free(configFile);
	return readBuffer;
}

int config_file_write(char* filename, char* contents) {
	file_handle *configFile = (file_handle*)calloc(1, sizeof(file_handle));
	concat_path(configFile->name, devices[DEVICE_CONFIG]->initial->name, filename);

	u32 len = strlen(contents);
	print_gecko("config_file_write: escribiendo %i bytes en %s\r\n", len, configFile->name);
	devices[DEVICE_CONFIG]->deleteFile(configFile);
	if(devices[DEVICE_CONFIG]->writeFile(configFile, contents, len) == len &&
		!devices[DEVICE_CONFIG]->closeFile(configFile)) {
		free(configFile);
		return 1;
	}
	devices[DEVICE_CONFIG]->closeFile(configFile);
	free(configFile);
	return 0;
}

void config_file_delete(char* filename) {
	file_handle *configFile = (file_handle*)calloc(1, sizeof(file_handle));
	concat_path(configFile->name, devices[DEVICE_CONFIG]->initial->name, filename);
	print_gecko("config_file_delete: borrando %s\r\n", configFile->name);
	devices[DEVICE_CONFIG]->deleteFile(configFile);
	free(configFile);
}

int config_update_global(bool checkConfigDevice) {
	if(checkConfigDevice && !config_set_device()) return 0;

	char *configString = NULL;
	size_t len = 0;
	FILE *fp = open_memstream(&configString, &len);
	if(!fp) return 0;

	// Write out Swiss settings
	fprintf(fp, "# Archivo de configuracion de Swiss!\r\n# Todo lo escrito aqui se perdera!\r\n# Traducido por Lopez Tutoriales\r\n#!!Inicio de configuracion de Swiss!!\r\n");
	fprintf(fp, "Velocidad de SD/IDE=%s\r\n", swissSettings.exiSpeed ? "32MHz":"16MHz");
	fprintf(fp, "Modo de Video de Swiss=%s\r\n", uiVModeStr[swissSettings.uiVMode]);
	fprintf(fp, "Activar Depuracion USB Gecko=%s\r\n", swissSettings.debugUSB ? "Si":"No");
	fprintf(fp, "Ocultar tipo de archivo desconocido=%s\r\n", swissSettings.hideUnknownFileTypes ? "Si":"No");
	fprintf(fp, "Parar motor de DVD al encender=%s\r\n", swissSettings.stopMotor ? "Si":"No");
	fprintf(fp, "Activar Depuracion WiiRD=%s\r\n", swissSettings.wiirdDebug ? "Si":"No");
	fprintf(fp, "Activar Gestion de Archivos=%s\r\n", swissSettings.enableFileManagement ? "Si":"No");
	fprintf(fp, "Desactivar GameID MemCard PRO=%s\r\n", disableMCPGameIDStr[swissSettings.disableMCPGameID]);
	fprintf(fp, "Desactivar Parches de Video=%s\r\n", disableVideoPatchesStr[swissSettings.disableVideoPatches]);
	fprintf(fp, "Forzar Video Activo=%s\r\n", swissSettings.forceVideoActive ? "Si":"No");
	fprintf(fp, "Forzar Estado DTV=%s\r\n", swissSettings.forceDTVStatus ? "Si":"No");
	fprintf(fp, "Pausar para cambio de resolucion=%s\r\n", swissSettings.pauseAVOutput ? "Si":"No");
	fprintf(fp, "SMBUsuario=%s\r\n", swissSettings.smbUser);
	fprintf(fp, "SMBClave=%s\r\n", swissSettings.smbPassword);
	fprintf(fp, "SMBNombreCompartir=%s\r\n", swissSettings.smbShare);
	fprintf(fp, "SMBIPHost=%s\r\n", swissSettings.smbServerIp);
	fprintf(fp, "AutoIniciar=%s\r\n", swissSettings.autoBoot ? "Si":"No");
	fprintf(fp, "AutoTrucos=%s\r\n", swissSettings.autoCheats ? "Si":"No");
	fprintf(fp, "Iniciar Red=%s\r\n", swissSettings.initNetworkAtStart ? "Si":"No");
	fprintf(fp, "Tipo IGR=%s\r\n", igrTypeStr[swissSettings.igrType]);
	fprintf(fp, "AVECompat=%s\r\n", aveCompatStr[swissSettings.aveCompat]);
	fprintf(fp, "Tipo de Explorador de Archivos=%s\r\n", fileBrowserStr[swissSettings.fileBrowserType]);
	fprintf(fp, "Iniciar a traves de IPL=%s\r\n", bs2BootStr[swissSettings.bs2Boot]);
	fprintf(fp, "FTPUsuario=%s\r\n", swissSettings.ftpUserName);
	fprintf(fp, "FTPClave=%s\r\n", swissSettings.ftpPassword);
	fprintf(fp, "FTPIPHost=%s\r\n", swissSettings.ftpHostIp);
	fprintf(fp, "FTPPuerto=%hu\r\n", swissSettings.ftpPort);
	fprintf(fp, "FTPModoPasv=%s\r\n", swissSettings.ftpUsePasv ? "Si":"No");
	fprintf(fp, "FSPIPHost=%s\r\n", swissSettings.fspHostIp);
	fprintf(fp, "FSPPuerto=%hu\r\n", swissSettings.fspPort);
	fprintf(fp, "FSPClave=%s\r\n", swissSettings.fspPassword);
	fprintf(fp, "FSPRutaMTU=%hu\r\n", swissSettings.fspPathMtu);
	fprintf(fp, "DireccionIPv4=%s\r\n", swissSettings.bbaLocalIp);
	fprintf(fp, "MascaraRedIPv4=%hu\r\n", swissSettings.bbaNetmask);
	fprintf(fp, "PuertaEnlaceIPv4=%s\r\n", swissSettings.bbaGateway);
	fprintf(fp, "DHCPEnIPv4=%s\r\n", swissSettings.bbaUseDhcp ? "Si":"No");
	fprintf(fp, "Mostrar Archivos Ocultos=%s\r\n", swissSettings.showHiddenFiles ? "Si":"No");
	fprintf(fp, "Lista de Recientes=%s\r\n", recentListLevelStr[swissSettings.recentListLevel]);
	fprintf(fp, "VersionGCLoaderHW=%i\r\n", swissSettings.gcloaderHwVersion);
	fprintf(fp, "VersionGCLoaderTop=%s\r\n", swissSettings.gcloaderTopVersion);
	fprintf(fp, "Autocargar=%s\r\n", swissSettings.autoload);
	fprintf(fp, "Aplanar Directorio=%s\r\n", swissSettings.flattenDir);

	// Write out the default game config portion too
	fprintf(fp, "Forzar Modo de Video=%s\r\n", gameVModeStr[swissSettings.gameVMode]);
	fprintf(fp, "Forzar Escala Horizontal=%s\r\n", forceHScaleStr[swissSettings.forceHScale]);
	fprintf(fp, "Forzar Offset Vertical=%+hi\r\n", swissSettings.forceVOffset);
	fprintf(fp, "Forzar Filtro Vertical=%s\r\n", forceVFilterStr[swissSettings.forceVFilter]);
	fprintf(fp, "Forzar Renderizado de Campo=%s\r\n", forceVJitterStr[swissSettings.forceVJitter]);
	fprintf(fp, "Desactivar Tramado Alfa=%s\r\n", swissSettings.disableDithering ? "Si":"No");
	fprintf(fp, "Forzar Filtro Anisotropico=%s\r\n", swissSettings.forceAnisotropy ? "Si":"No");
	fprintf(fp, "Forzar Pantalla Ancha=%s\r\n", forceWidescreenStr[swissSettings.forceWidescreen]);
	fprintf(fp, "Forzar Tasa de Sondeo=%s\r\n", forcePollRateStr[swissSettings.forcePollRate]);
	fprintf(fp, "Invertir Stick de Camara=%s\r\n", invertCStickStr[swissSettings.invertCStick]);
	fprintf(fp, "Cambiar Stick de Camara=%s\r\n", swapCStickStr[swissSettings.swapCStick]);
	fprintf(fp, "Nivel de Trigger Digital=%hhu\r\n", swissSettings.triggerLevel);
	fprintf(fp, "Emular Streaming de Audio=%s\r\n", emulateAudioStreamStr[swissSettings.emulateAudioStream]);
	fprintf(fp, "Emular Velocidad de Lectura=%s\r\n", emulateReadSpeedStr[swissSettings.emulateReadSpeed]);
	fprintf(fp, "Emular Memory Card=%s\r\n", swissSettings.emulateMemoryCard ? "Si":"No");
	fprintf(fp, "Emular Adaptador de Red=%s\r\n", swissSettings.emulateEthernet ? "Si":"No");
	fprintf(fp, "Preferir Inicio Limpio=%s\r\n", swissSettings.preferCleanBoot ? "Si":"No");
	fprintf(fp, "#!!Fin de Configuracion de Swiss!!\r\n\r\n");
	fclose(fp);

	ensure_path(DEVICE_CONFIG, SWISS_BASE_DIR, NULL);
	ensure_path(DEVICE_CONFIG, SWISS_SETTINGS_DIR, NULL);
	
	concat_path(txtbuffer, SWISS_SETTINGS_DIR, SWISS_SETTINGS_FILENAME);
	int res = config_file_write(txtbuffer, configString);
	free(configString);
	if(checkConfigDevice) {
		config_unset_device();
	}
	return res;
}

int config_update_recent(bool checkConfigDevice) {
	if(checkConfigDevice && !config_set_device()) return 0;

	char *configString = NULL;
	size_t len = 0;
	FILE *fp = open_memstream(&configString, &len);
	if(!fp) return 0;

	fprintf(fp, "# Lista de Recientes. Creado por Swiss\r\n# Traducido por Lopez Tutoriales\r\n");
	int i;
	for(i = 0; i < RECENT_MAX; i++) {
		fprintf(fp, "Reciente_%i=%s\r\n", i, swissSettings.recent[i]);
		//print_gecko("added recent num %i [%s]\r\n", i, swissSettings.recent[i]);
	}
	fclose(fp);

	ensure_path(DEVICE_CONFIG, SWISS_BASE_DIR, NULL);
	ensure_path(DEVICE_CONFIG, SWISS_SETTINGS_DIR, NULL);
	
	concat_path(txtbuffer, SWISS_SETTINGS_DIR, SWISS_RECENTLIST_FILENAME);
	int res = config_file_write(txtbuffer, configString);
	free(configString);
	if(checkConfigDevice) {
		config_unset_device();
	}
	return res;
}


int config_update_game(ConfigEntry* entry, bool checkConfigDevice) {
	if(checkConfigDevice && !config_set_device()) return 0;

	char *configString = NULL;
	size_t len = 0;
	FILE *fp = open_memstream(&configString, &len);
	if(!fp) return 0;

	fprintf(fp, "# Configuracion especifica para un juego. Creado por Swiss\r\n# Traducido por Lopez Tutoriales\r\n");
	fprintf(fp, "ID=%.4s\r\n", entry->game_id);
	fprintf(fp, "Nombre=%.64s\r\n", entry->game_name);
	fprintf(fp, "Comentario=%.128s\r\n", entry->comment);
	fprintf(fp, "Estado=%.32s\r\n", entry->status);
	fprintf(fp, "Forzar Modo de Video=%s\r\n", gameVModeStr[entry->gameVMode]);
	fprintf(fp, "Forzar Escala Horizontal=%s\r\n", forceHScaleStr[entry->forceHScale]);
	fprintf(fp, "Forzar Offset Vertical=%+hi\r\n", entry->forceVOffset);
	fprintf(fp, "Forzar Filtro Vertical=%s\r\n", forceVFilterStr[entry->forceVFilter]);
	fprintf(fp, "Forzar Renderizado de Campo=%s\r\n", forceVJitterStr[entry->forceVJitter]);
	fprintf(fp, "Desactivar Tramado Alfa=%s\r\n", entry->disableDithering ? "Si":"No");
	fprintf(fp, "Forzar Filtro Anisotropico=%s\r\n", entry->forceAnisotropy ? "Si":"No");
	fprintf(fp, "Forzar Pantalla Ancha=%s\r\n", forceWidescreenStr[entry->forceWidescreen]);
	fprintf(fp, "Forzar Tasa de Sondeo=%s\r\n", forcePollRateStr[entry->forcePollRate]);
	fprintf(fp, "Invertir Stick de Camara=%s\r\n", invertCStickStr[entry->invertCStick]);
	fprintf(fp, "Cambiar Stick de Camara=%s\r\n", swapCStickStr[entry->swapCStick]);
	fprintf(fp, "Nivel de Trigger Digital=%hhu\r\n", entry->triggerLevel);
	fprintf(fp, "Emular Streaming de Audio=%s\r\n", emulateAudioStreamStr[entry->emulateAudioStream]);
	fprintf(fp, "Emular Velocidad de Lectura=%s\r\n", emulateReadSpeedStr[entry->emulateReadSpeed]);
	fprintf(fp, "Emular Adaptador de Red=%s\r\n", entry->emulateEthernet ? "Si":"No");
	fprintf(fp, "Preferir Inicio Limpio=%s\r\n", entry->preferCleanBoot ? "Si":"No");
	fclose(fp);

	ensure_path(DEVICE_CONFIG, SWISS_BASE_DIR, NULL);
	ensure_path(DEVICE_CONFIG, SWISS_SETTINGS_DIR, NULL);
	ensure_path(DEVICE_CONFIG, SWISS_GAME_SETTINGS_DIR, NULL);
	
	concatf_path(txtbuffer, SWISS_GAME_SETTINGS_DIR, "%.4s.ini", entry->game_id);
	int res = config_file_write(txtbuffer, configString);
	free(configString);
	if(checkConfigDevice) {
		config_unset_device();
	}
	return res;
}

static char emulateReadSpeedEntries[][4] = {"GQSD", "GQSE", "GQSF", "GQSI", "GQSP", "GQSS", "GTOJ"};
static char emulateEthernetEntries[][4] = {"DPSJ", "GHEJ", "GKYE", "GKYJ", "GKYP", "GM4E", "GM4J", "GM4P", "GPJJ", "GPOE", "GPOJ", "GPOP", "GPSE", "GPSJ", "GPSP", "GTEE", "GTEJ", "GTEP", "GTEW", "PHEJ"};

void config_defaults(ConfigEntry *entry) {
	strcpy(entry->comment, "Sin Comentarios");
	strcpy(entry->status, "Desconocido");
	entry->gameVMode = swissSettings.gameVMode;
	entry->forceHScale = swissSettings.forceHScale;
	entry->forceVOffset = swissSettings.forceVOffset;
	entry->forceVOffset = swissSettings.aveCompat == 1 ? -3:0;
	entry->forceVFilter = swissSettings.forceVFilter;
	entry->forceVJitter = swissSettings.forceVJitter;
	entry->disableDithering = swissSettings.disableDithering;
	entry->forceAnisotropy = swissSettings.forceAnisotropy;
	entry->forceWidescreen = swissSettings.forceWidescreen;
	entry->forcePollRate = swissSettings.forcePollRate;
	entry->invertCStick = swissSettings.invertCStick;
	entry->swapCStick = swissSettings.swapCStick;
	entry->triggerLevel = swissSettings.triggerLevel;
	entry->emulateAudioStream = swissSettings.emulateAudioStream;
	entry->emulateReadSpeed = swissSettings.emulateReadSpeed;
	entry->emulateEthernet = swissSettings.emulateEthernet;
	entry->preferCleanBoot = swissSettings.preferCleanBoot;

	for(int i = 0; i < sizeof(emulateReadSpeedEntries) / sizeof(*emulateReadSpeedEntries); i++) {
		if(!strncmp(entry->game_id, emulateReadSpeedEntries[i], 4)) {
			entry->emulateReadSpeed = 1;
			break;
		}
	}
	for(int i = 0; i < sizeof(emulateEthernetEntries) / sizeof(*emulateEthernetEntries); i++) {
		if(!strncmp(entry->game_id, emulateEthernetEntries[i], 4)) {
			entry->emulateEthernet = 1;
			break;
		}
	}
}

// TODO kill this off in one major version from now. Don't add new settings to it.
void config_parse_legacy(char *configData, void (*progress_indicator)(char*, int, int)) {
	ConfigEntry configEntries[2048]; // That's a lot of Games!
	int configEntriesCount = 0;
	// Parse each entry and put it into our array
	char *line, *linectx = NULL;
	int first = 1;
	bool defaultPassed = false;
	line = strtok_r( configData, "\r\n", &linectx );
	while( line != NULL ) {
		//print_gecko("Line [%s]\r\n", line);
		if(line[0] != '#') {
			// Is this line a new game entry?
			char *name, *value = NULL;
			name = strtok_r(line, "=", &value);
			
			if(value != NULL) {
				//print_gecko("Name [%s] Value [%s]\r\n", name, value);

				if(!strcmp("ID", name)) {
					defaultPassed = true;
					if(!first) {
						configEntriesCount++;
					}
					strncpy(configEntries[configEntriesCount].game_id, value, 4);
					first = 0;
					// Fill this entry with defaults incase some values are missing..
					config_defaults(&configEntries[configEntriesCount]);
				}
				else if(!strcmp("Nombre", name)) {
					strncpy(configEntries[configEntriesCount].game_name, value, 64);
				}
				else if(!strcmp("Comentario", name)) {
					strncpy(configEntries[configEntriesCount].comment, value, 128);
				}
				else if(!strcmp("Estado", name)) {
					strncpy(configEntries[configEntriesCount].status, value, 32);
				}
				else if(!strcmp("Forzar Modo de Video", name)) {
					int *ptr = !defaultPassed ? &swissSettings.gameVMode : &configEntries[configEntriesCount].gameVMode;
					for(int i = 0; i < 15; i++) {
						if(!strcmp(gameVModeStr[i], value)) {
							*ptr = i;
							break;
						}
					}
				}
				else if(!strcmp("Forzar Escala Horizontal", name)) {
					int *ptr = !defaultPassed ? &swissSettings.forceHScale : &configEntries[configEntriesCount].forceHScale;
					for(int i = 0; i < 9; i++) {
						if(!strcmp(forceHScaleStr[i], value)) {
							*ptr = i;
							break;
						}
					}
				}
				else if(!strcmp("Forzar Offset Vertical", name)) {
					if(defaultPassed)
						configEntries[configEntriesCount].forceVOffset = atoi(value);
					else
						swissSettings.forceVOffset = atoi(value);
				}
				else if(!strcmp("Forzar Filtro Vertical", name)) {
					int *ptr = !defaultPassed ? &swissSettings.forceVFilter : &configEntries[configEntriesCount].forceVFilter;
					for(int i = 0; i < 4; i++) {
						if(!strcmp(forceVFilterStr[i], value)) {
							*ptr = i;
							break;
						}
					}
				}
				else if(!strcmp("Desactivar Tramado Alfa", name)) {
					if(defaultPassed)
						configEntries[configEntriesCount].disableDithering = !strcmp("Si", value);
					else
						swissSettings.disableDithering = !strcmp("Si", value);
				}
				else if(!strcmp("Forzar Filtro Anisotropico", name)) {
					if(defaultPassed)
						configEntries[configEntriesCount].forceAnisotropy = !strcmp("Si", value);
					else
						swissSettings.forceAnisotropy = !strcmp("Si", value);
				}
				else if(!strcmp("Forzar Pantalla Ancha", name)) {
					int *ptr = !defaultPassed ? &swissSettings.forceWidescreen : &configEntries[configEntriesCount].forceWidescreen;
					for(int i = 0; i < 3; i++) {
						if(!strcmp(forceWidescreenStr[i], value)) {
							*ptr = i;
							break;
						}
					}
				}
				else if(!strcmp("Invertir Stick de Camara", name)) {
					int *ptr = !defaultPassed ? &swissSettings.invertCStick : &configEntries[configEntriesCount].invertCStick;
					for(int i = 0; i < 4; i++) {
						if(!strcmp(invertCStickStr[i], value)) {
							*ptr = i;
							break;
						}
					}
				}
				else if(!strcmp("Emular Velocidad de Lectura", name)) {
					int *ptr = !defaultPassed ? &swissSettings.emulateReadSpeed : &configEntries[configEntriesCount].emulateReadSpeed;
					for(int i = 0; i < 3; i++) {
						if(!strcmp(emulateReadSpeedStr[i], value)) {
							*ptr = i;
							break;
						}
					}
				}
				else if(!strcmp("Emular Memory Card", name)) {
					swissSettings.emulateMemoryCard = !strcmp("Si", value);
				}
				
				// Swiss settings
				else if(!strcmp("Velocidad de SD/IDE", name)) {
					swissSettings.exiSpeed = !strcmp("32MHz", value);
				}
				else if(!strcmp("Modo de Video de Swiss", name)) {
					for(int i = 0; i < 5; i++) {
						if(!strcmp(uiVModeStr[i], value)) {
							swissSettings.uiVMode = i;
							break;
						}
					}
				}
				else if(!strcmp("Activar Depuracion USB Gecko", name)) {
					swissSettings.debugUSB = !strcmp("Si", value);
				}
				else if(!strcmp("Ocultar tipo de archivo desconocido", name)) {
					swissSettings.hideUnknownFileTypes = !strcmp("Si", value);
				}
				else if(!strcmp("Parar motor de DVD al encender", name)) {
					swissSettings.stopMotor = !strcmp("Si", value);
				}
				else if(!strcmp("Activar Depuracion WiiRD", name)) {
					swissSettings.wiirdDebug = !strcmp("Si", value);
				}
				else if(!strcmp("Activar Gestion de Archivos", name)) {
					swissSettings.enableFileManagement = !strcmp("Si", value);
				}
				else if(!strcmp("Desactivar Parches de Video", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(disableVideoPatchesStr[i], value)) {
							swissSettings.disableVideoPatches = i;
							break;
						}
					}
				}
				else if(!strcmp("Forzar Video Activo", name)) {
					swissSettings.forceVideoActive = !strcmp("Si", value);
				}
				else if(!strcmp("Forzar Estado DTV", name)) {
					swissSettings.forceDTVStatus = !strcmp("Si", value);
				}
				else if(!strcmp("SMBUsuario", name)) {
					strlcpy(swissSettings.smbUser, value, sizeof(swissSettings.smbUser));
				}
				else if(!strcmp("SMBClave", name)) {
					strlcpy(swissSettings.smbPassword, value, sizeof(swissSettings.smbPassword));
				}
				else if(!strcmp("SMBNombreCompartir", name)) {
					strlcpy(swissSettings.smbShare, value, sizeof(swissSettings.smbShare));
				}
				else if(!strcmp("SMBIPHost", name)) {
					strlcpy(swissSettings.smbServerIp, value, sizeof(swissSettings.smbServerIp));
				}
				else if(!strcmp("AutoTrucos", name)) {
					swissSettings.autoCheats = !strcmp("Si", value);
				}
				else if(!strcmp("Iniciar Red", name)) {
					swissSettings.initNetworkAtStart = !strcmp("Si", value);
				}
				else if(!strcmp("Tipo IGR", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(igrTypeStr[i], value)) {
							swissSettings.igrType = i;
							break;
						}
					}
				}
				else if(!strcmp("AVECompat", name)) {
					for(int i = 0; i < 5; i++) {
						if(!strcmp(aveCompatStr[i], value)) {
							swissSettings.aveCompat = i;
							break;
						}
					}
				}
				else if(!strcmp("Tipo de Explorador de Archivos", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(fileBrowserStr[i], value)) {
							swissSettings.fileBrowserType = i;
							break;
						}
					}
				}
				else if(!strcmp("Iniciar a traves de IPL", name)) {
					for(int i = 0; i < 4; i++) {
						if(!strcmp(bs2BootStr[i], value)) {
							swissSettings.bs2Boot = i;
							break;
						}
					}
				}
				else if(!strcmp("FTPUsuario", name)) {
					strlcpy(swissSettings.ftpUserName, value, sizeof(swissSettings.ftpUserName));
				}
				else if(!strcmp("FTPClave", name)) {
					strlcpy(swissSettings.ftpPassword, value, sizeof(swissSettings.ftpPassword));
				}
				else if(!strcmp("FTPIPHost", name)) {
					strlcpy(swissSettings.ftpHostIp, value, sizeof(swissSettings.ftpHostIp));
				}
				else if(!strcmp("FTPPuerto", name)) {
					swissSettings.ftpPort = atoi(value);
				}
				else if(!strcmp("FTPModoPasv", name)) {
					swissSettings.ftpUsePasv = !strcmp("Si", value);
				}
				else if(!strcmp("FSPIPHost", name)) {
					strlcpy(swissSettings.fspHostIp, value, sizeof(swissSettings.fspHostIp));
				}
				else if(!strcmp("FSPPuerto", name)) {
					swissSettings.fspPort = atoi(value);
				}
				else if(!strcmp("FSPClave", name)) {
					strlcpy(swissSettings.fspPassword, value, sizeof(swissSettings.fspPassword));
				}
				else if(!strcmp("Mostrar Archivos Ocultos", name)) {
					swissSettings.showHiddenFiles = !strcmp("Si", value);
				}
				else if(!strcmp("Lista de Recientes", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(recentListLevelStr[i], value)) {
							swissSettings.recentListLevel = i;
							break;
						}
					}
				}
				else if(!strcmp("VersionGCLoaderTop", name)) {
					strlcpy(swissSettings.gcloaderTopVersion, value, sizeof(swissSettings.gcloaderTopVersion));
				}
				else if(!strcmp("Autocargar", name)) {
					strlcpy(swissSettings.autoload, value, sizeof(swissSettings.autoload));
				}
				else if(!strncmp("Reciente_", name, strlen("Reciente_"))) {
					int recent_slot = atoi(name+strlen("Reciente_"));
					if(recent_slot >= 0 && recent_slot < RECENT_MAX) {
						//print_gecko("found recent num %i [%s]\r\n", recent_slot, value);
						strlcpy(swissSettings.recent[recent_slot], value, PATHNAME_MAX);
					}
				}
			}
		}
		// And round we go again
		line = strtok_r( NULL, "\r\n", &linectx);
	}

	if(configEntriesCount > 0 || !first)
		configEntriesCount++;
	
	 print_gecko("Encontradas %i entradas en el arch. de config. (heredado)\r\n",configEntriesCount);
	 
	 // Write out to individual files.
	 int i;
	 for(i = 0; i < configEntriesCount; i++) {
		 progress_indicator("Migrando ajustes al nuevo formato.", 1, (int)(((float)i / (float)configEntriesCount) * 100));
		 config_update_game(&configEntries[i], false);
	 }
	 // Write out a new swiss.ini
	 config_update_global(false);
	 // Write out new recent.ini
	 config_update_recent(false);
	 // Kill off the old swiss.ini
	 config_file_delete(SWISS_SETTINGS_FILENAME_LEGACY);
}

void config_parse_global(char *configData) {
	char *line, *linectx = NULL;
	line = strtok_r( configData, "\r\n", &linectx );
	while( line != NULL ) {
		//print_gecko("Line [%s]\r\n", line);
		if(line[0] != '#') {
			char *name, *value = NULL;
			name = strtok_r(line, "=", &value);
			
			if(value != NULL) {
				//print_gecko("Name [%s] Value [%s]\r\n", name, value);

				if(!strcmp("Forzar Modo de Video", name)) {
					for(int i = 0; i < 15; i++) {
						if(!strcmp(gameVModeStr[i], value)) {
							swissSettings.gameVMode = i;
							break;
						}
					}
				}
				else if(!strcmp("Forzar Escala Horizontal", name)) {
					for(int i = 0; i < 9; i++) {
						if(!strcmp(forceHScaleStr[i], value)) {
							swissSettings.forceHScale = i;
							break;
						}
					}
				}
				else if(!strcmp("Forzar Offset Vertical", name)) {
					swissSettings.forceVOffset = atoi(value);
				}
				else if(!strcmp("Forzar Filtro Vertical", name)) {
					for(int i = 0; i < 4; i++) {
						if(!strcmp(forceVFilterStr[i], value)) {
							swissSettings.forceVFilter = i;
							break;
						}
					}
				}
				else if(!strcmp("Forzar Renderizado de Campo", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(forceVJitterStr[i], value)) {
							swissSettings.forceVJitter = i;
							break;
						}
					}
				}
				else if(!strcmp("Desactivar Tramado Alfa", name)) {
					swissSettings.disableDithering = !strcmp("Si", value);
				}
				else if(!strcmp("Forzar Filtro Anisotropico", name)) {
					swissSettings.forceAnisotropy = !strcmp("Si", value);
				}
				else if(!strcmp("Forzar Pantalla Ancha", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(forceWidescreenStr[i], value)) {
							swissSettings.forceWidescreen = i;
							break;
						}
					}
				}
				else if(!strcmp("Forzar Tasa de Sondeo", name)) {
					for(int i = 0; i < 13; i++) {
						if(!strcmp(forcePollRateStr[i], value)) {
							swissSettings.forcePollRate = i;
							break;
						}
					}
				}
				else if(!strcmp("Invertir Stick de Camara", name)) {
					for(int i = 0; i < 4; i++) {
						if(!strcmp(invertCStickStr[i], value)) {
							swissSettings.invertCStick = i;
							break;
						}
					}
				}
				else if(!strcmp("Cambiar Stick de Camara", name)) {
					for(int i = 0; i < 4; i++) {
						if(!strcmp(swapCStickStr[i], value)) {
							swissSettings.swapCStick = i;
							break;
						}
					}
				}
				else if(!strcmp("Nivel de Trigger Digital", name)) {
					swissSettings.triggerLevel = atoi(value);
				}
				else if(!strcmp("Emular Streaming de Audio", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(emulateAudioStreamStr[i], value)) {
							swissSettings.emulateAudioStream = i;
							break;
						}
					}
				}
				else if(!strcmp("Emular Velocidad de Lectura", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(emulateReadSpeedStr[i], value)) {
							swissSettings.emulateReadSpeed = i;
							break;
						}
					}
				}
				else if(!strcmp("Emular Memory Card", name)) {
					swissSettings.emulateMemoryCard = !strcmp("Si", value);
				}
				else if(!strcmp("Emular Adaptador de Red", name)) {
					swissSettings.emulateEthernet = !strcmp("Si", value);
				}
				else if(!strcmp("Preferir Inicio Limpio", name)) {
					swissSettings.preferCleanBoot = !strcmp("Si", value);
				}
				
				// Swiss settings
				else if(!strcmp("Velocidad de SD/IDE", name)) {
					swissSettings.exiSpeed = !strcmp("32MHz", value);
				}
				else if(!strcmp("Modo de Video de Swiss", name)) {
					for(int i = 0; i < 5; i++) {
						if(!strcmp(uiVModeStr[i], value)) {
							swissSettings.uiVMode = i;
							break;
						}
					}
				}
				else if(!strcmp("Activar Depuracion USB Gecko", name)) {
					swissSettings.debugUSB = !strcmp("Si", value);
				}
				else if(!strcmp("Ocultar tipo de archivo desconocido", name)) {
					swissSettings.hideUnknownFileTypes = !strcmp("Si", value);
				}
				else if(!strcmp("Parar motor de DVD al encender", name)) {
					swissSettings.stopMotor = !strcmp("Si", value);
				}
				else if(!strcmp("Activar Depuracion WiiRD", name)) {
					swissSettings.wiirdDebug = !strcmp("Si", value);
				}
				else if(!strcmp("Activar Gestion de Archivos", name)) {
					swissSettings.enableFileManagement = !strcmp("Si", value);
				}
				else if(!strcmp("Desactivar GameID MemCard PRO", name)) {
					for(int i = 0; i < 4; i++) {
						if(!strcmp(disableMCPGameIDStr[i], value)) {
							swissSettings.disableMCPGameID = i;
							break;
						}
					}
				}
				else if(!strcmp("Desactivar Parches de Video", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(disableVideoPatchesStr[i], value)) {
							swissSettings.disableVideoPatches = i;
							break;
						}
					}
				}
				else if(!strcmp("Forzar Video Activo", name)) {
					swissSettings.forceVideoActive = !strcmp("Si", value);
				}
				else if(!strcmp("Forzar Estado DTV", name)) {
					swissSettings.forceDTVStatus = !strcmp("Si", value);
				}
				else if(!strcmp("Pausar para cambio de resolucion", name)) {
					swissSettings.pauseAVOutput = !strcmp("Si", value);
				}
				else if(!strcmp("SMBUsuario", name)) {
					strlcpy(swissSettings.smbUser, value, sizeof(swissSettings.smbUser));
				}
				else if(!strcmp("SMBClave", name)) {
					strlcpy(swissSettings.smbPassword, value, sizeof(swissSettings.smbPassword));
				}
				else if(!strcmp("SMBNombreCompartir", name)) {
					strlcpy(swissSettings.smbShare, value, sizeof(swissSettings.smbShare));
				}
				else if(!strcmp("SMBIPHost", name)) {
					strlcpy(swissSettings.smbServerIp, value, sizeof(swissSettings.smbServerIp));
				}
				else if(!strcmp("AutoIniciar", name)) {
					swissSettings.autoBoot = !strcmp("Si", value);
				}
				else if(!strcmp("AutoTrucos", name)) {
					swissSettings.autoCheats = !strcmp("Si", value);
				}
				else if(!strcmp("Iniciar Red", name)) {
					swissSettings.initNetworkAtStart = !strcmp("Si", value);
				}
				else if(!strcmp("Tipo IGR", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(igrTypeStr[i], value)) {
							swissSettings.igrType = i;
							break;
						}
					}
				}
				else if(!strcmp("AVECompat", name)) {
					for(int i = 0; i < 5; i++) {
						if(!strcmp(aveCompatStr[i], value)) {
							swissSettings.aveCompat = i;
							break;
						}
					}
				}
				else if(!strcmp("Tipo de Explorador de Archivos", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(fileBrowserStr[i], value)) {
							swissSettings.fileBrowserType = i;
							break;
						}
					}
				}
				else if(!strcmp("Iniciar a traves de IPL", name)) {
					for(int i = 0; i < 4; i++) {
						if(!strcmp(bs2BootStr[i], value)) {
							swissSettings.bs2Boot = i;
							break;
						}
					}
				}
				else if(!strcmp("FTPUsuario", name)) {
					strlcpy(swissSettings.ftpUserName, value, sizeof(swissSettings.ftpUserName));
				}
				else if(!strcmp("FTPClave", name)) {
					strlcpy(swissSettings.ftpPassword, value, sizeof(swissSettings.ftpPassword));
				}
				else if(!strcmp("FTPIPHost", name)) {
					strlcpy(swissSettings.ftpHostIp, value, sizeof(swissSettings.ftpHostIp));
				}
				else if(!strcmp("FTPPuerto", name)) {
					swissSettings.ftpPort = atoi(value);
				}
				else if(!strcmp("FTPModoPasv", name)) {
					swissSettings.ftpUsePasv = !strcmp("Si", value);
				}
				else if(!strcmp("FSPIPHost", name)) {
					strlcpy(swissSettings.fspHostIp, value, sizeof(swissSettings.fspHostIp));
				}
				else if(!strcmp("FSPPuerto", name)) {
					swissSettings.fspPort = atoi(value);
				}
				else if(!strcmp("FSPClave", name)) {
					strlcpy(swissSettings.fspPassword, value, sizeof(swissSettings.fspPassword));
				}
				else if(!strcmp("FSPRutaMTU", name)) {
					swissSettings.fspPathMtu = atoi(value);
				}
				else if(!strcmp("DireccionIPv4", name)) {
					strlcpy(swissSettings.bbaLocalIp, value, sizeof(swissSettings.bbaLocalIp));
				}
				else if(!strcmp("MascaraRedIPv4", name)) {
					swissSettings.bbaNetmask = atoi(value);
				}
				else if(!strcmp("PuertaEnlaceIPv4", name)) {
					strlcpy(swissSettings.bbaGateway, value, sizeof(swissSettings.bbaGateway));
				}
				else if(!strcmp("DHCPEnIPv4", name)) {
					swissSettings.bbaUseDhcp = !strcmp("Si", value);
				}
				else if(!strcmp("Mostrar Archivos Ocultos", name)) {
					swissSettings.showHiddenFiles = !strcmp("Si", value);
				}
				else if(!strcmp("Lista de Recientes", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(recentListLevelStr[i], value)) {
							swissSettings.recentListLevel = i;
							break;
						}
					}
				}
				else if(!strcmp("VersionGCLoaderHW", name)) {
					swissSettings.gcloaderHwVersion = atoi(value);
				}
				else if(!strcmp("VersionGCLoaderTop", name)) {
					strlcpy(swissSettings.gcloaderTopVersion, value, sizeof(swissSettings.gcloaderTopVersion));
				}
				else if(!strcmp("Autocargar", name)) {
					strlcpy(swissSettings.autoload, value, sizeof(swissSettings.autoload));
				}
				else if(!strcmp("Aplanar Directorio", name)) {
					strlcpy(swissSettings.flattenDir, value, sizeof(swissSettings.flattenDir));
				}
			}
		}
		// And round we go again
		line = strtok_r( NULL, "\r\n", &linectx);
	}
}

void config_parse_args(int argc, char *argv[]) {
	if(argc == 0 || argv == NULL) {
		return;
	}
	char *argz;
	size_t argz_len;
	argz_create(&argv[1], &argz, &argz_len);
	argz_stringify(argz, argz_len, '\n');
	if(argz != NULL) {
		config_parse_global(argz);
		free(argz);
	}
}

void config_parse_recent(char *configData) {
	char *line, *linectx = NULL;
	line = strtok_r( configData, "\r\n", &linectx );
	while( line != NULL ) {
		//print_gecko("Line [%s]\r\n", line);
		if(line[0] != '#') {
			char *name, *value = NULL;
			name = strtok_r(line, "=", &value);
			
			if(value != NULL) {
				//print_gecko("Name [%s] Value [%s]\r\n", name, value);
				if(!strncmp("Reciente_", name, strlen("Reciente_"))) {
					int recent_slot = atoi(name+strlen("Reciente_"));
					if(recent_slot >= 0 && recent_slot < RECENT_MAX) {
						//print_gecko("found recent num %i [%s]\r\n", recent_slot, value);
						strlcpy(swissSettings.recent[recent_slot], value, PATHNAME_MAX);
					}
				}
			}
		}
		// And round we go again
		line = strtok_r( NULL, "\r\n", &linectx);
	}
}

void config_parse_game(char *configData, ConfigEntry *entry) {
	// Parse each entry and put it into our array
	char *line, *linectx = NULL;
	line = strtok_r( configData, "\r\n", &linectx );
	while( line != NULL ) {
		//print_gecko("Line [%s]\r\n", line);
		if(line[0] != '#') {
			char *name, *value = NULL;
			name = strtok_r(line, "=", &value);
			
			if(value != NULL) {
				//print_gecko("Name [%s] Value [%s]\r\n", name, value);
				if(!strcmp("Nombre", name)) {
					strncpy(entry->game_name, value, 64);
				}
				else if(!strcmp("Comentario", name)) {
					strncpy(entry->comment, value, 128);
				}
				else if(!strcmp("Estado", name)) {
					strncpy(entry->status, value, 32);
				}
				else if(!strcmp("Forzar Modo de Video", name)) {
					for(int i = 0; i < 15; i++) {
						if(!strcmp(gameVModeStr[i], value)) {
							entry->gameVMode = i;
							break;
						}
					}
				}
				else if(!strcmp("Forzar Escala Horizontal", name)) {
					for(int i = 0; i < 9; i++) {
						if(!strcmp(forceHScaleStr[i], value)) {
							entry->forceHScale = i;
							break;
						}
					}
				}
				else if(!strcmp("Forzar Offset Vertical", name)) {
					entry->forceVOffset = atoi(value);
				}
				else if(!strcmp("Forzar Filtro Vertical", name)) {
					for(int i = 0; i < 4; i++) {
						if(!strcmp(forceVFilterStr[i], value)) {
							entry->forceVFilter = i;
							break;
						}
					}
				}
				else if(!strcmp("Forzar Renderizado de Campo", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(forceVJitterStr[i], value)) {
							entry->forceVJitter = i;
							break;
						}
					}
				}
				else if(!strcmp("Desactivar Tramado Alfa", name)) {
					entry->disableDithering = !strcmp("Si", value);
				}
				else if(!strcmp("Forzar Filtro Anisotropico", name)) {
					entry->forceAnisotropy = !strcmp("Si", value);
				}
				else if(!strcmp("Forzar Pantalla Ancha", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(forceWidescreenStr[i], value)) {
							entry->forceWidescreen = i;
							break;
						}
					}
				}
				else if(!strcmp("Forzar Tasa de Sondeo", name)) {
					for(int i = 0; i < 13; i++) {
						if(!strcmp(forcePollRateStr[i], value)) {
							entry->forcePollRate = i;
							break;
						}
					}
				}
				else if(!strcmp("Invertir Stick de Camara", name)) {
					for(int i = 0; i < 4; i++) {
						if(!strcmp(invertCStickStr[i], value)) {
							entry->invertCStick = i;
							break;
						}
					}
				}
				else if(!strcmp("Cambiar Stick de Camara", name)) {
					for(int i = 0; i < 4; i++) {
						if(!strcmp(swapCStickStr[i], value)) {
							entry->swapCStick = i;
							break;
						}
					}
				}
				else if(!strcmp("Nivel de Trigger Digital", name)) {
					entry->triggerLevel = atoi(value);
				}
				else if(!strcmp("Emular Streaming de Audio", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(emulateAudioStreamStr[i], value)) {
							entry->emulateAudioStream = i;
							break;
						}
					}
				}
				else if(!strcmp("Emular Velocidad de Lectura", name)) {
					for(int i = 0; i < 3; i++) {
						if(!strcmp(emulateReadSpeedStr[i], value)) {
							entry->emulateReadSpeed = i;
							break;
						}
					}
				}
				else if(!strcmp("Emular Adaptador de Red", name)) {
					entry->emulateEthernet = !strcmp("Si", value);
				}
				else if(!strcmp("Preferir Inicio Limpio", name)) {
					entry->preferCleanBoot = !strcmp("Si", value);
				}
			}
		}
		// And round we go again
		line = strtok_r( NULL, "\r\n", &linectx);
	}
}

void config_find(ConfigEntry *entry) {
	// Fill out defaults
	config_defaults(entry);
	
	if(!config_set_device()) {
		return;
	}
	print_gecko("config_find: Buscando arch. de ajustes con ID %s\r\n",entry->game_id);
	// See if we have an actual config file for this game
	concatf_path(txtbuffer, SWISS_GAME_SETTINGS_DIR, "%.4s.ini", entry->game_id);
	char* configEntry = config_file_read(txtbuffer);
	if(configEntry) {
		config_parse_game(configEntry, entry);
		free(configEntry);
	}
	config_unset_device();
}

/** 
	Initialises the configuration file
	Returns 1 on successful file open, 0 otherwise
*/
int config_init(void (*progress_indicator)(char*, int, int)) {
	progress_indicator("Cargando ajustes", 1, -2);
	if(!config_set_device()) {
		progress_indicator(NULL, 0, 0);
		return 0;
	}
	
	// Make the new settings base dir(s) if we don't have them already
	ensure_path(DEVICE_CONFIG, SWISS_BASE_DIR, NULL);
	ensure_path(DEVICE_CONFIG, SWISS_SETTINGS_DIR, NULL);
	ensure_path(DEVICE_CONFIG, SWISS_GAME_SETTINGS_DIR, NULL);
	
	// Read config (legacy /swiss.ini format)
	char* configData = config_file_read(SWISS_SETTINGS_FILENAME_LEGACY);
	if(configData != NULL) {
		progress_indicator(NULL, 0, 0);
		progress_indicator("Migrando ajustes al nuevo formato.", 1, -1);
		config_parse_legacy(configData, progress_indicator);
		free(configData);
	}
	
	// Read config (new format)
	concat_path(txtbuffer, SWISS_SETTINGS_DIR, SWISS_SETTINGS_FILENAME);
	configData = config_file_read(txtbuffer);
	if(configData != NULL) {
		config_parse_global(configData);
		free(configData);
	} else {
		// Store current/defaults.
		config_update_global(false);
	}
	
	// Read the recent list if enabled
	if(swissSettings.recentListLevel > 0) {
		concat_path(txtbuffer, SWISS_SETTINGS_DIR, SWISS_RECENTLIST_FILENAME);
		configData = config_file_read(txtbuffer);
		if(configData != NULL) {
			config_parse_recent(configData);
			free(configData);
		}
	}
	progress_indicator(NULL, 0, 0);
	config_unset_device();
	return 0;
}

SwissSettings backup;

void config_load_current(ConfigEntry *config) {
	// load settings for this game to current settings
	memcpy(&backup, &swissSettings, sizeof(SwissSettings));
	swissSettings.gameVMode = config->gameVMode;
	swissSettings.forceHScale = config->forceHScale;
	swissSettings.forceVOffset = config->forceVOffset;
	swissSettings.forceVFilter = config->forceVFilter;
	swissSettings.forceVJitter = config->forceVJitter;
	swissSettings.disableDithering = config->disableDithering;
	swissSettings.forceAnisotropy = config->forceAnisotropy;
	swissSettings.forceWidescreen = config->forceWidescreen;
	swissSettings.forcePollRate = config->forcePollRate;
	swissSettings.invertCStick = config->invertCStick;
	swissSettings.swapCStick = config->swapCStick;
	swissSettings.triggerLevel = config->triggerLevel;
	swissSettings.emulateAudioStream = config->emulateAudioStream;
	swissSettings.emulateReadSpeed = config->emulateReadSpeed;
	swissSettings.emulateEthernet = config->emulateEthernet;
	swissSettings.preferCleanBoot = config->preferCleanBoot;
}

void config_unload_current() {
	swissSettings.gameVMode = backup.gameVMode;
	swissSettings.forceHScale = backup.forceHScale;
	swissSettings.forceVOffset = backup.forceVOffset;
	swissSettings.forceVFilter = backup.forceVFilter;
	swissSettings.forceVJitter = backup.forceVJitter;
	swissSettings.disableDithering = backup.disableDithering;
	swissSettings.forceAnisotropy = backup.forceAnisotropy;
	swissSettings.forceWidescreen = backup.forceWidescreen;
	swissSettings.forcePollRate = backup.forcePollRate;
	swissSettings.invertCStick = backup.invertCStick;
	swissSettings.swapCStick = backup.swapCStick;
	swissSettings.triggerLevel = backup.triggerLevel;
	swissSettings.wiirdDebug = backup.wiirdDebug;
	swissSettings.sram60Hz = backup.sram60Hz;
	swissSettings.sramProgressive = backup.sramProgressive;
	swissSettings.emulateAudioStream = backup.emulateAudioStream;
	swissSettings.emulateReadSpeed = backup.emulateReadSpeed;
	swissSettings.emulateEthernet = backup.emulateEthernet;
	swissSettings.preferCleanBoot = backup.preferCleanBoot;
	swissSettings.sramVideo = backup.sramVideo;
}
