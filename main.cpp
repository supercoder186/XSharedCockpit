//External libraries
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include "XPLMMenus.h"
#include "XPLMProcessing.h"
#include "XPLMDataAccess.h"
#include "XPLMPlugin.h"
#include "XPLMUtilities.h"
#include "XPLMPlanes.h"

//Standard libraries
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <iterator>
#include <fstream>
#include <regex>

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
using std::ifstream;
using std::regex;
using std::smatch;

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
vector <int> indexes;
vector<string> master_overrides;
vector<string> slave_overrides;
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
int toggle_master(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void* inRefcon);
int toggle_slave(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void* inRefcon);
vector<string> split_once(string s, const char delimiter);

//X-Plane API functions
PLUGIN_API int XPluginStart(char * outName, char * outSig,	char * outDesc){
	strcpy(outName, "XSharedCockpit");
	strcpy(outSig, "com.thecodingaviator.xsharedcockpit");
	strcpy(outDesc, "A plugin to allow you to fly together with your friends");

	XPLMDebugString("XSharedCockpit has started init\n");

	auto command = XPLMCreateCommand("XSharedCockpit/toggle_master", "Toggle XSharedCockpit as Master");
	XPLMRegisterCommandHandler(command, toggle_master, 1, (void*)0);
	auto command2 = XPLMCreateCommand("XSharedCockpit/toggle_slave", "Toggle XSharedCockpit as Slave");
	XPLMRegisterCommandHandler(command2, toggle_slave, 1, (void*)0);

	menu_container_index = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "XSharedCockpit", 0, 0);
	menu_id = XPLMCreateMenu("Sample Menu", XPLMFindPluginsMenu(), menu_container_index, menu_handler, NULL);
	XPLMAppendMenuItemWithCommand(menu_id, "Toggle Master", command);
	XPLMAppendMenuItemWithCommand(menu_id, "Toggle Slave", command2);

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

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void * inParam){}

bool number_contains(int number, int check) {
	int result = number & check;
	return result == check;
}

bool string_contains(string s1, string s2) {
	return s1.find(s2) != std::string::npos;
}

bool replace(std::string& str, const std::string& from, const std::string& to) {
	size_t start_pos = str.find(from);
	if (start_pos == std::string::npos)
		return false;
	str.replace(start_pos, from.length(), to);
	return true;
}

void load_plugin() {
	char name[512];
	char path[512];
	XPLMGetNthAircraftModel(0, name, path);
	string cfg_file_path(path);
	size_t pos = cfg_file_path.find(name);
	cfg_file_path = cfg_file_path.substr(0, pos);
	cfg_file_path.append("smartcopilot.cfg");
	XPLMDebugString(cfg_file_path.c_str());
	ifstream fin;
	fin.open(cfg_file_path);
	string line;
	string current_section;

	dref_strings.clear();
	drefs.clear();
	dref_types.clear();

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

	while (fin) {
		getline(fin, line);
		regex comments("([ ]*[#;]+.*)");
		regex section_name_exp("^[ ]*\\[(.*)\\][ ]*");
		regex value_parser("(\\S+)[ ]*=[ ]*(\\S+)");
		smatch match;
		if (regex_match(line, comments)) {
			regex_search(line, match, comments);
			replace(line, match[0], "");
		}
		if (regex_match(line, section_name_exp)) {
			regex_search(line, match, section_name_exp);
			current_section = match[1];
		}
		else if (regex_match(line, value_parser)) {
			regex_search(line, match, value_parser);
			if (current_section == "OVERRIDE") {
				int v = stoi(match[2].str());
				if (number_contains(v, 1)) {
					master_overrides.push_back(match[1]);
				}
				if (number_contains(v, 8)) {
					slave_overrides.push_back(match[1]);
				}
			}
			else if (current_section == "CLICKS") {
				dref_strings.push_back(match[1].str().append("_scp"));
			}
			else if(current_section != "SETUP" && current_section != "WEATHER" && current_section!="COMMANDS" && current_section != "SLOW"){
				dref_strings.push_back(match[1]);
			}
		}
	}

	int idx = 0;
	for (auto i : dref_strings) {
		string output = to_string(idx).append(i).append("\n");
		XPLMDebugString(output.c_str());
		idx++;
	}

	for (auto dref_name : dref_strings) {
		string name;
		vector<string> values;
		if (!string_contains(name, "_FIXED_INDEX_")) {
			values = split_once(dref_name, '[');
			name = values[0];
		}
		else {
			name = dref_name;
			replace(name, "_FIXED_INDEX_", "");
			values.push_back(name);
		}

		auto dref = XPLMFindDataRef(name.c_str());
		drefs.push_back(dref);

		auto type = XPLMGetDataRefTypes(dref);

		if (type == xplmType_FloatArray || type == xplmType_IntArray) {
			if (values.size() > 1) {
				indexes.push_back(stoi(values[1]));

				XPLMDebugString(string("Index: ").append(values[1]).append("\n").c_str());
			}
			else {
				indexes.push_back(0);
			}
		}
		else {
			indexes.push_back(-1);
		}

		XPLMDebugString(name.append(": ").append(to_string(type)).append("\n").c_str());
		dref_types.push_back(type);
	}
	fin.close();
}

//My functions
void menu_handler(void* in_menu_ref, void* in_item_ref) {

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

void set_dataref(XPLMDataRef dref, string value_string, XPLMDataTypeID type, int i) {
	if (type == xplmType_Int) {
		auto value = stoi(value_string);
		XPLMSetDatai(dref, value);
	}
	else if (type == xplmType_Float || type == (xplmType_Float | xplmType_Double)) {
		auto value = stof(value_string);
		XPLMSetDataf(dref, value);
	}
	else if (type == xplmType_Double) {
		auto value = stod(value_string);
		XPLMSetDatad(dref, value);
	}
	else if (type == xplmType_IntArray) {
		int index = indexes.at(i);
		int value[1] = { stoi(value_string) };
		XPLMSetDatavi(dref, value, index, 1);
	}
	else if (type == xplmType_FloatArray) {
		int index = indexes.at(i);
		float value[1] = { stof(value_string) };
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
		int index = indexes.at(i);
		int value_arr[1];
		XPLMGetDatavi(dref, value_arr, index, 1);
		value = to_string(value_arr[0]);
	}
	else if (type == xplmType_FloatArray) {
		int index = indexes.at(i);
		float value_arr[1];
		XPLMGetDatavf(dref, value_arr, index, 1);
		value = to_string(value_arr[0]);
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
			.append(value)
			.append(" ");
		else {
			XPLMDebugString(string("No value found for ").append(dref_strings.at(i)).append("\n").c_str());
			dref_string = dref_string.append(" ");
		}
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

	vector<string> dref_value_strings = split(received, ' ');

	for (int i = 0; i < dref_value_strings.size(); i++) {
		auto dref_value_string = dref_value_strings.at(i);
		if (!dref_value_string.empty()) {
			auto type = dref_types.at(i);
			auto dref = drefs.at(i);
			set_dataref(dref, dref_value_string, type, i);
		}
	}
}

void set_overrides() {
	if (is_master) {
		for (auto ovrd : master_overrides) {
			XPLMDebugString(ovrd.append("\n").c_str());
			XPLMDebugString(to_string((int)running).append("\n").c_str());
			auto values = split_once(ovrd, '[');
			if (values.size() > 1) {
				int index = stoi(values[1]);
				XPLMDebugString(to_string(index).c_str());
				int value[1] = { running };
				XPLMSetDatavi(XPLMFindDataRef(values[0].c_str()), value, index, 1);
			}
			else {
				XPLMSetDatai(XPLMFindDataRef(ovrd.c_str()), running);
			}
		}
	}
	else {
		for (auto ovrd : slave_overrides) {
			XPLMDebugString(ovrd.append("\n").c_str());
			XPLMDebugString(to_string((int)running).append("\n").c_str());
			auto values = split_once(ovrd, '[');
			if (values.size() > 1) {
				int index = stoi(values[1]);
				int value[1] = { running };
				XPLMSetDatavi(XPLMFindDataRef(values[0].c_str()), value, index, 1);
			}
			else {
				XPLMSetDatai(XPLMFindDataRef(ovrd.c_str()), running);
			}
		}
	}
}

void start_master() {
	is_master = true;
	running = true;
	load_plugin();
	set_overrides();
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
	set_overrides();
}

void start_slave() {
	is_master = false;
	running = true;
	load_plugin();
	set_overrides();
	slave = udp::socket(io_service);
	master_endpoint = udp::endpoint(address::from_string(master_address), master_port);
	slave_endpoint = udp::endpoint(address::from_string(slave_address), slave_port);
	slave.open(udp::v4());
	slave.bind(slave_endpoint);
	int value[1] = { 1 };
	XPLMSetDatavi(XPLMFindDataRef("sim/operation/override/override_planepath"), value, 0, 1);
	XPLMDebugString("Override has been set\n");
	is_connected = true;
	XPLMRegisterFlightLoopCallback(loop, 2, NULL);
}

void stop_slave() {
	XPLMUnregisterFlightLoopCallback(loop, NULL);
	slave.close();
	XPLMDebugString("Disabling flight path override\n");
	int value[1] = { 0 };
	XPLMSetDatavi(XPLMFindDataRef("sim/operation/override/override_planepath"), value, 0, 1);
	is_connected = false;
	running = false;
	set_overrides();
}

int toggle_master(XPLMCommandRef inCommand,	XPLMCommandPhase inPhase, void* inRefcon){
	if (inPhase == xplm_CommandEnd) {
		if (!running) {
			start_master();
		}
		else if (running && is_master) {
			stop_master();
		}
	}
	return 1;
}

int toggle_slave(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void* inRefcon) {
	if (inPhase == xplm_CommandEnd) {
		if (!running) {
			start_slave();
		}
		else if (running && !is_master) {
			stop_slave();
		}
	}
	return 1;
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