// $Id: IOControl.cc,v 1.3 2009/01/23 23:56:54 vpashka Exp $
// -----------------------------------------------------------------------------
#include <sstream>
#include "ORepHelpers.h"
#include "UniSetTypes.h"
#include "Extensions.h"
#include "IOControl.h"
// -----------------------------------------------------------------------------
using namespace std;
using namespace UniSetTypes;
using namespace UniSetExtensions;
// -----------------------------------------------------------------------------
std::ostream& operator<<( std::ostream& os, IOControl::IOInfo& inf )
{
	return os << "(" << inf.si.id << ")" << conf->oind->getMapName(inf.si.id)
		<< " card=" << inf.ncard << " channel=" << inf.channel << " subdev=" << inf.subdev 
		<< " aref=" << inf.aref << " range=" << inf.range 
		<< " default=" << inf.defval << " safety=" << inf.safety;
}
// -----------------------------------------------------------------------------

IOControl::IOControl( UniSetTypes::ObjectId id, UniSetTypes::ObjectId icID,
						SharedMemory* ic, int numcards ):
	UniSetObject(id),
	polltime(500),
	cards(numcards+1),
	noCards(true),
	iomap(100),
	maxItem(0),
	filterT(0),
	shm(0),
	myid(id),
	blink_state(true),
	testLamp_S(UniSetTypes::DefaultObjectId),
	isTestLamp(false),
	sidHeartBeat(UniSetTypes::DefaultObjectId),
	force(false),
	force_out(false),
	activated(false),
	readconf_ok(false),
	term(false)
{
	cout << "$Id: IOControl.cc,v 1.3 2009/01/23 23:56:54 vpashka Exp $" << endl;
//	{
//		string myfullname = conf->oind->getNameById(id);
//		myname = ORepHelpers::getShortName(myfullname.c_str());
//	}

	string cname = conf->getArgParam("--io-confnode",myname);
	cnode = conf->getNode(cname);
	if( cnode == NULL )
		throw SystemError("Not find conf-node " + cname + " for " + myname);

	defCardNum = atoi( conf->getArgParam("--io-default-cardnum","-1").c_str());

	UniXML_iterator it(cnode);

	noCards = true;
	for( unsigned int i=1; i<=cards.size(); i++ )
	{
		stringstream s1;
		s1 << "--iodev" << i;
		stringstream s2;
		s2 << "iodev" << i;

		string iodev = conf->getArgParam(s1.str(),it.getProp(s2.str()));
		if( iodev.empty() || iodev == "/dev/null" )
		{
			unideb[Debug::LEVEL3] << myname << "(init): ����� N" << i 
								<< " ��������� (TestMode)!!! � �������� ���������� ������� '" 
								<< iodev << "'" << endl;
			cards[i] = 0;
			cout << "******************** CARD" << i << ": IO IMITATOR MODE ****************" << endl;			
		}
		else
		{
			noCards = false;
			cards[i] = new ComediInterface(iodev);
			cout << "card" << i << ": " << cards[i]->devname() << endl;
		}
		
		if( cards[i] )
		{
			for( int s=0; s<4; s++ )
			{
				stringstream t1;
				t1 << s1.str() << "-subdev" << s << "-type";
				stringstream t2;
				t2 << s2.str() << "-subdev" << s << "-type";
				
				string stype = conf->getArgParam(t1.str(),it.getProp(t2.str()));
				if( !stype.empty() )
				{
//					ComediInterface::SubdevType st = (ComediInterface::SubdevType)UniSetTypes::uni_atoi(stype.c_str());
					ComediInterface::SubdevType st = ComediInterface::str2type(stype.c_str());
					if( !stype.empty() && st == ComediInterface::Unknown )
					{
						ostringstream err;
						err << "Unknown subdev type '" << stype << " for " << t1 << " OR " << t2;
						throw SystemError(err.str());
					}

					unideb[Debug::INFO] << myname 
										<< "(init): card" << i 
										<< " subdev" << s << " set type " << stype << endl;

					cards[i]->configureSubdev(s,st);
				}
			}
		}
	}
	
	polltime = atoi(conf->getArgParam("--io-polltime",it.getProp("polltime")).c_str());
	if( !polltime )
		polltime = 150;

	force 		= atoi(conf->getArgParam("--io-force",it.getProp("force")).c_str());
	force_out 	= atoi(conf->getArgParam("--io-force-out",it.getProp("force_out")).c_str());

	filtersize = atoi(conf->getArgParam("--io-filtersize",it.getProp("filtersize")).c_str());
	if( filtersize<=0 )
		filtersize = 1;

	filterT = atof(conf->getArgParam("--io-filterT",it.getProp("filterT")).c_str());

	string testlamp = conf->getArgParam("--io-test-lamp",it.getProp("testlamp_s"));
	if( !testlamp.empty() )
	{
		testLamp_S = conf->getSensorID(testlamp);
		if( testLamp_S == DefaultObjectId )
		{
			ostringstream err;
			err << myname << ": �� ������ ������������� ��� ������� ��������: " << testlamp;
			unideb[Debug::CRIT] << myname << "(init): " << err.str() << endl;
			throw SystemError(err.str());
		}
	
		unideb[Debug::INFO] << myname << "(init): testLamp_S='" << testlamp << "'" << endl;
	}

	shm = new SMInterface(icID,&ui,myid,ic);

	// ���������� ������
	s_field = conf->getArgParam("--io-s-filter-field");
	s_fvalue = conf->getArgParam("--io-s-filter-value");

	unideb[Debug::INFO] << myname << "(init): read s_field='" << s_field
						<< "' s_fvalue='" << s_fvalue << "'" << endl;

	int blink_msec = atoi(conf->getArgParam("--io-blink-time",it.getProp("blink-time")).c_str());
	if( blink_msec<=0 )
		blink_msec = 300;
	
	ptBlink.setTiming(blink_msec);

	smReadyTimeout = atoi(conf->getArgParam("--io-sm-ready-timeout",it.getProp("ready_timeout")).c_str());
	if( smReadyTimeout == 0 )
		smReadyTimeout = 15000;
	else if( smReadyTimeout < 0 )
		smReadyTimeout = UniSetTimer::WaitUpTime;


	string sm_ready_sid = conf->getArgParam("--io-sm-ready-test-sid",it.getProp("sm_ready_test_sid"));
	sidTestSMReady = conf->getSensorID(sm_ready_sid.c_str());
	if( sidTestSMReady == DefaultObjectId )
	{
		sidTestSMReady = 4100; /* TestMode_S */
		unideb[Debug::WARN] << myname 
				<< "(init): �� ������ ������������� ������� ����� SM (--io-sm-ready-test-sid)." 
				<< " ��ң� TestMode_S(4100)" << endl;
	}
	else
		unideb[Debug::INFO] << myname << "(init): test-sid: " << sm_ready_sid << endl;


	// -----------------------
	string heart = conf->getArgParam("--io-heartbeat-id",it.getProp("heartbeat_id"));
	if( !heart.empty() )
	{
		sidHeartBeat = conf->getSensorID(heart);
		if( sidHeartBeat == DefaultObjectId )
		{
			ostringstream err;
			err << myname << ": �� ������ ������������� ��� ������� 'HeartBeat' " << heart;
			unideb[Debug::CRIT] << myname << "(init): " << err.str() << endl;
			throw SystemError(err.str());
		}

		int heartbeatTime = atoi(conf->getArgParam("--heartbeat-check-time","1000").c_str());
		if( heartbeatTime )
			ptHeartBeat.setTiming(heartbeatTime);
		else
			ptHeartBeat.setTiming(UniSetTimer::WaitUpTime);

		maxHeartBeat = atoi(conf->getArgParam("--io-heartbeat-max",it.getProp("heartbeat_max")).c_str());
		if( maxHeartBeat <=0 )
			maxHeartBeat = 10;
	}

	activateTimeout	= atoi(conf->getArgParam("--activate-timeout").c_str());
	if( activateTimeout<=0 )
		activateTimeout = 25000;

	if( !shm->isLocalwork() ) // ic
		ic->addReadItem( sigc::mem_fun(this,&IOControl::readItem) );
}

// --------------------------------------------------------------------------------

IOControl::~IOControl()
{
	// ����� �� �ݣ �������� �� ������ � ������� delete ���
	// ���� cdiagram ��������� ����� new
	// 
	for( unsigned int i=0; i<cards.size(); i++ )
		delete cards[i];

	delete shm;
}

// --------------------------------------------------------------------------------
void IOControl::execute()
{
//	set_signals(true);
	UniXML_iterator it(cnode);

	waitSM(); // ���������� ���������, ����� ��������� ���������������� ���������

	PassiveTimer pt(UniSetTimer::WaitUpTime);
	
	if( shm->isLocalwork() )
	{
		maxItem = 0;
		readConfiguration();
		cerr << "************************** readConfiguration: " << pt.getCurrent() << " msec " << endl;
	}
	else
	{
		// init iterators
		for( IOMap::iterator it=iomap.begin(); it!=iomap.end(); ++it )
		{
			shm->initAIterator(it->ait);
			shm->initDIterator(it->dit);
		}
		readconf_ok = true; // �.�. waitSM() ��� ���...
	}
	
	iomap.resize(maxItem);
	unideb[Debug::INFO] << myname << "(init): iomap size = " << iomap.size() << endl;

	cerr << myname << "(iomap size): " << iomap.size() << endl;

	// ������ ���������� �� ������-�������
	initIOCard();

	bool skip_iout = atoi(conf->getArgParam("--io-skip-init-output").c_str());
	if( !skip_iout )
		initOutputs();

	shm->initAIterator(aitHeartBeat);
	shm->initDIterator(ditTestLamp);

	PassiveTimer ptAct(activateTimeout);
	while( !activated && !ptAct.checkTime() )
	{	
		cout << myname << "(execute): wait activate..." << endl;
		msleep(300);
		if( activated )
		{
			cout << myname << "(execute): activate OK.." << endl;
			break;
		}
	}
			
	if( !activated )
		unideb[Debug::CRIT] << myname << "(execute): ************* don`t activate?! ************" << endl;

	try
	{
		// init first time....
		if( !force && !noCards )
		{
			uniset_mutex_lock l(iopollMutex,5000);
			force = true;
			iopoll();
			force = false;
		}
	}
	catch(...){}
	
	while(!term)
	{
		try
		{	
			if( !noCards )
			{
				check_testlamp();
			
				if( ptBlink.checkTime() )
				{
					ptBlink.reset();
					try
					{
						blink();
					}
					catch(...){}
				}
				
				uniset_mutex_lock l(iopollMutex,5000);
				iopoll();
			}

			if( sidHeartBeat!=DefaultObjectId && ptHeartBeat.checkTime() )
			{
				shm->localSaveValue(aitHeartBeat,sidHeartBeat,maxHeartBeat,myid);
				ptHeartBeat.reset();
			}
		}
		catch( Exception& ex )
		{
			unideb[Debug::LEVEL3] << myname << "(execute): " << ex << endl;
		}
		catch(CORBA::SystemException& ex)
		{
			unideb[Debug::LEVEL3] << myname << "(execute): �ORBA::SystemException: "
				<< ex.NP_minorString() << endl;
		}
		catch(...)
		{
			unideb[Debug::LEVEL3] << myname << "(execute): catch ..." << endl;
		}
		
		if( term )
			break;
	
		msleep( polltime );
	}
	
	term = false;
}
// --------------------------------------------------------------------------------
void IOControl::iopoll()
{
	ComediInterface* card = 0;
	int val = 0;

	for( IOMap::iterator it=iomap.begin(); it!=iomap.end(); ++it )
	{
		if( it->ignore || it->ncard == defCardNum )
			continue;

		card = 0;
		if( it->ncard >= 0 && it->ncard<cards.size() )
			card = cards[it->ncard];


		if( !card || it->subdev==DefaultSubdev || it->channel==DefaultChannel )
			continue;

//		cout  << conf->oind->getMapName(it->si.id) 
//				<< " subdev: " << it->subdev << " chan: " << it->channel << endl;

		if( it->si.id == DefaultObjectId )
		{
			cerr << myname << "(iopoll): sid=DefaultObjectId?!" << endl;
			continue;
		}

		IOBase* ib = &(*it);

		try
		{
			if( it->stype == UniversalIO::AnalogInput )
			{
				val = card->getAnalogChannel(it->subdev,it->channel, it->range, it->aref);
/*
				if( unideb.debugging(Debug::LEVEL3) )
				{
					unideb[Debug::LEVEL3] << myname << "(iopoll): read AI "
						<< " sid=" << it->si.id 
						<< " subdev=" << it->subdev 
						<< " chan=" << it->channel
						<< " val=" << val
						<< endl;
				}
*/				
				IOBase::processingAsAI( ib, val, shm, force );
			}
			else if( it->stype == UniversalIO::DigitalInput )
			{
				bool set = card->getDigitalChannel(it->subdev,it->channel);
/*
				if( unideb.debugging(Debug::LEVEL3) )
				{
					unideb[Debug::LEVEL3] << myname << "(iopoll): read DI "
						<< " sid=" << it->si.id 
						<< " subdev" << it->subdev 
						<< " chan=" << it->channel
						<< " state=" << set
						<< endl;
				}
*/
				IOBase::processingAsDI( ib, set, shm, force );
								
				// ������� �����������
				// ����� ����������.���������� ���� ������������
				if( it->si.id == testLamp_S )
					isTestLamp = set;
			}
			else if( it->stype == UniversalIO::AnalogOutput )
			{
				if( !it->lamp )
				{
					IOBase::processingAsAO( ib, shm, force_out );
					card->setAnalogChannel(it->subdev,it->channel,it->value,it->range,it->aref);
				}
				else // ���������� ����������
				{
					uniset_spin_lock lock(it->val_lock);
					long prev_val = it->value;
					if( force_out )
						it->value = shm->localGetValue(it->ait,it->si.id);

					switch( it->value )
					{
						case lmpOFF:
						{
							if( force_out && prev_val == lmpBLINK )
								delBlink(it);

							if( it->no_testlamp || (!it->no_testlamp && !isTestLamp) )
								card->setDigitalChannel(it->subdev,it->channel,0);
						}
						break;
	
						case lmpON:
						{
							if( force_out && prev_val == lmpBLINK )
								delBlink(it);

							if( it->no_testlamp || (!it->no_testlamp && !isTestLamp) )
								card->setDigitalChannel(it->subdev,it->channel,1);
						}
						break;

						case lmpBLINK:
						{
							if( force_out && prev_val != lmpBLINK )
							{
								addBlink(it);
								// � ����� ��������, ����� �� ���� ����� (��� ���������� �������� ��� ���������)
								card->setDigitalChannel(it->subdev,it->channel,1);
							}
						}
						break;
						
						default:
							continue;
					}
				}
			}
			else if( it->stype == UniversalIO::DigitalOutput )
			{
				bool set = IOBase::processingAsDO(ib,shm,force_out);
				if( !it->lamp || (it->lamp && !isTestLamp) )
					card->setDigitalChannel(it->subdev,it->channel,set);
			}
		}
		catch(IOController_i::NameNotFound &ex)
		{
			unideb[Debug::LEVEL3] << myname << "(iopoll):(NameNotFound) " << ex.err << endl;
		}
		catch(IOController_i::IOBadParam& ex )
		{
			unideb[Debug::LEVEL3] << myname << "(iopoll):(IOBadParam) " << ex.err << endl;
		}
		catch(IONotifyController_i::BadRange )
		{
			unideb[Debug::LEVEL3] << myname << "(iopoll): (BadRange)..." << endl;
		}
		catch( Exception& ex )
		{
			unideb[Debug::LEVEL3] << myname << "(iopoll): " << ex << endl;
		}
		catch(CORBA::SystemException& ex)
		{
			unideb[Debug::LEVEL3] << myname << "(iopoll): �ORBA::SystemException: "
				<< ex.NP_minorString() << endl;
		}
		catch(...)
		{
			unideb[Debug::LEVEL3] << myname << "(iopoll): catch ..." << endl;
		}
	}

	for( IOMap::iterator it=iomap.begin(); it!=iomap.end(); ++it )
			IOBase::processingThreshold(&(*it),shm,force);
}
// --------------------------------------------------------------------------------
void IOControl::readConfiguration()
{
	readconf_ok = false;

	xmlNode* root = conf->getXMLSensorsSection();
	if(!root)
	{
		ostringstream err;
		err << myname << "(readConfiguration): �� ����� ��������� ������� <sensors>";
		throw SystemError(err.str());
	}

	UniXML_iterator it(root);
	if( !it.goChildren() )
	{
		std::cerr << myname << "(readConfiguration): ������ <sensors> �� �������� ������ ?!!\n";
		return;
	}

	for( ;it.getCurrent(); it.goNext() )
	{
		if( check_item(it) )
			initIOItem(it);
	}
	
	readconf_ok = true;
}
// ------------------------------------------------------------------------------------------
bool IOControl::readItem( UniXML& xml, UniXML_iterator& it, xmlNode* sec )
{
	if( check_item(it) )
		initIOItem(it);
	
	return true;
}
// ------------------------------------------------------------------------------------------
bool IOControl::check_item( UniXML_iterator& it )
{
	if( s_field.empty() )
		return true;

	// ������ �������� �� �� ������ field
	if( s_fvalue.empty() && it.getProp(s_field).empty() )
		return false;

	// ������ �������� ��� field = value
	if( !s_fvalue.empty() && it.getProp(s_field)!=s_fvalue )
		return false;

	return true;
}
// ------------------------------------------------------------------------------------------

bool IOControl::initIOItem( UniXML_iterator& it )
{
	IOInfo inf;

	string c(it.getProp("card"));

	if( c.empty() )
		inf.ncard = defCardNum;
	else
		inf.ncard = atoi( c.c_str() );

	if( c.empty() || inf.ncard<0 || inf.ncard >= cards.size() )
	{
		dlog[Debug::LEVEL3] << myname 
							<< "(initIOItem): �� ������ ��� �������� ����� ����� (" 
							<< inf.ncard << ") ��� " << it.getProp("name") 
							<< " set default=" << defCardNum << endl;
		inf.ncard = defCardNum;
	}

	inf.subdev = atoi( it.getProp("subdev").c_str());
	
	if( inf.subdev < 0 )
		inf.subdev = DefaultSubdev;

	string jack = it.getProp("jack");
	if( !jack.empty() )
	{
		if( jack == "J1" )
			inf.subdev = 0;
		else if( jack == "J2" )
			inf.subdev = 1;
		else if( jack == "J3" )
			inf.subdev = 2;
		else if( jack == "J4" )
			inf.subdev = 3;
		else if( jack == "J5" )
			inf.subdev = 4;
		else
			inf.subdev = DefaultSubdev;
	}
	
	inf.channel = atoi((it.getProp("channel")).c_str());
	if( inf.channel<0 || inf.channel > 32 )
	{
		unideb[Debug::WARN] << myname << "(readItem): ����������� �����: " << inf.channel
							<< " ��� " << it.getProp("name") << endl;
		return false;
	}

	if( !IOBase::initItem(&inf,it,shm,&unideb,myname,filtersize,filterT) )
		return false;

	inf.lamp = atoi( it.getProp("lamp").c_str() );
	inf.no_testlamp = atoi( it.getProp("no_iotestlamp").c_str() );
	inf.aref = 0;
	inf.range = 0;

	if( inf.stype == UniversalIO::AnalogInput || inf.stype == UniversalIO::AnalogOutput )
	{
		inf.range = atoi((it.getProp("range")).c_str());
		if( inf.range < 0 || inf.range > 3 )
		{
			unideb[Debug::WARN] << myname << "(readItem): ����������� ����������� ��������(range): " << inf.range
							<< " ��� " << it.getProp("name") 
							<< " ���������� ��������: range=[0..3]" << endl;
			return false;
		}

		inf.aref = atoi((it.getProp("aref")).c_str());
		if( inf.aref < 0 || inf.aref > 3 )
		{
			unideb[Debug::WARN] << myname << "(readItem): ����������� ��� �����������: " << inf.aref
							<< " ��� " << it.getProp("name") << endl;
			return false;
		}
	}

	if( unideb.debugging(Debug::LEVEL3) )
		unideb[Debug::LEVEL3] << myname << "(readItem): add: " << inf.stype << " " << inf << endl;

	// ���� ������ ��� ��������
	// �� ����������� ��� �� 10 ��������� (� �������)
	// ����� ������������� �������� resize
	// ��� �������� ����������
	if( maxItem >= iomap.size() )
		iomap.resize(maxItem+10);
	
	iomap[maxItem++] = inf;
	return true;
}
// ------------------------------------------------------------------------------------------

bool IOControl::activateObject()
{
	// ������������ ��������� Startup 
	// ���� �� ����ģ� ������������� ��������
	// ��. sysCommand()
	{
		activated = false;
		UniSetObject::activateObject();
		activated = true;
	}

	return true;
}
// ------------------------------------------------------------------------------------------
void IOControl::sigterm( int signo )
{
	term = true;

	if( noCards )
		return;

	ComediInterface* card = 0;

	// ���������� ���������� ���������
	for( IOMap::iterator it=iomap.begin(); it!=iomap.end(); ++it )
	{
		if( it->ignore )
			continue;

		card = 0;
		if( it->ncard>=0 && it->ncard<cards.size() )
			card = cards[it->ncard];

		if( !card )
			continue;

		try
		{
			if( it->subdev==DefaultSubdev || it->safety == NoSafety )
				continue;

			if( it->stype == UniversalIO::DigitalOutput || it->lamp )
			{
				bool set = it->invert ? !((bool)it->safety) : (bool)it->safety;
				card->setDigitalChannel(it->subdev,it->channel,set);
			}
			else if( it->stype == UniversalIO::AnalogOutput )				
			{
				card->setAnalogChannel(it->subdev,it->channel,it->safety,it->range,it->aref);
			}
		}
		catch( Exception& ex )
		{
			unideb[Debug::LEVEL3] << myname << "(sigterm): " << ex << endl;
		}
		catch(...){}
	}
	
	
	while( term ){}
}
// -----------------------------------------------------------------------------
void IOControl::initOutputs()
{
	if( noCards )
		return;

	ComediInterface* card = 0;

	// ���������� �������� �� ���������
	for( IOMap::iterator it=iomap.begin(); it!=iomap.end(); ++it )
	{
		if( it->ignore )
			continue;

		card = 0;
		if( it->ncard>=0 && it->ncard<cards.size() )
			card = cards[it->ncard];

		try
		{
			if( !card || it->subdev==DefaultSubdev || it->channel==DefaultChannel )
				continue;

			if( it->lamp )
				card->setDigitalChannel(it->subdev,it->channel,(bool)it->defval);
			else if( it->stype == UniversalIO::DigitalOutput )
				card->setDigitalChannel(it->subdev,it->channel,(bool)it->defval);
			else if( it->stype == UniversalIO::AnalogOutput )				
				card->setAnalogChannel(it->subdev,it->channel,it->defval,it->range,it->aref);
		}
		catch( Exception& ex )
		{
			unideb[Debug::LEVEL3] << myname << "(initOutput): " << ex << endl;
		}
	}
}
// ------------------------------------------------------------------------------
void IOControl::initIOCard()
{
	if( noCards )
		return;

	ComediInterface* card = 0;

	for( IOMap::iterator it=iomap.begin(); it!=iomap.end(); ++it )
	{
		if( it->subdev == DefaultSubdev )
			continue;

		card = 0;
		if( it->ncard>=0 && it->ncard<cards.size() )
			card = cards[it->ncard];

		if( !card || it->subdev==DefaultSubdev || it->channel==DefaultChannel )
			continue;

		try
		{	
			// ��������������� ���������� ������ ���������� �����/������
			// ��� "��������" (�.�. ��� ��������� ����������� �������)
			if( it->lamp )
				card->configureChannel(it->subdev,it->channel,ComediInterface::DO);
			else if( it->stype == UniversalIO::DigitalInput )
				card->configureChannel(it->subdev,it->channel,ComediInterface::DI);
			else if( it->stype == UniversalIO::DigitalOutput )
				card->configureChannel(it->subdev,it->channel,ComediInterface::DO);
			else if( it->stype == UniversalIO::AnalogInput )
			{
				card->configureChannel(it->subdev,it->channel,ComediInterface::AI);
				it->df.init( card->getAnalogChannel(it->subdev, it->channel, it->range, it->aref) );
			}
			else if( it->stype == UniversalIO::AnalogOutput )
				card->configureChannel(it->subdev,it->channel,ComediInterface::AO);

		}
		catch( Exception& ex)
		{
			unideb[Debug::CRIT] << myname << "(initIOCard): sid=" << it->si.id 
										<< " " << ex << endl;
		}
	}
}	
// -----------------------------------------------------------------------------
void IOControl::blink()
{
	if( lstBlink.empty() )
		return;

	ComediInterface* card(0);
	
	for( BlinkList::iterator it=lstBlink.begin(); it!=lstBlink.end(); ++it )
	{
		IOMap::iterator& io(*it);

		if( io->subdev == DefaultSubdev || io->channel==DefaultChannel )
			continue;

		card = 0;
		if( io->ncard>=0 && io->ncard<cards.size() )
			card = cards[io->ncard];

		if( !card )
			continue;

		try
		{			
			card->setDigitalChannel(io->subdev,io->channel,blink_state);
		}
		catch( Exception& ex )
		{
			cerr << myname << "(blink): " << ex << endl;
		}
	}
	
	blink_state ^= true;
}
// -----------------------------------------------------------------------------
void IOControl::addBlink( IOMap::iterator& io )
{
	for( BlinkList::iterator it=lstBlink.begin(); it!=lstBlink.end(); ++it )
	{
		if( (*it) == io )
			return;
	}
	
	lstBlink.push_back(io);
}
// -----------------------------------------------------------------------------
void IOControl::delBlink( IOMap::iterator& io )
{
	for( BlinkList::iterator it=lstBlink.begin(); it!=lstBlink.end(); ++it )
	{
		if( (*it) == io )
		{
			lstBlink.erase(it);
			return;
		}
	}
}
// -----------------------------------------------------------------------------
void IOControl::check_testlamp()
{
	if( testLamp_S == DefaultObjectId )
		return;

	try
	{
		if( force_out )
			isTestLamp = shm->localGetState( ditTestLamp, testLamp_S );
				
		if( !trTestLamp.change(isTestLamp) )
			return; // ���� ��������� �� ��������, �� ���������� ������...
		
		if( isTestLamp )
			blink_state = true; // ������ ���� ������ ��������...

		cout << myname << "(check_test_lamp): ************* test lamp " 
			<< isTestLamp << " *************" << endl;

		// �������� �� ������ � ��������� ������ �������� �������...
		for( IOMap::iterator it=iomap.begin(); it!=iomap.end(); ++it )
		{
			if( !it->lamp || it->no_testlamp )
				continue;
		
			if(  it->stype == UniversalIO::AnalogOutput )
			{
				if( isTestLamp )
					addBlink(it);
				else if( it->value != lmpBLINK )
					delBlink(it);
			}
			else if( it->stype == UniversalIO::DigitalOutput )
			{
				if( isTestLamp )
					addBlink(it);
				else
					delBlink(it);
			}
		}
	}
	catch( Exception& ex)
	{
		unideb[Debug::CRIT] << myname << "(check_testlamp): " << ex << endl;
	}
	catch(...)
	{
		
	}
}

// -----------------------------------------------------------------------------
IOControl* IOControl::init_iocontrol( int argc, char* argv[], 
										UniSetTypes::ObjectId icID, SharedMemory* ic )
{
	string name = conf->getArgParam("--io-name","IOControl1");
	if( name.empty() )
	{
		cerr << "(iocontrol): �� ����� name'" << endl;
		return 0;
	}

	ObjectId ID = conf->getObjectID(name);	
	if( ID == UniSetTypes::DefaultObjectId )
	{
		cerr << "(iocontrol): ������������� '" << name 
			<< "' �� ������ � ����. �����!"
			<< " � ������ " << conf->getObjectsSection() << endl;
		return 0;
	}

	int numcards = atoi(conf->getArgParam("--io-numcards","1").c_str());
	if( numcards <=0 )
		numcards = 1;


	unideb[Debug::INFO] << "(iocontrol): name = " << name << "(" << ID << ")" << endl;
	return new IOControl(ID,icID,ic,numcards);
}
// -----------------------------------------------------------------------------
void IOControl::help_print( int argc, char* argv[] )
{
	cout << "--io-confnode name - ������������ ��� ��������� ��������� xml-����" << endl;
	cout << "--io-name name		- ID ��������. �� ��������� IOController1." << endl;
	cout << "--io-numcards		- ���������� ���� �/�. �� ��������� 1." << endl;
	cout << "--iodev0 dev		- ������������ ��� card='0' ��������� ���� comedi-����������." << endl;
	cout << "--iodev1 dev		- ������������ ��� card='1' ��������� ���� comedi-����������." << endl;
	cout << "--iodev2 dev		- ������������ ��� card='2' ��������� ���� comedi-����������." << endl;
	cout << "--iodev3 dev		- ������������ ��� card='3' ��������� ���� comedi-����������." << endl;
	cout << "--iodevX dev		- ������������ ��� card='X' ��������� ���� comedi-����������." << endl;
	cout << "                     'X'  �� ������ ���� ������ --io-numcards" << endl;

	cout << "--iodevX-subdevX-type name	- ��������� ���� ������������� ��� UNIO." << endl ;
	cout << "                             ���������: TBI0_24,TBI24_0,TBI16_8" << endl;

	cout << "--io-default_cardnum		- ����� ����� �� ���������. �� ��������� -1." << endl;
	cout << "                             ���� ������, �� �� ����� ������������� ��� ��������" << endl;
	cout << "                             � ������� �� ������ ���� 'card'." << endl;

	cout << "--io-test-lamp		- ��� ������� ���� � �������� ������� ������ '��������' ������������ ��������� ������." << endl;
	cout << "--io-conf-field fname	- ��������� �� ����. ����� ��� ������� � ����� fname='1'" << endl;
	cout << "--io-polltime msec	- ����� ����� ������� ����. �� ��������� 200 ����." << endl;
	cout << "--io-filtersize val	- ����������� ������� ��� ���������� ������." << endl;
	cout << "--io-filterT val		- ���������� ������� �������." << endl;
	cout << "--io-s-filter-field	- ������������� � configure.xml �� �������� ����������� ������ ����������� � ��� �������� ��������" << endl;
	cout << "--io-s-filter-value	- �������� �������������� �� �������� ����������� ������ ����������� � ��� �������� ��������" << endl;
	cout << "--io-blink-time msec	- ������� �������, ����. �� ��������� � configure.xml" << endl;
	cout << "--io-heartbeat-id		- ������ ������� ������ � ��������� ���������� heartbeat-�������." << endl;
	cout << "--io-heartbeat-max  	- ������������ �������� heartbeat-�ޣ����� ��� ������� ��������. �� ��������� 10." << endl;
	cout << "--io-ready-timeout		- ����� �������� ���������� SM � ������, ����. (-1 - ����� '�����')" << endl;    
	cout << "--io-force				- ��������� �������� � SM, ���������� ��, ���� �������� �� ��������" << endl;
	cout << "--io-force-out			- ��������� ������ ������������� (�� �� ������)" << endl;
	cout << "--io-skip-init-output	- �� ���������������� '������' ��� ������" << endl;
	cout << "--io-sm-ready-test-sid - ������������ ��������� ������, ��� �������� ���������� SharedMemory" << endl;
}
// -----------------------------------------------------------------------------
void IOControl::processingMessage( UniSetTypes::VoidMessage* msg )
{
	try
	{
		switch( msg->type )
		{
			case Message::SensorInfo:
			{
				SensorMessage sm( msg );
				sensorInfo( &sm );
				break;
			}

			case Message::Timer:
			{
				TimerMessage tm(msg);
				timerInfo(&tm);
				break;
			}

			case Message::SysCommand:
			{
				SystemMessage sm( msg );
				sysCommand( &sm );
				break;
			}

			default:
				break;
		}	
	}
	catch(Exception& ex)
	{
		cout  << myname << "(processingMessage): " << ex << endl;
	}
}
// -----------------------------------------------------------------------------
void IOControl::sysCommand( SystemMessage* sm )
{
	switch( sm->command )
	{
		case SystemMessage::StartUp:
		{
			PassiveTimer ptAct(activateTimeout);
			while( !activated && !ptAct.checkTime() )
			{	
				cout << myname << "(sysCommand): wait activate..." << endl;
				msleep(300);
				if( activated )
					break;
			}
			
			if( !activated )
				unideb[Debug::CRIT] << myname << "(sysCommand): ************* don`t activate?! ************" << endl;
		
			askSensors(UniversalIO::UIONotify);
			break;
		}				
		
		case SystemMessage::FoldUp:								
		case SystemMessage::Finish:
			askSensors(UniversalIO::UIODontNotify);
			break;
		
		case SystemMessage::WatchDog:
		{
			// ����������� (������ �� �������� ���������� ��� ������)
			// ���� �ģ� ��������� ������ 
			// (�.�. IOControl  ������� � ����� �������� � SharedMemory2)
			// �� ������������ WatchDog �� ����, �.�. �� � ��� �ģ� ���������� SM
			// ��� ������ ��������, � ���� SM �������, �� ������ � ���� ���������(IOControl)
			if( shm->isLocalwork() )
				break;

			askSensors(UniversalIO::UIONotify);
			// ������������� ��������� ���������...
			try
			{
				if( !force )
				{
					force = true;
					uniset_mutex_lock l(iopollMutex,8000);
					iopoll();
					force = false;
				}
			}
			catch(...){}
		}	
		break;

		case SystemMessage::LogRotate:
		{
			// ������������� ����
			unideb << myname << "(sysCommand): logRotate" << endl;
			string fname = unideb.getLogFile();
			if( !fname.empty() )
			{
				unideb.logFile(fname.c_str());
				unideb << myname << "(sysCommand): ***************** UNIDEB LOG ROTATE *****************" << endl;
			}

			unideb << myname << "(sysCommand): logRotate" << endl;
			fname = unideb.getLogFile();
			if( !fname.empty() )
			{
				unideb.logFile(fname.c_str());
				unideb << myname << "(sysCommand): ***************** GGDEB LOG ROTATE *****************" << endl;
			}
		}
		break;

		default:
			break;
	}
}
// -----------------------------------------------------------------------------
void IOControl::askSensors( UniversalIO::UIOCommand cmd )
{
	if( force_out )
		return;

	waitSM();
	if( sidTestSMReady!=DefaultObjectId && 
		!shm->waitSMworking(sidTestSMReady ,activateTimeout,50) )
	{
		ostringstream err;
		err << myname 
			<< "(askSensors): �� ��������� ����������(work) SharedMemory � ������ � ������� " 
			<< activateTimeout << " ����";

		unideb[Debug::CRIT] << err.str() << endl;
		kill(SIGTERM,getpid());	// ��������� (�������������) �������...
		throw SystemError(err.str());
	}

	PassiveTimer ptAct(activateTimeout);
	while( !readconf_ok && !ptAct.checkTime() )
	{	
		cout << myname << "(askSensors): wait read configuration..." << endl;
		msleep(50);
		if( readconf_ok )
			break;
	}
			
	if( !readconf_ok )
		unideb[Debug::CRIT] << myname << "(askSensors): ************* don`t read configuration?! ************" << endl;

	try
	{
		if( testLamp_S != DefaultObjectId )
			shm->askSensor(testLamp_S,cmd);
	}
	catch( Exception& ex)
	{
		unideb[Debug::CRIT] << myname << "(askSensors): " << ex << endl;
	}

	ComediInterface* card = 0;
	for( IOMap::iterator it=iomap.begin(); it!=iomap.end(); ++it )
	{
		if( it->ignore )
			continue;

		card = 0;
		if( it->ncard>=0 && it->ncard<cards.size() )
			card = cards[it->ncard];
		
		if( !card || it->subdev==DefaultSubdev || it->channel==DefaultChannel )
			continue;

		if( it->stype == UniversalIO::AnalogOutput ||
			it->stype == UniversalIO::DigitalOutput )
		{
			try
			{
				shm->askSensor(it->si.id,cmd,myid);
			}
			catch( Exception& ex)
			{
				unideb[Debug::CRIT] << myname << "(askSensors): " << ex << endl;
			}
		}
	}
}
// -----------------------------------------------------------------------------
void IOControl::sensorInfo( UniSetTypes::SensorMessage* sm )
{
	if( unideb.debugging(Debug::LEVEL1) )
	{
		unideb[Debug::LEVEL1] << myname << "(sensorInfo): sm->id=" << sm->id 
						<< " val=" << sm->value << endl;
	}
	
	if( force_out )
		return;

	if( sm->id == testLamp_S )
	{
		unideb[Debug::INFO] << myname << "(sensorInfo): test_lamp=" << sm->state << endl;
		isTestLamp = sm->state;
	}

	for( IOMap::iterator it=iomap.begin(); it!=iomap.end(); ++it )
	{
		if( it->si.id == sm->id )
		{
			if( unideb.debugging(Debug::INFO) )
			{
				unideb[Debug::INFO] << myname << "(sensorInfo): sid=" << sm->id
					<< " state=" << sm->state
					<< " value=" << sm->value
					<< endl;
			}
		
			if( it->stype == UniversalIO::AnalogOutput )
			{
				long prev_val = 0;
				long cur_val = 0;
				{
					uniset_spin_lock lock(it->val_lock);
					prev_val = it->value;
					it->value = sm->value;
					cur_val = sm->value;
				}

				if( it->lamp )
				{
					switch( cur_val )
					{
						case lmpOFF:
							delBlink(it);
						break;
	
						case lmpON:
							delBlink(it);
						break;

						case lmpBLINK:
						{
							if( prev_val != lmpBLINK )
							{
								addBlink(it);
								// � ����� ��������, ����� �� ���� �����
								// (��� ���������� �������� ��� ���������)
								if( it->ignore || it->subdev==DefaultSubdev || it->channel==DefaultChannel )
									break;
								
								ComediInterface* card = 0;
								if( it->ncard>=0 && it->ncard<cards.size() )
									card = cards[it->ncard];
								
								if( card )
									card->setDigitalChannel(it->subdev,it->channel,1);
							}
						}
						break;
						
						default:
							break;
					}
				}
			}
			else if( it->stype == UniversalIO::DigitalOutput )
			{
				if( unideb.debugging(Debug::LEVEL1) )
				{
					unideb[Debug::LEVEL1] << myname << "(sensorInfo): DO: sm->id=" << sm->id 
							<< " val=" << sm->value << endl;
				}
				uniset_spin_lock lock(it->val_lock);
				it->value = sm->state ? 1:0;
			}
			break;
		}
		
	}
}
// -----------------------------------------------------------------------------
void IOControl::timerInfo( UniSetTypes::TimerMessage* tm )
{

}
// -----------------------------------------------------------------------------
void IOControl::waitSM()
{
	if( !shm->waitSMready(smReadyTimeout,50) )
	{
		ostringstream err;
		err << myname << "(execute): �� ��������� ���������� SharedMemory � ������ � ������� "
					<< smReadyTimeout << " ����";

		unideb[Debug::CRIT] << err.str() << endl;
		throw SystemError(err.str());
	}
}
// -----------------------------------------------------------------------------