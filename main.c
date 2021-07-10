#include <psp2/display.h>
#include <psp2/io/fcntl.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/power.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "graphics.h"

#define printf psvDebugScreenPrintf

int cp(const char *to, const char *from)
{
    SceUID fd_to, fd_from;
    char buf[16*1024];
    ssize_t nread;
    int saved_errno;
	//
    fd_from = sceIoOpen(from, SCE_O_RDONLY, 0777);
    if (fd_from < 0)
        return -1;

    fd_to = sceIoOpen(to, SCE_O_WRONLY|SCE_O_CREAT, 0777);
    if (fd_to < 0)
        goto out_error;

    while (nread = sceIoRead(fd_from, buf, sizeof buf), nread > 0)
    {
        char *out_ptr = buf;
        ssize_t nwritten;

        do {
            nwritten = sceIoWrite(fd_to, out_ptr, nread);

            if (nwritten >= 0)
            {
                nread -= nwritten;
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        } while (nread > 0);
    }

    if (nread == 0)
    {
        if (sceIoClose(fd_to) < 0)
        {
            fd_to = -1;
            goto out_error;
        }
        sceIoClose(fd_from);

        /* Success! */
        return 0;
    }

  out_error:
    saved_errno = errno;

    sceIoClose(fd_from);
    if (fd_to >= 0)
        sceIoClose(fd_to);

    errno = saved_errno;
    return -1;
}

int WriteFile(char *file, void *buf, int size) {
	SceUID fd = sceIoOpen(file, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0777);
	if (fd < 0)
		return fd;

	int written = sceIoWrite(fd, buf, size);

	sceIoClose(fd);
	return written;
}


int main(int argc, char *argv[]) {
	void *buf = malloc(0x100);
	
	psvDebugScreenInit();
    printf("VSOI v0.4\n\n");
    
	SceUID fd;
	fd = sceIoOpen("vs0:app/NPXS10000/eboot.origin", SCE_O_RDONLY, 0777);

	if (fd < 0)
	{
		// First run
		printf("First run detected. Replacing Near with VitaShell...\n");
    
		// Mount vs0 as RW
		printf("Unmounting partition with id 0x%X\n", 0x300); // 0x300 is vs0
		vshIoUmount(0x300, 0, 0, 0);
		
		printf("Mounting partition 0x%X with RW permissions...\n", 0x300);
		_vshIoMount(0x300, 0, 2, buf);
	
		// Copy VitaShell's eboot.bin to vs0:app/NPXS10000/eboot.bin
		
		// Backup Near's eboot elsewhere
		if (cp("vs0:app/NPXS10000/eboot.origin", "vs0:app/NPXS10000/eboot.bin") != 0)
			printf("Error backing up the eboot.\n");
		else
			printf("Eboot backup created.\n");
	
		// Remove Near's eboot and copy VitaShell's to that directory
		fd = sceIoOpen("ux0:data/vsEboot.bin", SCE_O_RDONLY, 0777);
		if (fd >= 0)
		{
			printf("Using ux0:data/vsEboot.bin");
			sceIoRemove("vs0:app/NPXS10000/eboot.bin");
			if (cp("vs0:app/NPXS10000/eboot.bin", "ux0:data/vsEboot.bin"))
				printf("Successfully copied eboot to directory!\n");
			else
				printf("Error copying eboot to directory!\n");
		}
		else
		{
			fd = sceIoOpen("app0:vsEboot.bin", SCE_O_RDONLY, 0777);		
			if (fd >= 0)
			{
				printf("Using app0:vsEboot.bin\n");
				sceIoRemove("vs0:app/NPXS10000/eboot.bin");
				if (cp("vs0:app/NPXS10000/eboot.bin", "app0:vsEboot.bin") >= 0)
					printf("Successfully copied eboot to directory!\n");
				else
					printf("Error copying eboot to directory!\n");					
			}
			else
			{
				printf("ERROR: VitaShell eboot not found! Exiting in 5 seconds...\n");
				sceKernelDelayThread(5 * 1000 * 1000);
				return 0;
			}
		}

		// Remove app.db and reboot to force db rebuild	
		printf("Removing app.db...\n");
		sceIoRemove("ur0:shell/db/app.db");
		
		printf("\n\nRebooting in 10 seconds...");
		sceKernelDelayThread(10 * 1000 * 1000);
		scePowerRequestColdReset();
	}
	else
	{
		// Second run
		printf("Second run detected. Restoring Near in 10 seconds (exit now if you don't want to do this)...\n");
		sceKernelDelayThread(10 * 1000 * 1000);

		sceIoClose(fd);

		// Mount vs0 as RW
		printf("Unmounting partition with id 0x%X\n", 0x300); // 0x300 is vs0
		vshIoUmount(0x300, 0, 0, 0);
		
		printf("Mounting partition 0x%X with RW permissions...\n", 0x300);
		_vshIoMount(0x300, 0, 2, buf);

		// Restore Near's eboot
		sceIoRemove("vs0:app/NPXS10000/eboot.bin");
		if (cp("vs0:app/NPXS10000/eboot.bin", "vs0:app/NPXS10000/eboot.origin") >= 0)
			printf("Successfully restored eboot to directory!\n");
		else
		{
			printf("Error restoring eboot to directory!\n");
			sceKernelDelayThread(10 * 1000 * 1000);
			return 0;
		}

		// Remove backup eboot
		if (sceIoRemove("vs0:app/NPXS10000/eboot.origin") < 0) 
			printf("Backup eboot not found.\n");
		else
			printf("Removed existing backup eboot.\n");

		// Remove app.db and reboot to force db rebuild	
		printf("Removing app.db...\n");
		sceIoRemove("ur0:shell/db/app.db");
		
		printf("\n\nRebooting in 10 seconds...");
		sceKernelDelayThread(10 * 1000 * 1000);
		scePowerRequestColdReset();
	}

	return 0;
}
