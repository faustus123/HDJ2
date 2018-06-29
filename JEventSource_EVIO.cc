// $Id$
//
//    File: JEventxample1.cc
// Created: Wed Jan  3 12:37:53 EST 2018
// Creator: davidl (on Linux jana2.jlab.org 3.10.0-693.11.1.el7.x86_64)
//

#include <memory>
#include <utility>
#include <string>

#include <JApplication.h>
#include <JANA/JEventSourceGeneratorT.h>
#include "JEventSource_EVIO.h"
#include "JEventEVIOBuffer.h"


//-------------------------------------------------------------------------
// Plugin glue
extern "C"{
void InitPlugin(JApplication *app){
	InitJANAPlugin(app);

	app->Add( new JEventSourceGeneratorT<JEventSource_EVIO>() );
	
	app->GetJThreadManager()->AddQueue( JQueueSet::JQueueType::Events, new JQueueWithBarriers("Parsed", 100, 100) );
}
} // "C"

//-----------------------------------
// JEventSource_EVIO  Constructor
//-----------------------------------
JEventSource_EVIO::JEventSource_EVIO(std::string source_name, JApplication *app):JEventSource(source_name, app)
{
	gPARMS->SetDefaultParameter("EVIO:VERBOSE", VERBOSE, "Set verbosity level for processing and debugging statements while parsing. 0=no debugging messages. 10=all messages");

	// Try to open the file.
	if(VERBOSE>0) jout << "Attempting to open EVIO file \"" << this->mName<< "\" ..." << endl;
	hdevio = new HDEVIO( this->mName, true, VERBOSE);
	if( ! hdevio->is_open ){
		cerr << hdevio->err_mess.str() << endl;
		throw JException("Failed to open EVIO file: " + this->mName, __FILE__, __LINE__); // throw exception indicating error
	}

	// Tell JANA how many times to call GetEvent in a row while it has the lock.
	// This will reduce the number of times the lock must be obtained.
	// Note: It may not always call GetEvent maxtimes in a row. It applies
	// an algorithm to decide how many, up to that limit.
	SetNumEventsToGetAtOnce(1, 10);
}

//-----------------------------------
// GetEvent
//-----------------------------------
std::shared_ptr<const JEvent> JEventSource_EVIO::GetEvent(void)
{
	// JANA will only call this for one thread at a time so
	// no need to worry about locks.

	// Get JEventEVIOBuffer from pool. The JEventEVIOBuffer object has
	// its own buffer that we will read the EVIO event into, growing it
	// if needed to hold the entire buffer. 
	JEventEVIOBuffer *jevent = GetJEventEVIOBufferFromPool();
	uint32_t* &buff          = jevent->buff;
	uint32_t  &buff_len      = jevent->buff_len;

	bool allow_swap = false;

	hdevio->readNoFileBuff(buff, buff_len, allow_swap);
//	evioworker->pos = hdevio->last_event_pos;
	if(hdevio->err_code == HDEVIO::HDEVIO_USER_BUFFER_TOO_SMALL){
		delete[] buff;
		buff_len = hdevio->last_event_len;
		buff = new uint32_t[buff_len];
		hdevio->readNoFileBuff(buff, buff_len, allow_swap);
	}

	// Check if read was successful
	if( hdevio->err_code==HDEVIO::HDEVIO_OK ){
		// HDEVIO_OK

		uint32_t myjobtype = JEventEVIOBuffer::JOB_FULL_PARSE;
		if(hdevio->swap_needed) myjobtype |= JEventEVIOBuffer::JOB_SWAP;

		jevent->jobtype = (JEventEVIOBuffer::JOBTYPE)myjobtype;
		jevent->istreamorder = istreamorder++;

		// Return the JEvent as a shared_ptr. Supply our own deleter function
		// to return the JEventEVIOBuffer object to our pool when it's done
		// instead of actually deleting it.
		return std::shared_ptr<JEvent>( (JEvent*)jevent, [this,jevent](JEvent*evt){ this->ReturnJEventEVIOBufferToPool(jevent); } );

	}else{
		// Problem reading in event

		ReturnJEventEVIOBufferToPool(jevent);
		
		if(LOOP_FOREVER && NEVENTS_PROCESSED>=1){
			if(hdevio){
				hdevio->rewind();
				throw JEventSource::RETURN_STATUS::kTRY_AGAIN;
			}
		}
		cout << hdevio->err_mess.str() << endl;
		if( hdevio->err_code == HDEVIO::HDEVIO_EOF ){
			throw JEventSource::RETURN_STATUS::kNO_MORE_EVENTS;
		}

		japp->SetExitCode(hdevio->err_code);
		throw JEventSource::RETURN_STATUS::kERROR;
	}
}

//-----------------------------------
// GetProcessEventTask
//-----------------------------------
std::shared_ptr<JTaskBase> JEventSource_EVIO::GetProcessEventTask(std::shared_ptr<const JEvent>&& aEvent)
{
	// Create task to process events obtained from source via GetEvent().
	// i.e. JEventEVIOBuffer object. The task will parse a single JEventEVIOBuffer
	// event and create one or more JEventEVIOParsed events from it. Those parsed
	// events will be placed in the Parsed event queue with a task to run the
	// event processors.

	// Define function that will be executed by the task
	auto sParseBuffer = [](const std::shared_ptr<const JEvent>& aEvent) -> void
	{
		// The JEvent passed into this should be a JEventEVIOBuffer. We skip the expensive
		// dynamic cast and assume it is.
		((JEventEVIOBuffer*)aEvent.get())->Process();

	};
	auto sPackagedTask = std::packaged_task<void(const std::shared_ptr<const JEvent>&)>(sParseBuffer);

	// Get the JTask, set it up, and return it
	auto sTask = mApplication->GetVoidTask(); //std::make_shared<JTask<void>>(aEvent, sPackagedTask);
	sTask->SetEvent(std::move(aEvent));
	sTask->SetTask(std::move(sPackagedTask));
	return std::static_pointer_cast<JTaskBase>(sTask);
}

//-----------------------------------
// GetJEventEVIOBufferFromPool
//-----------------------------------
JEventEVIOBuffer* JEventSource_EVIO::GetJEventEVIOBufferFromPool(void)
{
	// n.b. this is called only from GetEvent which JANA guarantees
	// will only be called by one thread at a time. No need for a lock
	// while accessing buff_pool. Multiple threads may call ReturnBufferToPool
	// though so we do need to use a lock if we need to access buff_pool_recycled.

	// Check if buff_pool is empty. If it is, swap everything from
	// buff_pool_recycled into buff_pool.
	if( buff_pool.empty() ){
		std::lock_guard<std::mutex> lck(buff_pool_recycled_mutex);
		buff_pool_recycled.swap( buff_pool );
	}
	
	// Check if buff_pool is still empty. If so, then we need to allocate
	// a new buffer. Otherwise grab one from the pool.
	JEventEVIOBuffer *evt = nullptr;
	if( buff_pool.empty() ){
		// n.b We rely on the external JANA mechanism to limit how
		// many events are simultaneously in memory and therefore
		// how big the buffer pool can grow.

		// Create new JEventEVIOBuffer object
		evt = new JEventEVIOBuffer(mApplication);

		// Get the JQueue where parsed events should be placed. This will be
		// part of the JQueueSet that the JThreadManager associated with this
		// event source.
		evt->mParsedQueue = mApplication->GetJThreadManager()->GetQueue(this, JQueueSet::JQueueType::Events, "Parsed");

	}else{
		evt = buff_pool.top();
		buff_pool.pop();
	}

	return evt;
}

//-----------------------------------
// ReturnJEventEVIOBufferToPool
//-----------------------------------
void JEventSource_EVIO::ReturnJEventEVIOBufferToPool( JEventEVIOBuffer *evt )
{
	// This is primarily called from the JEventEVIOBuffer destructor via
	// the lambda function passed into it when it was created. It may also
	// be called from GetEvent if there was a problem reading the event
	// and the attempt was aboted.

	std::lock_guard<std::mutex> lck(buff_pool_recycled_mutex);

	evt->Release();
	buff_pool_recycled.push( evt );
}



