//
//    File: JEventSource_EVIO.h
// Created: Thu Jun  7 02:37:51 EDT 2018
// Creator: davidl (on Darwin harriet.jlab.org 15.6.0 i386)
//
// ------ Last repository commit info -----
// [ Date: ]
// [ Author: ]
// [ Source: ]
// [ Revision: ]
//
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
// Jefferson Science Associates LLC Copyright Notice:
// Copyright 251 2014 Jefferson Science Associates LLC All Rights Reserved. Redistribution
// and use in source and binary forms, with or without modification, are permitted as a
// licensed user provided that the following conditions are met:
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright notice, this
//    list of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
// 3. The name of the author may not be used to endorse or promote products derived
//    from this software without specific prior written permission.
// This material resulted from work developed under a United States Government Contract.
// The Government retains a paid-up, nonexclusive, irrevocable worldwide license in such
// copyrighted data to reproduce, distribute copies to the public, prepare derivative works,
// perform publicly and display publicly and to permit others to do so.
// THIS SOFTWARE IS PROVIDED BY JEFFERSON SCIENCE ASSOCIATES LLC "AS IS" AND ANY EXPRESS
// OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
// JEFFERSON SCIENCE ASSOCIATES, LLC OR THE U.S. GOVERNMENT BE LIABLE TO LICENSEE OR ANY
// THIRD PARTES FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
// OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
//
// Description:
//
// This class implements a source capable of reading EVIO formatted data.
// Parsing of the EVIO is done in multiple stages:
//
//  1. Read in a buffer (may contain multiple EVIO events)
//
//  2. Split buffer into individual EVIO blocks
//
//  3. Parse EVIO buffer into 1 or more events
//
//  The 1st stage must be done serially as it reads from the file(stream). The JANA
// framework will ensure that GetEvent is called by only a single thread at a
// time by use of an atomic lock. (See JEventSource::GetProcessEventTasks).
// Furthermore, it will call GetEvent multiple times in a row while holding
// the lock so that it does not need to be locked for every buffer that is read.
// The number of times is set via the SetNumEventsToGetAtOnce method of the
// JEventSource base class.
//
// The second stage should be very quick as it simply breaks the buffer up into
// multiple (or possibly only one) block. The block is wrapped in a JEventEVIOBlock
// and placed on a JQueue.
//
// The 3rd stage is where most of the hard work is done. Here, the data is actually
// parsed word by word, disentangling the events while creating a JEventEVIO for each
// and placing it on a JQueue.
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#ifndef _JEventSource_EVIO_h_
#define _JEventSource_EVIO_h_

#include <memory>
#include <utility>
#include <cstdint>
#include <mutex>
#include <deque>

#include <JANA/JApplication.h>
#include <JANA/JEventSource.h>
#include <JANA/JQueueWithBarriers.h>

#include <JEventEVIOBuffer.h>
#include <HDEVIO.h>


class  JEventSource_EVIO: public JEventSource{
	public:
	
		// Constructor must take string and JApplication pointer as arguments
		// and pass them into JEventSource constructor.
		JEventSource_EVIO(std::string source_name, JApplication *app);
		~JEventSource_EVIO();

		// This method will be called by JANA when the source should actually
		// be opened for reading.
		void Open(void);
	
		// A description of this source type must be provided as a static member
		static std::string GetDescription(void){ return "EVIO Event source"; }

		// This method is called to read in a single "event"
		std::shared_ptr<const JEvent> GetEvent(void);

		// This is called to generate a JTask capable of processing and event
		// returned by the GetEvent method above.
		std::shared_ptr<JTaskBase> GetProcessEventTask(std::shared_ptr<const JEvent>&& aEvent);

	protected:
		int                VERBOSE = 0;
		bool          LOOP_FOREVER = false;
		uint64_t NEVENTS_PROCESSED = 0;
		uint64_t      istreamorder = 0;
	
		HDEVIO *hdevio = nullptr;
		std::deque< JEventEVIOBuffer* > buff_pool;
		std::deque< JEventEVIOBuffer* > buff_pool_recycled;
		std::mutex buff_pool_recycled_mutex;
	
		JEventEVIOBuffer* GetJEventEVIOBufferFromPool(void);
		void ReturnJEventEVIOBufferToPool( JEventEVIOBuffer *jeventeviobuffer );
	
		JQueueInterface *mParsedQueue = nullptr;

	private:
		std::atomic<uint64_t> mNcallsGetEvent{0};

};

#endif // _JEventSource_EVIO_h_
