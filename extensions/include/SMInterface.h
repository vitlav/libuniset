/* $Id: SMInterface.h,v 1.1 2008/12/14 21:57:50 vpashka Exp $ */
//--------------------------------------------------------------------------
#ifndef SMInterface_H_
#define SMInterface_H_
//--------------------------------------------------------------------------
#include <string>
#include "UniSetTypes.h"
#include "IONotifyController.h"
#include "UniversalInterface.h"
class SMInterface
{
	public:

		SMInterface( UniSetTypes::ObjectId _shmID, UniversalInterface* ui, 
						UniSetTypes::ObjectId myid, IONotifyController* ic=0 );
		~SMInterface();

		void setState ( UniSetTypes::ObjectId, bool state );
		void setValue ( UniSetTypes::ObjectId, long value );
		bool saveState ( IOController_i::SensorInfo& si, bool state, UniversalIO::IOTypes type, UniSetTypes::ObjectId supplier );
		bool saveValue ( IOController_i::SensorInfo& si, long value, UniversalIO::IOTypes type, UniSetTypes::ObjectId supplier );

		bool saveLocalState ( UniSetTypes::ObjectId id, bool state, UniversalIO::IOTypes type=UniversalIO::DigitalInput );
		bool saveLocalValue ( UniSetTypes::ObjectId id, long value, UniversalIO::IOTypes type=UniversalIO::AnalogInput );
		
		void setUndefinedState( IOController_i::SensorInfo& si, bool undefined, UniSetTypes::ObjectId supplier );

		long getValue ( UniSetTypes::ObjectId id );
		bool getState ( UniSetTypes::ObjectId id );
		
		void askSensor( UniSetTypes::ObjectId id, UniversalIO::UIOCommand cmd,
						UniSetTypes::ObjectId backid = UniSetTypes::DefaultObjectId );

		inline bool alarm( UniSetTypes::AlarmMessage& msg,  UniSetTypes::ObjectId messenger )
		{
			return ui->alarm(msg,messenger);
		}
		
		IOController_i::DSensorInfoSeq* getDigitalSensorsMap();
		IOController_i::ASensorInfoSeq* getAnalogSensorsMap();
		IONotifyController_i::ThresholdsListSeq* getThresholdsList();

		void localSaveValue( IOController::AIOStateList::iterator& it, 
								UniSetTypes::ObjectId sid, 
								CORBA::Long newvalue, UniSetTypes::ObjectId sup_id );

		void localSaveState( IOController::DIOStateList::iterator& it, 
								UniSetTypes::ObjectId sid, 
								CORBA::Boolean newstate, UniSetTypes::ObjectId sup_id );

		void localSetState( IOController::DIOStateList::iterator& it, 
							UniSetTypes::ObjectId sid, 
							CORBA::Boolean newstate, UniSetTypes::ObjectId sup_id );

		void localSetValue( IOController::AIOStateList::iterator& it, 
							UniSetTypes::ObjectId sid, 
							CORBA::Long value, UniSetTypes::ObjectId sup_id );

		bool localGetState( IOController::DIOStateList::iterator& it, 
							UniSetTypes::ObjectId sid );
	  	long localGetValue( IOController::AIOStateList::iterator& it, 
							UniSetTypes::ObjectId sid );

		/*! ������� ����������� �������� ��������̣����� ��������� ��� ���������� �������� 
			// ��� ���������� �������� ������������� ��� �������� ������� ���.
			// ��. ������ ����������� � ������� localSaveState
		*/
		void localSetUndefinedState( IOController::AIOStateList::iterator& it, 
									bool undefined, UniSetTypes::ObjectId sid );
	
		// ����������� �������
		IOController::DIOStateList::iterator dioEnd();
		IOController::AIOStateList::iterator aioEnd();
		void initAIterator( IOController::AIOStateList::iterator& it );
		void initDIterator( IOController::DIOStateList::iterator& it );
			
		bool exist();
		bool waitSMready( int msec, int pause=5000 );
		bool waitSMworking( UniSetTypes::ObjectId, int msec, int pause=3000 );

		inline bool isLocalwork(){ return (ic==NULL); }
		inline UniSetTypes::ObjectId ID(){ return myid; }
		
	protected:
		IONotifyController* ic;
		UniversalInterface* ui;
		CORBA::Object_var oref;
		UniSetTypes::ObjectId shmID;
		UniSetTypes::ObjectId myid;
};

//--------------------------------------------------------------------------
#endif