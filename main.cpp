#include "XPLMMenus.h"
#include "XPLMProcessing.h"
#include "XPLMDataAccess.h"
#include "XPLMPlugin.h"
#include "XPLMUtilities.h"
#include "pluginpath.h"
#include <string.h>
#include <cstring>
#include <vector>
#if IBM
	#include <windows.h>
#endif

//Using defs
using std::vector;
using std::string;

//Variable defs
int menu_container_index;
XPLMMenuID menu_id;

//Function defs
void menu_handler(void*, void*);
void load_plugin();
float loop(float elapsed1, float elapsed2, int ctr, void* refcon);


PLUGIN_API int XPluginStart(char * outName, char * outSig,	char * outDesc){
	strcpy(outName, "XSharedCockpit");
	strcpy(outSig, "com.thecodingaviator.xsharedcockpit");
	strcpy(outDesc, "A plugin to allow you to fly together with your friends");

	menu_container_index = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "XSharedCockpit", 0, 0);
	menu_id = XPLMCreateMenu("Sample Menu", XPLMFindPluginsMenu(), menu_container_index, menu_handler, NULL);
	XPLMAppendMenuItem(menu_id, "Toggle Master", (void*)"Toggle Master", 1);
	XPLMAppendMenuItem(menu_id, "Toggle Slave", (void*)"Toggle Slave", 1);
	XPLMAppendMenuItem(menu_id, "Reload all plugins", (void*)"Reload Plugins", 1);
	return 1;
}

PLUGIN_API void	XPluginStop(void) {
	XPLMDestroyMenu(menu_id);
}

PLUGIN_API void XPluginDisable(void) { }
PLUGIN_API int  XPluginEnable(void)  { return 1; }
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void * inParam) { }

void load_plugin() {
	
}

void menu_handler(void* in_menu_ref, void* in_item_ref) {
	if (!strcmp((const char*)in_item_ref, "Toggle Master"))
	{

		//Toggle Master
	}
	else if (!strcmp((const char*)in_item_ref, "Toggle Slave"))
	{
		//Toggle Slave
	}
}

float loop(float elapsed1, float elapsed2, int ctr, void* refcon) {
	return -1;
}