
#include <JANA/JEventProcessor.h>

class JEventProcessorTest: public JEventProcessor
{
	public:

	void Process(const std::shared_ptr<const JEvent>& aEvent){
//		_DBG_<< __FUNCTION__ << _DBG_ENDL_;
	}

};



