#ifndef UNetExchange_H_
#define UNetExchange_H_
// -----------------------------------------------------------------------------
#include <ostream>
#include <string>
#include <queue>
#include <cc++/socket.h>
#include "UniSetObject_LT.h"
#include "Trigger.h"
#include "Mutex.h"
#include "SMInterface.h"
#include "SharedMemory.h"
#include "ThreadCreator.h"
#include "UNetReceiver.h"
#include "UNetSender.h"
// -----------------------------------------------------------------------------
/*!
	\page pageUNetExchangeUDP Сетевой обмен на основе UDP (UNetUDP)
		
		- \ref pgUNetUDP_Common
		- \ref pgUNetUDP_Conf
		- \ref pgUNetUDP_Reserv

	\section pgUNetUDP_Common Общее описание
		Обмен построен  на основе протокола UDP. 
		Основная идея заключается в том, что каждый узел на порту равном своему ID
	посылает в сеть UDP-пакеты содержащие данные считанные из локальной SM. Формат данных - это набор
	пар [id,value]. Другие узлы принимают их. Помимо этого данный процесс запускает 
	по потоку приёма для каждого другого узла и ловит пакеты от них, сохраняя данные в SM.

	\par 
		При своём старте процесс считывает из секции \<nodes> список узлов которые необходимо "слушать", 
	а также параметры своего узла. Открывает по потоку приёма на каждый узел и поток
	передачи для своих данных. Помимо этого такие же потоки для резервных каналов, если они включены
	(см. \ref pgUNetUDP_Reserv ).
	
	\section pgUNetUDP_Conf Пример конфигурирования
		По умолчанию при считывании используется \b unet_broadcast_ip (указанный в секции \<nodes>)
	и \b id узла - в качестве порта.
	Но можно переопределять эти параметры, при помощи указания \b unet_port и/или \b unet_broadcast_ip,
	для конкретного узла (\<item>).
	\code
	<nodes port="2809" unet_broadcast_ip="192.168.56.255">
		<item ip="127.0.0.1" name="LocalhostNode" textname="Локальный узел" unet_ignore="1" unet_port="3000" unet_broadcast_ip="192.168.57.255">
		<iocards>
			...
		</iocards>
		</item>
		<item ip="192.168.56.10" name="Node1" textname="Node1" unet_port="3001"/>
		<item ip="192.168.56.11" name="Node2" textname="Node2" unet_port="3002"/>
	</nodes>
	\endcode

	\section pgUNetUDP_Reserv Настройка резервного канала связи
		В текущей реализации поддерживается возможность обмена по двум подсетям (эзернет-каналам).
	Она основана на том, что, для каждого узла помимо основного "читателя",
	создаётся дополнительный "читатель"(поток) слушающий другой ip-адрес и порт. 
	А так же, для локального узла создаётся дополнительный "писатель"(поток), 
	который посылает данные в (указанную) вторую подсеть. Для того, чтобы задействовать 
	второй канал, достаточно объявить в настройках переменные
	\b unet_broadcast_ip2. А также в случае необходимости для конкретного узла 
	можно указать \b unet_broadcast_ip2 и \b unet_port2.

	Переключение между "каналами" происходит по следующей логике:

	При старте включается только первый канал. Второй канал работает в режиме "пассивного" чтения. 
	Т.е. все пакеты принимаются, но данные в SharedMemory не сохраняются. 
	Если во время работы пропадает связь по первому каналу, идёт переключение на второй канал. 
	Первый канал переводиться в "пассивный" режим, а второй канал, переводится в "нормальный"(активный)
	режим. Далее работа ведётся по второму каналу, независимо от того, что связь на первом 
	канале может восстановиться. Это сделано для защиты от постоянных перескакиваний 
	с канала на канал. Работа на втором канале будет вестись, пока не пропадёт связь 
	на нём. Тогда будет попытка переключиться обратно на первый канал и так "по кругу".

	В свою очередь "писатели"(если они не отключены) всегда посылают данные в оба канала.
*/
// -----------------------------------------------------------------------------
class UNetExchange:
	public UniSetObject_LT
{
	public:
		UNetExchange( UniSetTypes::ObjectId objId, UniSetTypes::ObjectId shmID, SharedMemory* ic=0, const std::string& prefix="unet" );
		virtual ~UNetExchange();
	
		/*! глобальная функция для инициализации объекта */
		static UNetExchange* init_unetexchange( int argc, const char* argv[],
											UniSetTypes::ObjectId shmID, SharedMemory* ic=0, const std::string& prefix="unet" );

		virtual UNetReceiver* create_receiver( const std::string& h, const ost::tpport_t p, SMInterface* shm );
		virtual UNetSender* create_sender( const std::string& h, const ost::tpport_t p, SMInterface* shm,
					const std::string& s_field="", const std::string& s_fvalue="", SharedMemory* ic=0 );

		/*! глобальная функция для вывода help-а */
		static void help_print( int argc, const char* argv[] );

		bool checkExistUNetHost( const std::string& host, ost::tpport_t port );
		
		std::list<UNetReceiver*> get_receivers();
		
		/*! игнорировать запись датчика в SM */
		void setIgnore(UniSetTypes::ObjectId id = UniSetTypes::DefaultObjectId, bool set = true);

	protected:
		UNetExchange();

		xmlNode* cnode;
		std::string s_field;
		std::string s_fvalue;

		SMInterface* shm;
		void step();
		

		virtual void processingMessage( UniSetTypes::VoidMessage *msg );
		void sysCommand( UniSetTypes::SystemMessage *msg );
		void sensorInfo( UniSetTypes::SensorMessage*sm );
		void timerInfo( UniSetTypes::TimerMessage *tm );
		void askSensors( UniversalIO::UIOCommand cmd );
		void waitSMReady();
		void receiverEvent( UNetReceiver* r, UNetReceiver::Event ev );

		virtual bool activateObject();
		
		// действия при завершении работы
		virtual void sigterm( int signo );

		void initIterators();
		void startReceivers();
		

		enum Timer
		{
			tmStep
		};

	private:
		bool initPause;
		UniSetTypes::uniset_mutex mutex_start;

		PassiveTimer ptHeartBeat;
		UniSetTypes::ObjectId sidHeartBeat;
		int maxHeartBeat;
		IOController::AIOStateList::iterator aitHeartBeat;
		UniSetTypes::ObjectId test_id;

		int steptime;	/*!< периодичность вызова step, [мсек] */

		bool activated;
		int activateTimeout;

		struct ReceiverInfo
		{
			ReceiverInfo():r1(0),r2(0),
				sidRespond(UniSetTypes::DefaultObjectId),
				respondInvert(false),
				sidLostPackets(UniSetTypes::DefaultObjectId)
			{}

			ReceiverInfo(UNetReceiver* _r1, UNetReceiver* _r2 ):
				r1(_r1),r2(_r2),
				sidRespond(UniSetTypes::DefaultObjectId),
				respondInvert(false),
				sidLostPackets(UniSetTypes::DefaultObjectId)
			{}
			
			UNetReceiver* r1;  	/*!< приём по первому каналу */
			UNetReceiver* r2;	/*!< приём по второму каналу */

			void step( SMInterface* shm, const std::string& myname );

			inline void setRespondID( UniSetTypes::ObjectId id, bool invert=false )
			{ 
				sidRespond = id; 
				respondInvert = invert;
			}
			inline void setLostPacketsID( UniSetTypes::ObjectId id ){ sidLostPackets = id; }
			inline void initIterators( SMInterface* shm )
			{
 				shm->initAIterator(aitLostPackets);
				shm->initDIterator(ditRespond);
			}

			// Сводная информация по двум каналам
			// сумма потерянных пакетов и наличие связи
			// хотя бы по одному каналу
			// ( реализацию см. ReceiverInfo::step() )
			UniSetTypes::ObjectId sidRespond;
			IOController::DIOStateList::iterator ditRespond;
			bool respondInvert;
			UniSetTypes::ObjectId sidLostPackets;
			IOController::AIOStateList::iterator aitLostPackets;
		};
		
		typedef std::list<ReceiverInfo> ReceiverList;
		ReceiverList recvlist;

		bool no_sender;  /*!< флаг отключения посылки сообщений (создания потока для посылки)*/
		UNetSender* sender;
		UNetSender* sender2;
};
// -----------------------------------------------------------------------------
#endif // UNetExchange_H_
// -----------------------------------------------------------------------------
