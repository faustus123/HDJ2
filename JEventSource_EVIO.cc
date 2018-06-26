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

	// Get DEVIOWorker from pool. The DEVIOWorker object maintains
	// its own buffer that we will read the EVIO event into. It
	// also maintains its own pool of DParsedEvent objects.
	JEventEVIOBuffer *evt = GetJEventEVIOBufferFromPool();
	uint32_t* &buff     = evt->buff;
	uint32_t  &buff_len = evt->buff_len;

	bool swap_needed = false;
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

		uint32_t myjobtype = DEVIOWorker::JOB_FULL_PARSE;
		if(swap_needed && SWAP) myjobtype |= DEVIOWorker::JOB_SWAP;

		evioworker->jobtype = (DEVIOWorkerThread::JOBTYPE)myjobtype;
		evioworker->istreamorder = istreamorder++;


		// Create a JEventEVIOBuffer object to encapsulate the DEVIOWorker
		// object. Give it a lambda function to return the DEVIOWorker to
		// the out pool when it is evewntually destroyed.
		auto jevent = new JEventEVIOBuffer( evioworker, [this](DEVIOWorker *evioworker){this->ReturnEVIOWorkerToPool(evioworker);} );
		jevent->swap_needed = hdevio->swap_needed;

		// Return the JEvent as a shared_ptr
		return std::shared_ptr<JEvent>( (JEvent*)jevent );

	}else{
		// Problem reading in event
		ReturnBufferToPool( std::move(vbuff) );
		
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





	}else if(hdevio->err_code!=HDEVIO::HDEVIO_OK){
		if(LOOP_FOREVER && NEVENTS_PROCESSED>=1){
			if(hdevio){
				hdevio->rewind();
				continue;
			}
		}else{
			cout << hdevio->err_mess.str() << endl;
			if(hdevio->err_code != HDEVIO::HDEVIO_EOF){
				bool ignore_error = false;
				if( (!TREAT_TRUNCATED_AS_ERROR) && (hdevio->err_code == HDEVIO::HDEVIO_FILE_TRUNCATED) ) ignore_error = true;
				if(!ignore_error) japp->SetExitCode(hdevio->err_code);
			}
		}
		break;
	}else{
		// HDEVIO_OK
		swap_needed = hdevio->swap_needed;
	}








//	// Get buffer from pool
//	vector<uint32_t> &&vbuff = GetBufferFromPool();
	
	// Set buffer size to match its capacity here. We do this now since
	// the compiler could choose to initialize all of the "new" values,
	// overwriting anything read into the buffer below. We'll resize it
	// back down after a successful read.
	vbuff.resize(vbuff.capacity() );
	uint32_t *buff = vbuff.data();
	uint32_t buff_len = vbuff.size();

	// Try reading in EVIO buffer. If buffer is too small, then resize it
	// and try again.
	bool allow_swap = false;
	hdevio->readNoFileBuff(buff, buff_len, allow_swap);
	if(hdevio->err_code == HDEVIO::HDEVIO_USER_BUFFER_TOO_SMALL){
		buff_len = hdevio->last_event_len;
		vbuff.resize(buff_len);
		buff = vbuff.data();
		hdevio->readNoFileBuff(buff, buff_len, allow_swap);
	}

	// Check if read was successful
	if( hdevio->err_code==HDEVIO::HDEVIO_OK ){

		// HDEVIO_OK
		vbuff.resize(buff_len);  // shrink size back to actual amount of data

		// Create a JEvent object, passing ownership of the vbuff object data and a
		// lambda function so it can return it to us when the JEventEVIOBuffer is destroyed
		auto jevent = new JEventEVIOBuffer( std::move(vbuff), [this](vector<uint32_t> &&vbuff){this->ReturnBufferToPool(std::move(vbuff));} );
		jevent->swap_needed = hdevio->swap_needed;

		// Return the JEvent as a shared_ptr
		return std::shared_ptr<JEvent>( (JEvent*)jevent );

	}else{
		// Problem reading in event
		ReturnBufferToPool( std::move(vbuff) );
		
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
	// Create task to process events obtained from source via GetEvent.
	// i.e. JEventEVIOBuffer objects. The tasks will parse a single JEventEVIOBuffer
	// event and create one or more JEventEVIOParsed events from it. The parsed
	// events will be placed in the Parsed event queue with a task to run the
	// event processors.

	// Define function that will be executed by the task
	auto sParseBuffer = [](const std::shared_ptr<const JEvent>& aEvent) -> void
	{
		// The JEvent passed into this should be a JEventEVIOBuffer. We skip the expensive
		// dynamic cast and assume it is.
		(JEventEVIOBuffer*)(aEvent.get())->Process();

	};
	auto sPackagedTask = std::packaged_task<void(const std::shared_ptr<const JEvent>&)>(sParseBuffer);

	// Get the JTask, set it up, and return it
	auto sTask = aApplication->GetVoidTask(); //std::make_shared<JTask<void>>(aEvent, sPackagedTask);
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

		// Create new JEventEVIOBuffer object giving it a function so it
		// can be returned to our pool later.
	 	auto recycle_fn = [this](JEventEVIOBuffer *b){ this->ReturnBufferToPool(b); }
		evt = new JEventEVIOBuffer(this, recycle_fn);

		// Get the JQueue where parsed events should be placed. This will be
		// part of the JQueueSet that the JThreadManager associated with this
		// event source.
		evt->mParsedQueue = mApplication->GetJThreadManager()->GetQueue(this, JQueueSet::JQueueType:Events, "Parsed");

	}else{
		auto evt = buff_pool.top();
		buff_pool.pop();
	}

	return evt;
}

//-----------------------------------
// ReturnBufferToPool
//-----------------------------------
void JEventSource_EVIO::ReturnJEventEVIOBufferToPool( JEventEVIOBuffer *jeventeviobuffer )
{
	// This is primarily called from the JEventEVIOBuffer destructor via
	// the lambda function passed into it when it was created. It may also
	// be called from GetEvent if there was a problem reading the event
	// and the attempt was aboted.

	std::lock_guard<std::mutex> lck(buff_pool_recycled_mutex);

	buff_pool_recycled.emplace( std::move(jeventeviobuffer) );
}



