/****************************************************************************
 * Final steps of the installation.
 * Installs the hooks at their target addresses.
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Ndless code.
 *
 * The Initial Developer of the Original Code is Olivier ARMAND
 * <olivier.calc@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2010-2014
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): 
 *                 Geoffrey ANNEHEIM <geoffrey.anneheim@gmail.com>, Excale
 ****************************************************************************/

#include <os.h>
#include <ngc.h>

#include "calchook.h"
#include "ndless.h"

// OS-specific
// Call to the dialog box display telling that the format isn't recognized.
static unsigned const ploader_hook_addrs[] = {0x10009984, 0x1000995C, 0x10009924, 0x10009924, 0x100098CC, 0x100098CC,
						0x1000A988, 0x1000A95C, 0x1000A920, 0x1000A924,
						0x1000A810, 0x1000A7D0, 0x0, 0x0,
						0x0, 0x0, 0x1000A79C, 0x1000A78C,
						0x1000A7B0, 0x1000A7AC,
						0x1000A868, 0x1000A864};

// initialized at load time. Kept in resident program memory, use nl_is_3rd_party_loader to read it.
static BOOL loaded_by_3rd_party_loader = FALSE;

BOOL ins_loaded_by_3rd_party_loader(void) {
	return loaded_by_3rd_party_loader;
}

static unsigned const end_of_init_addrs[] = {0x100104F0, 0x10010478, 0x100104BC, 0x1001046C, 0x1000ED30, 0x1000ECE0,
						0x1001264C, 0x100125D0, 0x10012470, 0x10012424,
						0x0, 0x0, 0x0, 0x0,
						0x0, 0x0, 0x0, 0x0,
						0x10012420, 0x100123CC,
						0x100124F4, 0x100124A0};

void ins_uninstall(void) {
	ut_calc_reboot();
}

/* argv[0]=
 *         NULL if loaded by Ndless's stage1 at installation or OS startup
 *         "L" if loaded by a third party loader such as nLaunchy
 *         <path to ndless_resources> if run from the OS documents screen for uninstallation      
 */
int main(int __attribute__((unused)) argc, char* argv[]) {
	ut_debug_trace(INSTTR_INS_ENTER);
	ut_read_os_version_index();
	BOOL installed = FALSE;
// useless if non persistent and won't work since stage1 set it up
#if 0
	struct next_descriptor *installed_next_descriptor = ut_get_next_descriptor();
	if (installed_next_descriptor) {
		if (*(unsigned*)installed_next_descriptor->ext_name == 0x534C444E) // 'NDLS'
			installed = TRUE;
		else
			ut_panic("unknown N-ext");
	}
#endif

	if (!argv[0] || argv[0][0] == 'L') // not opened from the Documents screen
		ints_setup_handlers();
	else
		installed = TRUE;

	//These initialization routines need syscalls, so execute them after ints_setup_handlers
	extern void initialise_monitor_handles();
	initialise_monitor_handles();
	extern void __cpp_init();
	__cpp_init();

	if (installed && nl_loaded_by_3rd_party_loader())
		return 0; // do nothing

	if (!installed) {
		// 3.9, 4.0 don't need to be rebooted
		if(ut_os_version_index < 10)
		{
			// Startup programs cannot be run safely there, as stage1 is being executed in unregistered memory. Run them asynchronously in another hook.
			if(end_of_init_addrs[ut_os_version_index] != 0)
				HOOK_INSTALL(end_of_init_addrs[ut_os_version_index], plh_startup_hook);
		}
		else // 3.9, 4.0
		{
			// Run startup programs (and successmsg hook installation) now
			plh_startup();

			// Patch the annoying TI_RM_GetString error message
			if(ut_os_version_index == 10) // 3.9.0
				*(volatile uint32_t *) 0x10111778 = 0xEA000004;
			else if(ut_os_version_index == 11) // 3.9.0 CAS
				*(volatile uint32_t *) 0x10111574 = 0xEA000004;
			else if(ut_os_version_index == 16) // 3.9.1 CX
				*(volatile uint32_t *) 0x1011193C = 0xEA000004;
			else if(ut_os_version_index == 17) // 3.9.1 CX CAS
				*(volatile uint32_t *) 0x10111768 = 0xEA000004;
			else if(ut_os_version_index == 20) // 4.0.3 CX
				*(volatile uint32_t *) 0x1011811C = 0xEA000004;
			else if(ut_os_version_index == 21) // 4.0.3 CX CAS
				*(volatile uint32_t *) 0x10117F6C = 0xEA000004;

			// The next HOOK_INSTALL invocation clears the cache for us
		}

		if(ut_os_version_index < 6)
			HOOK_INSTALL(ploader_hook_addrs[ut_os_version_index], plh_hook_31);
		else if(ut_os_version_index) //plh_hook_36 works for 3.9.1 as well
			HOOK_INSTALL(ploader_hook_addrs[ut_os_version_index], plh_hook_36);

		lua_install_hooks();
		calchook_install();
	}

	if (argv[0] && argv[0][0] == 'L') { // third-party launcher
		loaded_by_3rd_party_loader = TRUE;
		return 0;
	}
	
	if (installed) { // ndless_resources.tns run: uninstall
		if (show_msgbox_2b("Ndless", "Do you really want to uninstall Ndless r" STRINGIFY(NDLESS_REVISION) "?\nThe device will reboot.", "Yes", "No") == 2)
			return 0;

		ins_uninstall();
	}

	if(ut_os_version_index < 6) {
		PCFD fd = NU_Open("/phoenix/install/TI-Nspire.tnc", 0, 0); // any file will do to get a fd
		if (fd > 0){
			NU_Close(fd - 1); // the FILE* is unknown, this is an heuristic
			NU_Close(fd);
		}

		// Continue OS startup
		// Simulate the prolog of the thread function for correct function return. Set r4 to a dummy variable, written to by a sub-function that follows.
		unsigned const init_task_return_addrs[] = {0x10001548, 0x10001548, 0x10001510, 0x10001510, 0x100014F8, 0x100014F8};
		__asm volatile("add lr, pc, #8; stmfd sp!, {r4-r6,lr}; sub sp, sp, #0x18; mov r4, sp; mov pc, %0" : : "r" (init_task_return_addrs[ut_os_version_index]));
	}

	return 0;
}


// OS-specific
// gui_gc_drawIcon + 4
const unsigned ins_successmsg_hook_addrs[] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
						0x1002DE38, 0x1002DDC8, 0x1002D388, 0x1002D348,
						0x1002E2C0, 0x1002E224, 0x0, 0x0,
						0x0, 0x0, 0x1002D804, 0x1002D798,
						0x1002D818, 0x1002D7C0,
						0x1002F4EC, 0x1002F494};

void ins_install_successmsg_hook(void) {
	if(ins_successmsg_hook_addrs[ut_os_version_index] == 0)
		return;

	HOOK_INSTALL(ins_successmsg_hook_addrs[ut_os_version_index], ins_successsuccessmsg_hook);
}

// chained after the startup programs execution
HOOK_DEFINE(ins_successsuccessmsg_hook) {
	// OS-specific: reg number
	if (HOOK_SAVED_REGS(ins_successsuccessmsg_hook)[2] == 0x171) {
		Gc gc = (Gc)HOOK_SAVED_REGS(ins_successsuccessmsg_hook)[0];
		gui_gc_setColor(gc, has_colors ? 0x32cd32 : 0x505050);
		gui_gc_setFont(gc, SerifRegular9);
		gui_gc_drawString(gc, (char*) u"Ndless installed!", 25, 4, GC_SM_TOP);
	}
	HOOK_RESTORE_RETURN(ins_successsuccessmsg_hook);
}
