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
	// no need to worry about mutexes or locks.

	// Get buffer from pool
	vector<uint32_t> &&vbuff = GetBufferFromPool();
	
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
// GetBufferFromPool
//-----------------------------------
vector<uint32_t> && JEventSource_EVIO::GetBufferFromPool(void)
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
	if( buff_pool.empty() ){
		// n.b We rely on the external JANA mechanism to limit how
		// many events are simultaneously in memory and therefore
		// how big the buffer pool can grow.
		vector<uint32_t> v(4000); // preallocate 16kB as starting size
		return std::move( v );
	}else{
		auto v = buff_pool.top();
		buff_pool.pop();
		return std::move( v );
	}
}

//-----------------------------------
// ReturnBufferToPool
//-----------------------------------
void JEventSource_EVIO::ReturnBufferToPool( vector<uint32_t> &&vbuff )
{
	// This is primarily called from the JEventEVIOBuffer destructor via
	// the lambda function passed into it when it was created. It may also
	// be called from GetEvent if there was a problem reading the event
	// and the attempt was aboted.

	std::lock_guard<std::mutex> lck(buff_pool_recycled_mutex);

	buff_pool_recycled.emplace( std::move(vbuff) );
}

