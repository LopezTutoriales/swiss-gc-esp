#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <gccore.h>
#include <ogc/exi.h>
#include <ogc/machine/processor.h>
#include "deviceHandler.h"
#include "FrameBufferMagic.h"
#include "IPLFontWrite.h"
#include "swiss.h"
#include "main.h"
#include "info.h"
#include "config.h"
#include "settings.h"
#include "exi.h"
#include "bba.h"

#define page_x_ofs_key (30)
#define page_x_ofs_val (410)
#define page_y_line (25)
#define page_nav_y (390)
#define page_saveexit_y (425)
#define label_size (0.75f)

ConfigEntry tempConfig;
SwissSettings tempSettings;
char *uiVModeStr[] = {"Auto", "480i", "480p", "576i", "576p"};
char *gameVModeStr[] = {"Auto", "480i", "480sf", "240p", "960i", "480p", "1080i60", "540p60", "576i", "576sf", "288p", "1152i", "576p", "1080i50", "540p50"};
char *forceHScaleStr[] = {"Auto", "1:1", "11:10", "9:8", "640px", "656px", "672px", "704px", "720px"};
char *forceVFilterStr[] = {"Auto", "0", "1", "2"};
char *forceVJitterStr[] = {"Auto", "Activado", "Desactivado"};
char *forceWidescreenStr[] = {"No", "3D", "2D+3D"};
char *forcePollRateStr[] = {"No", "VSync", "1000Hz", "500Hz", "350Hz", "300Hz", "250Hz", "200Hz", "150Hz", "150Hz", "120Hz", "120Hz", "100Hz"};
char *invertCStickStr[] = {"No", "X", "Y", "X e Y"};
char *swapCStickStr[] = {"No", "X", "Y", "X e Y"};
char *disableMCPGameIDStr[] = {"No", "Ranura A", "Ranura B", "Ranuras A y B"};
char *disableVideoPatchesStr[] = {"Ninguno", "Juego", "Todos"};
char *emulateAudioStreamStr[] = {"Desactivado", "Auto", "Activado"};
char *emulateReadSpeedStr[] = {"No", "Si", "Wii"};
char *igrTypeStr[] = {"Desactivado", "Reiniciar", "igr.dol"};
char *aveCompatStr[] = {"CMPV-DOL", "GCVideo", "AVE-RVL", "AVE N-DOL", "AVE P-DOL"};
char *fileBrowserStr[] = {"Estandar", "Ancho Completo", "Carrusel"};
char *bs2BootStr[] = {"No", "Si", "Sonido 1", "Sonido 2"};
char *sramLang[] = {"Ingles", "Aleman", "Frances", "Espanol", "Italiano", "Holandes", "Japones", "Ingles (US)"};
char *recentListLevelStr[] = {"Desactivada", "Perezoso", "Activada"};

static char *tooltips_global[PAGE_GLOBAL_MAX+1] = {
	"Sonido de Sistema:\n\nEstablece la salida de audio por defecto usada por la\nmayoria de los juegos",
	"Posicion de Pantalla:\n\nAjusta la posicion horizontal de la pantalla en los juegos",
	"Idioma de Sistema:\n\nIdioma del sistema usado en los juegos, principalmente en\njuegos PAL multi-5",
	"Dispositivo de Configuracion:\n\nDispositivo que Swiss usara para cargar/guardar el swiss.ini.\nEste ajuste se guarda en la SRAM y permanecera al reiniciar.",
	NULL,
	"Tipo de Explorador de Archivos:\n\nEstandar - Muesta archivos con el minimo detalle (por defecto)\n\nCarrusel - Adecuado para usar solo con Juego/DOL, considere\ncombinar esta opcion con la opcion 'Gestion de Archivos' en\ndesactivado y 'Ocultar tipo de archivos desconocidos' en\nactivado para una mejor experiencia.",
	"Gestion de Archivos:\n\nCuando esta activado, pulsar Z en una entrada en el\nexplorador de archivos te permitira administrarla.",
	"Lista de Recientes:\n\n(Activada) - Pulsa Start para mostrar una lista de juegos\nrecientes.\n(Perezoso) - Igual que activada pero la lista actualiza\nsolo las entradas nuevas.\n(Desactivada) - La lista de recientes esta totalmente\ndesactivada.\n\nLas opciones perezoso/desactivada existen para minimizar\nla escritura en la SD.",
	NULL,
	"Ocultar tipo de archivo desconocido:\n\nDesactivado - Muestra todos los archivos (por defecto)\nActivado - Swiss ocultara los tipo de archivos desconocidos\n\nTipo de archivos conocidos:\n Ejecutables de GameCube (.bin/.dol/.elf)\n Imagenes de disco (.gcm/.iso/.nkit.iso/.tgc)\n Musica MP3 (.mp3) Archivos de flash de WASP/WKF (.fzn)\n Archivos de Memory Card de GameCube (.gci/.gcs/.sav)\n Ejecutables de GameCube con parametros agregados (.dol+cli).",
	"Aplanar directorio:\n\nAplana la estructura de un directorio coincidiendo con\nun patron global.",
	"Parar motor de DVD al encender:\n\nDesactivado - Lo deja como esta (por defecto)\nActivado - Para el lector de DVD cuando Swiss inicia\n\nEsta opcion es principalmente para usuarios que arrancan\ndesde exploits de juegos donde el disco ya esta girando.",
	"Velocidad de SD/IDE:\n\nLa velocidad para usar en el bus EXI para los adaptadores\nde tarjetas SD o en dispositivos IDE-EXI.\n32 MHz puede que no funcione en algunas tarjetas SD.",
	"Compatibilidad AVE:\n\nEstablece el modo de compatibilidad para el codificador de\naudio/video utilizado.\n\nAVE N-DOL - Salida PAL como NTSC 50\nAVE P-DOL - Desactiva el modo de escaneo progresivo\nCMPV-DOL - Activa 1080i y 540p\nGCVideo - Aplica soluciones de firmware para GC (por defecto)\nAVE-RVL - Soporta 960i y 1152i sin WiiVideo.",
	"Forzar Estado DTV:\n\nDesactivado - Usa la senal de la interfaz de video (por defecto)\nActivado - Fuerza la senal en caso de fallo de hardware.",
	"Salida de Depurador USB Gecko:\n\nSi hay un USB Gecko en la ranura B, se depurara desde Swiss\ny el juego (si el juego soporta la salida a traves de OSReport).\nSi nada lee los datos del dispositivo, es posible que Swiss o\nel juego se cuelguen."
};

static char *tooltips_network[PAGE_NETWORK_MAX+1] = {
	"Iniciar red al encender:\n\nDesactivado - No inicia la red aunque este presente (por defecto)\nActivado - Si hay un adaptador de red, se iniciara al encender\n\nSi esta iniciada la red, navega a la IP en el navegador web\npara hacer un backup de varios datos."
};

static char *tooltips_game_global[PAGE_GAME_GLOBAL_MAX+1] = {
	"Reinicio en el juego: (A + Z + Start)\n\nReiniciar: Reinicia parcialmente la GameCube\nigr.dol: Memoria baja (< 0x81300000) igr.dol en la raiz de\nla tarjeta SD.",
	"Iniciar a traves de IPL:\n\nCuando esta activado, los juegos iniciaran con la pantalla\ndel logo de GameCube y el menu principal accesible con los\nparches aplicados.",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"Pausar para cambio de resolucion:\n\nCuando esta activado, un cambio en la resolucion de video\npausara el juego durante 2 segundos.",
	"Autocargar todos los trucos:\n\nSi esta activado, y hay un archivo de trucos para un juego\nen particular\nEj: /swiss/cheats/GPOP8D.txt (en un dispositivo compatible)\ntodos los trucos del archivo se activaran.",
	"Depuracion WiiRD:\n\nDesactivado - Arranque normal (por defecto)\nActivado - Iniciara el juego con el depurador WiiRD\nhabilitado y en pausa\n\nEl depurador WiiRD ocupa mucha memoria y puede ocasionar\nproblemas."
};

static char *tooltips_game[PAGE_GAME_MAX+1] = {
	NULL,
	NULL,
	"Forzar Offset Vertical:\n\n+0 - Valor estandar\n-2 - GCVideo-DVI compatible (480i)\n-3 - GCVideo-DVI compatible (por defecto)\n-4 - GCVideo-DVI compatible (240p)\n-12 - Datapath VisionRGB (480p).",
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	"Forzar Tasa de Sondeo:\n\nVsync - Mayor compatibilidad\n1000Hz - Latencia de entrada mas baja.",
	"Invertir Stick de Camara:\n\nNo - Deja el Stick C como esta (por defecto)\nX - Invierte el eje X en el Stick C\nY - Invierte el eje Y en el Stick C\nX e Y - Invierte ambos ejes en el Stick C",
	"Cambiar Stick de Camara:\n\nNo - Deja el Stick C como esta (por defecto)\nX - Cambia el eje X del Stick C por el Stick de Control\nY - Cambia el eje Y del Stick C por el Stick de Control\nX e Y - Cambia ambos ejes del Stick C por el Stick de Control",
	"Nivel de Trigger Digital:\n\nEstablece el umbral en el que se presiona completamente el\nBoton L/R.",
	"Emular Streaming de Audio:\n\nEl streaming de audio es una caracteristica de hardware que\npermite que la unidad de disco reproduzca una pista de audio\ncomprimida en segundo plano.\n\nLa emulacion es necesaria para dispositivos que no estan\nconectados a la interfaz de DVD o para aquellos que no la\nimplementan independientemente.",
	"Emular Velocidad de Lectura:\n\nNo - Inicia la transferencia inmediatamente (por defecto)\nSi - Retrasa la transferencia para simular la transferencia\n     de la unidad de disco de GameCube\nWii - Retrasa la transferencia para simular la transferencia\n      de la unidad de disco de Wii\n\nEsto es necesario para evitar errores de programacion ofuscados\npor el medio original o para hacer speedrunning.",
	"Emular Adaptador de Red:\n\nSolo disponible con el Protocolo de Servicio de Archivos.\n\nLos paquetes no destinados al hipervisor se reenvian a la\nMAC virtual. La direccion MAC virtual es la misma que la\nMAC fisica. La MAC/PHY fisico conserva su configuracion en\nSwiss, incluyendo la velocidad de enlace.",
	"Preferir Inicio Limpio:\n\nCuando esta activado, la GameCube se reiniciara y el juego\niniciara mediante procesos normales sin aplicar cambios.\nEs posible que se apliquen restricciones regionales.\n\nSolo disponible para dispositivos conectados a la interfaz\nde DVD."
};

syssram* sram;
syssramex* sramex;

// Number of settings (including Back, Next, Save, Exit buttons) per page
int settings_count_pp[5] = {PAGE_GLOBAL_MAX, PAGE_NETWORK_MAX, PAGE_GAME_GLOBAL_MAX, PAGE_GAME_DEFAULTS_MAX, PAGE_GAME_MAX};

void refreshSRAM(SwissSettings *settings) {
	bool writeSram = false;
	sram = __SYS_LockSram();
	if(!__SYS_CheckSram()) {
		memset(sram, 0, sizeof(syssram));
		sram->flags |= 0x10;
		sram->flags |= 0x04;
		writeSram = true;
	}
	settings->sramHOffset = sram->display_offsetH;
	settings->sram60Hz = (sram->ntd >> 6) & 1;
	settings->sramLanguage = sram->lang;
	settings->sramProgressive = (sram->flags >> 7) & 1;
	settings->sramStereo = (sram->flags >> 2) & 1;
	settings->sramVideo = sram->flags & 3;
	__SYS_UnlockSram(writeSram);
	if(writeSram)
		while(!__SYS_SyncSram());
	sramex = __SYS_LockSramEx();
	settings->configDeviceId = sramex->__padding0;
	if(settings->configDeviceId > DEVICE_ID_MAX || !(getDeviceByUniqueId(settings->configDeviceId)->features & FEAT_CONFIG_DEVICE)) {
		settings->configDeviceId = DEVICE_ID_UNK;
	}
	__SYS_UnlockSramEx(0);
}

char* getConfigDeviceName(SwissSettings *settings) {
	DEVICEHANDLER_INTERFACE *configDevice = getDeviceByUniqueId(settings->configDeviceId);
	return configDevice != NULL ? (char*)(configDevice->deviceName) : "Ninguno";
}

char* get_tooltip(int page_num, int option) {
	char *textPtr = NULL;
	if(page_num == PAGE_GLOBAL) {
		textPtr = tooltips_global[option];
	}
	else if(page_num == PAGE_NETWORK) {
		textPtr = tooltips_network[option];
	}
	else if(page_num == PAGE_GAME_GLOBAL) {
		textPtr = tooltips_game_global[option];
	}
	else if(page_num == PAGE_GAME_DEFAULTS) {
		textPtr = tooltips_game[option];
	}
	else if(page_num == PAGE_GAME) {
		textPtr = tooltips_game[option];
	}
	return textPtr;
}

void add_tooltip_label(uiDrawObj_t* page, int page_num, int option) {
	if(get_tooltip(page_num, option)) {
		DrawAddChild(page, DrawFadingLabel(484, 54, "Pulsa Y para ayuda", 0.65f));
	}
}

void drawSettingEntryString(uiDrawObj_t* page, int *y, char *label, char *key, bool selected, bool enabled) {
	if(selected) {
		DrawAddChild(page, DrawStyledLabel(20, *y, "\225", label_size, false, enabled ? defaultColor:deSelectedColor));
	}
	DrawAddChild(page, DrawStyledLabel(page_x_ofs_key, *y, label, label_size, false, enabled ? defaultColor:deSelectedColor));
	DrawAddChild(page, DrawStyledLabel(page_x_ofs_val, *y, key, label_size, false, enabled && selected ? defaultColor:deSelectedColor));
	*y += page_y_line; 
}

void drawSettingEntryBoolean(uiDrawObj_t* page, int *y, char *label, bool boolval, bool selected, bool enabled) {
	drawSettingEntryString(page, y, label, boolval ? "Si" : "No", selected, enabled);
}

void drawSettingEntryNumeric(uiDrawObj_t* page, int *y, char *label, int num, bool selected, bool enabled) {
	sprintf(txtbuffer, "%i", num);
	drawSettingEntryString(page, y, label, txtbuffer, selected, enabled);
}

uiDrawObj_t* settings_draw_page(int page_num, int option, ConfigEntry *gameConfig) {
	uiDrawObj_t* page = DrawEmptyBox(20,60, getVideoMode()->fbWidth-20, 460);
	char sramHOffsetStr[8];
	char forceVOffsetStr[8];
	char triggerLevelStr[8];
	
	// Save Settings to current device (**Shown on all tabs**)
	/** Global Settings (Page 1/) */
	// System Sound [Mono/Stereo]
	// Screen Position [+/-0]
	// System Language [English/German/French/Spanish/Italian/Dutch]
	// SD/IDE Speed [16/32 MHz]
	// Swiss Video Mode [576i (PAL 50Hz), 480i (NTSC 60Hz), 480p (NTSC 60Hz), etc]
	// In-Game Reset [Yes/No]
	// Configuration Device [Writable device name]
	// AVE Compatibility
	// Filebrowser Type [Standard / Carousel]
	// ShowHiddenFiles [Yes/No]
	// Recent List: [On/Lazy/Off]

	/** Advanced Settings (Page 2/) */
	// Enable USB Gecko Debug via Slot B [Yes/No]
	// Hide Unknown file types [Yes/No]	// TODO Implement
	// Stop DVD Motor on startup [Yes/No]
	// Enable WiiRD debugging in Games [Yes/No]
	// Enable File Management [Yes/No]
	// Auto-load all cheats [Yes/No]
	// Init network at startup [Yes/No]
	
	/** Current Game Settings - only if a valid GCM file is highlighted (Page 3/) */
	// Force Video Mode [576i (PAL 50Hz), 480i (NTSC 60Hz), 480p (NTSC 60Hz), Auto, etc]
	// Force Horizontal Scale [Auto/1:1/11:10/9:8/640px/656px/672px/704px/720px]
	// Force Vertical Offset [+/-0]
	// Force Vertical Filter [Auto/0/1/2]
	// Force Anisotropic Filter [Yes/No]
	// Force Widescreen [No/3D/2D+3D]
	// Force Text Encoding [Auto/ANSI/SJIS]
	// Disable Audio Streaming [Yes/No]

	bool isNavOption = false;
	// Add paging and save/cancel buttons
	if(page_num != PAGE_MIN) {
		isNavOption = option == settings_count_pp[page_num]-(page_num != PAGE_MAX ? 3:2);
		DrawAddChild(page, DrawSelectableButton(50, page_nav_y, -1, 420, "Volver", isNavOption));
	}
	if(page_num != PAGE_MAX) {
		isNavOption = isNavOption || option == settings_count_pp[page_num]-2;
		DrawAddChild(page, DrawSelectableButton(450, page_nav_y, -1, 420, "Siguiente", option == settings_count_pp[page_num]-2 ? B_SELECTED:B_NOSELECT));
	}
	DrawAddChild(page, DrawSelectableButton(110, page_saveexit_y, -1, 460, "Guardar y Salir", option == settings_count_pp[page_num]-1 ? B_SELECTED:B_NOSELECT));
	DrawAddChild(page, DrawSelectableButton(330, page_saveexit_y, -1, 460, "Descartar y Salir", option ==  settings_count_pp[page_num] ? B_SELECTED:B_NOSELECT));
	isNavOption = isNavOption || (option >= settings_count_pp[page_num]-1);
	
	int page_y_ofs = 110;
	// Page specific buttons
	if(page_num == PAGE_GLOBAL) {
		int settings_per_page = 10;
		int scrollBarHeight = 90+(settings_per_page*20);
		int scrollBarTabHeight = (int)((float)scrollBarHeight/(float)SET_PAGE_1_NEXT);
		DrawAddChild(page, DrawVertScrollBar(getVideoMode()->fbWidth-45, page_y_ofs, 25, scrollBarHeight, (float)((float)option/(float)(SET_PAGE_1_NEXT-1)),scrollBarTabHeight));
		DrawAddChild(page, DrawLabel(page_x_ofs_key, 65, "Ajustes Globales (1/5):"));
		bool dtvEnable = swissSettings.aveCompat < 3;
		bool dbgEnable = devices[DEVICE_CUR] != &__device_usbgecko && deviceHandler_getDeviceAvailable(&__device_usbgecko);
		// TODO settings to a new typedef that ties type etc all together, then draw a "page" of these rather than this at some point.
		if(option < SET_STOP_MOTOR) {
			drawSettingEntryString(page, &page_y_ofs, "Sonido de Sistema:", swissSettings.sramStereo ? "Estereo":"Mono", option == SET_SYS_SOUND, true);
			sprintf(sramHOffsetStr, "%+hi", swissSettings.sramHOffset);
			drawSettingEntryString(page, &page_y_ofs, "Posicion de Pantalla:", sramHOffsetStr, option == SET_SCREEN_POS, true);
			drawSettingEntryString(page, &page_y_ofs, "Idioma de Sistema:", swissSettings.sramLanguage > SRAM_LANG_MAX ? "Desconocido" : sramLang[swissSettings.sramLanguage], option == SET_SYS_LANG, true);
			drawSettingEntryString(page, &page_y_ofs, "Dispositivo de Configuracion:", getConfigDeviceName(&swissSettings), option == SET_CONFIG_DEV, true);
			drawSettingEntryString(page, &page_y_ofs, "Modo de Video de Swiss:", uiVModeStr[swissSettings.uiVMode], option == SET_SWISS_VIDEOMODE, true);
			drawSettingEntryString(page, &page_y_ofs, "Tipo de Explorador de Archivos:", fileBrowserStr[swissSettings.fileBrowserType], option == SET_FILEBROWSER_TYPE, true);
			drawSettingEntryBoolean(page, &page_y_ofs, "Gestion de Archivos:", swissSettings.enableFileManagement, option == SET_FILE_MGMT, true);
			drawSettingEntryString(page, &page_y_ofs, "Lista de Recientes:", recentListLevelStr[swissSettings.recentListLevel], option == SET_RECENT_LIST, true);
			drawSettingEntryBoolean(page, &page_y_ofs, "Mostrar Archivos Ocultos:", swissSettings.showHiddenFiles, option == SET_SHOW_HIDDEN, true);
			drawSettingEntryBoolean(page, &page_y_ofs, "Ocultar tipo de archivo desconocido:", swissSettings.hideUnknownFileTypes, option == SET_HIDE_UNK, true);
			drawSettingEntryString(page, &page_y_ofs, "Aplanar directorio:", swissSettings.flattenDir, option == SET_FLATTEN_DIR, true);
		} else {
			drawSettingEntryBoolean(page, &page_y_ofs, "Parar motor de DVD al encender:", swissSettings.stopMotor, option == SET_STOP_MOTOR, true);
			drawSettingEntryString(page, &page_y_ofs, "Velocidad de SD/IDE:", swissSettings.exiSpeed ? "32 MHz":"16 MHz", option == SET_EXI_SPEED, true);
			drawSettingEntryString(page, &page_y_ofs, "Compatibilidad AVE:", aveCompatStr[swissSettings.aveCompat], option == SET_AVE_COMPAT, true);
			drawSettingEntryBoolean(page, &page_y_ofs, "Forzar Estado DTV:", swissSettings.forceDTVStatus, option == SET_FORCE_DTVSTATUS, dtvEnable);
			drawSettingEntryBoolean(page, &page_y_ofs, "Salida de Depurador USB Gecko:", swissSettings.debugUSB, option == SET_ENABLE_USBGECKODBG, dbgEnable);
		}
	}
	else if(page_num == PAGE_NETWORK) {
		int settings_per_page = 10;
		int scrollBarHeight = 90+(settings_per_page*20);
		int scrollBarTabHeight = (int)((float)scrollBarHeight/(float)SET_PAGE_2_BACK);
		DrawAddChild(page, DrawVertScrollBar(getVideoMode()->fbWidth-45, page_y_ofs, 25, scrollBarHeight, (float)((float)option/(float)(SET_PAGE_2_BACK-1)),scrollBarTabHeight));
		DrawAddChild(page, DrawLabel(page_x_ofs_key, 65, "Ajustes de Red (2/5):"));
		bool netEnable = net_initialized || bba_exists(LOC_ANY);
		// TODO settings to a new typedef that ties type etc all together, then draw a "page" of these rather than this at some point.
		if(option < SET_FTP_USER) {
			drawSettingEntryBoolean(page, &page_y_ofs, "Iniciar red al encender:", swissSettings.initNetworkAtStart, option == SET_INIT_NET, true);
			drawSettingEntryString(page, &page_y_ofs, "Direccion IPv4:", swissSettings.bbaLocalIp, option == SET_BBA_LOCALIP, netEnable);
			drawSettingEntryNumeric(page, &page_y_ofs, "Mascara de red IPv4:", swissSettings.bbaNetmask, option == SET_BBA_NETMASK, netEnable);
			drawSettingEntryString(page, &page_y_ofs, "Puerta de enlace IPv4:", swissSettings.bbaGateway, option == SET_BBA_GATEWAY, netEnable);
			drawSettingEntryBoolean(page, &page_y_ofs, "IPv4 usa DHCP:", swissSettings.bbaUseDhcp, option == SET_BBA_DHCP, netEnable);
			drawSettingEntryString(page, &page_y_ofs, "IP Host FSP:", swissSettings.fspHostIp, option == SET_FSP_HOSTIP, netEnable);
			drawSettingEntryNumeric(page, &page_y_ofs, "Puerto FSP:", swissSettings.fspPort, option == SET_FSP_PORT, netEnable);
			drawSettingEntryString(page, &page_y_ofs, "Clave FSP:", "*****", option == SET_FSP_PASS, netEnable);
			drawSettingEntryNumeric(page, &page_y_ofs, "Ruta MTU FSP:", swissSettings.fspPathMtu, option == SET_FSP_PMTU, netEnable);
			drawSettingEntryString(page, &page_y_ofs, "IP Host FTP:", swissSettings.ftpHostIp, option == SET_FTP_HOSTIP, netEnable);
			drawSettingEntryNumeric(page, &page_y_ofs, "Puerto FTP:", swissSettings.ftpPort, option == SET_FTP_PORT, netEnable);
		} else {
			drawSettingEntryString(page, &page_y_ofs, "Usuario FTP:", swissSettings.ftpUserName, option == SET_FTP_USER, netEnable);
			drawSettingEntryString(page, &page_y_ofs, "Clave FTP:", "*****", option == SET_FTP_PASS, netEnable);
			drawSettingEntryBoolean(page, &page_y_ofs, "Modo PSAV FTP:", swissSettings.ftpUsePasv, option == SET_FTP_PASV, netEnable);
			drawSettingEntryString(page, &page_y_ofs, "IP Host SMB:", swissSettings.smbServerIp, option == SET_SMB_HOSTIP, netEnable);
			drawSettingEntryString(page, &page_y_ofs, "Compartir SMB:", swissSettings.smbShare, option == SET_SMB_SHARE, netEnable);
			drawSettingEntryString(page, &page_y_ofs, "Usuario SMB:", swissSettings.smbUser, option == SET_SMB_USER, netEnable);
			drawSettingEntryString(page, &page_y_ofs, "Clave SMB:", "*****", option == SET_SMB_PASS, netEnable);
		}
	}
	else if(page_num == PAGE_GAME_GLOBAL) {
		DrawAddChild(page, DrawLabel(page_x_ofs_key, 65, "Ajustes Globales de Juego (3/5):"));
		bool enabledVideoPatches = swissSettings.disableVideoPatches < 2;
		bool emulatedMemoryCard = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_MEMCARD);
		bool dbgEnable = devices[DEVICE_CUR] != &__device_usbgecko && deviceHandler_getDeviceAvailable(&__device_usbgecko);
		drawSettingEntryString(page, &page_y_ofs, "Reinicio en el juego:", igrTypeStr[swissSettings.igrType], option == SET_IGR, true);
		drawSettingEntryString(page, &page_y_ofs, "Iniciar a traves de IPL:", bs2BootStr[swissSettings.bs2Boot], option == SET_BS2BOOT, true);
		drawSettingEntryBoolean(page, &page_y_ofs, "Iniciar sin indicaciones:", swissSettings.autoBoot, option == SET_AUTOBOOT, true);
		drawSettingEntryBoolean(page, &page_y_ofs, "Emular Memory Card:", swissSettings.emulateMemoryCard, option == SET_EMULATE_MEMCARD, emulatedMemoryCard);
		drawSettingEntryString(page, &page_y_ofs, "Desactivar GameID MemCard PRO:", disableMCPGameIDStr[swissSettings.disableMCPGameID], option == SET_ENABLE_MCPGAMEID, true);
		drawSettingEntryBoolean(page, &page_y_ofs, "Forzar Video Activo:", swissSettings.forceVideoActive, option == SET_FORCE_VIDACTIVE, enabledVideoPatches);
		drawSettingEntryString(page, &page_y_ofs, "Desactivar Parches de Video:", disableVideoPatchesStr[swissSettings.disableVideoPatches], option == SET_ENABLE_VIDPATCH, true);
		drawSettingEntryBoolean(page, &page_y_ofs, "Pausar para cambio de resolucion:", swissSettings.pauseAVOutput, option == SET_PAUSE_AVOUTPUT, true);
		drawSettingEntryBoolean(page, &page_y_ofs, "Autocargar todos los trucos:", swissSettings.autoCheats, option == SET_ALL_CHEATS, true);
		drawSettingEntryBoolean(page, &page_y_ofs, "Depuracion WiiRD:", swissSettings.wiirdDebug, option == SET_WIIRDDBG, dbgEnable);
	}
	else if(page_num == PAGE_GAME_DEFAULTS) {
		int settings_per_page = 10;
		int scrollBarHeight = 90+(settings_per_page*20);
		int scrollBarTabHeight = (int)((float)scrollBarHeight/(float)SET_PAGE_4_BACK);
		DrawAddChild(page, DrawVertScrollBar(getVideoMode()->fbWidth-45, page_y_ofs, 25, scrollBarHeight, (float)((float)option/(float)(SET_PAGE_4_BACK-1)),scrollBarTabHeight));
		DrawAddChild(page, DrawLabel(page_x_ofs_key, 65, "Ajustes Por Defecto de Juego (4/5):"));
		bool enabledVideoPatches = swissSettings.disableVideoPatches < 2;
		bool emulatedAudioStream = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_AUDIO_STREAMING);
		bool emulatedReadSpeed = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_READ_SPEED);
		bool emulatedEthernet = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_ETHERNET);
		bool enabledCleanBoot = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->location == LOC_DVD_CONNECTOR);
		if(option < SET_DEFAULT_TRIGGER_LEVEL) {
			drawSettingEntryString(page, &page_y_ofs, "Forzar Modo de Video:", gameVModeStr[swissSettings.gameVMode], option == SET_DEFAULT_FORCE_VIDEOMODE, enabledVideoPatches);
			drawSettingEntryString(page, &page_y_ofs, "Forzar Escala Horizontal:", forceHScaleStr[swissSettings.forceHScale], option == SET_DEFAULT_HORIZ_SCALE, enabledVideoPatches);
			sprintf(forceVOffsetStr, "%+hi", swissSettings.forceVOffset);
			drawSettingEntryString(page, &page_y_ofs, "Forzar Offset Vertical:", forceVOffsetStr, option == SET_DEFAULT_VERT_OFFSET, enabledVideoPatches);
			drawSettingEntryString(page, &page_y_ofs, "Forzar Filtro Vertical:", forceVFilterStr[swissSettings.forceVFilter], option == SET_DEFAULT_VERT_FILTER, enabledVideoPatches);
			drawSettingEntryString(page, &page_y_ofs, "Forzar Renderizado de Campo:", forceVJitterStr[swissSettings.forceVJitter], option == SET_FIELD_RENDER, enabledVideoPatches);
			drawSettingEntryBoolean(page, &page_y_ofs, "Desactivar Tramado Alfa:", swissSettings.disableDithering, option == SET_DEFAULT_ALPHA_DITHER, enabledVideoPatches);
			drawSettingEntryBoolean(page, &page_y_ofs, "Forzar Filtro Anisotropico:", swissSettings.forceAnisotropy, option == SET_DEFAULT_ANISO_FILTER, true);
			drawSettingEntryString(page, &page_y_ofs, "Forzar Pantalla Ancha:", forceWidescreenStr[swissSettings.forceWidescreen], option == SET_DEFAULT_WIDESCREEN, true);
			drawSettingEntryString(page, &page_y_ofs, "Forzar Tasa de Sondeo:", forcePollRateStr[swissSettings.forcePollRate], option == SET_DEFAULT_POLL_RATE, true);
			drawSettingEntryString(page, &page_y_ofs, "Invertir Stick de Camara:", invertCStickStr[swissSettings.invertCStick], option == SET_DEFAULT_INVERT_CAMERA, true);
			drawSettingEntryString(page, &page_y_ofs, "Cambiar Stick de Camara:", swapCStickStr[swissSettings.swapCStick], option == SET_DEFAULT_SWAP_CAMERA, true);
		} else {
			sprintf(triggerLevelStr, "%hhu", swissSettings.triggerLevel);
			drawSettingEntryString(page, &page_y_ofs, "Nivel de Trigger Digital:", triggerLevelStr, option == SET_DEFAULT_TRIGGER_LEVEL, true);
			drawSettingEntryString(page, &page_y_ofs, "Emular Streaming de Audio:", emulateAudioStreamStr[swissSettings.emulateAudioStream], option == SET_DEFAULT_AUDIO_STREAM, emulatedAudioStream);
			drawSettingEntryString(page, &page_y_ofs, "Emular Velocidad de Lectura:", emulateReadSpeedStr[swissSettings.emulateReadSpeed], option == SET_DEFAULT_READ_SPEED, emulatedReadSpeed);
			drawSettingEntryBoolean(page, &page_y_ofs, "Emular Adaptador de Red:", swissSettings.emulateEthernet, option == SET_DEFAULT_EMULATE_ETHERNET, emulatedEthernet);
			drawSettingEntryBoolean(page, &page_y_ofs, "Preferir Inicio Limpio:", swissSettings.preferCleanBoot, option == SET_DEFAULT_CLEAN_BOOT, enabledCleanBoot);
		}
	}
	else if(page_num == PAGE_GAME) {
		int settings_per_page = 10;
		int scrollBarHeight = 90+(settings_per_page*20);
		int scrollBarTabHeight = (int)((float)scrollBarHeight/(float)SET_PAGE_5_BACK);
		DrawAddChild(page, DrawVertScrollBar(getVideoMode()->fbWidth-45, page_y_ofs, 25, scrollBarHeight, (float)((float)option/(float)(SET_PAGE_5_BACK-1)),scrollBarTabHeight));
		DrawAddChild(page, DrawLabel(page_x_ofs_key, 65, "Ajustes de Juego Actual (5/5):"));
		bool enabledGamePatches = gameConfig != NULL && !gameConfig->forceCleanBoot;
		if(enabledGamePatches) {
			bool enabledVideoPatches = swissSettings.disableVideoPatches < 2;
			bool emulatedAudioStream = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_AUDIO_STREAMING);
			bool emulatedReadSpeed = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_READ_SPEED);
			bool emulatedEthernet = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_ETHERNET);
			bool enabledCleanBoot = devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->location == LOC_DVD_CONNECTOR);
			if(option < SET_TRIGGER_LEVEL) {
				drawSettingEntryString(page, &page_y_ofs, "Forzar Modo de Video:", gameVModeStr[gameConfig->gameVMode], option == SET_FORCE_VIDEOMODE, enabledVideoPatches);
				drawSettingEntryString(page, &page_y_ofs, "Forzar Escala Horizontal:", forceHScaleStr[gameConfig->forceHScale], option == SET_HORIZ_SCALE, enabledVideoPatches);
				sprintf(forceVOffsetStr, "%+hi", gameConfig->forceVOffset);
				drawSettingEntryString(page, &page_y_ofs, "Forzar Offset Vertical:", forceVOffsetStr, option == SET_VERT_OFFSET, enabledVideoPatches);
				drawSettingEntryString(page, &page_y_ofs, "Forzar Filtro Vertical:", forceVFilterStr[gameConfig->forceVFilter], option == SET_VERT_FILTER, enabledVideoPatches);
				drawSettingEntryString(page, &page_y_ofs, "Forzar Renderizado de Campo:", forceVJitterStr[gameConfig->forceVJitter], option == SET_FIELD_RENDER, enabledVideoPatches);
				drawSettingEntryBoolean(page, &page_y_ofs, "Desactivar Tramado Alfa:", gameConfig->disableDithering, option == SET_ALPHA_DITHER, enabledVideoPatches);
				drawSettingEntryBoolean(page, &page_y_ofs, "Forzar Filtro Anisotropico:", gameConfig->forceAnisotropy, option == SET_ANISO_FILTER, true);
				drawSettingEntryString(page, &page_y_ofs, "Forzar Pantalla Ancha:", forceWidescreenStr[gameConfig->forceWidescreen], option == SET_WIDESCREEN, true);
				drawSettingEntryString(page, &page_y_ofs, "Forzar Tasa de Sondeo:", forcePollRateStr[gameConfig->forcePollRate], option == SET_POLL_RATE, true);
				drawSettingEntryString(page, &page_y_ofs, "Invertir Stick de Camara:", invertCStickStr[gameConfig->invertCStick], option == SET_INVERT_CAMERA, true);
				drawSettingEntryString(page, &page_y_ofs, "Cambiar Stick de Camara:", swapCStickStr[gameConfig->swapCStick], option == SET_SWAP_CAMERA, true);
			} else {
				sprintf(triggerLevelStr, "%hhu", gameConfig->triggerLevel);
				drawSettingEntryString(page, &page_y_ofs, "Nivel de Trigger Digital:", triggerLevelStr, option == SET_TRIGGER_LEVEL, true);
				drawSettingEntryString(page, &page_y_ofs, "Emular Streaming de Audio:", emulateAudioStreamStr[gameConfig->emulateAudioStream], option == SET_AUDIO_STREAM, emulatedAudioStream);
				drawSettingEntryString(page, &page_y_ofs, "Emular Velocidad de Lectura:", emulateReadSpeedStr[gameConfig->emulateReadSpeed], option == SET_READ_SPEED, emulatedReadSpeed);
				drawSettingEntryBoolean(page, &page_y_ofs, "Emular Adaptador de Red:", gameConfig->emulateEthernet, option == SET_EMULATE_ETHERNET, emulatedEthernet);
				drawSettingEntryBoolean(page, &page_y_ofs, "Preferir Inicio Limpio:", gameConfig->preferCleanBoot, option == SET_CLEAN_BOOT, enabledCleanBoot);
				drawSettingEntryString(page, &page_y_ofs, "Restablecer ajustes por defecto", NULL, option == SET_DEFAULTS, true);
			}
		}
		else {
			// Just draw the defaults again
			if(option < SET_TRIGGER_LEVEL) {
				drawSettingEntryString(page, &page_y_ofs, "Forzar Modo de Video:", gameVModeStr[swissSettings.gameVMode], option == SET_FORCE_VIDEOMODE, false);
				drawSettingEntryString(page, &page_y_ofs, "Forzar Escala Horizontal:", forceHScaleStr[swissSettings.forceHScale], option == SET_HORIZ_SCALE, false);
				sprintf(forceVOffsetStr, "%+hi", swissSettings.forceVOffset);
				drawSettingEntryString(page, &page_y_ofs, "Forzar Offset Vertical:", forceVOffsetStr, option == SET_VERT_OFFSET, false);
				drawSettingEntryString(page, &page_y_ofs, "Forzar Filtro Vertical:", forceVFilterStr[swissSettings.forceVFilter], option == SET_VERT_FILTER, false);
				drawSettingEntryString(page, &page_y_ofs, "Forzar Renderizado de Campo:", forceVJitterStr[swissSettings.forceVJitter], option == SET_FIELD_RENDER, false);
				drawSettingEntryBoolean(page, &page_y_ofs, "Desactivar Tramado Alfa:", swissSettings.disableDithering, option == SET_ALPHA_DITHER, false);
				drawSettingEntryBoolean(page, &page_y_ofs, "Forzar Filtro Anisotropico:", swissSettings.forceAnisotropy, option == SET_ANISO_FILTER, false);
				drawSettingEntryString(page, &page_y_ofs, "Forzar Pantalla Ancha:", forceWidescreenStr[swissSettings.forceWidescreen], option == SET_WIDESCREEN, false);
				drawSettingEntryString(page, &page_y_ofs, "Forzar Tasa de Sondeo:", forcePollRateStr[swissSettings.forcePollRate], option == SET_POLL_RATE, false);
				drawSettingEntryString(page, &page_y_ofs, "Invertir Stick de Camara:", invertCStickStr[swissSettings.invertCStick], option == SET_INVERT_CAMERA, false);
				drawSettingEntryString(page, &page_y_ofs, "Cambiar Stick de Camara:", swapCStickStr[swissSettings.swapCStick], option == SET_SWAP_CAMERA, false);
			} else {
				sprintf(triggerLevelStr, "%hhu", swissSettings.triggerLevel);
				drawSettingEntryString(page, &page_y_ofs, "Nivel de Trigger Digital:", triggerLevelStr, option == SET_TRIGGER_LEVEL, false);
				drawSettingEntryString(page, &page_y_ofs, "Emular Streaming de Audio:", emulateAudioStreamStr[swissSettings.emulateAudioStream], option == SET_AUDIO_STREAM, false);
				drawSettingEntryString(page, &page_y_ofs, "Emular Velocidad de Lectura:", emulateReadSpeedStr[swissSettings.emulateReadSpeed], option == SET_READ_SPEED, false);
				drawSettingEntryBoolean(page, &page_y_ofs, "Emular Adaptador de Red:", swissSettings.emulateEthernet, option == SET_EMULATE_ETHERNET, false);
				drawSettingEntryBoolean(page, &page_y_ofs, "Preferir Inicio Limpio:", swissSettings.preferCleanBoot, option == SET_CLEAN_BOOT, false);
				drawSettingEntryString(page, &page_y_ofs, "Restablecer ajustes por defecto", NULL, option == SET_DEFAULTS, false);
			}
		}
	}
	// If we have a tooltip for this page/option, add a fading label telling the user to press Y for help
	add_tooltip_label(page, page_num, option);
	
	DrawPublish(page);
	return page;
}

void settings_toggle(int page, int option, int direction, ConfigEntry *gameConfig) {
	if(page == PAGE_GLOBAL) {
		switch(option) {
			case SET_SYS_SOUND:
				swissSettings.sramStereo ^= 1;
			break;
			case SET_SCREEN_POS:
				if(swissSettings.aveCompat == 1) {
					swissSettings.sramHOffset /= 2;
					swissSettings.sramHOffset += direction;
					swissSettings.sramHOffset *= 2;
				}
				else {
					swissSettings.sramHOffset += direction;
				}
				VIDEO_SetAdjustingValues(swissSettings.sramHOffset, 0);
			break;
			case SET_SYS_LANG:
				swissSettings.sramLanguage += direction;
				if(swissSettings.sramLanguage > SYS_LANG_DUTCH)
					swissSettings.sramLanguage = SYS_LANG_ENGLISH;
				if(swissSettings.sramLanguage < SYS_LANG_ENGLISH)
					swissSettings.sramLanguage = SYS_LANG_DUTCH;
			break;
			case SET_CONFIG_DEV:
			{
				int curDevicePos = -1;
				
				// Set it to the first writable device available
				if(swissSettings.configDeviceId == DEVICE_ID_UNK) {
					for(int i = 0; i < MAX_DEVICES; i++) {
						if(allDevices[i] != NULL && (allDevices[i]->features & FEAT_CONFIG_DEVICE)) {
							swissSettings.configDeviceId = allDevices[i]->deviceUniqueId;
							return;
						}
					}
				}
				
				// get position in allDevices for current save device
				for(int i = 0; i < MAX_DEVICES; i++) {
					if(allDevices[i] != NULL && allDevices[i]->deviceUniqueId == swissSettings.configDeviceId) {
						curDevicePos = i;
						break;
					}
				}

				if(curDevicePos >= 0) {
					if(direction > 0) {
						curDevicePos = allDevices[curDevicePos+1] == NULL ? 0 : curDevicePos+1;
					}
					else {
						curDevicePos = curDevicePos > 0 ? curDevicePos-1 : 0;
					}
					// Go to next writable device
					while((allDevices[curDevicePos] == NULL) || !(allDevices[curDevicePos]->features & FEAT_CONFIG_DEVICE)) {
						curDevicePos += direction;
						curDevicePos = (curDevicePos + MAX_DEVICES) % MAX_DEVICES;
					}
					if(allDevices[curDevicePos] != NULL) {
						swissSettings.configDeviceId = allDevices[curDevicePos]->deviceUniqueId;
					}
				}
			}
			break;
			case SET_SWISS_VIDEOMODE:
				swissSettings.uiVMode += direction;
				swissSettings.uiVMode = (swissSettings.uiVMode + 5) % 5;
			break;
			case SET_FILEBROWSER_TYPE:
				swissSettings.fileBrowserType += direction;
				swissSettings.fileBrowserType = (swissSettings.fileBrowserType + 3) % 3;
			break;
			case SET_FILE_MGMT:
				swissSettings.enableFileManagement ^=1;
			break;
			case SET_RECENT_LIST:
				swissSettings.recentListLevel += direction;
				swissSettings.recentListLevel = (swissSettings.recentListLevel + 3) % 3;
			break;
			case SET_SHOW_HIDDEN:
				swissSettings.showHiddenFiles ^= 1;
			break;
			case SET_HIDE_UNK:
				swissSettings.hideUnknownFileTypes ^= 1;
			break;
			case SET_FLATTEN_DIR:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA, "Directorio Actual", &swissSettings.flattenDir, sizeof(swissSettings.flattenDir) - 1);
			break;
			case SET_STOP_MOTOR:
				swissSettings.stopMotor ^= 1;
			break;
			case SET_EXI_SPEED:
				swissSettings.exiSpeed ^= 1;
			break;
			case SET_AVE_COMPAT:
				swissSettings.aveCompat += direction;
				swissSettings.aveCompat = (swissSettings.aveCompat + 5) % 5;
			break;
			case SET_FORCE_DTVSTATUS:
				if(swissSettings.aveCompat < 3)
					swissSettings.forceDTVStatus ^= 1;
			break;
			case SET_ENABLE_USBGECKODBG:
				if(devices[DEVICE_CUR] != &__device_usbgecko && deviceHandler_getDeviceAvailable(&__device_usbgecko))
					swissSettings.debugUSB ^= 1;
			break;
		}
		switch(option) {
			case SET_SWISS_VIDEOMODE:
			case SET_AVE_COMPAT:
			case SET_FORCE_DTVSTATUS:
			{
				// Change Swiss video mode if it was modified.
				GXRModeObj *forcedMode = getVideoModeFromSwissSetting(swissSettings.uiVMode);
				DrawVideoMode(forcedMode);
			}
			break;
		}
	}
	else if(page == PAGE_NETWORK) {
		switch(option) {
			case SET_INIT_NET:
				swissSettings.initNetworkAtStart ^= 1;
			break;
			case SET_BBA_LOCALIP:
				DrawGetTextEntry(ENTRYMODE_IP, "Direccion IPv4", &swissSettings.bbaLocalIp, sizeof(swissSettings.bbaLocalIp) - 1);
			break;
			case SET_BBA_NETMASK:
				DrawGetTextEntry(ENTRYMODE_NUMERIC, "Mascara de red IPv4", &swissSettings.bbaNetmask, 2);
			break;
			case SET_BBA_GATEWAY:
				DrawGetTextEntry(ENTRYMODE_IP, "Puerta de enlace IPv4", &swissSettings.bbaGateway, sizeof(swissSettings.bbaGateway) - 1);
			break;
			case SET_BBA_DHCP:
				swissSettings.bbaUseDhcp ^= 1;
			break;
			case SET_FSP_HOSTIP:
				DrawGetTextEntry(ENTRYMODE_IP, "IP Host FSP", &swissSettings.fspHostIp, sizeof(swissSettings.fspHostIp) - 1);
			break;
			case SET_FSP_PORT:
				DrawGetTextEntry(ENTRYMODE_NUMERIC, "Puerto FSP", &swissSettings.fspPort, 5);
			break;
			case SET_FSP_PASS:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA|ENTRYMODE_MASKED, "Clave FSP", &swissSettings.fspPassword, sizeof(swissSettings.fspPassword) - 1);
			break;
			case SET_FSP_PMTU:
				DrawGetTextEntry(ENTRYMODE_NUMERIC, "Ruta MTU FSP", &swissSettings.fspPathMtu, 4);
			break;
			case SET_FTP_HOSTIP:
				DrawGetTextEntry(ENTRYMODE_IP, "IP Host FTP", &swissSettings.ftpHostIp, sizeof(swissSettings.ftpHostIp) - 1);
			break;
			case SET_FTP_PORT:
				DrawGetTextEntry(ENTRYMODE_NUMERIC, "Puerto FTP", &swissSettings.ftpPort, 5);
			break;
			case SET_FTP_USER:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA, "Usuario FTP", &swissSettings.ftpUserName, sizeof(swissSettings.ftpUserName) - 1);
			break;
			case SET_FTP_PASS:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA|ENTRYMODE_MASKED, "Clave FTP", &swissSettings.ftpPassword, sizeof(swissSettings.ftpPassword) - 1);
			break;
			case SET_FTP_PASV:
				swissSettings.ftpUsePasv ^= 1;
			break;
			case SET_SMB_HOSTIP:
				DrawGetTextEntry(ENTRYMODE_IP, "IP Host SMB", &swissSettings.smbServerIp, sizeof(swissSettings.smbServerIp) - 1);
			break;
			case SET_SMB_SHARE:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA, "Compartir SMB", &swissSettings.smbShare, sizeof(swissSettings.smbShare) - 1);
			break;
			case SET_SMB_USER:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA, "Usuario SMB", &swissSettings.smbUser, sizeof(swissSettings.smbUser) - 1);
			break;
			case SET_SMB_PASS:
				DrawGetTextEntry(ENTRYMODE_NUMERIC|ENTRYMODE_ALPHA|ENTRYMODE_MASKED, "Clave SMB", &swissSettings.smbPassword, sizeof(swissSettings.smbPassword) - 1);
			break;
		}
	}
	else if(page == PAGE_GAME_GLOBAL) {
		switch(option) {
			case SET_IGR:
				swissSettings.igrType += direction;
				swissSettings.igrType = (swissSettings.igrType + 3) % 3;
			break;
			case SET_BS2BOOT:
				swissSettings.bs2Boot += direction;
				swissSettings.bs2Boot = (swissSettings.bs2Boot + 4) % 4;
			break;
			case SET_AUTOBOOT:
				swissSettings.autoBoot ^=1;
			break;
			case SET_EMULATE_MEMCARD:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_MEMCARD))
					swissSettings.emulateMemoryCard ^= 1;
			break;
			case SET_ENABLE_MCPGAMEID:
				swissSettings.disableMCPGameID += direction;
				swissSettings.disableMCPGameID = (swissSettings.disableMCPGameID + 4) % 4;
			break;
			case SET_FORCE_VIDACTIVE:
				if(swissSettings.disableVideoPatches < 2)
					swissSettings.forceVideoActive ^= 1;
			break;
			case SET_ENABLE_VIDPATCH:
				swissSettings.disableVideoPatches += direction;
				swissSettings.disableVideoPatches = (swissSettings.disableVideoPatches + 3) % 3;
			break;
			case SET_PAUSE_AVOUTPUT:
				swissSettings.pauseAVOutput ^=1;
			break;
			case SET_ALL_CHEATS:
				swissSettings.autoCheats ^=1;
			break;
			case SET_WIIRDDBG:
				if(devices[DEVICE_CUR] != &__device_usbgecko && deviceHandler_getDeviceAvailable(&__device_usbgecko))
					swissSettings.wiirdDebug ^=1;
			break;
		}
	}
	else if(page == PAGE_GAME_DEFAULTS) {
		switch(option) {
			case SET_DEFAULT_FORCE_VIDEOMODE:
				if(swissSettings.disableVideoPatches < 2) {
					swissSettings.gameVMode += direction;
					swissSettings.gameVMode = (swissSettings.gameVMode + 15) % 15;
					if(!getDTVStatus()) {
						while(in_range(swissSettings.gameVMode, 4, 7) || in_range(swissSettings.gameVMode, 11, 14)) {
							swissSettings.gameVMode += direction;
							swissSettings.gameVMode = (swissSettings.gameVMode + 15) % 15;
						}
					}
					else if(swissSettings.aveCompat != 0) {
						while(in_range(swissSettings.gameVMode, 6, 7) || in_range(swissSettings.gameVMode, 13, 14)) {
							swissSettings.gameVMode += direction;
							swissSettings.gameVMode = (swissSettings.gameVMode + 15) % 15;
						}
					}
				}
			break;
			case SET_DEFAULT_HORIZ_SCALE:
				if(swissSettings.disableVideoPatches < 2) {
					swissSettings.forceHScale += direction;
					swissSettings.forceHScale = (swissSettings.forceHScale + 9) % 9;
				}
			break;
			case SET_DEFAULT_VERT_OFFSET:
				if(swissSettings.disableVideoPatches < 2)
					swissSettings.forceVOffset += direction;
			break;
			case SET_DEFAULT_VERT_FILTER:
				if(swissSettings.disableVideoPatches < 2) {
					swissSettings.forceVFilter += direction;
					swissSettings.forceVFilter = (swissSettings.forceVFilter + 4) % 4;
				}
			break;
			case SET_DEFAULT_FIELD_RENDER:
				if(swissSettings.disableVideoPatches < 2) {
					swissSettings.forceVJitter += direction;
					swissSettings.forceVJitter = (swissSettings.forceVJitter + 3) % 3;
				}
			break;
			case SET_DEFAULT_ALPHA_DITHER:
				if(swissSettings.disableVideoPatches < 2)
					swissSettings.disableDithering ^= 1;
			break;
			case SET_DEFAULT_ANISO_FILTER:
				swissSettings.forceAnisotropy ^= 1;
			break;
			case SET_DEFAULT_WIDESCREEN:
				swissSettings.forceWidescreen += direction;
				swissSettings.forceWidescreen = (swissSettings.forceWidescreen + 3) % 3;
			break;
			case SET_DEFAULT_POLL_RATE:
				swissSettings.forcePollRate += direction;
				swissSettings.forcePollRate = (swissSettings.forcePollRate + 13) % 13;
			break;
			case SET_DEFAULT_INVERT_CAMERA:
				swissSettings.invertCStick += direction;
				swissSettings.invertCStick = (swissSettings.invertCStick + 4) % 4;
			break;
			case SET_DEFAULT_SWAP_CAMERA:
				swissSettings.swapCStick += direction;
				swissSettings.swapCStick = (swissSettings.swapCStick + 4) % 4;
			break;
			case SET_DEFAULT_TRIGGER_LEVEL:
				swissSettings.triggerLevel += direction * 10;
				swissSettings.triggerLevel = (swissSettings.triggerLevel + 210) % 210;
			break;
			case SET_DEFAULT_AUDIO_STREAM:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_AUDIO_STREAMING)) {
					swissSettings.emulateAudioStream += direction;
					swissSettings.emulateAudioStream = (swissSettings.emulateAudioStream + 3) % 3;
				}
			break;
			case SET_DEFAULT_READ_SPEED:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_READ_SPEED)) {
					swissSettings.emulateReadSpeed += direction;
					swissSettings.emulateReadSpeed = (swissSettings.emulateReadSpeed + 3) % 3;
				}
			break;
			case SET_DEFAULT_EMULATE_ETHERNET:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_ETHERNET))
					swissSettings.emulateEthernet ^= 1;
			break;
			case SET_DEFAULT_CLEAN_BOOT:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->location == LOC_DVD_CONNECTOR))
					swissSettings.preferCleanBoot ^= 1;
			break;
		}
	}
	else if(page == PAGE_GAME && gameConfig != NULL && !gameConfig->forceCleanBoot) {
		switch(option) {
			case SET_FORCE_VIDEOMODE:
				if(swissSettings.disableVideoPatches < 2) {
					gameConfig->gameVMode += direction;
					gameConfig->gameVMode = (gameConfig->gameVMode + 15) % 15;
					if(!getDTVStatus()) {
						while(in_range(gameConfig->gameVMode, 4, 7) || in_range(gameConfig->gameVMode, 11, 14)) {
							gameConfig->gameVMode += direction;
							gameConfig->gameVMode = (gameConfig->gameVMode + 15) % 15;
						}
					}
					else if(swissSettings.aveCompat != 0) {
						while(in_range(gameConfig->gameVMode, 6, 7) || in_range(gameConfig->gameVMode, 13, 14)) {
							gameConfig->gameVMode += direction;
							gameConfig->gameVMode = (gameConfig->gameVMode + 15) % 15;
						}
					}
				}
			break;
			case SET_HORIZ_SCALE:
				if(swissSettings.disableVideoPatches < 2) {
					gameConfig->forceHScale += direction;
					gameConfig->forceHScale = (gameConfig->forceHScale + 9) % 9;
				}
			break;
			case SET_VERT_OFFSET:
				if(swissSettings.disableVideoPatches < 2)
					gameConfig->forceVOffset += direction;
			break;
			case SET_VERT_FILTER:
				if(swissSettings.disableVideoPatches < 2) {
					gameConfig->forceVFilter += direction;
					gameConfig->forceVFilter = (gameConfig->forceVFilter + 4) % 4;
				}
			break;
			case SET_FIELD_RENDER:
				if(swissSettings.disableVideoPatches < 2) {
					gameConfig->forceVJitter += direction;
					gameConfig->forceVJitter = (gameConfig->forceVJitter + 3) % 3;
				}
			break;
			case SET_ALPHA_DITHER:
				if(swissSettings.disableVideoPatches < 2)
					gameConfig->disableDithering ^= 1;
			break;
			case SET_ANISO_FILTER:
				gameConfig->forceAnisotropy ^= 1;
			break;
			case SET_WIDESCREEN:
				gameConfig->forceWidescreen += direction;
				gameConfig->forceWidescreen = (gameConfig->forceWidescreen + 3) % 3;
			break;
			case SET_POLL_RATE:
				gameConfig->forcePollRate += direction;
				gameConfig->forcePollRate = (gameConfig->forcePollRate + 13) % 13;
			break;
			case SET_INVERT_CAMERA:
				gameConfig->invertCStick += direction;
				gameConfig->invertCStick = (gameConfig->invertCStick + 4) % 4;
			break;
			case SET_SWAP_CAMERA:
				gameConfig->swapCStick += direction;
				gameConfig->swapCStick = (gameConfig->swapCStick + 4) % 4;
			break;
			case SET_TRIGGER_LEVEL:
				gameConfig->triggerLevel += direction * 10;
				gameConfig->triggerLevel = (gameConfig->triggerLevel + 210) % 210;
			break;
			case SET_AUDIO_STREAM:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_AUDIO_STREAMING)) {
					gameConfig->emulateAudioStream += direction;
					gameConfig->emulateAudioStream = (gameConfig->emulateAudioStream + 3) % 3;
				}
			break;
			case SET_READ_SPEED:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_READ_SPEED)) {
					gameConfig->emulateReadSpeed += direction;
					gameConfig->emulateReadSpeed = (gameConfig->emulateReadSpeed + 3) % 3;
				}
			break;
			case SET_EMULATE_ETHERNET:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->emulable & EMU_ETHERNET))
					gameConfig->emulateEthernet ^= 1;
			break;
			case SET_CLEAN_BOOT:
				if(devices[DEVICE_CUR] == NULL || (devices[DEVICE_CUR]->location == LOC_DVD_CONNECTOR))
					gameConfig->preferCleanBoot ^= 1;
			break;
			case SET_DEFAULTS:
				if(direction == 0)
					config_defaults(gameConfig);
			break;
		}
	}
}

int show_settings(int page, int option, ConfigEntry *config) {
	// Copy current settings to a temp copy in case the user cancels out
	if(config != NULL) {
		memcpy(&tempConfig, config, sizeof(ConfigEntry));
	}
	memcpy(&tempSettings, &swissSettings, sizeof(SwissSettings));
	
	GXRModeObj *oldmode = getVideoMode();
	while (padsButtonsHeld() & PAD_BUTTON_A){ VIDEO_WaitVSync (); }
	while(1) {
		uiDrawObj_t* settingsPage = settings_draw_page(page, option, config);
		while (!((padsButtonsHeld() & PAD_BUTTON_RIGHT) 
			|| (padsButtonsHeld() & PAD_BUTTON_LEFT) 
			|| (padsButtonsHeld() & PAD_BUTTON_UP) 
			|| (padsButtonsHeld() & PAD_BUTTON_DOWN) 
			|| (padsButtonsHeld() & PAD_BUTTON_B)
			|| (padsButtonsHeld() & PAD_BUTTON_A)
			|| (padsButtonsHeld() & PAD_BUTTON_Y)
			|| (padsButtonsHeld() & PAD_TRIGGER_R)
			|| (padsButtonsHeld() & PAD_TRIGGER_L)))
			{ VIDEO_WaitVSync (); }
		u16 btns = padsButtonsHeld();
		if(btns & PAD_BUTTON_Y) {
			char *tooltip = get_tooltip(page, option);
			if(tooltip) {
				uiDrawObj_t* tooltipBox = DrawPublish(DrawTooltip(tooltip));
				while (padsButtonsHeld() & PAD_BUTTON_Y){ VIDEO_WaitVSync (); }
				while (!((padsButtonsHeld() & PAD_BUTTON_Y) || (padsButtonsHeld() & PAD_BUTTON_B))){ VIDEO_WaitVSync (); }
				DrawDispose(tooltipBox);
			}
		}
		if(btns & PAD_BUTTON_RIGHT) {
			// If we're on a button (Back, Next, Save, Exit), allow left/right movement
			if((page == PAGE_MIN || page == PAGE_MAX) && (option >= settings_count_pp[page]-2) && option < settings_count_pp[page]) {
				option++;
			}
			else if((page != PAGE_MIN && page != PAGE_MAX) && (option >= settings_count_pp[page]-3) && option < settings_count_pp[page]) {
				option++;
			}
			else {
				settings_toggle(page, option, 1, config);
			}
		}
		if(btns & PAD_BUTTON_LEFT) {
			// If we're on a button (Back, Next, Save, Exit), allow left/right movement
			if((page == PAGE_MIN || page == PAGE_MAX) && (option > settings_count_pp[page]-2)) {
				option--;
			}
			else if((page != PAGE_MIN && page != PAGE_MAX) && (option > settings_count_pp[page]-3)) {
				option--;
			}
			else {
				settings_toggle(page, option, -1, config);
			}
		}
		if((btns & PAD_BUTTON_DOWN) && option < settings_count_pp[page])
			option++;
		if((btns & PAD_BUTTON_UP) && option > PAGE_MIN)
			option--;
		if((btns & PAD_TRIGGER_R) && page < PAGE_MAX) {
			page++; option = 0;
		}
		if((btns & PAD_TRIGGER_L) && page > PAGE_GLOBAL) {
			page--; option = 0;
		}
		if((btns & PAD_BUTTON_B))
			option = settings_count_pp[page];
		// Handle all options/buttons here
		if((btns & PAD_BUTTON_A)) {
			// Generic Save/Cancel/Back/Next button actions
			if(option == settings_count_pp[page]-1) {
				uiDrawObj_t *msgBox = DrawPublish(DrawProgressBar(true, 0, "Guardando cambios\205"));
				// Save settings to SRAM
				swissSettings.sram60Hz = getTVFormat() != VI_PAL;
				swissSettings.sramProgressive = getScanMode() == VI_PROGRESSIVE;
				if(swissSettings.aveCompat == 1) {
					swissSettings.sramHOffset &= ~1;
				}
				VIDEO_SetAdjustingValues(swissSettings.sramHOffset, 0);
				sram = __SYS_LockSram();
				sram->display_offsetH = swissSettings.sramHOffset;
				sram->ntd = swissSettings.sram60Hz ? (sram->ntd|0x40):(sram->ntd&~0x40);
				sram->lang = swissSettings.sramLanguage;
				sram->flags = swissSettings.sramProgressive ? (sram->flags|0x80):(sram->flags&~0x80);
				sram->flags = swissSettings.sramStereo ? (sram->flags|0x04):(sram->flags&~0x04);
				sram->flags = (swissSettings.sramVideo&0x03)|(sram->flags&~0x03);
				__SYS_UnlockSram(1);
				while(!__SYS_SyncSram());
				sramex = __SYS_LockSramEx();
				sramex->__padding0 = swissSettings.configDeviceId;
				__SYS_UnlockSramEx(1);
				while(!__SYS_SyncSram());
				// Update our .ini (in memory)
				if(config != NULL) {
					config_update_game(config, true);
					DrawDispose(msgBox);
				}
				// flush settings to .ini
				if(config_update_global(true)) {
					DrawDispose(msgBox);
					msgBox = DrawPublish(DrawMessageBox(D_INFO,"Configuracion Guardada!"));
					sleep(1);
					DrawDispose(msgBox);
				}
				else {
					DrawDispose(msgBox);
					msgBox = DrawPublish(DrawMessageBox(D_INFO,"Fallo al Guardar Config!"));
					sleep(1);
					DrawDispose(msgBox);
				}
				DrawDispose(settingsPage);
				return 1;
			}
			if(option == settings_count_pp[page]) {
				// Exit without saving (revert)
				if(config != NULL) {
					memcpy(config, &tempConfig, sizeof(ConfigEntry));
				}
				memcpy(&swissSettings, &tempSettings, sizeof(SwissSettings));
				VIDEO_SetAdjustingValues(swissSettings.sramHOffset, 0);
				DrawDispose(settingsPage);
				DrawVideoMode(oldmode);
				return 0;
			}
			if((page != PAGE_MAX) && (option == settings_count_pp[page]-2)) {
				page++; option = 0;
			}
			if((page != PAGE_MIN) && (option == settings_count_pp[page]-(page != PAGE_MAX ? 3:2))) {
				page--; option = 0;
			}
			// These use text input, allow them to be accessed with the A button
			if(page == PAGE_GLOBAL && option == SET_FLATTEN_DIR) {
				settings_toggle(page, option, 0, config);
			}
			if(page == PAGE_NETWORK && (in_range(option, SET_BBA_LOCALIP, SET_BBA_GATEWAY) ||
										in_range(option, SET_FSP_HOSTIP,  SET_FTP_PASS) ||
										in_range(option, SET_SMB_HOSTIP,  SET_SMB_PASS))) {
				settings_toggle(page, option, 0, config);
			}
			if(page == PAGE_GAME && option == SET_DEFAULTS) {
				settings_toggle(page, option, 0, config);
			}
		}
		while ((padsButtonsHeld() & PAD_BUTTON_RIGHT) 
				|| (padsButtonsHeld() & PAD_BUTTON_LEFT) 
				|| (padsButtonsHeld() & PAD_BUTTON_UP) 
				|| (padsButtonsHeld() & PAD_BUTTON_DOWN) 
				|| (padsButtonsHeld() & PAD_BUTTON_B) 
				|| (padsButtonsHeld() & PAD_BUTTON_A)
				|| (padsButtonsHeld() & PAD_BUTTON_Y)
				|| (padsButtonsHeld() & PAD_TRIGGER_R)
				|| (padsButtonsHeld() & PAD_TRIGGER_L))
			{ VIDEO_WaitVSync (); }
		DrawDispose(settingsPage);
	}
}
