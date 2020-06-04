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
vector<string> split_once(string s, const char delimiter);


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
	for (auto const& dref_name : dref_strings) {
		auto values = split_once(dref_name, '[');

		auto name = values[0];
		auto dref = XPLMFindDataRef(name.c_str());
		drefs.push_back(dref);

		auto type = XPLMGetDataRefTypes(dref);
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


std::vector<std::string> split(std::string s, const char delimiter)
{
	size_t start = 0;
	size_t end = s.find_first_of(delimiter);

	std::vector<std::string> output;

	while (end <= std::string::npos)
	{
		string sub = s.substr(start, end - start);
		if (!sub.empty())
			output.emplace_back(sub);

		if (end == std::string::npos)
			break;

		start = end + 1;
		end = s.find_first_of(delimiter, start);
	}

	return output;
}

vector<string> split_once(std::string s, const char delimiter) {
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

void set_dataref(XPLMDataRef dref, string value_string, XPLMDataTypeID type) {
	XPLMDebugString(to_string(type).append("\n").c_str());

	if (type == xplmType_Int) {
		auto values = split_once(value_string, '=');
		auto value = stoi(values[1]);
		XPLMSetDatai(dref, value);
	}
	else if (type == xplmType_Float || type == (xplmType_Float | xplmType_Double)) {
		auto values = split_once(value_string, '=');
		auto value = stof(values[1]);
		XPLMSetDataf(dref, value);
	}
	else if (type == xplmType_Double) {
		auto values = split_once(value_string, '=');
		auto value = stod(values[1]);
		XPLMSetDatad(dref, value);
	}
	else if (type == xplmType_IntArray) {
		auto values = split_once(value_string, '=');
		auto temp = split_once(values[0], '[');
		int index = stoi(temp.at(1));
		int value[1] = { stoi(values[1]) };
		XPLMSetDatavi(dref, value, index, 1);
	}
	else if (type == xplmType_FloatArray) {
		auto values = split_once(value_string, '=');
		auto temp = split_once(values[0], '[');
		int index = stoi(temp.at(1));
		float value[1] = { stof(values[1]) };
		XPLMSetDatavf(dref, value, index, 1);
	}	
}

string get_dataref(XPLMDataRef dref, XPLMDataTypeID type, int i) {
	string value;
	if (type == xplmType_Int) {
		value = to_string(XPLMGetDatai(dref));
	}
	else if (type == xplmType_Float || type == (xplmType_Float | xplmType_Double)) {
		value = to_string(XPLMGetDataf(dref));
	}
	else if (type == xplmType_Double) {
		value = to_string(XPLMGetDatad(dref));
	}
	else if (type == xplmType_IntArray) {
		int index = stoi(split(dref_strings.at(i), '[').at(1));
		int value_arr[1];
		XPLMGetDatavi(dref, value_arr, index, 1);
		value = to_string(value[0]);
	}
	else if (type == xplmType_FloatArray) {
		int index = stoi(split(dref_strings.at(i), '[').at(1));
		float value_arr[1];
		XPLMGetDatavf(dref, value_arr, index, 1);
		value = to_string(value[0]);
	}

	return value;
}


void send_datarefs() {
	if (!is_connected) {
		return;
	}

	string dref_string = "";

	for (int i = 0; i < drefs.size(); i++) {
		XPLMDataTypeID type = dref_types.at(i);
		auto dref = drefs.at(i);

		string value = get_dataref(dref, type, i);

		if (!value.empty()) 
			dref_string = dref_string
				.append(to_string(i))
				.append("=")
				.append(value)
				.append(" ");
	}

	boost::system::error_code err;
	master.send_to(boost::asio::buffer(dref_string), slave_endpoint, 0, err);
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

	XPLMDebugString("Splitting the string\n");
	vector<string> dref_value_strings = split(received, ' ');
	XPLMDebugString(to_string(dref_value_strings.size()).c_str());

	for (int i = 0; i < dref_value_strings.size(); i++) {
		XPLMDebugString(to_string(i).c_str());
		XPLMDebugString("\n");
		auto dref_value_string = dref_value_strings.at(i);
		auto type = dref_types.at(i);
		auto dref = drefs.at(i);
		XPLMDebugString(dref_value_string.c_str());
		XPLMDebugString("\nSetting the dataref value Type: ");
		set_dataref(dref, dref_value_string, type);
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

void start_slave() {
	auto type = XPLMGetDataRefTypes(XPLMFindDataRef("sim/operation/override/override_planepath[0]"));
	is_master = false;
	running = true;
	slave = udp::socket(io_service);
	master_endpoint = udp::endpoint(address::from_string(master_address), master_port);
	slave_endpoint = udp::endpoint(address::from_string(slave_address), slave_port);
	slave.open(udp::v4());
	slave.bind(slave_endpoint);
	set_dataref(XPLMFindDataRef("sim/operation/override/override_planepath"), "sim/operation/override/override_planepath[0]=1", xplmType_IntArray);
	XPLMDebugString("Override has been set\n");
	is_connected = true;
	XPLMRegisterFlightLoopCallback(loop, 2, NULL);
}

void stop_slave() {
	XPLMUnregisterFlightLoopCallback(loop, NULL);
	slave.close();
	XPLMDebugString("Disabling flight path override\n");
	set_dataref(XPLMFindDataRef("sim/operation/override/override_planepath"), "sim/operation/override/override_planepath[0]=0", xplmType_IntArray);
	is_connected = false;
	running = false;
}

void toggle_master() {
	if (!running) {
		start_master();
	}
	else if (running && is_master) {
		stop_master();
	}
}

void toggle_slave() {
	if (!running) {
		start_slave();
	}
	else if (running && !is_master) {
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