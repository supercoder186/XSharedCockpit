#include "XPLMMenus.h"
#include "XPLMProcessing.h"
#include "XPLMDataAccess.h"
#include "XPLMPlugin.h"
#include "XPLMUtilities.h"
#include "XPLMPlanes.h"
#include <string>
#include <cstring>
#include <vector>
#if IBM
	#include <windows.h>
#endif

//Using defs
using std::vector;
using std::string;
using std::size_t;

//Variable defs
int menu_container_index;
XPLMMenuID menu_id;
vector<string> dref_strings;
vector <XPLMDataRef> drefs;

//Constant defs
const string master_address = "127.0.0.1";
const int master_port = 49000;
const string slave_adrress = "127.0.0.1";
const int slave_port = 49001;

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

	load_plugin();

	dref_strings.push_back("sim/flightmodel/position/local_x");
	dref_strings.push_back("sim/flightmodel/position/local_y");
	dref_strings.push_back("sim/flightmodel/position/local_z");
	dref_strings.push_back("sim/flightmodel/position/psi");
	dref_strings.push_back("sim/flightmodel/position/theta");
	dref_strings.push_back("sim/flightmodel/position/phi");
	dref_strings.push_back("sim/flightmodel/position/P");
	dref_strings.push_back("sim/flightmodel/position/Q");
	dref_strings.push_back("sim/flightmodel/position/R");
	dref_strings.push_back("sim/flightmodel/position/local_vx");
	dref_strings.push_back("sim/flightmodel/position/local_vy");
	dref_strings.push_back("sim/flightmodel/position/local_vz");

	XPLMDebugString("XSharedCockpit has started");
	return 1;
}

PLUGIN_API void	XPluginStop(void) {
	XPLMDestroyMenu(menu_id);
}

PLUGIN_API void XPluginDisable(void) {
	drefs.clear();
}

PLUGIN_API int  XPluginEnable(void) {
	XPLMDebugString("XSharedCockpit initializing");
	for (auto const& dref_name: dref_strings)
		drefs.push_back(XPLMFindDataRef(dref_name.c_str()));

	XPLMDebugString("XSharedCockpit initialized");
	return 1; 
}


PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void * inParam) { }

void load_plugin() {
	char name[512];
	char path[512];
	XPLMGetNthAircraftModel(0, name, path);
	string cfg_file(path);
	size_t pos = cfg_file.find(name);
	cfg_file = cfg_file.substr(0, pos);
	cfg_file.append("smartcopilot.cfg");
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