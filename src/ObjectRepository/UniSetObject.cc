/* This file is part of the UniSet project
 * Copyright (c) 2002 Free Software Foundation, Inc.
 * Copyright (c) 2002 Pavel Vainerman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
// --------------------------------------------------------------------------
/*! \file
 *  \author Pavel Vainerman
*/
// -------------------------------------------------------------------------- 
#include <unistd.h>
#include <signal.h>
#include <iomanip>
#include <pthread.h>
#include <sys/types.h>
#include <sstream>

#include "Exceptions.h"
#include "ORepHelpers.h"
#include "ObjectRepository.h"
#include "UniversalInterface.h"
#include "UniSetObject.h"
#include "ObjectsManager.h"
#include "Debug.h"

// ------------------------------------------------------------------------------------------
using namespace std;
using namespace UniSetTypes;

#define CREATE_TIMER	new ThrPassiveTimer(); 	
// new PassiveSysTimer();

// ------------------------------------------------------------------------------------------
UniSetObject::UniSetObject():
ui(UniSetTypes::DefaultObjectId),
mymngr(NULL),
msgpid(0),
reg(false),
active(false),
threadcreate(false),
tmr(NULL),
myid(UniSetTypes::DefaultObjectId),
oref(0),
thr(NULL),
SizeOfMessageQueue(1000),
MaxCountRemoveOfMessage(10),
stMaxQueueMessages(0),
stCountOfQueueFull(0)
{
	tmr = CREATE_TIMER;
	myname = "noname";
	section = "nonameSection";
	init_object();
}
// ------------------------------------------------------------------------------------------
UniSetObject::UniSetObject( ObjectId id ):
ui(id),
mymngr(NULL),
msgpid(0),
reg(false),
active(false),
threadcreate(true),
tmr(NULL),
myid(id),
oref(0),
thr(NULL),
SizeOfMessageQueue(1000),
MaxCountRemoveOfMessage(10),
stMaxQueueMessages(0),
stCountOfQueueFull(0)
{
	tmr = CREATE_TIMER;
	if (myid >=0)
	{
		string myfullname = ui.getNameById(id);
		myname = ORepHelpers::getShortName(myfullname.c_str());
		section = ORepHelpers::getSectionName(myfullname.c_str());
	}
	else
	{
		threadcreate = false;
		myid = UniSetTypes::DefaultObjectId;
		myname = "noname";
		section = "nonameSection";
	}

	init_object();
}


UniSetObject::UniSetObject(const string name, const string section):
ui(UniSetTypes::DefaultObjectId),
mymngr(NULL),
msgpid(0),
reg(false),
active(false),
threadcreate(true),
tmr(NULL),
myid(UniSetTypes::DefaultObjectId),
oref(0),
thr(NULL),
SizeOfMessageQueue(1000),
MaxCountRemoveOfMessage(10),
stMaxQueueMessages(0),
stCountOfQueueFull(0)
{
	/*! \warning UniverslalInterface не инициализируется идентификатором объекта */
	tmr = CREATE_TIMER;
	myname = section + "/" + name;
	myid = ui.getIdByName(myname);
	if( myid == DefaultObjectId )
	{
		unideb[Debug::WARN] << "name: my ID not found!" << endl;
		throw Exception(name+": my ID not found!");
	}

	init_object();
	ui.initBackId(myid);
}

// ------------------------------------------------------------------------------------------
UniSetObject::~UniSetObject() 
{
	disactivate();
	delete tmr;
	if(thr)
		delete thr;
}
// ------------------------------------------------------------------------------------------
void UniSetObject::init_object()
{
	SizeOfMessageQueue = conf->getArgPInt("--uniset-object-size-message-queue",conf->getField("SizeOfMessageQueue"), 1000);
	
	MaxCountRemoveOfMessage = conf->getArgInt("--uniset-object-maxcount-remove-message",conf->getField("MaxCountRemoveOfMessage"));
	if( MaxCountRemoveOfMessage <= 0 )
		MaxCountRemoveOfMessage = SizeOfMessageQueue / 4;
	if( MaxCountRemoveOfMessage <= 0 )
		MaxCountRemoveOfMessage = 10;
	recvMutexTimeout = conf->getArgPInt("--uniset-object-receive-mutex-timeout",conf->getField("RecvMutexTimeout"), 10000);

	pushMutexTimeout = conf->getArgPInt("--uniset-object-push-mutex-timeout",conf->getField("PushMutexTimeout"), 9000);
	
	if( unideb.debugging(Debug::INFO) )
	{
		unideb[Debug::INFO] << myname << "(init): SizeOfMessageQueue=" << SizeOfMessageQueue
			<< " MaxCountRemoveOfMessage=" << MaxCountRemoveOfMessage
			<< " recvMutexTimeout=" << recvMutexTimeout
			<< " pushMutexTimeout=" << pushMutexTimeout
			<< endl;
	}
}
// ------------------------------------------------------------------------------------------

/*!
 *	\param om - указазтель на менджер управляющий объектом
 *	\return Возращает \a true если инициализация прошда успешно, и \a false если нет
*/
bool UniSetObject::init( ObjectsManager* om )
{
	if( unideb.debugging(Debug::INFO) )
	  unideb[Debug::INFO] << myname << ": init..." << endl;
	this->mymngr = om;
	if( unideb.debugging(Debug::INFO) )
		unideb[Debug::INFO] << myname << ": init ok..." << endl;
	return true;
}
// ------------------------------------------------------------------------------------------
void UniSetObject::setID( UniSetTypes::ObjectId id )
{
	if( myid!=UniSetTypes::DefaultObjectId )
		throw ObjectNameAlready("ObjectId already set(setID)");

	string myfullname = ui.getNameById(id);
	myname = ORepHelpers::getShortName(myfullname.c_str()); 
	section = ORepHelpers::getSectionName(myfullname.c_str());
	myid = id;
	ui.initBackId(myid);
}

// ------------------------------------------------------------------------------------------
/*!
 *	\param  vm - указатель на структуру, которая заполняется если есть сообщение
 *	\return Возвращает \a true если сообщение есть, и \a false если нет
*/
bool UniSetObject::receiveMessage( VoidMessage& vm )
{
	{	// lock
		uniset_mutex_lock mlk(qmutex, recvMutexTimeout);
			
		if( !queueMsg.empty() )
		{
			// контроль переполнения
			if( queueMsg.size() > SizeOfMessageQueue ) 
			{
				if( unideb.debugging(Debug::CRIT) )
				  unideb[Debug::CRIT] << myname <<"(receiveMessages): messages queue overflow!" << endl << flush;
				cleanMsgQueue(queueMsg);
				// обновляем статистику по переполнениям
				stCountOfQueueFull++;
				stMaxQueueMessages=0;	
			}

			if( !queueMsg.empty() )
			{
//			      if( unideb.debugging(Debug::CRIT) )
//				unideb[Debug::CRIT] << myname <<"(receiveMessages): get new msg.." << endl << flush;

				vm = queueMsg.top(); // получили сообщение
//				Проверка на последовательное вынимание			
//				cout << myname << ": receive message....tm=" << vm.time << " msec=" << vm.time_msec << "\tprior="<< vm.priority << endl;
				queueMsg.pop(); // удалили сообщение из очереди
				return true;
			}
		}	
	} // unlock queue

	return false;	
}

// ------------------------------------------------------------------------------------------
// структура определяющая минимальное количество полей
// по которым можно судить о схожести сообщений
// используется локально и только в функции очистки очереди сообщений
struct MsgInfo
{
	MsgInfo():
	type(Message::Unused),
	id(DefaultObjectId),
	acode(DefaultMessageCode),
	ccode(DefaultMessageCode),
	ch(0),
	node(DefaultObjectId)
	{
//		struct timezone tz;
		tm.tv_sec = 0;
		tm.tv_usec = 0;
//		gettimeofday(&tm,&tz);
	}

	MsgInfo( AlarmMessage& am ):
	type(am.type),
	id(am.id),
	acode(am.alarmcode),
	ccode(am.causecode),
	ch(am.character),
	tm(am.tm),
	node(am.node)
	{}

	MsgInfo( InfoMessage& am ):
	type(am.type),
	id(am.id),
	acode(am.infocode),
	ccode(0),
	ch(am.character),
	tm(am.tm),
	node(am.node)
	{}

	int type;
	ObjectId id;		// от кого
	MessageCode acode;	// код сообщения
	MessageCode ccode;	// код причины
	int ch;				// характер
	struct timeval tm;	// время
	ObjectId node;		// откуда

   	inline bool operator < ( const MsgInfo& mi ) const
	{
		if( type != mi.type )
			return type < mi.type; 

		if( id != mi.id )
			return id < mi.id;

		if( node != mi.node )
			return node < mi.node; 

		if( acode != mi.acode )
			return acode < mi.acode;

		if( ch != mi.ch )
			return ch < mi.ch;

		if( tm.tv_sec != mi.tm.tv_sec )
			return tm.tv_sec < mi.tm.tv_sec;

		return tm.tv_usec < mi.tm.tv_usec;
	}	
	
};

// структура определяющая минимальное количество полей
// по которым можно судить о схожести сообщений
// используется локально и только в функции очистки очереди сообщений
struct CInfo
{
	CInfo():
     sensor_id(DefaultObjectId),
     value(0),
     time(0),
     time_usec(0),
     confirm(0)
	{
	}
	
	CInfo( ConfirmMessage& cm ):
		sensor_id(cm.sensor_id),
		value(cm.value),
		time(cm.time),
		time_usec(cm.time_usec),
		confirm(cm.confirm)
	{}

	long sensor_id;   /* ID датчика */
	double value;     /* значение датчика */
	time_t time;      /* время, когда датчик получил сигнал */
	time_t time_usec; /* время в микросекундах */
	time_t confirm;   /* время, когда произошло квитирование */

   	inline bool operator < ( const CInfo& mi ) const
	{
		if( sensor_id != mi.sensor_id )
			return sensor_id < mi.sensor_id;

		if( value != mi.value )
			return value < mi.value;

		if( time != mi.time )
			return time < mi.time;

		return time_usec < mi.time_usec;
	}
};

// ------------------------------------------------------------------------------------------
bool UniSetObject::waitMessage(VoidMessage& vm, timeout_t timeMS)
{
	if( receiveMessage(vm) )
		return true;
	tmr->wait(timeMS);
	return receiveMessage(vm);
}
// ------------------------------------------------------------------------------------------
void UniSetObject::registered()
{
	if( unideb.debugging(Debug::INFO) )
		unideb[Debug::INFO] << myname << ": registration..." << endl;

	if( myid == UniSetTypes::DefaultObjectId )
	{
		if( unideb.debugging(Debug::INFO) )
			unideb[Debug::INFO] << myname << "(registered): myid=DefaultObjectId \n";
		return;
	}

	if( !mymngr )
	{
		unideb[Debug::WARN] << myname << "(registered): unknown my manager" << endl;
		string err(myname+": unknown my manager");
		throw ORepFailed(err.c_str());
	}

	if( !oref )
	{
		unideb[Debug::CRIT] << myname << "(registered): oref is NULL!..." << endl;
		return;
	}

	try
	{
		for( int i=0; i<2; i++ )
		{		
			try
			{
				ui.registered(myid, getRef(),true);
				break;
			}
			catch( ObjectNameAlready& al )
			{
/*! 
	\warning По умолчанию объекты должны быть уникальны! Поэтому если идёт попытка повторной регистрации. 
	Мы чистим существующую ссылку и заменяем её на новую.	
	Это сделано для более надежной работы, иначе может получится, что если объект перед завершением
	не очистил за собой ссылку(не разрегистрировался), то больше он никогда не сможет вновь зарегистрироваться.
	Т.к. \b надёжной функции проверки "жив" ли объект пока нет...
	(так бы можно было проверить и если "не жив", то смело заменять ссылку на новую). Но существует обратная сторона:
	если заменяемый объект "жив" и завершит свою работу, то он может почистить за собой ссылку и это тогда наш(новый) 
	объект станет недоступен другим, а знать об этом не будет!!!
	
*/
				unideb[Debug::CRIT] << myname << "(registered): replace object (ObjectNameAlready)" << endl;
				reg = true;
				unregister();
//				unideb[Debug::CRIT] << myname << "(registered): не смог зарегестрироваться в репозитории объектов (ObjectNameAlready)" << endl;
//				throw al;
			}
		}
	}
	catch( ORepFailed )
	{
		string err(myname+": don`t registration in object reposotory");
		throw ORepFailed(err.c_str());
	}
	catch(Exception& ex)
	{
		unideb[Debug::WARN] << myname << "(registered):  " << ex << endl;
		string err(myname+": don`t registration in object reposotory");
		throw ORepFailed(err.c_str());
	}
	reg = true;
}
// ------------------------------------------------------------------------------------------
void UniSetObject::unregister()
{
	if( myid<0 ) // || !reg )
		return;

	if( myid == UniSetTypes::DefaultObjectId )
	{
		if( unideb.debugging(Debug::INFO) )
			unideb[Debug::INFO] << myname << "(unregister): myid=DefaultObjectId \n";
		reg = false;
		return;
	}

	if( !oref )
	{
		unideb[Debug::WARN] << myname << "(unregister): oref NULL!" << endl;
		reg = false;
		return;
	}


	try
	{
		if( unideb.debugging(Debug::INFO) )
			unideb[Debug::INFO] << myname << ": unregister "<< endl;

		ui.unregister(myid);

		if( unideb.debugging(Debug::INFO) )
			unideb[Debug::INFO] << myname << ": unregister ok. "<< endl;
	}
	catch(...)
	{
		unideb[Debug::WARN] << myname << ": don`t registration in object repository" << endl;
	}
	
	reg = false;
}
// ------------------------------------------------------------------------------------------
CORBA::Boolean UniSetObject::exist()
{
	return true;
}
// ------------------------------------------------------------------------------------------
void UniSetObject::termWaiting()
{
    if( tmr!=NULL )
		tmr->terminate();
}
// ------------------------------------------------------------------------------------------
void UniSetObject::setRecvMutexTimeout( unsigned long msec )
{
	recvMutexTimeout = msec;
}
// ------------------------------------------------------------------------------------------
void UniSetObject::setPushMutexTimeout( unsigned long msec )
{
	pushMutexTimeout = msec;
}
// ------------------------------------------------------------------------------------------
void UniSetObject::setThreadPriority( int p )
{
	if( thr )
		thr->setPriority(p);
}
// ------------------------------------------------------------------------------------------
void UniSetObject::push(const TransportMessage& tm)
{
	{ // lock
		uniset_mutex_lock mlk(qmutex,pushMutexTimeout);
		// контроль переполнения
		if( !queueMsg.empty() && queueMsg.size()>SizeOfMessageQueue )
		{
			if( unideb.debugging(Debug::CRIT) )
			  unideb[Debug::CRIT] << myname <<"(push): message queue overflow!" << endl << flush;
			cleanMsgQueue(queueMsg);

			// обновляем статистику
			stCountOfQueueFull++;
			stMaxQueueMessages=0;	
		}

//		if( unideb.debugging(Debug::CRIT) )
//		  unideb[Debug::CRIT] << myname <<"(push): push new msg.." << endl << flush;

		VoidMessage v(tm);
		queueMsg.push(v);
		
		// максимальное число ( для статистики )
		if( queueMsg.size() > stMaxQueueMessages )
			stMaxQueueMessages = queueMsg.size();

	} // unlock

	termWaiting();
}
// ------------------------------------------------------------------------------------------
struct tmpConsumerInfo
{
	tmpConsumerInfo(){}
	
	map<UniSetTypes::KeyType,VoidMessage> smap;
	map<int,VoidMessage> tmap;
	map<int,VoidMessage> sysmap;
	map<MsgInfo,VoidMessage> amap;
	map<MsgInfo,VoidMessage> imap;
	map<CInfo,VoidMessage> cmap;
	list<VoidMessage> lstOther;
};

void UniSetObject::cleanMsgQueue( MessagesQueue& q )
{
	if( unideb.debugging(Debug::CRIT) )
	{
 		unideb[Debug::CRIT] << myname << "(cleanMsgQueue): msg queue cleaning..." << endl << flush;
		unideb[Debug::CRIT] << myname << "(cleanMsgQueue): current size of queue: " << q.size() << endl << flush;
	}

	// проходим по всем известным нам типам(базовым)
	// ищем все совпадающие сообщения и оставляем только последние...
	VoidMessage m;
	map<UniSetTypes::ObjectId,tmpConsumerInfo> consumermap;

//		while( receiveMessage(vm) );
//		while нельзя использовать потому-что, из параллельного потока
//		могут запихивать в очередь ещё сообщения.. И это цикл никогда не прервётся...

	while( !q.empty() )
	{
		m = q.top();
		q.pop();
			
		switch( m.type )
		{
			case Message::SensorInfo:
			{
				SensorMessage sm(&m);
				UniSetTypes::KeyType k(key(sm.id,sm.node));
				// т.к. из очереди сообщений сперва вынимаются самые старые, потом свежее и т.п.
				// то достаточно просто сохранять последнее сообщение для одинаковых Key
				consumermap[sm.consumer].smap[k] = m;
			}
			break;

			case Message::Timer:
			{
				TimerMessage tm(&m);
				// т.к. из очереди сообщений сперва вынимаются самые старые, потом свежее и т.п.
				// то достаточно просто сохранять последнее сообщение для одинаковых TimerId
				consumermap[tm.consumer].tmap[tm.id] = m;
			}
			break;

			case Message::SysCommand:
			{
				SystemMessage sm(&m);
				consumermap[sm.consumer].sysmap[sm.command] = m;
			}
			break;

			case Message::Alarm:
			{
				AlarmMessage am(&m);
				MsgInfo mi(am);
				// т.к. из очереди сообщений сперва вынимаются самые старые, потом свежее и т.п.
				// то достаточно просто сохранять последнее сообщение для одинаковых MsgInfo
				consumermap[am.consumer].amap[mi] = m;
			}
			break;

			case Message::Info:
			{
				InfoMessage im(&m);
				MsgInfo mi(im);
				// т.к. из очереди сообщений сперва вынимаются самые старые, потом свежее и т.п.
				// то достаточно просто сохранять последнее сообщение для одинаковых MsgInfo
				consumermap[im.consumer].imap[mi] = m;
			}
			break;

		
			case Message::Confirm:
			{
				ConfirmMessage cm(&m);
				CInfo ci(cm);
				// т.к. из очереди сообщений сперва вынимаются самые старые, потом свежее и т.п.
				// то достаточно просто сохранять последнее сообщение для одинаковых MsgInfo
				consumermap[cm.consumer].cmap[ci] = m;
			}
			break;

			case Message::Unused:
				// просто выкидываем (игнорируем)
			break;
			
			default:
				// сразу помещаем в очередь
				consumermap[m.consumer].lstOther.push_front(m);
			break;

		}
	}	

	if( unideb.debugging(Debug::CRIT) )
		unideb[Debug::CRIT] << myname << "(cleanMsgQueue): ******** cleanup RESULT ********" << endl;
	
	for( map<UniSetTypes::ObjectId,tmpConsumerInfo>::iterator it0 = consumermap.begin(); 
			it0!=consumermap.end(); ++it0 )
	{
		if( unideb.debugging(Debug::CRIT) )
		{
			unideb[Debug::CRIT] << myname << "(cleanMsgQueue): CONSUMER=" << it0->first << endl;
	  		unideb[Debug::CRIT] << myname << "(cleanMsgQueue): after clean SensorMessage: " << it0->second.smap.size() << endl;
	  		unideb[Debug::CRIT] << myname << "(cleanMsgQueue): after clean TimerMessage: " << it0->second.tmap.size() << endl;
	  		unideb[Debug::CRIT] << myname << "(cleanMsgQueue): after clean SystemMessage: " << it0->second.sysmap.size() << endl;
	  		unideb[Debug::CRIT] << myname << "(cleanMsgQueue): after clean AlarmMessage: " << it0->second.amap.size() << endl;
	  		unideb[Debug::CRIT] << myname << "(cleanMsgQueue): after clean InfoMessage: " << it0->second.imap.size() << endl;
	  		unideb[Debug::CRIT] << myname << "(cleanMsgQueue): after clean ConfirmMessage: " << it0->second.cmap.size() << endl;
	  		unideb[Debug::CRIT] << myname << "(cleanMsgQueue): after clean other: " << it0->second.lstOther.size() << endl;
		}
		
		// теперь ОСТАВШИЕСЯ запихиваем обратно в очередь...
		map<UniSetTypes::KeyType,VoidMessage>::iterator it=it0->second.smap.begin();
		for( ; it!=it0->second.smap.end(); ++it )
		{
			q.push(it->second);
		}

		map<int,VoidMessage>::iterator it1=it0->second.tmap.begin();
		for( ; it1!=it0->second.tmap.end(); ++it1 )
		{
			q.push(it1->second);
		}

		map<int,VoidMessage>::iterator it2=it0->second.sysmap.begin();
		for( ; it2!=it0->second.sysmap.end(); ++it2 )
		{
			q.push(it2->second);
		}

		map<MsgInfo,VoidMessage>::iterator it3=it0->second.amap.begin();
		for( ; it3!=it0->second.amap.end(); ++it3 )
		{
			q.push(it3->second);
		}

		map<MsgInfo,VoidMessage>::iterator it4=it0->second.imap.begin();
		for( ; it4!=it0->second.imap.end(); ++it4 )
		{
			q.push(it4->second);
		}

		map<CInfo,VoidMessage>::iterator it5=it0->second.cmap.begin();
		for( ; it5!=it0->second.cmap.end(); ++it5 )
		{
			q.push(it5->second);
		}

		list<VoidMessage>::iterator it6=it0->second.lstOther.begin();
		for( ; it6!=it0->second.lstOther.end(); ++it6 )
			q.push(*it6);
	}

	if( unideb.debugging(Debug::CRIT) )
	{
	    unideb[Debug::CRIT] << myname 
		<< "(cleanMsgQueue): ******* result size of queue: " 
		<< q.size()
		<< " < " << getMaxSizeOfMessageQueue() << endl;
	}
		
	if( q.size() >= getMaxSizeOfMessageQueue() )
	{
		if( unideb.debugging(Debug::CRIT) )
		{
		  unideb[Debug::CRIT] << myname << "(cleanMsgQueue): clean failed. size > " << q.size() << endl;
		  unideb[Debug::CRIT] << myname << "(cleanMsgQueue): remove " << getMaxCountRemoveOfMessage() << " old messages " << endl;
		}
		for( unsigned int i=0; i<getMaxCountRemoveOfMessage(); i++ )
		{
			q.top(); 
			q.pop(); 
			if( q.empty() )
			    break;
		}
		
		if( unideb.debugging(Debug::CRIT) )
		  unideb[Debug::CRIT] << myname << "(cleanMsgQueue): result size=" << q.size() << endl;
	}
}
// ------------------------------------------------------------------------------------------
unsigned int UniSetObject::countMessages()
{
	{ // lock
		uniset_mutex_lock mlk(qmutex, 200);
		return queueMsg.size();
	}
}
// ------------------------------------------------------------------------------------------
bool UniSetObject::disactivate()
{
	if( !isActive() )
	{
		try
		{
			disactivateObject();
		}
		catch(...){}
		return true;
	}

	setActive(false); // завершаем поток обработки сообщений
	tmr->stop();

	// Очищаем очередь
	{ // lock
		uniset_mutex_lock mlk(qmutex, 400);
		while( !queueMsg.empty() )
			queueMsg.pop(); 
	}

	try
	{
		if( unideb.debugging(Debug::INFO) )
			unideb[Debug::INFO] << "disactivateObject..." << endl;

		PortableServer::POA_var poamngr = mymngr->getPOA();
		if( !PortableServer::POA_Helper::is_nil(poamngr) )
		{
			try
			{
				disactivateObject();
			}
			catch(...){}
			unregister();
			PortableServer::ObjectId_var oid = poamngr->servant_to_id(static_cast<PortableServer::ServantBase*>(this));
			poamngr->deactivate_object(oid);
			if( unideb.debugging(Debug::INFO) )
				unideb[Debug::INFO] << "ok..." << endl;
			return true;
		}
		unideb[Debug::WARN] << "manager already destroyed.." << endl;
	}
	catch(CORBA::TRANSIENT)
	{
		unideb[Debug::WARN] << "isExist: нет связи..."<< endl;
	}
	catch( CORBA::SystemException& ex )
    {
		unideb[Debug::WARN] << "UniSetObject: "<<"поймали CORBA::SystemException: " << ex.NP_minorString() << endl;
    }
    catch(CORBA::Exception& ex)
    {
		unideb[Debug::WARN] << "UniSetObject: "<<"поймали CORBA::Exception." << endl;
    }
	catch(Exception& ex)
    {
		unideb[Debug::WARN] << "UniSetObject: "<< ex << endl;
    }
    catch(...)
    {
		unideb[Debug::WARN] << "UniSetObject: "<<" catch ..." << endl;
    }

	return false;
}

// ------------------------------------------------------------------------------------------
bool UniSetObject::activate()
{
	if( unideb.debugging(Debug::INFO) )
		unideb[Debug::INFO] << myname << ": activate..." << endl;

	if( mymngr == NULL )
	{
		unideb[Debug::CRIT] << myname << "(activate): mymngr=NULL!!! activate failure..." << endl;
		return false;
	}

	PortableServer::POA_var poa = mymngr->getPOA();
	if( poa == NULL || CORBA::is_nil(poa) )
	{
		string err(myname+": не задан менеджер");
		throw ORepFailed(err.c_str());
	}

	if( conf->isTransientIOR() )
	{
	    // activate witch generate id
		poa->activate_object(static_cast<PortableServer::ServantBase*>(this));
	}
	else
	{
		// А если myid==UniSetTypes::DefaultObjectId 
		// то myname = noname. ВСЕГДА! 
		if( myid == UniSetTypes::DefaultObjectId )
		{
			unideb[Debug::CRIT] << myname << "(activate): Не задан ID!!! activate failure..." << endl;
			// вызываем на случай если она переопределена в дочерних классах
			// Например в ObjectsManager, если здесь не вызвать, то не будут инициализированы подчинённые объекты.
			// (см. ObjectsManager::activateObject)
			activateObject();
			return false;
		}

	    // Always use the same object id.
    	PortableServer::ObjectId_var oid =
		PortableServer::string_to_ObjectId(myname.c_str());

//		cerr << myname << "(activate): " << _refcount_value() << endl;

    	// Activate object...
	    poa->activate_object_with_id(oid, this);
	}
	

	
	oref = poa->servant_to_reference(static_cast<PortableServer::ServantBase*>(this) );

	registered();
	// Запускаем поток обработки сообщений
	setActive(true);

	if( myid!=UniSetTypes::DefaultObjectId && threadcreate )
	{
		thr = new ThreadCreator<UniSetObject>(this, &UniSetObject::work);
		thr->start();
	}
	else 
	{
		if( unideb.debugging(Debug::INFO) )
		{
			unideb[Debug::INFO] << myname << ": ?? не задан ObjectId...(" 
					<< "myid=" << myid << " threadcreate=" << threadcreate 
					<< ")" << endl;
		}
		thread(false);
	}

	activateObject();
	if( unideb.debugging(Debug::INFO) )
		unideb[Debug::INFO] << myname << ": activate ok." << endl;
	return true;
}
// ------------------------------------------------------------------------------------------
void UniSetObject::work()
{
	if( unideb.debugging(Debug::INFO) )
		unideb[Debug::INFO] << myname << ": thread processing messages run..." << endl;
	if( thr )
		msgpid = thr->getTID();
	while( isActive() )
	{
		callback();
	}
	unideb[Debug::WARN] << myname << ": thread processing messages stop..." << endl;	
}
// ------------------------------------------------------------------------------------------
void UniSetObject::callback()
{
	try
	{
		if( waitMessage(msg) )
			processingMessage(&msg);
	}
	catch(...){}
}
// ------------------------------------------------------------------------------------------
void UniSetObject::processingMessage( UniSetTypes::VoidMessage *msg )
{
	if( unideb.debugging(Debug::INFO) )
		unideb[Debug::INFO] << myname << ": default processing messages..." << endl;	
}
// ------------------------------------------------------------------------------------------
UniSetTypes::SimpleInfo* UniSetObject::getInfo()
{
	ostringstream info;
	info.setf(ios::left, ios::adjustfield);
	info << "(" << myid << ")" << setw(40) << myname << "\n==================================================\n";
	info << "tid=" << setw(10);
	if( threadcreate )
	{
		if(thr)	
		{
			msgpid = thr->getTID();	// заодно(на всякий) обновим и внутреннюю информацию
			info << msgpid;  
		}
		else
			info << "не запущен";
	}
	else
		info << "откл.";  
	
	info << "\tcount=" << countMessages();
	info << "\tmaxMsg=" << stMaxQueueMessages;
	info << "\tqFull("<< SizeOfMessageQueue << ")=" << stCountOfQueueFull;
//	info << "\n";
	
	SimpleInfo* res = new SimpleInfo();
	res->info 	=  info.str().c_str(); // CORBA::string_dup(info.str().c_str());
	res->id 	=  myid;
	
	return res; // ._retn();
}
// ------------------------------------------------------------------------------------------
ostream& operator<<(ostream& os, UniSetObject& obj )
{
	SimpleInfo_var si = obj.getInfo();
	return os << si->info;
}
// ------------------------------------------------------------------------------------------

bool UniSetObject::PriorVMsgCompare::operator()(const UniSetTypes::VoidMessage& lhs, 
												const UniSetTypes::VoidMessage& rhs) const
{
	if( lhs.priority == rhs.priority )
	{
		if( lhs.tm.tv_sec == rhs.tm.tv_sec )
			return lhs.tm.tv_usec >= rhs.tm.tv_usec;
		return lhs.tm.tv_sec >= rhs.tm.tv_sec;
	}
	
	return lhs.priority < rhs.priority;
}
// ------------------------------------------------------------------------------------------
void UniSetObject::setActive( bool set )
{
	uniset_mutex_lock l(act_mutex,300);
	active = set;
}
// ------------------------------------------------------------------------------------------
bool UniSetObject::isActive()
{
	uniset_mutex_lock l(act_mutex,200);
	return active;
}
// ------------------------------------------------------------------------------------------
#undef CREATE_TIMER
