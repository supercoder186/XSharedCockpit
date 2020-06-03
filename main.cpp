//External libraries
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
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

//#if IBM
//	#include <windows.h>
//#endif

//Using object defs
using std::vector;
using std::string;
using std::size_t;
using std::istringstream;
using boost::asio::ip::udp;
using boost::asio::ip::address;


//Using function defs
using std::to_string;
using std::copy;
using std::replace;
using std::stoi;
using std::stod;
using std::stof;
using std::back_inserter;
using std::getline;

//Namespace using defs
using namespace boost::placeholders;

//Variable defs
int menu_container_index;
XPLMMenuID menu_id;
vector<string> dref_strings;
vector <XPLMDataRef> drefs;
vector <XPLMDataTypeID> dref_types;
boost::asio::io_service io_service;
udp::socket master{ io_service };
udp::endpoint master_endpoint{};
udp::socket slave{ io_service };
udp::endpoint slave_endpoint;


//Constant defs
const string master_address = "127.0.0.1";
const int master_port = 49000;
const string slave_address = "127.0.0.1";
const int slave_port = 49001;
bool running = false;
bool is_master = false;
bool is_connected = false;

//Function defs
void menu_handler(void*, void*);
void load_plugin();
float loop(float elapsed1, float elapsed2, int ctr, void* refcon);
bool number_contains(int number, int check);
void toggle_master();
void toggle_slave();


//X-Plane API functions
PLUGIN_API int XPluginStart(char * outName, char * outSig,	char * outDesc){
	strcpy(outName, "XSharedCockpit");
	strcpy(outSig, "com.thecodingaviator.xsharedcockpit");
	strcpy(outDesc, "A plugin to allow you to fly together with your friends");

	XPLMDebugString("XSharedCockpit has started init\n");

	menu_container_index = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "XSharedCockpit", 0, 0);
	menu_id = XPLMCreateMenu("Sample Menu", XPLMFindPluginsMenu(), menu_container_index, menu_handler, NULL);
	XPLMAppendMenuItem(menu_id, "Toggle Master", (void*)"Toggle Master", 1);
	XPLMAppendMenuItem(menu_id, "Toggle Slave", (void*)"Toggle Slave", 1);


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

	XPLMDebugString("XSharedCockpit has started\n");
	return 1;
}

PLUGIN_API void	XPluginStop(void) {
	XPLMDestroyMenu(menu_id);
}

PLUGIN_API void XPluginDisable(void) {
	drefs.clear();
}

PLUGIN_API int XPluginEnable(void) {
	XPLMDebugString("XSharedCockpit initializing\n");
	//load_plugin();
	for (auto const& dref_name : dref_strings) {
		auto dref = XPLMFindDataRef(dref_name.c_str());
		drefs.push_back(dref);
		dref_types.push_back(XPLMGetDataRefTypes(dref));
	}

	XPLMDebugString("XSharedCockpit initialized\n");
	return 1; 
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void * inParam) {}

void load_plugin() {
	char name[512];
	char path[512];
	XPLMGetNthAircraftModel(0, name, path);
	string cfg_file_path(path);
	size_t pos = cfg_file_path.find(name);
	cfg_file_path = cfg_file_path.substr(0, pos);
	cfg_file_path.append("smartcopilot.cfg");
	//XPLMDebugString(cfg_file_path.c_str());
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
		XPLMDebugString("Toggling slave\n");
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
		XPLMDataTypeID type = dref_types.at(i);
		string value;
		auto dref = drefs.at(i);
		if (type == xplmType_Int) {
			value = to_string(XPLMGetDatai(dref));
		} else if (type == xplmType_Float || type == (xplmType_Float | xplmType_Double)) {
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

	boost::system::error_code err;
	master.send_to(boost::asio::buffer(dref_string), slave_endpoint, 0, err);
}

vector<string> split(std::string s, const char delimiter) {
	vector<string> output;
	auto delimiter_pos = s.find(delimiter);
	if (delimiter_pos > s.size()) {
		output.push_back(s);
	}
	else {
		output.push_back(s.substr(0, delimiter_pos));
		output.push_back(s.substr(delimiter_pos + 1, s.size() - 1));
	}
	return output;
}

void sync_datarefs() {
	if (!is_connected) {
		return;
	}

	const auto available = slave.available();

	if (available == 0) {
		return;
	}

	char* buffer = new char[available];
	auto bytes_transferred = slave.receive_from(boost::asio::buffer(buffer, available), master_endpoint);

	string received(buffer);
	received = received.substr(0, bytes_transferred);
	XPLMDebugString(received.c_str());
	XPLMDebugString("\n");

	vector<string> dref_value_strings = split(received, ' ');

	for (int i = 0; i < dref_value_strings.size(); i++) {
		auto dref_value_string = dref_value_strings.at(i);
		auto type = dref_types.at(i);
		auto dref = drefs.at(i);
		vector<string> values = split(dref_value_string, '=');

		if (type == xplmType_Int) {
			auto value = stoi(values[1]);
			XPLMSetDatai(dref, value);
		}
		else if (type == xplmType_Float || type == (xplmType_Float | xplmType_Double)) {
			auto value = stof(values[1]);
			XPLMSetDataf(dref, value);
		}
		else if (type == xplmType_Double) {
			auto value = stod(values[1]);
			XPLMSetDatad(dref, value);
		}
	}
}

void start_master() {
	is_master = true;
	running = true;
	XPLMDebugString("Declaring socket and endpoints\n");
	master = udp::socket(io_service);
	master_endpoint = udp::endpoint(address::from_string(master_address), master_port);
	slave_endpoint = udp::endpoint(address::from_string(slave_address), slave_port);
	XPLMDebugString("Opening and binding socket\n");
	master.open(udp::v4());
	master.bind(master_endpoint);
	is_connected = true;
	XPLMRegisterFlightLoopCallback(loop, 2, NULL);
}

void stop_master() {
	XPLMUnregisterFlightLoopCallback(loop , NULL);
	string close("close");
	boost::system::error_code err;
	master.send_to(boost::asio::buffer(close), slave_endpoint, 0, err);
	master.close();
	is_connected = false;
	running = false;
}

void toggle_master() {
	if (!running) {
		start_master();
	} else if (running && is_master) {
		stop_master();
	}
}

void start_slave() {
	auto type = XPLMGetDataRefTypes(XPLMFindDataRef("sim/operation/override/override_planepath[0]"));
	XPLMDebugString(to_string(type).c_str());
	XPLMDebugString("\n");
	is_master = false;
	running = true;
	XPLMDebugString("Starting slave\n");
	slave = udp::socket(io_service);
	XPLMDebugString("Declaring endpoints\n");
	master_endpoint = udp::endpoint(address::from_string(master_address), master_port);
	slave_endpoint = udp::endpoint(address::from_string(slave_address), slave_port);
	XPLMDebugString("Opening socket\n");
	slave.open(udp::v4());
	XPLMDebugString("Binding socket\n");
	slave.bind(slave_endpoint);
	XPLMDebugString("Setting flight path override\n");
	XPLMSetDatai(XPLMFindDataRef("sim/operation/override/override_planepath[0]"), 1);
	is_connected = true;
	XPLMRegisterFlightLoopCallback(loop, 2, NULL);
}

void stop_slave() {
	XPLMUnregisterFlightLoopCallback(loop, NULL);
	slave.close();
	XPLMDebugString("Disabling flight path override\n");
	XPLMSetDatai(XPLMFindDataRef("sim/operation/override/override_planepath[0]"), 0);
	is_connected = false;
	running = false;
}

void toggle_slave() {
	if (!running) {
		XPLMDebugString("Running function start slave\n");
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