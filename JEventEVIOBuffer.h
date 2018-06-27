//
//    File: JEventEVIOBuffer.h
// Created: Fri Jun  8 10:46:34 EDT 2018
// Creator: davidl (on Darwin amelia.jlab.org 17.5.0 i386)
//
// ------ Last repository commit info -----
// [ Date ]
// [ Author ]
// [ Source ]
// [ Revision ]
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
// This wraps a DEVIOWorker object containing an event or block of events in a JEvent
// object. This is to allow parsing to be done later by another thread so that
// the parsing may be done in parallel.
//
// There are two constructors here, but the second one is what is used to
// allow the buffer ("vbuff") to be recycled through a pool managed by
// the JEventSource_EVIO class. The recycle_fn is made from a lambda that
// will call the Recycle method of the specific JEvenSource_EVIO object that
// created this event. Doing it this way allows us to avoid requiring that the
// JEventSource_EVIO class be defined here.
//
// If the first form of the constructor is used (no arguments), then the memory
// used by vbuff will be created and destroyed with the JEvent object in the
// standard way.
//- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
#ifndef _JEventEVIOBuffer_h_
#define _JEventEVIOBuffer_h_

#include <atomic>
#include <list>
#include <iterator>

#include <JANA/JEvent.h>
using namespace std;

#include <HDEVIO.h>
#include <DParsedEvent.h>
#include <DModuleType.h>
#include <JQueueInterface.h>

class JEventEVIOBuffer:JEvent{
	public:
	
		enum JOBTYPE{
			JOB_NONE       = 0x0,
			JOB_QUIT       = 0x1,
			JOB_SWAP       = 0x2,
			JOB_FULL_PARSE = 0x4,
			JOB_ASSOCIATE  = 0x8
		};

		JEventEVIOBuffer(){}
		virtual ~JEventEVIOBuffer(){}

		void SetMaxParsedEvents(uint32_t max) { MAX_PARSED_EVENTS = max; }
		void SetROCIDsToParse(set<uint32_t> &rocids) { ROCIDS_TO_PARSE = rocids; }

		// These are owned by JEventSource and
		// are set in the constructor
		uint32_t            MAX_PARSED_EVENTS;
		set<uint32_t>       ROCIDS_TO_PARSE;
	
		// Pool of parsed events
		vector<DParsedEvent*> parsed_event_pool;
	
		// List of parsed events we are currently filling
		list<DParsedEvent*> current_parsed_events;

		// JQueue to place parsed events into
		JQueueInterface *mParsedQueue = nullptr;

		int VERBOSE;
		uint64_t Nrecycled;
		uint64_t MAX_EVENT_RECYCLES;
		uint64_t MAX_OBJECT_RECYCLES;
	
		atomic<bool> done;
		JOBTYPE jobtype;
		uint64_t istreamorder;
		uint64_t run_number_seed;

		uint32_t buff_len;
		uint32_t *buff;
		streampos pos;

		bool  PARSE_F250;
		bool  PARSE_F125;
		bool  PARSE_F1TDC;
		bool  PARSE_CAEN1290TDC;
		bool  PARSE_CONFIG;
		bool  PARSE_BOR;
		bool  PARSE_EPICS;
		bool  PARSE_EVENTTAG;
		bool  PARSE_TRIGGER;
	
		bool  LINK_TRIGGERTIME;
		bool  LINK_CONFIG;
	
		void Prune(void);
		void MakeEvents(void);
		void PublishEvents(void);
		void ParseBank(void);
	
		void      ParseEventTagBank(uint32_t* &iptr, uint32_t *iend);
		void         ParseEPICSbank(uint32_t* &iptr, uint32_t *iend);
		void           ParseBORbank(uint32_t* &iptr, uint32_t *iend);
		void      ParseTSscalerBank(uint32_t* &iptr, uint32_t *iend);
		void    Parsef250scalerBank(uint32_t rocid, uint32_t* &iptr, uint32_t *iend);
		void      ParseControlEvent(uint32_t* &iptr, uint32_t *iend);
		void       ParsePhysicsBank(uint32_t* &iptr, uint32_t *iend);
		void  ParseBuiltTriggerBank(uint32_t* &iptr, uint32_t *iend);
		void          ParseDataBank(uint32_t* &iptr, uint32_t *iend);
		void       ParseDVertexBank(uint32_t* &iptr, uint32_t *iend);
		void ParseDEventRFBunchBank(uint32_t* &iptr, uint32_t *iend);

		void        ParseJLabModuleData(uint32_t rocid, uint32_t* &iptr, uint32_t *iend);
		void                ParseTIBank(uint32_t rocid, uint32_t* &iptr, uint32_t *iend);
		void              ParseCAEN1190(uint32_t rocid, uint32_t* &iptr, uint32_t *iend);
		void   ParseModuleConfiguration(uint32_t rocid, uint32_t* &iptr, uint32_t *iend);
		void              Parsef250Bank(uint32_t rocid, uint32_t* &iptr, uint32_t *iend);
		void     MakeDf250WindowRawData(DParsedEvent *pe, uint32_t rocid, uint32_t slot, uint32_t itrigger, uint32_t* &iptr);
		void              Parsef125Bank(uint32_t rocid, uint32_t* &iptr, uint32_t *iend);
		void     MakeDf125WindowRawData(DParsedEvent *pe, uint32_t rocid, uint32_t slot, uint32_t itrigger, uint32_t* &iptr);
		void             ParseF1TDCBank(uint32_t rocid, uint32_t* &iptr, uint32_t *iend);

		void LinkAllAssociations(void);

		inline uint32_t F1TDC_channel(uint32_t chip, uint32_t chan_on_chip, int modtype);

		void DumpBinary(const uint32_t *iptr, const uint32_t *iend, uint32_t MaxWords=0, const uint32_t *imark=NULL);

};

//----------------
// F1TDC_channel
//----------------
inline uint32_t JEventEVIOBuffer::F1TDC_channel(uint32_t chip, uint32_t chan_on_chip, int modtype)
{
    /// Convert a F1TDC chip number and channel on the chip to the
    /// front panel channel number. This is based on "Input Channel Mapping"
    /// section at the very bottom of the document F1TDC_V2_V3_4_29_14.pdf

    uint32_t channel_map[8] = {0, 0, 1, 1, 2, 2, 3, 3};
    switch(modtype){
        case DModuleType::F1TDC32:
            return (4 * chip) + channel_map[ chan_on_chip&0x7 ];
        case DModuleType::F1TDC48:
            return (chip <<3) | chan_on_chip;
        default:
            _DBG_ << "Calling F1TDC_channel for module type: " << DModuleType::GetName((DModuleType::type_id_t)modtype) << endl;
            throw JException("F1TDC_channel called for non-F1TDC module type");
    }
    return 1000000; // (should never get here)
}


#endif // _JEventEVIOBuffer_h_


