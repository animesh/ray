/*
 	Ray
    Copyright (C) 2010, 2011  Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You have received a copy of the GNU General Public License
    along with this program (COPYING).  
	see <http://www.gnu.org/licenses/>


 	Funding:

Sébastien Boisvert has a scholarship from the Canadian Institutes of Health Research (Master's award: 200910MDR-215249-172830 and Doctoral award: 200902CGM-204212-172830).

*/

#ifndef _Library
#define _Library

#include<BufferedData.h>
#include<ExtensionData.h>
#include <OnDiskAllocator.h>
#include<common_functions.h>
#include<map>
#include<StaticVector.h>
#include<TimePrinter.h>
#include <ReadFetcher.h>
#include <VirtualCommunicator.h>
#include <Parameters.h>
#include <SeedingData.h>
#include <RingAllocator.h>
#include <LibraryWorker.h>
using namespace std;

class Library{
	
	int m_detectedDistances;
	vector<int> m_libraryIndexes;
	VirtualCommunicator*m_virtualCommunicator;
	BufferedData m_bufferedData;
	int m_ready;
	int m_rank;
	StaticVector*m_outbox;
	StaticVector*m_inbox;
	int*m_sequence_id;
	int*m_sequence_idInFile;
	ExtensionData*m_ed;
	RingAllocator*m_outboxAllocator;
	int m_size;
	TimePrinter*m_timePrinter;
	int*m_mode;
	int*m_master_mode;
	Parameters*m_parameters;
	int*m_fileId;
	SeedingData*m_seedingData;
	int m_libraryIterator;
	map<int,map<int,int> > m_libraryDistances;
	map<int,int>::iterator m_libraryIndex;
	bool m_libraryIndexInitiated;
	bool m_workerInitiated;
	bool m_initiatedIterator;
	uint64_t m_SEEDING_i;
	set<uint64_t> m_activeWorkers;
	set<uint64_t>::iterator m_activeWorkerIterator;
	int m_completedJobs;
	int m_maximumAliveWorkers;
	int m_maximumWorkers;

	map<uint64_t,LibraryWorker> m_aliveWorkers;
	bool m_communicatorWasTriggered;
	vector<uint64_t> m_workersDone;
	vector<uint64_t> m_waitingWorkers;
	vector<uint64_t> m_activeWorkersToRestore;

	OnDiskAllocator m_allocator;
	void updateStates();
public:
	Library();
	void allocateBuffers();
	void updateDistances();
	void setReadiness();
	int getRank();
	int getSize();
	void detectDistances();

	void sendLibraryDistances();

	void constructor(int m_rank,StaticVector*m_outbox,RingAllocator*m_outboxAllocator,
	int*m_sequence_id,int*m_sequence_idInFile,ExtensionData*m_ed,int m_size,
TimePrinter*m_timePrinter,int*m_mode,int*m_master_mode,
Parameters*m_parameters,int*m_fileId,SeedingData*m_seedingData,StaticVector*inbox,VirtualCommunicator*vc
);

};

#endif
