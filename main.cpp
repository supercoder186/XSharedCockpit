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
#include <vector>
#include <map>
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
//Sent by the master
vector<string> master_dref_strings;
vector <XPLMDataRef> master_drefs;
vector <XPLMDataTypeID> master_dref_types;
vector <int> master_indexes;
//Sent by the slave
vector<string> slave_dref_strings;
vector <XPLMDataRef> slave_drefs;
vector <XPLMDataTypeID> slave_dref_types;
vector <int> slave_indexes;
vector<string> master_overrides;
vector<string> slave_overrides;
vector<string> command_strings;
vector<XPLMCommandRef> commands;
vector<void*> command_pointers;
string command_string;
bool running = false;
bool is_master = false;
bool is_connected = false;
string master_address = "127.0.0.1";
int master_port = 49000;
string slave_address = "127.0.0.1";
int slave_port = 49001;

boost::asio::io_service io_service;
udp::socket master{ io_service };
udp::endpoint master_endpoint{};
udp::socket slave{ io_service };
udp::endpoint slave_endpoint;


//Function defs
void menu_handler(void*, void*);
void load_plugin();
float loop(float elapsed1, float elapsed2, int ctr, void* refcon);
bool number_contains(int number, int check);
int toggle_master(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void* inRefcon);
void stop_master();
int toggle_slave(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void* inRefcon);
void stop_slave();
int command_handler(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void* inRefcon);
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

	master_dref_strings.push_back("sim/flightmodel/position/local_x");
	master_dref_strings.push_back("sim/flightmodel/position/local_y");
	master_dref_strings.push_back("sim/flightmodel/position/local_z");
	master_dref_strings.push_back("sim/flightmodel/position/psi");
	master_dref_strings.push_back("sim/flightmodel/position/theta");
	master_dref_strings.push_back("sim/flightmodel/position/phi");
	master_dref_strings.push_back("sim/flightmodel/position/P");
	master_dref_strings.push_back("sim/flightmodel/position/Q");
	master_dref_strings.push_back("sim/flightmodel/position/R");
	master_dref_strings.push_back("sim/flightmodel/position/local_vx");
	master_dref_strings.push_back("sim/flightmodel/position/local_vy");
	master_dref_strings.push_back("sim/flightmodel/position/local_vz");

	XPLMDebugString("XSharedCockpit has started\n");
	return 1;
}

PLUGIN_API void	XPluginStop(void) {
	XPLMDestroyMenu(menu_id);
}

PLUGIN_API void XPluginDisable(void) {
	if (running) {
		if (is_master) {
			stop_master();
		}
		else {
			stop_slave();
		}
	}
}

PLUGIN_API int XPluginEnable(void) {
	XPLMDebugString("XSharedCockpit initializing\n");
	for (auto const& dref_name : master_dref_strings) {
		auto values = split_once(dref_name, '[');

		auto name = values[0];
		auto dref = XPLMFindDataRef(name.c_str());
		master_drefs.push_back(dref);

		auto type = XPLMGetDataRefTypes(dref);
		master_dref_types.push_back(XPLMGetDataRefTypes(dref));
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
	return s1.find(s2) != string::npos;
}

bool replace(string& str, const string& from, const string& to) {
	size_t start_pos = str.find(from);
	if (start_pos == string::npos)
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
	cfg_file_path.append("xsharedcockpit.cfg");
	ifstream fin;
	fin.open(cfg_file_path);
	string line;
	string current_section;

	master_dref_strings.clear();
	master_drefs.clear();
	master_dref_types.clear();
	commands.clear();
	command_strings.clear();
	command_pointers.clear();
	master_indexes.clear();
	master_overrides.clear();
	slave_overrides.clear();
	command_string = "";

	master_dref_strings.push_back("sim/flightmodel/position/local_x");
	master_dref_strings.push_back("sim/flightmodel/position/local_y");
	master_dref_strings.push_back("sim/flightmodel/position/local_z");
	master_dref_strings.push_back("sim/flightmodel/position/psi");
	master_dref_strings.push_back("sim/flightmodel/position/theta");
	master_dref_strings.push_back("sim/flightmodel/position/phi");
	master_dref_strings.push_back("sim/flightmodel/position/P");
	master_dref_strings.push_back("sim/flightmodel/position/Q");
	master_dref_strings.push_back("sim/flightmodel/position/R");
	master_dref_strings.push_back("sim/flightmodel/position/local_vx");
	master_dref_strings.push_back("sim/flightmodel/position/local_vy");
	master_dref_strings.push_back("sim/flightmodel/position/local_vz");

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
				master_dref_strings.push_back(match[1].str().append("_SCP"));
			}
			else if (current_section == "COMMANDS") {
				command_strings.push_back(match[1]);
			}
			else if (current_section == "SETUP") {
				auto name = match[1].str();
				if (name == "MASTER_ADDRESS") {
					XPLMDebugString("Master address set to ");
					XPLMDebugString(match[2].str().append("\n").c_str());
					master_address = match[2];
				}
				else if (name == "SLAVE_ADDRESS") {
					XPLMDebugString("Slave address set to ");
					XPLMDebugString(match[2].str().append("\n").c_str());
					slave_address = match[2];
				}
				else if (name == "MASTER_PORT") {
					XPLMDebugString("Master port set to ");
					XPLMDebugString(match[2].str().append("\n").c_str());
					master_port = stoi(match[2]);
				}
				else if (name == "SLAVE_PORT") {
					XPLMDebugString("Slave address set to ");
					XPLMDebugString(match[2].str().append("\n").c_str());
					slave_port = stoi(match[2]);
				}
			}
			else if (current_section == "TRIGGERS" || current_section == "TRANSPONDER") {
				master_dref_strings.push_back(match[1]);
				slave_dref_strings.push_back(match[1]);
			}
			else if(current_section == "CONTINUED" || current_section == "SEND_BACK"){
				master_dref_strings.push_back(match[1]);
			}
		}
	}
	fin.close();

	for (auto dref_name : master_dref_strings) {
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
		master_drefs.push_back(dref);

		auto type = XPLMGetDataRefTypes(dref);

		if (type == xplmType_FloatArray || type == xplmType_IntArray) {
			if (values.size() > 1) {
				master_indexes.push_back(stoi(values[1]));
			}
			else {
				master_indexes.push_back(0);
			}
		}
		else {
			master_indexes.push_back(-1);
		}
		master_dref_types.push_back(type);
	}

	for (auto dref_name : slave_dref_strings) {
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
		slave_drefs.push_back(dref);

		auto type = XPLMGetDataRefTypes(dref);

		if (type == xplmType_FloatArray || type == xplmType_IntArray) {
			if (values.size() > 1) {
				slave_indexes.push_back(stoi(values[1]));
			}
			else {
				slave_indexes.push_back(0);
			}
		}
		else {
			slave_indexes.push_back(-1);
		}
		slave_dref_types.push_back(type);
	}

	int idx = 0;
	for (auto i : command_strings) {
		auto command = XPLMFindCommand(i.c_str());
		void* pointer = (void*) new string(to_string(idx));
		command_pointers.push_back(pointer);

		commands.push_back(command);
		if (is_master) {
			XPLMRegisterCommandHandler(command, command_handler, true, pointer);
		}
		idx++;
	}
}

//Dummy menu handler
void menu_handler(void* in_menu_ref, void* in_item_ref) {

}

//My functions
vector<string> split(string s, const char delimiter)
{
	size_t start = 0;
	size_t end = s.find_first_of(delimiter);

	vector<string> output;

	while (end <= string::npos)
	{
		string sub = s.substr(start, end - start);
		if (!sub.empty())
			output.emplace_back(sub);

		if (end == string::npos)
			break;

		start = end + 1;
		end = s.find_first_of(delimiter, start);
	}

	return output;
}

vector<string> split_once(string s, const char delimiter) {
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
		int index = (!is_master) ? master_indexes.at(i) : slave_indexes.at(i);

		int value[1] = { stoi(value_string) };
		XPLMSetDatavi(dref, value, index, 1);
	}
	else if (type == xplmType_FloatArray) {
		int index = (!is_master) ? master_indexes.at(i) : slave_indexes.at(i);

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
		int index = (is_master) ? master_indexes.at(i) : slave_indexes.at(i);
		
		int value_arr[1];
		XPLMGetDatavi(dref, value_arr, index, 1);
		value = to_string(value_arr[0]);
	}
	else if (type == xplmType_FloatArray) {
		int index = (is_master) ? master_indexes.at(i) : slave_indexes.at(i);

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

	vector<XPLMDataRef> drefs = (is_master) ? master_drefs : slave_drefs;
	vector<XPLMDataTypeID> dref_types = (is_master) ? master_dref_types : slave_dref_types;

	for (int i = 0; i < drefs.size(); i++) {
		XPLMDataTypeID type = dref_types.at(i);
		auto dref = drefs.at(i);

		string value = get_dataref(dref, type, i);

		dref_string = dref_string
			.append(value)
			.append(" ");
	}

	dref_string = dref_string.append(command_string);
	command_string = ",";

	boost::system::error_code err;
	if (is_master) {
		master.send_to(boost::asio::buffer(dref_string), slave_endpoint, 0, err);
	}
	else {
		slave.send_to(boost::asio::buffer(dref_string), master_endpoint, 0, err);
	}
}

void sync_datarefs() {
	if (!is_connected) {
		return;
	}

	udp::endpoint receive_endpoint = (is_master) ? slave_endpoint : master_endpoint;
	const size_t available = (is_master) ? master.available() : slave.available();

	if (available == 0) {
		return;
	}

	char* buffer = new char[available];
	size_t bytes_transferred = (is_master) ? master.receive_from(boost::asio::buffer(buffer, available), receive_endpoint) : slave.receive_from(boost::asio::buffer(buffer, available), receive_endpoint);

	string received(buffer);
	received = received.substr(0, bytes_transferred);

	vector<string> dref_value_strings = split(received, ' ');
	vector<XPLMDataTypeID> dref_types = (is_master) ? slave_dref_types : master_dref_types;
	vector<XPLMDataRef> drefs = (is_master) ? slave_drefs : master_drefs;

	for (int i = 0; i < dref_value_strings.size() - 1; i++) {
		auto dref_value_string = dref_value_strings.at(i);
		if (!dref_value_string.empty()) {
			auto type = dref_types.at(i);
			auto dref = drefs.at(i);
			set_dataref(dref, dref_value_string, type, i);
		}
	}
	
	vector<string> command_value_strings = split(dref_value_strings.at(dref_value_strings.size() - 1), ',');
	for (int i = 0; i < command_value_strings.size(); i++) {
		string j = command_value_strings[i];
		size_t length = j.length();
		int idx = stoi(j.substr(0, length - 1));
		int phase = stoi(j.substr(length - 1, length));
		if (phase == 0) {
			XPLMCommandBegin(commands[idx]);
		}
		else if (phase == 2) {
			XPLMCommandEnd(commands[idx]);
		}
	}
}

void set_overrides() {
	if (is_master) {
		for (auto ovrd : master_overrides) {
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
	master = udp::socket(io_service);
	master_endpoint = udp::endpoint(address::from_string(master_address), master_port);
	slave_endpoint = udp::endpoint(address::from_string(slave_address), slave_port);
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

	int idx = 0;
	for (auto i : commands) {
		XPLMUnregisterCommandHandler(i, command_handler, true, command_pointers[idx]);
		idx++;
	}
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
	is_connected = true;
	XPLMRegisterFlightLoopCallback(loop, 2, NULL);
}

void stop_slave() {
	XPLMUnregisterFlightLoopCallback(loop, NULL);
	slave.close();
	int value[1] = { 0 };
	XPLMSetDatavi(XPLMFindDataRef("sim/operation/override/override_planepath"), value, 0, 1);
	is_connected = false;
	running = false;
	set_overrides();
}

//Command handlers
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

int command_handler(XPLMCommandRef inCommand, XPLMCommandPhase inPhase, void* inRefcon) {
	if (inPhase == xplm_CommandBegin || inPhase == xplm_CommandEnd) {
		string* sp = static_cast<string*>(inRefcon);
		command_string = command_string.append(*sp).append(to_string(inPhase)).append(",");
	}
	return 1;
}

//Loop callback
float loop(float elapsed1, float elapsed2, int ctr, void* refcon) {
	if (running) {
		send_datarefs();
		sync_datarefs();
	}
	return -1;
}