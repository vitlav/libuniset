// --------------------------------------------------------------------------
//!  \version $Id: rtustate.cc,v 1.1 2008/12/14 21:57:50 vpashka Exp $
// --------------------------------------------------------------------------
#include <string>
#include <getopt.h>
#include "modbus/ModbusRTUMaster.h"
#include "RTUStorage.h"
// --------------------------------------------------------------------------
using namespace UniSetTypes;
using namespace std;
// --------------------------------------------------------------------------
static struct option longopts[] = {
	{ "help", no_argument, 0, 'h' },
	{ "slave", required_argument, 0, 'q' },
	{ "device", required_argument, 0, 'd' },
	{ "verbose", no_argument, 0, 'v' },
	{ "speed", required_argument, 0, 's' },
	{ NULL, 0, 0, 0 }
};
// --------------------------------------------------------------------------
static void print_help()
{
	printf("Usage: rtustate -q addr\n");
	printf("-h|--help 		- this message\n");
	printf("[-q|--slave] addr                 - Slave address. Default: 0x01.\n");
	printf("[-d|--device] dev                 - use device dev. Default: /dev/ttyS0\n");
	printf("[-s|--speed] speed                - 9600,14400,19200,38400,57600,115200. Default: 38400.\n");
	printf("[-t|--timeout] msec               - Timeout. Default: 2000.\n");
	printf("[-v|--verbose]                    - Print all messages to stdout\n");
}
// --------------------------------------------------------------------------
int main( int argc, char **argv )
{   
	int optindex = 0;
	int opt = 0;
	int verb = 0;
	string dev("/dev/ttyS0");
	string speed("38400");
	ModbusRTU::ModbusAddr slaveaddr = 0x01;
	int tout = 2000;
	DebugStream dlog;

	try
	{
		while( (opt = getopt_long(argc, argv, "hva:d:s:t:q:",longopts,&optindex)) != -1 ) 
		{
			switch (opt) 
			{
				case 'h':
					print_help();	
				return 0;

				case 'd':	
					dev = string(optarg);
				break;

				case 's':	
					speed = string(optarg);
				break;

				case 't':	
					tout = atoi(optarg);
				break;

				case 'q':	
					slaveaddr = ModbusRTU::str2mbAddr(optarg);
				break;

				case 'v':	
					verb = 1;
				break;

				case '?':
				default:
					printf("? argumnet\n");
					return 0;
			}
		}

		if( verb )
		{
			cout << "(init): dev=" << dev 
					<< " speed=" << speed
					<< " timeout=" << tout << " msec "
					<< endl;					
		}		
		
		ModbusRTUMaster mb(dev);

		if( verb )
			dlog.addLevel( Debug::type(Debug::CRIT | Debug::WARN | Debug::INFO) );
		
		mb.setTimeout(tout);
		mb.setSpeed(speed);
		mb.setLog(dlog);

		RTUStorage rtu(slaveaddr);
		
		rtu.poll(&mb);
		cout << rtu << endl;
		
		for( int i=0; i<24; i++ )
			cout << "UNIO1 AI" << i << ": " << rtu.getFloat( RTUStorage::nJ1, i, UniversalIO::AnalogInput ) << endl;
		for( int i=0; i<24; i++ )
			cout << "UNIO1 DI" << i << ": " << rtu.getState( RTUStorage::nJ1, i, UniversalIO::DigitalInput ) << endl;
	}
	catch( ModbusRTU::mbException& ex )
	{
		cerr << "(rtustate): " << ex << endl;
	}
	catch(SystemError& err)
	{
		cerr << "(rtustate): " << err << endl;
	}
	catch(Exception& ex)
	{
		cerr << "(rtustate): " << ex << endl;
	}
	catch(...)
	{
		cerr << "(rtustate): catch(...)" << endl;
	}

	return 0;
}
// --------------------------------------------------------------------------