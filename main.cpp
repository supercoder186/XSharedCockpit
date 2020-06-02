//External libraries
#include "XPLMMenus.h"
#include "XPLMProcessing.h"
#include "XPLMDataAccess.h"
#include "XPLMPlugin.h"
#include "XPLMUtilities.h"
#include "XPLMPlanes.h"
#include "SimpleIni.h"

//Standard libraries
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <iterator>
#include <evpp/udp/udp_server.h>
#include <evpp/udp/udp_message.h>

#if IBM
	#include <windows.h>
#endif

//Using object defs
using std::vector;
using std::string;
using std::size_t;
using std::istringstream;

//Using function defs
using std::to_string;
using std::copy;
using std::replace;
using std::stoi;
using std::stod;
using std::stof;
using std::back_inserter;
using std::getline;

//Variable defs
int menu_container_index;
XPLMMenuID menu_id;
vector<string> dref_strings;
vector <XPLMDataRef> drefs;
vector <XPLMDataTypeID> dref_types;

//Constant defs
const string master_address = "127.0.0.1";
const int master_port = 49000;
const string slave_adrress = "127.0.0.1";
const int slave_port = 49001;
bool running = false;
bool is_master = false;
bool is_connected = false;

//Function defs
void menu_handler(void*, void*);
void load_plugin();
float loop(float elapsed1, float elapsed2, int ctr, void* refcon);
bool number_contains(int number, int check);


//X-Plane API functions
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

PLUGIN_API int XPluginEnable(void) {
	XPLMDebugString("XSharedCockpit initializing");
	for (auto const& dref_name : dref_strings) {
		auto dref = XPLMFindDataRef(dref_name.c_str());
		drefs.push_back(dref);
		dref_types.push_back(XPLMGetDataRefTypes(dref));
	}

	XPLMDebugString("XSharedCockpit initialized");
	return 1; 
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void * inParam) { }

void load_plugin() {
	char name[512];
	char path[512];
	XPLMGetNthAircraftModel(0, name, path);
	string cfg_file_path(path);
	size_t pos = cfg_file_path.find(name);
	cfg_file_path = cfg_file_path.substr(0, pos);
	cfg_file_path.append("smartcopilot.cfg");
	XPLMDebugString(cfg_file_path.c_str());
	CSimpleIniA ini;
	ini.SetUnicode();
	ini.LoadFile(cfg_file_path.c_str());
}

//My functions
void menu_handler(void* in_menu_ref, void* in_item_ref) {
	if (!strcmp((const char*)in_item_ref, "Toggle Master"))	{
		toggle_master();
	}
	else if (!strcmp((const char*)in_item_ref, "Toggle Slave"))	{
		toggle_slave();
	}
}

bool number_contains(int number, int check) {
	for (int i = 16; i >= 1; i /= 2) {
		if (number > i && i == check) {
			return true;
		}
	}

	return false;
}

void send_datarefs() {
	if (!is_connected) {
		return;
	}

	string dref_string = "";

	for (int i = 0; i < drefs.size(); i++) {
		auto type = dref_types.at(i);
		string value;
		auto dref = drefs.at(i);
		if (type == xplmType_Int) {
			value = to_string(XPLMGetDatai(dref));
		} else if (type == xplmType_Float) {
			value = to_string(XPLMGetDataf(dref));
		} else if (type == xplmType_Double) {
			value = to_string(XPLMGetDatad(dref));
		}

		if (!value.empty()) {
			dref_string = dref_string
				.append(to_string(i))
				.append("=")
				.append(value)
				.append(" ");
		}
	}

	XPLMDebugString(dref_string.c_str());
}

template <typename Out>
void split(const std::string& s, char delim, Out result) {
	istringstream iss(s);
	string item;
	while (getline(iss, item, delim)) {
		*result++ = item;
	}
}

vector<string> split(const std::string& s, char delim) {
	vector<string> elems;
	split(s, delim, back_inserter(elems));
	return elems;
}

void sync_datarefs() {
	if (!is_connected) {
		return;
	}

	string recieved; // This is a dummy string right now, I will implement UDP soon
	
	vector<string> dref_value_strings = split(recieved, ' ');

	for (int i = 0; i < dref_value_strings.size(); i++) {
		auto dref_value_string = dref_value_strings.at(i);
		auto type = dref_types.at(i);
		auto dref = drefs.at(i);
		vector<string> values = split(dref_value_string, '=');

		if (type == xplmType_Int) {
			auto value = stoi(values[1]);
			XPLMSetDatai(dref, value);
		} else if (type == xplmType_Float) {
			auto value = stof(values[1]);
			XPLMSetDataf(dref, value);
		} else if (type == xplmType_Double) {
			auto value = stod(values[1]);
			XPLMSetDatad(dref, value);
		}
	}
}

void start_master() {

}

void stop_master() {

}

void toggle_master() {
	if (!running) {
		start_master();
	} else if (running && is_master) {
		stop_master();
	}
}

void start_slave() {

}

void stop_slave() {

}

void toggle_slave() {
	if (!running) {
		start_slave();
	} else if (running && !is_master) {
		stop_slave();
	}
}

float loop(float elapsed1, float elapsed2, int ctr, void* refcon) {
	if (running) {
		if (is_master) {
			send_datarefs();
		}
		else {
			sync_datarefs();
		}
	}
	return -1;
}