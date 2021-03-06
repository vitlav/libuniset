#include "modbus/ModbusServerSlot.h"
// -------------------------------------------------------------------------
using namespace ModbusRTU;
using namespace std;
// -------------------------------------------------------------------------
ModbusServerSlot::ModbusServerSlot()
{
}
// -------------------------------------------------------------------------
ModbusServerSlot::~ModbusServerSlot()
{

}
// -------------------------------------------------------------------------
void ModbusServerSlot::connectReadCoil( ReadCoilSlot sl )
{
	slReadCoil = sl;
}
// -------------------------------------------------------------------------
void ModbusServerSlot::connectReadInputStatus( ReadInputStatusSlot sl )
{
	slReadInputStatus = sl;
}
// -------------------------------------------------------------------------

void ModbusServerSlot::connectReadOutput( ReadOutputSlot sl )
{
	slReadOutputs = sl;
}
// -------------------------------------------------------------------------
void ModbusServerSlot::connectReadInput( ReadInputSlot sl )
{
	slReadInputs = sl;
}
// -------------------------------------------------------------------------
void ModbusServerSlot::connectForceCoils( ForceCoilsSlot sl )
{
	slForceCoils = sl;
}
// -------------------------------------------------------------------------
void ModbusServerSlot::connectWriteOutput( WriteOutputSlot sl )
{
	slWriteOutputs = sl;
}

// ------------------------------------------------------------------------
void ModbusServerSlot::connectWriteSingleOutput( WriteSingleOutputSlot sl )
{
	slWriteSingleOutputs = sl;
}

// -------------------------------------------------------------------------
void ModbusServerSlot::connectDiagnostics( DiagnosticsSlot sl )
{
	slDiagnostics = sl;
}
// -------------------------------------------------------------------------
void ModbusServerSlot::connectMEIRDI( MEIRDISlot sl )
{
	slMEIRDI = sl;
}
// -------------------------------------------------------------------------
void ModbusServerSlot::connectForceSingleCoil( ForceSingleCoilSlot sl )
{
	slForceSingleCoil = sl;
}
// -------------------------------------------------------------------------

void ModbusServerSlot::connectJournalCommand( JournalCommandSlot sl )
{
	slJournalCommand = sl;
}

// -------------------------------------------------------------------------
void ModbusServerSlot::connectSetDateTime( SetDateTimeSlot sl )
{
	slSetDateTime = sl;
}
// -------------------------------------------------------------------------
void ModbusServerSlot::connectRemoteService( RemoteServiceSlot sl )
{
	slRemoteService = sl;
}
// -------------------------------------------------------------------------
void ModbusServerSlot::connectFileTransfer( FileTransferSlot sl )
{
	slFileTransfer = sl;
}
// -------------------------------------------------------------------------
