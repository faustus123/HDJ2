
#include <JANA/JEventProcessor.h>

#include <DAQ/Df125FDCPulse.h>

class JEventProcessorTest: public JEventProcessor
{
	public:

	void Process(const std::shared_ptr<const JEvent>& aEvent){
//		_DBG_<< __FUNCTION__ << _DBG_ENDL_;

//		auto fdcpulses = aEvent->GetT<Df125FDCPulse>();
//		_DBG_<< " Nfdcpulses = " << fdcpulses.size() << _DBG_ENDL_;
	}

};



