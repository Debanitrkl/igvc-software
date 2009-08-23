#include <stdio.h>    /* Standard input/output definitions */
#include <stdlib.h>
#include <unistd.h>   /* UNIX standard function definitions */
#include <fcntl.h>    /* File control definitions */
#include <iostream>
#include <cstring>//for memcpy

#include <sys/time.h>
#include <time.h>

#include "ArduinoInterface.h"
#include "ArduinoCmds.hpp"

// TODO:	add a timeouts to serial functions
//			add checks for disconnected arduino
//			add error reporting

#define WAIT_TIME 20 //Used for setting VTIME, each tick is 0.1 s
//#define WAIT_TIME 3 //Used for setting VTIME, each tick is 0.1 s
#define MIN_BYTES_TO_RET 0 //Minimum nuber of bytes for read to return.
#define BAUD B9600  //Serial baudrate
#define SERIAL_PORT "/dev/ttyUSB0"
#define MODULE_ID 'e'

#define SERIAL_TIMOUT_SEC 2

/**
 * Opens a connection to an arduino.
 *
 */
ArduinoInterface::ArduinoInterface()
{

	//Init vars
	tx_num = 1;
	rx_num = 1;
}

bool ArduinoInterface::initLink(byte arduinoID)
{
	/* See if anything is connected on ports USB0 through USB9 */
	char serialAddress[] = SERIAL_PORT;
	for (int i=0; i<10; i++)
	{
		printf("Trying port #%d.\n", (int) i);

		serialAddress[11]=i+'0';
		arduinoFD = serialportInit(serialAddress, BAUD);

		//add a delay -- it seems somthing gets lost if we don't wait
		//TODO: figure out what the problem is
		usleep(3 * 1e6);

		if (arduinoFD == -1)
		{
			printf("no module found.\n");
			//retry//no module found
		}
		else
		{
			/* Check which arduino is connected to the port */
			//byte reply;
			if (sendCommand(ARDUINO_ID_CMD, NULL, 0))
			//if (sendCommand(ARDUINO_GET_ID, NULL, 0))
			{
				std::cout << "could not communicate with arduino (outbound) on link " << serialAddress << std::endl;
				exit(-1);//handle this!
			}

			byte cmdout;
			byte* dataout = NULL;
			if (recvCommand(cmdout, dataout))
			{
				std::cout << "could not communicate with arduino (inbound) on link " << serialAddress << std::endl;
				if (dataout != NULL)
				{
					delete[] dataout;
				}
				exit(-1);//handle this!
			}

			if (cmdout != ARDUINO_ID_CMD)
			{
				std::cout << "arduino responded incorectly during init on link " << serialAddress << std::endl;
				if (dataout != NULL)
				{
					delete[] dataout;
				}
				exit(-1);//handle this!
			}
			byte readid = *(dataout);
			delete[] dataout;
			printf("found module %c\n", readid);
			if (readid == arduinoID)   //might want to use more than one byte for identifcation
			{
				printf("correct module.\n");
				//setArduinoTime();
				break;
			}
			else
			{
				printf("wrong module.\n");
			}
		}
	}
	if (arduinoFD == -1)
	{
		printf("Error: MotorEncoders: could not find arduino\n");
		exit(1);
	}
}

/**
 * Closes the connection to the arduino.
 *
 */
ArduinoInterface::~ArduinoInterface(void)
{
//	for(std::list<DataPacket*>::iterator it = tx_packet_list.begin(); it != tx_packet_list.end(); it++){
//		delete *it;
//	}

	if (arduinoFD == -1)
	{
		return;
	}
	close(arduinoFD);
}

/**
 * Reads the status of the motor controller.
 *
 * @param status	OUT: the motor controller's status.
 * @param size		the size of status in bytes.
 * @return			<tt>true</tt> if an error occurs;
 * 					<tt>false</tt> if successful.
 *					More detailed information can be obtained by querying
 *					<tt>errno</tt> and <tt>strerror(errno)</tt>.
 */
/*
bool ArduinoInterface::getStatus(DataPacket& out_status) {
	if (arduinoFD == -1) {
		return false;
	}

	if(sendCommand('r', (byte*)NULL, 0, out_status)){
		return true;
	}

	return false;
}
*/
/**
 * Set the specified variable in the motor controller to the specified value.
 *
 * @param var		the variable to change.
 * @param value		the new value for the variable.
 * @param size		size of value in bytes.
 * @return			<tt>true</tt> if an error occurs;
 * 					<tt>false<writeFully(arduinoFD, "r", 1)/tt> if successful.
 *					More detailed information can be obtained by querying
 *					<tt>errno</tt> and <tt>strerror(errno)</tt>.
 */
bool ArduinoInterface::setVar(int var, void *value, int size)
{
	if (arduinoFD == -1)
	{
		return false;
	}

	char buf[2+size]; // TODO: is there a better way to initalize this?
	buf[0] = 'w';
	buf[1] = var;
	for (int i = 0; i < size; i++)
	{
		buf[2+i] = ((byte *)value)[i];
	}
	if (writeFully(arduinoFD, buf, sizeof(buf)))
	{
		return true;
	}
	return false;
}

// ### PRIVATE FUNCTIONS: SERIAL API ###

/**
 * Write exactly <tt>numBytes</tt> bytes to file <tt>fd</tt>
 * from buffer <tt>buf</tt>.
 *
 * @param fd		the file to write to.
 * @param buf		the buffer to read from.
 * @param numBytes	the number of bytes to write.
 * @return			<tt>true</tt> if an error occurs;
 * 					<tt>false</tt> if successful.
 */
bool ArduinoInterface::writeFully(int fd, void* buf, size_t numBytes)
{
	char* src = (char*) buf;
	size_t numLeft = numBytes;
	ssize_t numWritten;

	while (numLeft > 0)
	{
		numWritten = write(fd, src, numLeft);
		if (numWritten < 0)
		{
			return true;
		}

		numLeft -= numWritten;
		src += numWritten;
	}
	return false;
}

/**
 * Read exactly <tt>numBytes</tt> bytes from file <tt>fd</tt>
 * into buffer <tt>buf</tt>.
 *
 * @param fd		the file to read from.
 * @param buf		the buffer to read into.
 * @param numBytes	the number of bytes to read.
 * @return			<tt>true</tt> if an error occurs;
 * 					<tt>false</tt> if successful.
 */
bool ArduinoInterface::readFully(int fd, void* buf, size_t numBytes)
{
	char* dst = (char*) buf;
	size_t numLeft = numBytes;
	ssize_t numRead;

	while (numLeft > 0)
	{
		numRead = read(fd, dst, numLeft);
		if (numRead < 0)
		{
			return true;
		}

		numLeft -= numRead;
		dst += numRead;

		//if( (getTime() - t0) > )

	}
	return false;
}

/**
 * Open a serial port at device <tt>serialport</tt> and initialize
 * it for raw mode with a baud rate of <tt>baud</tt>.
 * All other parameters are set by the function.
 *
 * @param serialport	the serial port to open and initialize.
 * @param baud			the baud rate at which to run the serial port.
 * @return 				<tt>fd</tt> the file descriptor for the open serial port;
 *                       -1 if an error occurs.
 */
int ArduinoInterface::serialportInit(const char* serialport, speed_t baud)
{
	struct termios toptions;
	int fd;

	//fprintf(stderr,"init_serialport: opening port %s @ %d bps\n",
	//		serialport,baud);

	fd = open(serialport, (O_RDWR | O_NOCTTY | O_NDELAY) &~ O_NONBLOCK);
	if (fd == -1)
	{
		//perror("serialport_init: Unable to open port ");
		return -1;
	}

	if (tcgetattr(fd, &toptions) < 0)
	{
		//perror("serialport_init: Couldn't get term attributes");
		return -1;
	}

	cfsetispeed(&toptions, baud);
	cfsetospeed(&toptions, baud);

	// 8N1
	toptions.c_cflag &= ~PARENB;
	toptions.c_cflag &= ~CSTOPB;
	toptions.c_cflag &= ~CSIZE;
	toptions.c_cflag |= CS8;

	// no flow control
	toptions.c_cflag &= ~CRTSCTS;

	toptions.c_cflag |= CREAD | CLOCAL;  // turn on READ & ignore ctrl lines
	toptions.c_iflag &= ~(IXON | IXOFF | IXANY); // turn off s/w flow ctrl

	toptions.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // make raw
	toptions.c_oflag &= ~OPOST; // make raw

	/* see: http://unixwiz.net/techtips/termios-vmin-vtime.html */
	// Set read-timeout for reading each byte
	toptions.c_cc[VTIME] = WAIT_TIME;
	//toptions.c_cc[VMIN] = MIN_BYTES_TO_RET;

	if ( tcsetattr(fd, TCSANOW, &toptions) < 0)
	{
		//perror("serialport_init: Couldn't set term attributes");
		return -1;
	}

	return fd;
}

/**
 * Clear any bytes in the incoming serial buffer. TODO: do this for outgoing buffer, too.
 *
 * @param fd		the file to clear.
 */
bool ArduinoInterface::serialFlush(int fd)   // TODO: is there a function for this?
{
	char dst[1];
	ssize_t numRead = 0;

	while (numRead)
	{
		numRead = read(fd, dst, 1);
	}
	if (numRead < 0)
	{
		return true;
	}
}

void savePacket(DataPacket* pk)
{

}

/**
 * Read exactly <tt>numBytes</tt> bytes from file <tt>fd</tt>
 * into buffer <tt>buf</tt>.
 *
 * @param cmd		the arduino command
 * @param data_tx		the asociated data body
 * @param size_tx		the length of the data body
 * @param data_rx	a pace to put the return messege body
 * @param size_rx	the expected/max length of the rx messge body
 * @return			<tt>true</tt> if an error occurs;
 * 					<tt>false</tt> if successful.
 */



//getResponse();

//true on error
bool ArduinoInterface::arduinoResendPacket(int pknum, DataPacket& pk_out)
{

	sendCommand(ARDUINO_RSND_PK_CMD, &pknum, sizeof(int));

	//Get the Response
	DataPacket pk_rx;
	readFully(arduinoFD, &(pk_rx.header), PACKET_HEADER_SIZE);
	pk_rx.data = new byte[pk_rx.header.size];
	readFully(arduinoFD, pk_rx.data, pk_rx.header.size);
	rx_num++;

	//std::cout << "header\n" << pk_rx;

	switch (pk_rx.header.cmd)
	{
	case ARDUINO_ERROR_RESP:
	{
		int errormsg;
		memcpy(&errormsg, pk_rx.data, sizeof(int));
		std::cout << "error requesting packet num: " << pknum << std::endl << "got error num: " << errormsg << std::endl;
		switch (errormsg)
		{
		case DROPPED_PACKET:
		{
			std::cout << "dropped packet" << std::endl;
			break;
		}
		case REQUESTED_PACKET_OUT_OF_RANGE:
		{
			std::cout << "packet not in cache" << std::endl;
			break;
		}
		default:
		{
			std::cout << "unknown error" << std::endl;
			break;
		}
		}
		return true;
		break;
	}
	case ARDUINO_RSND_PK_RESP:
	{
		memcpy(&(pk_out.header), pk_rx.data, PACKET_HEADER_SIZE);
		if (pknum != pk_out.header.packetnum)
		{
			return true;
		}

		pk_out.data = new byte[pk_out.header.size];
		memcpy(pk_out.data, pk_rx.data + PACKET_HEADER_SIZE, pk_out.header.size);

		//std::cout << "resend pk debug intern" << std::endl;
		//std::cout << "data size: " << (int) pk_out.header.size << std::endl;
		//std::cout << "dataloc: " << (int) pk_out.data << std::endl;

		//encoder_reply_t parsed_data;
		//memcpy(&parsed_data, pk_out.data, sizeof(encoder_reply_t));
		//std::cout << pk_out.header << std::endl;
		//std::cout << parsed_data << "\n\n";

		break;
	}
	}
	return false;
}

void ArduinoInterface::savePacket(DataPacket pk)
{
	if (tx_packet_list.size() > 49)
	{
		tx_packet_list.pop_back();
	}

	DataPacket pkstore = pk;

	tx_packet_list.push_front(pkstore);
}

//Get a packet that was set to the arduino from the list, by number
DataPacket ArduinoInterface::getSavedPacket(int packnum)
{
	std::list<DataPacket>::iterator it;
	for (it = tx_packet_list.begin(); it != tx_packet_list.end(); it++)
	{
		if ((*it).header.packetnum == packnum)
		{
			DataPacket out = *it;
			return(out);
		}
	}
}

struct timeval ArduinoInterface::getTime()
{
	struct timeval time;
	//struct timezone tz;
	//gettimeofday(&time, &tz);
	gettimeofday(&time, NULL);
	//unsigned int millis = time.tv_sec*1000 + ((long int)((double)time.tv_usec/1000));

	return(time);
}

bool ArduinoInterface::setArduinoTime()
{
	byte msg[9];
	msg[0] = MC_SETCLK;

	struct timeval t0 = getTime();

	memcpy(msg+1, &t0.tv_sec, sizeof(unsigned long));
	memcpy(msg+5, &t0.tv_usec, sizeof(unsigned long));

	DataPacket rx_p;

	sendCommand('w', msg, 9);
	recvCommand(rx_p.header.cmd, rx_p.data);
}


//new cmd struct notes
//arduino send pk
//true on error
bool ArduinoInterface::sendPacket(DataPacket pkout)
{

	savePacket(pkout);

	writeFully(arduinoFD, &(pkout.header), PACKET_HEADER_SIZE);
	if (pkout.header.size > 0)
	{
		writeFully(arduinoFD, pkout.data, pkout.header.size);
	}
	tx_num++;

	return false;
}


//arduino ret pk
//arduino process error
//true on error
bool ArduinoInterface::getPacket(DataPacket& out_pk_rx)
{
	readFully(arduinoFD, &(out_pk_rx.header), PACKET_HEADER_SIZE);
	if (out_pk_rx.header.size > 0)
	{
		out_pk_rx.data = new byte[out_pk_rx.header.size];
		readFully(arduinoFD, out_pk_rx.data, out_pk_rx.header.size);
	}
	rx_num++;
	/*
		if(out_pk_rx.header.packetnum != (rx_num-1)){//rx_num -1 'caue rx_num is incr inside getpacket
			std::cout << "dropped packet - rec header:" << std::endl;
			std::cout << out_pk_rx.header << std::endl;
			std::cout << "dropped packet - flushing link and requesting" << std::endl;

			DataPacket pk_rsnd;
			serialFlush(arduinoFD);
				if(!arduinoResendPacket(rx_num, pk_rsnd))
				{
					out_pk_rx.clear();
					out_pk_rx = pk_rsnd;
				}
				else
				{
					return true;
				}
		}
	*/
	//parse the icoming packet, test if it is an error packet
	if (out_pk_rx.header.cmd == 0xFF )
	{
		byte errorbuff[out_pk_rx.header.size];
		readFully(arduinoFD, errorbuff, out_pk_rx.header.size);
		byte errorcode;
		memcpy(&errorcode, errorbuff, 1 );
		switch (errorcode)
		{
		case DROPPED_PACKET:
		{
			std::cout << "arduino requested dropped packet - resending" << std::endl;
			DataPacket pk;
			int pknum;
			memcpy(&pknum, errorbuff+1, sizeof(int) );
			pk = getSavedPacket(pknum);
			writeFully(arduinoFD, &(pk.header), PACKET_HEADER_SIZE);
			writeFully(arduinoFD, pk.data, pk.header.size);
			break;
		}
		default:
			std::cout << "unknown error in recieved packet" << std::endl;
			return true;
			break;
		}
	}

	std::cout << "rec header:\n" << out_pk_rx.header << std::endl;

	return false;
}

bool ArduinoInterface::read_TimeOut(int fd, void * buf, size_t numBytes)
{
	char* dst = (char*) buf;
	size_t numLeft = numBytes;
	ssize_t numRead;

	struct timeval t0 = getTime();

	while ( numLeft > 0)
	{
		numRead = read(fd, dst, numLeft);
		if (numRead < 0)
		{
			return true;
		}

		numLeft -= numRead;
		dst += numRead;

		if ( (getTime().tv_sec - t0.tv_sec) < SERIAL_TIMOUT_SEC)
		{
			return true;
		}

	}
	return false;
}

//bool ArduinoInterface::sendCommand(char cmd, void * data_tx, int size_tx, void * data_rx, int size_rx){
bool ArduinoInterface::sendCommand(byte cmd, void * data_tx, int size_tx)
{
	//Send the Command
	DataPacket pk_tx;
	pk_tx.header.packetnum = tx_num;

	struct timeval t0 = getTime();
	pk_tx.header.timestamp_sec = t0.tv_sec;
	pk_tx.header.timestamp_usec = t0.tv_usec;

	pk_tx.header.cmd = cmd;
	pk_tx.header.size = size_tx;


	pk_tx.data = new byte[size_tx];
	memcpy(pk_tx.data, data_tx, size_tx);


	if (sendPacket(pk_tx))
	{
		std::cout << "could not send packet" << std::endl;
		return true;
	}

	return false;
}

//true on error
bool ArduinoInterface::recvCommand(byte& out_cmd, byte*& out_data_rx)
{

	DataPacket out_pk_rx;

	if (getPacket(out_pk_rx))
	{
		std::cout << "could not rec packet" << std::endl;
		return true;
	}

	out_cmd = out_pk_rx.header.cmd;

	out_data_rx = new byte[out_pk_rx.header.size];

	memcpy(out_data_rx, out_pk_rx.data, out_pk_rx.header.size);

	return false;
}
