/*
 *  pjControl.h
 *
 *  Created by Noah Shibley on 8/9/10.
 *  Updated by Martial GALLORINI on 19/03/2015
 *
	Video projector control class.
 *
 *
 */

#include "ofxPJControl.h"

#include <utility>
#include "ofMain.h"

ofxPJControl::ofxPJControl(): pjPort(4352), commMode(PJLINK_MODE), shutterState(false) {
	connected = false;
	projectorPowerState = false;
	commandSendAttempts = 0;
}

ofxPJControl::~ofxPJControl() {

}

void ofxPJControl::getErrors() {
	sendPjLinkCommand("%1ERST ?\r", true);

}

void ofxPJControl::updateProjectorPowerStatus() {
	sendPjLinkCommand("%1POWR ?\r", true);
}

bool ofxPJControl::getProjectorPower() {

	updateProjectorPowerStatus();

	return projectorPowerState;
}

void ofxPJControl::updateShutterStatus() {
	sendPjLinkCommand("%1AVMT ?\r", true);
}

void ofxPJControl::setProjectorType(int protocol) {
	commMode = protocol;
}

void ofxPJControl::setup(string ip, int protocol, string password) {
	setProjectorIP(ip);
	setProjectorType(protocol);
	setProjectorPassword(password);
}

void ofxPJControl::setup(string ip, int port, int protocol, string password) {
	setProjectorIP(std::move(ip));
	setProjectorType(protocol);
	setProjectorPort(port);
	setProjectorPassword(std::move(password));

}

void ofxPJControl::setProjectorIP(string IP_add) {
	IPAddress = std::move(IP_add);
}

void ofxPJControl::setProjectorPort(int port) {
	pjPort = port;
}

void ofxPJControl::setProjectorPassword(string passwd) {
	password = std::move(passwd);
}


void ofxPJControl::on() {
	if (commMode == NEC_MODE) {
		nec_On();
	}
	else if (commMode == PJLINK_MODE) {
		pjLink_On();
	}
	else if (commMode == CHRISTIE_MODE) {
		christie_On();
	}
	else if (commMode == SANYO_MODE) {
		sanyo_On();
	}
	else if (commMode == BARCO_MODE) {
		barco_On();
	}
}

void ofxPJControl::off() {
	if (commMode == NEC_MODE) {
		nec_Off();
	}
	else if (commMode == PJLINK_MODE) {
		pjLink_Off();
	}
	else if (commMode == CHRISTIE_MODE) {
		christie_Off();
	}
	else if (commMode == SANYO_MODE) {
		sanyo_Off();
	}
	else if (commMode == BARCO_MODE) {
		barco_Off();
	}
}

string ofxPJControl::connect() {
	string msgRx;
	connected = pjClient.setup(IPAddress, pjPort, true);
	if (connected) {
		ofLogNotice() << "connection established: " << IPAddress << ":" << pjPort;

		while (msgRx.length() < 8) {
			msgRx = pjClient.receiveRaw();
		}

		ofLogNotice() << "Received response: " << msgRx;

	}
	else {
		ofLogError() << "Failed to connect. Trying again in 1 second..";
		ofSleepMillis(1000);

		connected = pjClient.setup(IPAddress, pjPort, true);
		if (connected) {

			ofLogNotice() << "connection established: " << IPAddress << ":" << pjPort;
		}
		else
			ofLog() << "Connection failed again..";
	}

	return msgRx;
}

void ofxPJControl::processMessage(const string msgRx) {
	vector<string> msgRx_splited = ofSplitString(msgRx, "=");
	if (msgRx_splited.size() > 1) {

		if (msgRx_splited[0] == "%1POWR" || msgRx_splited[0] == "%1powr") {

			if (ofToInt(msgRx_splited[1]) == 1) {
				projectorPowerState = true;
			}
			else {
				projectorPowerState = false;
			}

			if (msgRx_splited[1] == "ERR3") {
				if (commandSendAttempts < 3) {

					sendPjLinkCommand(command);

				}

			}
		}

		if (msgRx_splited[0] == "%1AVMT") {

			if (ofToInt(msgRx_splited[1]) == 31) {
				shutterState = true;
			}
			else if (ofToInt(msgRx_splited[1]) == 30) {
				shutterState = false;
			}
			else {
				ofLogError() << "shutter state is something wrong" << msgRx_splited[1];
				shutterState = false;
			}

		}

		if (msgRx_splited[0] == "%1erst" || msgRx_splited[0] == "%1ERST") {
			auto errorMessage = msgRx_splited[1];
			auto fanError = errorMessage[0];
			auto lampError = errorMessage[1];
			auto temperatureError = errorMessage[2];
			auto coverOpenEror = errorMessage[3];
			auto filterError = errorMessage[4];
			auto otherError = errorMessage[5];

			ofLog() << "Error states: (0: No error, 1: Warning, 2:Error)";
			ofLog() << "Fan Error: " << fanError;
			ofLog() << "Lamp Error " << lampError;
			ofLog() << "Temp error: " << temperatureError;
			ofLog() << "Cover Open: " << coverOpenEror;
			ofLog() << "Filter Error: " << filterError;
			ofLog() << "Other error: " << otherError;


		}
	}
}

bool ofxPJControl::sendPjLinkCommand(const string& command, bool waitResponse) {
	string msgRx;
	bool commandSent;

	this->command = command;

	if (!pjClient.isConnected()) {
		msgRx = connect();
	}

	if (connected) {
		string authToken;

		//eg. PJLINK 1 604cc14d
		if (msgRx[7] == '1') {
			ofLogNotice() << "with authentication";
			MD5Engine md5;
			md5.reset();
			string hash = msgRx.substr(9, 8);
			ofLogNotice() << hash << endl;

			md5.update(hash + password);
			authToken = DigestEngine::digestToHex(md5.digest());
		}

		ofLogNotice() << "sending command: " << authToken + command;
		pjClient.sendRaw(authToken + command);
		msgRx = "";

		if (waitResponse) {
			while (msgRx.length() < 8) {
				msgRx = pjClient.receiveRaw();
			}
		}

		ofLogNotice() << "received response: " << msgRx;

		pjClient.close();

		processMessage(msgRx);
		commandSent = true;

		commandSendAttempts++;
	}
	else {
		commandSent = false;
		ofLogError() << "still not connected.";
		pjClient.close();

	}


	return commandSent;
}

void ofxPJControl::sendCommand(string command) {
	if (!pjClient.isConnected()) {
		ofLogNotice() << "connecting to : " << IPAddress << ":" << pjPort;
		connected = pjClient.setup(IPAddress, pjPort, true);
		ofLogNotice() << "connection state : " << connected;
	}
	ofLogNotice() << "sending command : " << command;
	pjClient.sendRaw(command);
	ofSleepMillis(100);
	ofLogNotice() << "Response length (Bytes) : " << pjClient.getNumReceivedBytes();
	msgRx = "";
	if (pjClient.getNumReceivedBytes() > 0) {
		msgRx = pjClient.receiveRaw();
		ofLogNotice() << "received response : " << msgRx;
	}

	pjClient.close();
}

void ofxPJControl::nec_On() {

	pjClient.close(); //close any open connections first
	char* buffer = new char[6]; //02H 00H 00H 00H 00H 02H (the on command in hex)
	buffer[0] = 2;
	buffer[1] = 0;
	buffer[2] = 0;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = 2;

	//pjClient.setVerbose(true);
	if (!pjClient.isConnected()) {
		connected = pjClient.setup(IPAddress, NEC_PORT);
		ofLogNotice() << "connection established: " << IPAddress << ":" << NEC_PORT;
	}
	ofLogNotice() << "sending command: ON";

	pjClient.sendRawBytes(buffer, 6);

	printf("sent: %x %x %x %x %x %x\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);

	char* rxBuffer = new char[6];

	pjClient.receiveRawBytes(rxBuffer, 6);

	printf("received: %x %x %x %x %x %x\n", rxBuffer[0], rxBuffer[1], rxBuffer[2], rxBuffer[3], rxBuffer[4],
	       rxBuffer[5]);

	projectorPowerState = true;

	delete[] rxBuffer;
	delete[] buffer;
}

void ofxPJControl::nec_Off() {

	char* buffer = new char[6]; //02H 01H 00H 00H 00H 03H (the off command in hex)
	buffer[0] = 2;
	buffer[1] = 1;
	buffer[2] = 0;
	buffer[3] = 0;
	buffer[4] = 0;
	buffer[5] = 3;

	projectorPowerState = true;

	//pjClient.setVerbose(true);

	if (!pjClient.isConnected()) {
		connected = pjClient.setup(IPAddress, NEC_PORT);
		ofLogNotice() << "connection established: " << IPAddress << ":" << NEC_PORT;
	}

	ofLogNotice() << "sending command: OFF ";

	pjClient.sendRawBytes(buffer, 6);
	printf("send: %x %x %x %x %x %x\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4], buffer[5]);


	char* rxBuffer = new char[6];

	pjClient.receiveRawBytes(rxBuffer, 6);

	printf("receive: %x %x %x %x %x %x\n", rxBuffer[0], rxBuffer[1], rxBuffer[2], rxBuffer[3], rxBuffer[4],
	       rxBuffer[5]);

	projectorPowerState = false;

	delete[] rxBuffer;
	delete[] buffer;
}

void ofxPJControl::pjLink_On() {
	string command = "%1POWR 1\r";
	sendPjLinkCommand(command);
	projectorPowerState = true;


}

void ofxPJControl::pjLink_Off() {
	string command = "%1POWR 0\r";
	sendPjLinkCommand(command);
}

void ofxPJControl::sanyo_On() {
	string command = "PWR ON\r";
	sendCommand(command);
	projectorPowerState = true;
}

void ofxPJControl::sanyo_Off() {
	string command = "PWR OFF\r";
	sendCommand(command);
	projectorPowerState = false;
}

void ofxPJControl::christie_On() {
	string command = "(PWR1)";
	sendCommand(command);
	projectorPowerState = true; //projector on
}

void ofxPJControl::christie_Off() {
	string command = "(PWR0)";
	sendCommand(command);
	projectorPowerState = false; //projector off
}

void ofxPJControl::barco_On() {
	string command = ":POWR 1\r";
	sendCommand(command);
	projectorPowerState = true; //projector on
}

void ofxPJControl::barco_Off() {
	string command = ":POWR 0\r";
	sendCommand(command);
	projectorPowerState = false; //projector off
}

void ofxPJControl::avMute(bool muteAV) {
	if (muteAV) {
		sendPjLinkCommand("%1AVMT 31\r");
	}
	else {
		sendPjLinkCommand("%1AVMT 30\r");
	}
	shutterState = muteAV;
}

void ofxPJControl::christieShutter(bool b) {
	if (b) {
		string command = "(SHU 1)";
		sendCommand(command);
	}
	else {
		string command = "(SHU 0)";
		sendCommand(command);
	}
	shutterState = b;
}

void ofxPJControl::digitalcomShutter(bool b) {
	if (b) {
		string cmd = "*shutter = on\r";
		sendCommand(cmd);
	}
	else {
		string cmd = "*shutter = off\r";
		sendCommand(cmd);
	}
	shutterState = b;
}

void ofxPJControl::inputSelect(int input) {
	string command;

	switch (input) {
	case SONY_INPUT_A:
		command = "%1INPT 11\r";
		break;
	case SONY_INPUT_B:
		command = "%1INPT 31\r";
		break;
	case SONY_INPUT_C:
		command = "%1INPT 32\r";
		break;
	case SONY_INPUT_D:
		command = "%1INPT 51\r";
		break;
	default:
		command = "";
		break;
	}

	if (!command.empty()) {
		sendPjLinkCommand(command);
	}
}
