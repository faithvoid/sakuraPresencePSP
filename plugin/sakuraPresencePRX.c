#include <pspkernel.h>
#include <pspdebug.h>
#include <pspnet.h>
#include <pspnet_apctl.h>
#include <pspnet_inet.h>
#include <pspwlan.h>
#include <psputility_netmodules.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspumd.h>
#include <pspge.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

PSP_MODULE_INFO("sakuraPresencePRX", 0x1000, 1, 1);

// TODO: Move these to a .cfg file
#define SERVER_IP "192.168.1.110"
#define SERVER_PORT 1102
#define LOG_FILE "ms0:/log.txt"

// Log each step of the process to log.txt in case of error.
void logline(const char* msg) {
    SceUID fd = sceIoOpen(LOG_FILE, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0666);
    if (fd >= 0) {
        sceIoWrite(fd, msg, strlen(msg));
        sceIoWrite(fd, "\n", 1);
        sceIoClose(fd);
    }
}

// Set up the network by initializing network modules, then connecting to the first available wireless network. 
int ensureNetworkReady() {
    int err;

    logline("Loading net modules...");
    if ((err = sceUtilityLoadNetModule(PSP_NET_MODULE_COMMON)) < 0) return err;
    if ((err = sceUtilityLoadNetModule(PSP_NET_MODULE_INET)) < 0) return err;
    if ((err = sceNetInit(128 * 1024, 0x30, 0x1000, 0x30, 0x1000)) < 0) return err;
    if ((err = sceNetInetInit()) < 0) return err;
    if ((err = sceNetApctlInit(0x1800, 48)) < 0) return err;
    if ((err = sceNetApctlConnect(1)) < 0) return err;

    int lastState = -1;
    while (1) {
        int state;
        sceNetApctlGetState(&state);
        if (state != lastState) {
            lastState = state;
        }
        if (state == 4) break;
    }

    logline("Network connected!");
    return 0;
}

// Attempt to extract game ID from disc0, unsure if this works or not. Might work from umd0: if not, but not all games mount via disc0/umd0, so this needs a fallback.
int extractGameID(char* gameID, size_t gameIDSize) {
    logline("Opening PARAM.SFO...");
    SceUID fd = sceIoOpen("disc0:/PSP_GAME/PARAM.SFO", PSP_O_RDONLY, 0777);
    if (fd < 0) {
        logline("Failed to open PARAM.SFO");
        return -1;
    }

    int size = sceIoLseek(fd, 0, PSP_SEEK_END);
    sceIoLseek(fd, 0, PSP_SEEK_SET);
    if (size <= 0 || size > 2048) {
        sceIoClose(fd);
        logline("Invalid PARAM.SFO size");
        return -1;
    }

    static unsigned char buffer[2048];
    if (sceIoRead(fd, buffer, size) != size) {
        sceIoClose(fd);
        logline("Failed to read PARAM.SFO");
        return -1;
    }
    sceIoClose(fd);

    uint32_t keyTableOff = *(uint32_t *)(buffer + 8);
    uint32_t dataTableOff = *(uint32_t *)(buffer + 12);
    uint32_t entryCount = *(uint32_t *)(buffer + 16);
    unsigned char* entries = buffer + 20;

    for (uint32_t i = 0; i < entryCount; i++) {
        unsigned char* entry = entries + i * 16;
        uint16_t keyOff = *(uint16_t *)(entry + 0);
        uint32_t dataOff = *(uint32_t *)(entry + 12);
        const char* key = (const char*)(buffer + keyTableOff + keyOff);
        if (strcmp(key, "DISC_ID") == 0 || strcmp(key, "TITLE_ID") == 0) {
            const char* value = (const char*)(buffer + dataTableOff + dataOff);
            strncpy(gameID, value, gameIDSize - 1);
            gameID[gameIDSize - 1] = '\0';
            logline("Game ID found");
            return 0;
        }
    }

    logline("Game ID not found");
    return -1;
}

// Send system type and title ID as packet to sakuraPresence
void sendGameID(const char* gameID) {
    int sock = sceNetInetSocket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        logline("Socket creation failed");
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    char packet[128];
    strcpy(packet, "{\"psp\":true,\"id\":\"");
    strcat(packet, gameID);
    strcat(packet, "\"}");

    int sent = sceNetInetSendto(sock, packet, strlen(packet), 0, (struct sockaddr*)&addr, sizeof(addr));
    if (sent > 0)
        logline("Game ID sent");
    else
        logline("sendto failed");

    sceNetInetClose(sock);
}

void cleanupNetwork() {
    logline("Cleaning up network");
    sceNetApctlDisconnect();
    sceNetApctlTerm();
    sceNetInetTerm();
    sceNetTerm();
    sceUtilityUnloadNetModule(PSP_NET_MODULE_INET);
    sceUtilityUnloadNetModule(PSP_NET_MODULE_COMMON);
}

// Main SakuraPresencePRX thread
int sendThread(SceSize args, void *argp) {
    logline("Thread started");
    if (ensureNetworkReady() == 0) {
        char gameID[32] = {0};
        if (extractGameID(gameID, sizeof(gameID)) == 0) {
            sendGameID(gameID);
        } else {
            logline("extractGameID failed");
        }
        cleanupNetwork();
    } else {
        logline("Network init failed");
    }
    logline("Thread done");
    sceKernelExitDeleteThread(0);
    return 0;
}

int module_start(SceSize args, void *argp) {
    logline("module_start");
    int thid = sceKernelCreateThread("sakuraPresenceThread", sendThread, 0x18, 0x10000, PSP_THREAD_ATTR_USER, NULL);
    if (thid >= 0) {
        sceKernelStartThread(thid, 0, NULL);
    } else {
        logline("Thread creation failed");
    }
    return 0;
}

int module_stop(SceSize args, void *argp) {
    logline("module_stop");
    return 0;
}
