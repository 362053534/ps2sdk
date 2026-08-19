/*
# _____     ___ ____     ___ ____
#  ____|   |    ____|   |        | |____|
# |     ___|   |____ ___|    ____| |    \    PS2DEV Open Source Project.
#-----------------------------------------------------------------------
# (c) 2020 Francisco Javier Trujillo Mata <fjtrujy@gmail.com>
# Licenced under Academic Free License version 2.0
# Review ps2sdk README & LICENSE files for further details.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <kernel.h>
#include <loadfile.h>
#include <iopcontrol.h>
#include <sifrpc.h>
#include <errno.h>
#include <ps2sdkapi.h>
#include "../../../include/elf-loader.h"

#include "../../elf.h"

#define ELF_MAGIC 0x464c457f
#define ELF_PT_LOAD 1
#define ELF_LOADER_RESIDENT_ADDRESS 0x00094000

#ifdef LOADER_ENABLE_DEBUG_COLORS
#define SET_GS_BGCOLOUR(colour) {*((volatile unsigned long int *)0x120000E0) = colour;}
#else
#define SET_GS_BGCOLOUR(colour)
#endif

// Color status helper in BGR format
#define WHITE_BG 0xFFFFFF // start main
#define CYAN_BG 0xFFFF00 // proper argc count
#define RED_BG  0x0000FF // wrong argc count
#define GREEN_BG 0x00FF00 // before SifLoadELF
#define BLUE_BG 0xFF0000 // after SifLoadELF
#define YELLOW_BG 0x00FFFF // good SifLoadELF return
#define MAGENTA_BG 0xFF00FF // wrong SifLoadELF return
#define ORANGE_BG 0x00A5FF  // after reset IOP
#define BROWN_BG 0x2A2AA5  // before FlushCache
#define PURPBLE_BG 0x800080  // before ExecPS2


//--------------------------------------------------------------
// Redefinition of init/deinit libc:
//--------------------------------------------------------------
// DON'T REMOVE is for reducing binary size. 
// These funtios are defined as weak in /libc/src/init.c
//--------------------------------------------------------------
   void _libcglue_init() {}
   void _libcglue_deinit() {}
   void _libcglue_args_parse(int argc, char **argv) {}

   DISABLE_PATCHED_FUNCTIONS();
   DISABLE_EXTRA_TIMERS_FUNCTIONS();
   PS2_DISABLE_AUTOSTART_PTHREAD();

//--------------------------------------------------------------
//Start of function code:
//--------------------------------------------------------------
// Clear user memory
// PS2Link (C) 2003 Tord Lindstrom (pukko@home.se)
//         (C) 2003 adresd (adresd_ps2dev@yahoo.com)
//--------------------------------------------------------------
static void wipeUserMem(void)
{
	int i;
	for (i = 0x100000; i < GetMemorySize(); i += 64) {
		asm volatile(
			"\tsq $0, 0(%0) \n"
			"\tsq $0, 16(%0) \n"
			"\tsq $0, 32(%0) \n"
			"\tsq $0, 48(%0) \n" ::"r"(i));
	}
}

static unsigned int residentCopyChecksum(const void *buffer, unsigned int size)
{
	const unsigned char *data = (const unsigned char *)buffer;
	unsigned int checksum = 2166136261u;
	unsigned int i;

	for (i = 0; i < size; i++) {
		checksum ^= data[i];
		checksum *= 16777619u;
	}

	return checksum;
}

static void residentCopyFatal(void)
{
	SET_GS_BGCOLOUR(RED_BG);
	for (;;)
		asm volatile("nop");
}

static void applyResidentCopies(void)
{
	const elf_loader_resident_copy_table_t *table;
	unsigned int memorySize;
	unsigned int i;

	table = (const elf_loader_resident_copy_table_t *)(ELF_LOADER_RESIDENT_ADDRESS + ELF_LOADER_RESIDENT_COPY_TABLE_OFFSET);
	if (table->magic == 0)
		return;

	/* 识别到复制表后必须完整通过校验，禁止静默回退。 */
	if (table->magic != ELF_LOADER_RESIDENT_COPY_MAGIC ||
		table->version != ELF_LOADER_RESIDENT_COPY_VERSION ||
		table->count == 0 || table->count > ELF_LOADER_RESIDENT_COPY_MAX_COUNT ||
		table->checksum != residentCopyChecksum(table, sizeof(*table) - sizeof(table->checksum)))
		residentCopyFatal();

	memorySize = GetMemorySize();
	for (i = 0; i < table->count; i++) {
		const elf_loader_resident_copy_entry_t *entry = &table->entries[i];
		unsigned int source = (unsigned int)entry->source;
		unsigned int destination = (unsigned int)entry->destination;
		unsigned int sourceEnd = source + entry->size;
		unsigned int destinationEnd = destination + entry->size;

		if (entry->size == 0 || source < 0x00100000 || destination < 0x00100000 ||
			sourceEnd < source || destinationEnd < destination ||
			sourceEnd > memorySize || destinationEnd > memorySize ||
			(source < destinationEnd && destination < sourceEnd))
			residentCopyFatal();

		memcpy(entry->destination, entry->source, entry->size);
	}
}

//--------------------------------------------------------------
//End of func:  void wipeUserMem(void)
//--------------------------------------------------------------
// *** MAIN ***
// 
//--------------------------------------------------------------
int main(int argc, char *argv[])
{
	static t_ExecData elfdata;
	int ret, i, new_argc;

	elfdata.epc = 0;

	SET_GS_BGCOLOUR(WHITE_BG);
	// arg[0] 分区，没有则为 ""
	// arg[1] ELF 路径，或 mem:XXXXXXXX
	// arg[2] 复位标志（"0" 保留 IOP，其余值复位）
	// arg[3+] 调用方参数（mem: 启动时 argv[3] 是 POPStarter 的虚拟 ELF 名）
	if (argc < 3) {
		SET_GS_BGCOLOUR(RED_BG);
		return -EINVAL;
	}

	int reset = (argv[2][0] != '0');

	char *new_argv[argc - 2];
	int fullPath_length = 1 + strlen(argv[0]) + strlen(argv[1]);
	char fullPath[fullPath_length];
	char virtualName[256];
	strcpy(fullPath, argv[0]);
	strcat(fullPath, argv[1]);
	if (!strncmp(argv[1], "mem:", 4) && argc > 3) {
		// POPStarter 直接启动约定：目标 ELF 的 argv[0] 必须是虚拟 ELF 名。
		strncpy(virtualName, argv[3], sizeof(virtualName) - 1);
		virtualName[sizeof(virtualName) - 1] = '\0';
		new_argv[0] = virtualName;
		for (i = 4; i < argc; i++) {
			new_argv[i - 3] = argv[i];
		}
		new_argc = argc - 3;
	} else {
		new_argv[0] = fullPath;
		for (i = 3; i < argc; i++) {
			new_argv[i - 2] = argv[i];
		}
		new_argc = argc - 2;
	}

	SET_GS_BGCOLOUR(CYAN_BG);

	// Initialize
	SifInitRpc(0);
	if (!strncmp(argv[1], "mem:", 4)) {
		u8 *boot_elf = (u8 *)strtoul(argv[1] + 4, NULL, 16);
		elf_header_t *eh = (elf_header_t *)boot_elf;
		elf_pheader_t *eph;

		SET_GS_BGCOLOUR(GREEN_BG);
		if (_lw((u32)&eh->ident) != ELF_MAGIC)
			return -EINVAL;

		eph = (elf_pheader_t *)(boot_elf + eh->phoff);
		for (i = 0; i < eh->phnum; i++) {
			if (eph[i].type != ELF_PT_LOAD)
				continue;

			memcpy(eph[i].vaddr, boot_elf + eph[i].offset, eph[i].filesz);
			if (eph[i].memsz > eph[i].filesz)
				memset((u8 *)eph[i].vaddr + eph[i].filesz, 0, eph[i].memsz - eph[i].filesz);
		}

		/* 目标ELF落位后再搬运保留数据，避免覆盖仍在运行的调用方。 */
		applyResidentCopies();

		elfdata.epc = eh->entry;
		elfdata.gp = 0;
		ret = 0;
	} else {
		wipeUserMem();

		//Writeback data cache before loading ELF.
		FlushCache(0);
		SET_GS_BGCOLOUR(GREEN_BG);
		SifLoadFileInit();
		ret = SifLoadElf(argv[1], &elfdata);
		SifLoadFileExit();
	}
	SET_GS_BGCOLOUR(BLUE_BG);
	if (ret == 0 && elfdata.epc != 0) {
		SET_GS_BGCOLOUR(YELLOW_BG);

		if (reset) {
			// Let's reset IOP because ELF was already loaded in memory
			while(!SifIopReset(NULL, 0)){};
			while (!SifIopSync()) {};

			SET_GS_BGCOLOUR(ORANGE_BG);

			SifInitRpc(0);
			// Load modules.
			SifLoadFileInit();
			SifLoadModule("rom0:SIO2MAN", 0, NULL);
			SifLoadModule("rom0:MCMAN", 0, NULL);
			SifLoadModule("rom0:MCSERV", 0, NULL);
			SifLoadFileExit();
			SifExitRpc();
		}

		SET_GS_BGCOLOUR(BROWN_BG);

		FlushCache(0);
		FlushCache(2);

		SET_GS_BGCOLOUR(PURPBLE_BG);
		
		return ExecPS2((void *)elfdata.epc, (void *)elfdata.gp, new_argc, new_argv);
	} else {
		SET_GS_BGCOLOUR(MAGENTA_BG);
		SifExitRpc();
		return -ENOENT;
	}
}

//--------------------------------------------------------------
//End of func:  int main(int argc, char *argv[])
//--------------------------------------------------------------
//End of file:  loader.c
//--------------------------------------------------------------
