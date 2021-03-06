#include <sstream>
#include "Exceptions.h"
#include "Extensions.h"
#include "UNetExchange.h"
// -----------------------------------------------------------------------------
using namespace std;
using namespace UniSetTypes;
using namespace UniSetExtensions;
// -----------------------------------------------------------------------------
UNetExchange::UNetExchange( UniSetTypes::ObjectId objId, UniSetTypes::ObjectId shmId, SharedMemory* ic, const std::string& prefix ):
UniSetObject_LT(objId),
shm(0),
initPause(0),
activated(false),
no_sender(false),
sender(0),
sender2(0)
{
	if( objId == DefaultObjectId )
		throw UniSetTypes::SystemError("(UNetExchange): objId=-1?!! Use --" + prefix +"-unet-name" );

	cnode = conf->getNode(myname);
	if( cnode == NULL )
		throw UniSetTypes::SystemError("(UNetExchange): Not found conf-node for " + myname );

	shm = new SMInterface(shmId,&ui,objId,ic);

	UniXML_iterator it(cnode);

	// определяем фильтр
	s_field = conf->getArgParam("--" + prefix + "-filter-field");
	s_fvalue = conf->getArgParam("--" + prefix + "-filter-value");
	dlog[Debug::INFO] << myname << "(init): read filter-field='" << s_field
						<< "' filter-value='" << s_fvalue << "'" << endl;
	
	const string n_field(conf->getArgParam("--" + prefix + "-nodes-filter-field"));
	const string n_fvalue(conf->getArgParam("--" + prefix + "-nodes-filter-value"));
	dlog[Debug::INFO] << myname << "(init): read nodes-filter-field='" << n_field
						<< "' nodes-filter-value='" << n_fvalue << "'" << endl;

	int recvTimeout = conf->getArgPInt("--" + prefix + "-recv-timeout",it.getProp("recvTimeout"), 5000);
	int prepareTime = conf->getArgPInt("--" + prefix + "-preapre-time",it.getProp("prepareTime"), 2000);
	int lostTimeout = conf->getArgPInt("--" + prefix + "-lost-timeout",it.getProp("lostTimeout"), recvTimeout);
	int recvpause = conf->getArgPInt("--" + prefix + "-recvpause",it.getProp("recvpause"), 10);
	int sendpause = conf->getArgPInt("--" + prefix + "-sendpause",it.getProp("sendpause"), 100);
	int updatepause = conf->getArgPInt("--" + prefix + "-updatepause",it.getProp("updatepause"), 100);
	steptime = conf->getArgPInt("--" + prefix + "-steptime",it.getProp("steptime"), 1000);
	int maxDiff = conf->getArgPInt("--" + prefix + "-maxdifferense",it.getProp("maxDifferense"), 1000);
	int maxProcessingCount = conf->getArgPInt("--" + prefix + "-maxprocessingcount",it.getProp("maxProcessingCount"), 100);

	no_sender = conf->getArgInt("--" + prefix + "-nosender",it.getProp("nosender"));

	xmlNode* nodes = conf->getXMLNodesSection();
	if( !nodes )
	  throw UniSetTypes::SystemError("(UNetExchange): Not found <nodes>");

	UniXML_iterator n_it(nodes);
	
	string default_ip(n_it.getProp(prefix+"_broadcast_ip"));
	string default_ip2(n_it.getProp(prefix+"_broadcast_ip2"));

	if( !n_it.goChildren() )
		throw UniSetTypes::SystemError("(UNetExchange): Items not found for <nodes>");

	for( ; n_it.getCurrent(); n_it.goNext() )
	{
		if( n_it.getIntProp(prefix+"_ignore") )
		{
			dlog[Debug::INFO] << myname << "(init): unet_ignore.. for " << n_it.getProp("name") << endl;
			continue;
		}
		
		// проверяем фильтры для подсетей
		if( !UniSetTypes::check_filter(n_it,n_field,n_fvalue) )
			continue;

		// Если указано поле unet_broadcast_ip непосредственно у узла - берём его
		// если указано общий broadcast ip для всех узлов - берём его
		string h("");
		string h2("");
		if( !default_ip.empty() )
			h = default_ip;
		if( !n_it.getProp(prefix+"_broadcast_ip").empty() )
			h = n_it.getProp(prefix+"_broadcast_ip");

		if( !default_ip2.empty() )
			h2 = default_ip2;
		if( !n_it.getProp(prefix+"_broadcast_ip2").empty() )
			h2 = n_it.getProp(prefix+"_broadcast_ip2");
		
		if( h.empty() )
		{
			ostringstream err;
			err << myname << "(init): Unknown broadcast IP for " << n_it.getProp("name");
			dlog[Debug::CRIT] << err.str() << endl;
			throw UniSetTypes::SystemError(err.str());
		}

		if( h2.empty() && dlog.debugging(Debug::INFO) )
			dlog[Debug::INFO] << myname << "(init): ip2 not used..." << endl;

		// Если указано поле unet_port - используем его
		// Иначе port = идентификатору узла
		int p = n_it.getIntProp("id");
		if( !n_it.getProp(prefix+"_port").empty() )
			p = n_it.getIntProp(prefix+"_port");

		int p2 = p; // по умолчанию порт на втором канале такой же как на первом
		if( !n_it.getProp(prefix+"_port2").empty() )
			p2 = n_it.getIntProp(prefix+"_port2");

		string n(n_it.getProp("name"));
		if( n == conf->getLocalNodeName() )
		{
			if( no_sender )
			{
				dlog[Debug::INFO] << myname << "(init): sender OFF for this node...(" 
						<< n_it.getProp("name") << ")" << endl;
				continue;
			}
			
			dlog[Debug::INFO] << myname << "(init): init sender.. my node " << n_it.getProp("name") << endl;
			sender = create_sender(h,p,shm,s_field,s_fvalue,ic);
			sender->setSendPause(sendpause);

			try
			{
				// создаём "писателя" для второго канала если задан
				if( !h2.empty() )
				{
					dlog[Debug::INFO] << myname << "(init): init sender2.. my node " << n_it.getProp("name") << endl;
					sender2 = create_sender(h2,p2,shm,s_field,s_fvalue,ic);
					sender2->setSendPause(sendpause);
				}
			}
			catch(...)
			{
				// т.е. это "резервный канал", то игнорируем ошибку его создания
				// при запуске "интерфейс" может быть и не доступен...
				sender2 = 0;
				dlog[Debug::CRIT] << myname << "(ignore): DON`T CREATE 'UNetSender' for " << h2 << ":" << p2 << endl;
			}

			continue;
		}

		dlog[Debug::INFO] << myname << "(init): add UNetReceiver for " << h << ":" << p << endl;

		if( checkExistUNetHost(h,p) )
		{
			dlog[Debug::INFO] << myname << "(init): " << h << ":" << p << " already added! Ignore.." << endl;
			continue;
		}

		bool resp_invert = n_it.getIntProp(prefix+"_respond_invert");
		
		string s_resp_id(n_it.getProp(prefix+"_respond1_id"));
		UniSetTypes::ObjectId resp_id = UniSetTypes::DefaultObjectId;
		if( !s_resp_id.empty() )
		{
			resp_id = conf->getSensorID(s_resp_id);
			if( resp_id == UniSetTypes::DefaultObjectId )
			{
				ostringstream err;
				err << myname << ": Unknown RespondID.. Not found id for '" << s_resp_id << "'" << endl;
				dlog[Debug::CRIT] << myname << "(init): " << err.str() << endl;
				throw SystemError(err.str());
			}
		}

		string s_resp2_id(n_it.getProp(prefix+"_respond2_id"));
		UniSetTypes::ObjectId resp2_id = UniSetTypes::DefaultObjectId;
		if( !s_resp2_id.empty() )
		{
			resp2_id = conf->getSensorID(s_resp2_id);
			if( resp2_id == UniSetTypes::DefaultObjectId )
			{
				ostringstream err;
				err << myname << ": Unknown RespondID(2).. Not found id for '" << s_resp2_id << "'" << endl;
				dlog[Debug::CRIT] << myname << "(init): " << err.str() << endl;
				throw SystemError(err.str());
			}
		}		
		
		string s_lp_id(n_it.getProp(prefix+"_lostpackets1_id"));
		UniSetTypes::ObjectId lp_id = UniSetTypes::DefaultObjectId;
		if( !s_lp_id.empty() )
		{
			lp_id = conf->getSensorID(s_lp_id);
			if( lp_id == UniSetTypes::DefaultObjectId )
			{
				ostringstream err;
				err << myname << ": Unknown LostPacketsID.. Not found id for '" << s_lp_id << "'" << endl;
				dlog[Debug::CRIT] << myname << "(init): " << err.str() << endl;
				throw SystemError(err.str());
			}
		}
		
		string s_lp2_id(n_it.getProp(prefix+"_lostpackets2_id"));
		UniSetTypes::ObjectId lp2_id = UniSetTypes::DefaultObjectId;
		if( !s_lp2_id.empty() )
		{
			lp2_id = conf->getSensorID(s_lp2_id);
			if( lp2_id == UniSetTypes::DefaultObjectId )
			{
				ostringstream err;
				err << myname << ": Unknown LostPacketsID(2).. Not found id for '" << s_lp2_id << "'" << endl;
				dlog[Debug::CRIT] << myname << "(init): " << err.str() << endl;
				throw SystemError(err.str());
			}
		}

		string s_lp_comm_id(n_it.getProp(prefix+"_lostpackets_id"));
		UniSetTypes::ObjectId lp_comm_id = UniSetTypes::DefaultObjectId;
		if( !s_lp_comm_id.empty() )
		{
			lp_comm_id = conf->getSensorID(s_lp_comm_id);
			if( lp_comm_id == UniSetTypes::DefaultObjectId )
			{
				ostringstream err;
				err << myname << ": Unknown LostPacketsID(comm).. Not found id for '" << s_lp_comm_id << "'" << endl;
				dlog[Debug::CRIT] << myname << "(init): " << err.str() << endl;
				throw SystemError(err.str());
			}
		}

		string s_resp_comm_id(n_it.getProp(prefix+"_respond_id"));
		UniSetTypes::ObjectId resp_comm_id = UniSetTypes::DefaultObjectId;
		if( !s_resp_comm_id.empty() )
		{
			resp_comm_id = conf->getSensorID(s_resp_comm_id);
			if( resp_comm_id == UniSetTypes::DefaultObjectId )
			{
				ostringstream err;
				err << myname << ": Unknown RespondID(comm).. Not found id for '" << s_resp_comm_id << "'" << endl;
				dlog[Debug::CRIT] << myname << "(init): " << err.str() << endl;
				throw SystemError(err.str());
			}
		}	


		dlog[Debug::INFO] << myname << "(init): (node='" << n << "') add receiver " 
						<< h2 << ":" << p2 << endl;
		UNetReceiver* r = create_receiver(h,p,shm);

		// на всякий принудительно разблокируем, 
		// чтобы не зависеть от значения по умолчанию
		r->setLockUpdate(false);

		r->setReceiveTimeout(recvTimeout);
		r->setPrepareTime(prepareTime);
		r->setLostTimeout(lostTimeout);
		r->setReceivePause(recvpause);
		r->setUpdatePause(updatepause);
		r->setMaxDifferens(maxDiff);
		r->setMaxProcessingCount(maxProcessingCount);
		r->setRespondID(resp_id,resp_invert);
		r->setLostPacketsID(lp_id);
		r->connectEvent( sigc::mem_fun(this, &UNetExchange::receiverEvent) );

		UNetReceiver* r2 = 0;
		try
		{
			if( !h2.empty() ) // создаём читателя впо второму каналу
			{
				dlog[Debug::INFO] << myname << "(init): (node='" << n << "') add reserv receiver " 
						<< h2 << ":" << p2 << endl;
				
				r2 = create_receiver(h2,p2,shm);

				// т.к. это резервный канал (по началу блокируем его)
				r2->setLockUpdate(true);

				r2->setReceiveTimeout(recvTimeout);
				r2->setLostTimeout(lostTimeout);
				r2->setReceivePause(recvpause);
				r2->setUpdatePause(updatepause);
				r2->setMaxDifferens(maxDiff);
				r2->setMaxProcessingCount(maxProcessingCount);
				r2->setRespondID(resp2_id,resp_invert);
				r2->setLostPacketsID(lp2_id);
				r2->connectEvent( sigc::mem_fun(this, &UNetExchange::receiverEvent) );
			}
		}
		catch(...)
		{
			// т.е. это "резервный канал", то игнорируем ошибку его создания
			// при запуске "интерфейс" может быть и не доступен...
			r2 = 0;
			dlog[Debug::CRIT] << myname << "(ignore): DON`T CREATE 'UNetReceiver' for " << h2 << ":" << p2 << endl;
		}

		ReceiverInfo ri(r,r2);
		ri.setRespondID(resp_comm_id,resp_invert);
		ri.setLostPacketsID(lp_comm_id);
		recvlist.push_back(ri);
	}
	
	// -------------------------------
	// ********** HEARTBEAT *************
	string heart = conf->getArgParam("--" + prefix + "-heartbeat-id",it.getProp("heartbeat_id"));
	if( !heart.empty() )
	{
		sidHeartBeat = conf->getSensorID(heart);
		if( sidHeartBeat == DefaultObjectId )
		{
			ostringstream err;
			err << myname << ": не найден идентификатор для датчика 'HeartBeat' " << heart;
			dlog[Debug::CRIT] << myname << "(init): " << err.str() << endl;
			throw SystemError(err.str());
		}

		int heartbeatTime = conf->getArgPInt("--" + prefix + "-heartbeat-time",it.getProp("heartbeatTime"),conf->getHeartBeatTime());
		if( heartbeatTime )
			ptHeartBeat.setTiming(heartbeatTime);
		else
			ptHeartBeat.setTiming(UniSetTimer::WaitUpTime);

		maxHeartBeat = conf->getArgPInt("--" + prefix + "-heartbeat-max", it.getProp("heartbeat_max"), 10);
		test_id = sidHeartBeat;
	}
	else
	{
		test_id = conf->getSensorID("TestMode_S");
		if( test_id == DefaultObjectId )
		{
			ostringstream err;
			err << myname << "(init): test_id unknown. 'TestMode_S' not found...";
			dlog[Debug::CRIT] << myname << "(init): " << err.str() << endl;
			throw SystemError(err.str());
		}
	}

	dlog[Debug::INFO] << myname << "(init): test_id=" << test_id << endl;

	activateTimeout	= conf->getArgPInt("--" + prefix + "-activate-timeout", 20000);
}
// -----------------------------------------------------------------------------
UNetExchange::~UNetExchange()
{
	for( ReceiverList::iterator it=recvlist.begin(); it!=recvlist.end(); ++it )
	{
		if( it->r1 )
			delete it->r1;
		if( it->r2 )
			delete it->r2;
	}

	delete sender;
	delete sender2;
	delete shm;
}
// -----------------------------------------------------------------------------
UNetReceiver* UNetExchange::create_receiver( const std::string& h, const ost::tpport_t p, SMInterface* shm )
{
	return new UNetReceiver(h,p,shm);
}
// -----------------------------------------------------------------------------
UNetSender* UNetExchange::create_sender( const std::string& h, const ost::tpport_t p, SMInterface* shm,
					const std::string& s_field, const std::string& s_fvalue, SharedMemory* ic )
{
	return new UNetSender(h,p,shm,s_field,s_fvalue,ic);
}
// -----------------------------------------------------------------------------
bool UNetExchange::checkExistUNetHost( const std::string& addr, ost::tpport_t port )
{
	ost::IPV4Address a1(addr.c_str());
	for( ReceiverList::iterator it=recvlist.begin(); it!=recvlist.end(); ++it )
	{
		if( it->r1->getAddress() == a1.getAddress() && it->r1->getPort() == port )
			return true;
	}

	return false;
}
// -----------------------------------------------------------------------------
void UNetExchange::startReceivers()
{
	for( ReceiverList::iterator it=recvlist.begin(); it!=recvlist.end(); ++it )
	{
		if( it->r1 )
			it->r1->start();
		if( it->r2 )
			it->r2->start();
	}
}
// -----------------------------------------------------------------------------
void UNetExchange::waitSMReady()
{
	// waiting for SM is ready...
	int ready_timeout = conf->getArgInt("--unet-sm-ready-timeout","15000");
	if( ready_timeout == 0 )
		ready_timeout = 15000;
	else if( ready_timeout < 0 )
		ready_timeout = UniSetTimer::WaitUpTime;

	if( !shm->waitSMready(ready_timeout,50) )
	{
		ostringstream err;
		err << myname << "(waitSMReady): Не дождались готовности SharedMemory к работе в течение " << ready_timeout << " мсек";
		dlog[Debug::CRIT] << err.str() << endl;
		throw SystemError(err.str());
	}
}
// -----------------------------------------------------------------------------
void UNetExchange::timerInfo( TimerMessage *tm )
{
	if( !activated )
		return;

	if( tm->id == tmStep )
		step();
}
// -----------------------------------------------------------------------------
void UNetExchange::step()
{
	if( !activated )
		return;

	if( sidHeartBeat!=DefaultObjectId && ptHeartBeat.checkTime() )
	{
		try
		{
			shm->localSaveValue(aitHeartBeat,sidHeartBeat,maxHeartBeat,getId());
			ptHeartBeat.reset();
		}
		catch(Exception& ex)
		{
			dlog[Debug::CRIT] << myname << "(step): (hb) " << ex << std::endl;
		}
	}

	for( ReceiverList::iterator it=recvlist.begin(); it!=recvlist.end(); ++it )
		it->step(shm, myname);
}

// -----------------------------------------------------------------------------
void UNetExchange::ReceiverInfo::step( SMInterface* shm, const std::string& myname )
{
	try
	{
		if( sidRespond != DefaultObjectId )
		{
			bool resp = ( (r1 && r1->isRecvOK()) || (r2 && r2->isRecvOK()) );
			if( respondInvert )
				resp = !resp;
			
			shm->localSaveState(ditRespond,sidRespond,resp,shm->ID());
		}
	}
	catch( Exception& ex )
	{
		dlog[Debug::CRIT] << myname << "(ReceiverInfo::step): (respond): " << ex << std::endl;
	}

	try
	{
		if( sidLostPackets != DefaultObjectId )
		{
			long l = 0;
			if( r1 )
				l += r1->getLostPacketsNum();
			if( r2 )
				l += r2->getLostPacketsNum();

			shm->localSaveValue(aitLostPackets,sidLostPackets,l,shm->ID());
		}
	}
	catch( Exception& ex )
	{
		dlog[Debug::CRIT] << myname << "(ReceiverInfo::step): (lostpackets): " << ex << std::endl;
	}	
}
// -----------------------------------------------------------------------------
void UNetExchange::processingMessage( UniSetTypes::VoidMessage *msg )
{
	try
	{
		switch(msg->type)
		{
			case UniSetTypes::Message::SysCommand:
			{
				UniSetTypes::SystemMessage sm( msg );
				sysCommand( &sm );
			}
			break;

			case Message::SensorInfo:
			{
				SensorMessage sm( msg );
				sensorInfo(&sm);
			}
			break;

			case Message::Timer:
			{
				TimerMessage tm(msg);
				timerInfo(&tm);
			}
			break;

			default:
				break;
		}
	}
	catch( SystemError& ex )
	{
		dlog[Debug::CRIT] << myname << "(SystemError): " << ex << std::endl;
//		throw SystemError(ex);
		raise(SIGTERM);
	}
	catch( Exception& ex )
	{
		dlog[Debug::CRIT] << myname << "(processingMessage): " << ex << std::endl;
	}
	catch(...)
	{
		dlog[Debug::CRIT] << myname << "(processingMessage): catch ..." << std::endl;
	}
}
// -----------------------------------------------------------------------------
void UNetExchange::sysCommand( UniSetTypes::SystemMessage *sm )
{
	switch( sm->command )
	{
		case SystemMessage::StartUp:
		{
			waitSMReady();

			// подождать пока пройдёт инициализация датчиков
			// см. activateObject()
			msleep(initPause);
			PassiveTimer ptAct(activateTimeout);
			while( !activated && !ptAct.checkTime() )
			{	
				cout << myname << "(sysCommand): wait activate..." << endl;
				msleep(300);
				if( activated )
					break;
			}
			
			if( !activated )
				dlog[Debug::CRIT] << myname << "(sysCommand): ************* don`t activate?! ************" << endl;
			
			{
				UniSetTypes::uniset_mutex_lock l(mutex_start, 10000);
				if( shm->isLocalwork() )
					askSensors(UniversalIO::UIONotify);
			}
			
			askTimer(tmStep,steptime);
			startReceivers();
			if( sender )
				sender->start();
			if( sender2 )
				sender2->start();
		}
		break;

		case SystemMessage::FoldUp:
		case SystemMessage::Finish:
			if( shm->isLocalwork() )
				askSensors(UniversalIO::UIODontNotify);
			break;
		
		case SystemMessage::WatchDog:
		{
			// ОПТИМИЗАЦИЯ (защита от двойного перезаказа при старте)
			// Если идёт автономная работа, то нужно заказывать датчики
			// если запущены в одном процессе с SharedMemory2,
			// то обрабатывать WatchDog не надо, т.к. мы и так ждём готовности SM
			// при заказе датчиков, а если SM вылетит, то вместе с этим процессом(UNetExchange)
			if( shm->isLocalwork() )
				askSensors(UniversalIO::UIONotify);
		}
		break;

		case SystemMessage::LogRotate:
		{
			// переоткрываем логи
			unideb << myname << "(sysCommand): logRotate" << std::endl;
			string fname = unideb.getLogFile();
			if( !fname.empty() )
			{
				unideb.logFile(fname);
				unideb << myname << "(sysCommand): ***************** UNIDEB LOG ROTATE *****************" << std::endl;
			}

			dlog << myname << "(sysCommand): logRotate" << std::endl;
			fname = dlog.getLogFile();
			if( !fname.empty() )
			{
				dlog.logFile(fname);
				dlog << myname << "(sysCommand): ***************** dlog LOG ROTATE *****************" << std::endl;
			}
		}
		break;

		default:
			break;
	}
}
// ------------------------------------------------------------------------------------------
void UNetExchange::askSensors( UniversalIO::UIOCommand cmd )
{
	if( !shm->waitSMworking(test_id,activateTimeout,50) )
	{
		ostringstream err;
		err << myname 
			<< "(askSensors): Не дождались готовности(work) SharedMemory к работе в течение " 
			<< activateTimeout << " мсек";
	
		dlog[Debug::CRIT] << err.str() << endl;
		kill(SIGTERM,getpid());	// прерываем (перезапускаем) процесс...
		throw SystemError(err.str());
	}

	if( sender )
		sender->askSensors(cmd);
	if( sender2 )
		sender2->askSensors(cmd);
}
// ------------------------------------------------------------------------------------------
void UNetExchange::sensorInfo( UniSetTypes::SensorMessage* sm )
{
	if( sender )
		sender->updateSensor( sm->id , sm->value );
	if( sender2 )
		sender2->updateSensor( sm->id , sm->value );
}
// ------------------------------------------------------------------------------------------
bool UNetExchange::activateObject()
{
	// блокирование обработки Starsp 
	// пока не пройдёт инициализация датчиков
	// см. sysCommand()
	{
		activated = false;
		UniSetTypes::uniset_mutex_lock l(mutex_start, 5000);
		UniSetObject_LT::activateObject();
		initIterators();
		activated = true;
	}

	return true;
}
// ------------------------------------------------------------------------------------------
void UNetExchange::sigterm( int signo )
{
	dlog[Debug::INFO] << myname << ": ********* SIGTERM(" << signo <<") ********" << endl;
	activated = false;
	for( ReceiverList::iterator it=recvlist.begin(); it!=recvlist.end(); ++it )
	{
		try
		{
			if( it->r1 )
				it->r1->stop();
		}
		catch(...){}
		try
		{
			if( it->r2 )
				it->r2->stop();
		}
		catch(...){}
	}

	try
	{
		if( sender )
			sender->stop();
	}
	catch(...){}
	try
	{
		if( sender2 )
			sender2->stop();
	}
	catch(...){}

	UniSetObject_LT::sigterm(signo);
}
// ------------------------------------------------------------------------------------------
void UNetExchange::initIterators()
{
	shm->initAIterator(aitHeartBeat);
	if( sender )
		sender->initIterators();
	if( sender2 )
		sender2->initIterators();

	for( ReceiverList::iterator it=recvlist.begin(); it!=recvlist.end(); it++ )
		it->initIterators(shm);
}
// -----------------------------------------------------------------------------
void UNetExchange::help_print( int argc, const char* argv[] )
{
	cout << "Default prefix='unet'" << endl;
	cout << "--prefix-name NameID            - Идентификтора процесса." << endl;
	cout << "--prefix-recv-timeout msec      - Время для фиксации события 'отсутсвие связи'" << endl;
	cout << "--prefix-prepare-time msec      - Время необходимое на подготовку (восстановление связи) при переключении на другой канал" << endl;
	cout << "--prefix-lost-timeout msec      - Время ожидания заполнения 'дырки' между пакетами. По умолчанию 5000 мсек." << endl;
	cout << "--prefix-recvpause msec         - Пауза между приёмами. По умолчанию 10" << endl;
	cout << "--prefix-sendpause msec         - Пауза между посылками. По умолчанию 100" << endl;
	cout << "--prefix-updatepause msec       - Пауза между обновлением информации в SM (Корелирует с recvpause и sendpause). По умолчанию 100" << endl;
	cout << "--prefix-steptime msec		   - Пауза между обновлением информации о связи с узлами." << endl;
	cout << "--prefix-maxdifferense num      - Маскимальная разница в номерах пакетов для фиксации события 'потеря пакетов' " << endl;
	cout << "--prefix-maxprocessingcount num - время на ожидание старта SM" << endl;
	cout << "--prefix-nosender [0,1]         - Отключить посылку." << endl;
	cout << "--prefix-sm-ready-timeout msec  - Время ожидание я готовности SM к работе. По умолчанию 15000" << endl;
	cout << "--prefix-filter-field name      - Название фильтрующего поля при формировании списка датчиков посылаемых данным узлом" << endl;
	cout << "--prefix-filter-value name      - Значение фильтрующего поля при формировании списка датчиков посылаемых данным узлом" << endl;
}
// -----------------------------------------------------------------------------
UNetExchange* UNetExchange::init_unetexchange( int argc, const char* argv[], UniSetTypes::ObjectId icID, 
												SharedMemory* ic, const std::string& prefix )
{
	string p("--" + prefix + "-name");
	string name = conf->getArgParam(p,"UNetExchange1");
	if( name.empty() )
	{
		cerr << "(unetexchange): Не задан name'" << endl;
		return 0;
	}

	ObjectId ID = conf->getObjectID(name);
	if( ID == UniSetTypes::DefaultObjectId )
	{
		cerr << "(unetexchange): Not found ObjectID for '" << name
			<< " in section '" << conf->getObjectsSection() << "'" << endl;
		return 0;
	}

	dlog[Debug::INFO] << "(unetexchange): name = " << name << "(" << ID << ")" << endl;
	return new UNetExchange(ID,icID,ic,prefix);
}
// -----------------------------------------------------------------------------
void UNetExchange::receiverEvent( UNetReceiver* r, UNetReceiver::Event ev )
{
	
	for( ReceiverList::iterator it=recvlist.begin(); it!=recvlist.end(); ++it )
	{
		if( it->r1 == r )
		{
			if( ev == UNetReceiver::evTimeout )
			{
				// если нет второго канала или нет связи
				// то и переключать не надо
				if( !it->r2 || !it->r2->isRecvOK() )
					return;
				
				// пропала связь по первому каналу...
				// переключаемся на второй
				it->r1->setLockUpdate(true);
				it->r2->setLockUpdate(false);
				
				if( dlog.debugging(Debug::LEVEL8) )
					dlog[Debug::INFO] << myname << "(event): " << r->getName() 
					<< ": timeout for channel1.. select channel 2" << endl;
			}
			else if( ev == UNetReceiver::evOK )
			{
				// если связь восстановилась..
				// проверяем, а что там со вторым каналом
				// если у него связи нет, то забираем себе..
				if( !it->r2 || !it->r2->isRecvOK() )
				{
					it->r1->setLockUpdate(false);
					if( it->r2 )
						it->r2->setLockUpdate(true);

					if( dlog.debugging(Debug::LEVEL8) )
						dlog[Debug::LEVEL8] << "(event): " << r->getName()
							<< ": link failed for channel2.. select again channel1.." << endl;
				}
			}
			
			return;
		}
		
		if( it->r2 == r )
		{
			if( ev == UNetReceiver::evTimeout )
			{
				// если первого канала нет или нет связи
				// то и переключать не надо
				if( !it->r1 || !it->r1->isRecvOK() )
					return;
				
				// пропала связь по второму каналу...
				// переключаемся на первый
				it->r1->setLockUpdate(false);
				it->r2->setLockUpdate(true);
				
				if( dlog.debugging(Debug::INFO) )
					dlog[Debug::INFO] << myname << "(event): " << r->getName() 
					<< ": timeout for channel2.. select channel 1" << endl;
				return;
			}
			else if( ev == UNetReceiver::evOK )
			{
				// если связь восстановилась..
				// проверяем, а что там со первым каналом
				// если у него связи нет, то забираем себе..
				if( !it->r1 || !it->r1->isRecvOK() )
				{
					if( it->r1 )
						it->r1->setLockUpdate(true);
					
					it->r2->setLockUpdate(false);

					if( dlog.debugging(Debug::LEVEL8) )
						dlog[Debug::LEVEL8] << myname << "(event): " << r->getName()
							<< ": link failed for channel1.. select again channel2.." << endl;
				}
			}

			return;
		}
	}
}
// -----------------------------------------------------------------------------
void UNetExchange::setIgnore( UniSetTypes::ObjectId id, bool set )
{
	std::list<UNetReceiver*> rList = get_receivers();
	std::list<UNetReceiver*>::iterator rIt = rList.begin();
	for(; rIt != rList.end(); ++ rIt )
		(*rIt)->setIgnore(id, set);
}
// -----------------------------------------------------------------------------
std::list<UNetReceiver*> UNetExchange::get_receivers()
{
	std::list<UNetReceiver*> tList;
	for( ReceiverList::iterator it=recvlist.begin(); it!=recvlist.end(); ++it )
	{
		if(it->r1)
			tList.push_back(it->r1);
		if(it->r2)
			tList.push_back(it->r2);
	}
	return tList;
}
// -----------------------------------------------------------------------------
