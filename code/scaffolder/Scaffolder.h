/*
 	Ray
    Copyright (C) 2011  Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You have received a copy of the GNU General Public License
    along with this program (gpl-3.0.txt).  
	see <http://www.gnu.org/licenses/>

*/

#ifndef _Scaffolder
#define _Scaffolder

#include <core/Parameters.h>
#include <structures/StaticVector.h>
#include <vector>
#include <memory/RingAllocator.h>
#include <core/constants.h>
#include <communication/VirtualCommunicator.h>
#include <assembler/ReadFetcher.h>
#include <sstream>
#include <scaffolder/ScaffoldingLink.h>
#include <scaffolder/SummarizedLink.h>
#include <scheduling/SwitchMan.h>
#include <profiling/TimePrinter.h>

#include <handlers/SlaveModeHandler.h>
#include <handlers/MasterModeHandler.h>
#include <plugins/CorePlugin.h>

#include <scaffolder/Scaffolder_adapters.h>

using namespace std;

/**
 * Scaffolder class, it uses MPI through the virtual communicator.
 *
 * \author Sébastien Boisvert
 */
class Scaffolder :  public CorePlugin{

	MessageTag RAY_MPI_TAG_SCAFFOLDING_LINKS_REPLY;
	MessageTag RAY_MPI_TAG_START_SCAFFOLDER;

	MessageTag RAY_MPI_TAG_REQUEST_VERTEX_READS;
	MessageTag RAY_MPI_TAG_CONTIG_INFO;
	MessageTag RAY_MPI_TAG_GET_CONTIG_CHUNK;
	MessageTag RAY_MPI_TAG_GET_COVERAGE_AND_DIRECTION;
	MessageTag RAY_MPI_TAG_GET_PATH_LENGTH;
	MessageTag RAY_MPI_TAG_GET_READ_MARKERS;
	MessageTag RAY_MPI_TAG_GET_READ_MATE;
	MessageTag RAY_MPI_TAG_GET_VERTEX_EDGES_COMPACT;
	MessageTag RAY_MPI_TAG_HAS_PAIRED_READ;
	MessageTag RAY_MPI_TAG_I_FINISHED_SCAFFOLDING;
	MessageTag RAY_MPI_TAG_SCAFFOLDING_LINKS;

	MasterMode RAY_MASTER_MODE_WRITE_SCAFFOLDS;

	SlaveMode RAY_SLAVE_MODE_DO_NOTHING;
	SlaveMode RAY_SLAVE_MODE_SCAFFOLDER;

	Adapter_RAY_MASTER_MODE_WRITE_SCAFFOLDS m_adapter_RAY_MASTER_MODE_WRITE_SCAFFOLDS;
	Adapter_RAY_SLAVE_MODE_SCAFFOLDER m_adapter_RAY_SLAVE_MODE_SCAFFOLDER;

	SwitchMan*m_switchMan;
	
	TimePrinter*m_timePrinter;

	int m_rankIdForContig;
	bool m_hasContigSequence_Initialised;
	FILE*m_fp;
	bool m_hasContigSequence;
	string m_contigSequence;
	map<uint64_t,int> m_contigLengths;
	int m_position;
	int m_theLength;
	vector<Kmer> m_contigPath;
	bool m_requestedContigChunk;

	vector<int> m_allScaffoldLengths;
	vector<int> m_allContigLengths;

	/* coverage values for the current path */
	vector<int> m_vertexCoverageValues;

	int m_scaffoldId;
	int m_positionOnScaffold;
	bool m_writeContigRequested;

	vector<vector<uint64_t> > m_scaffoldContigs;
	vector<vector<char> >m_scaffoldStrands;
	vector<vector<int> >m_scaffoldGaps;

	bool m_sentContigInfo;
	bool m_sentContigMeta;
	vector<uint64_t> m_masterContigs;
	vector<int> m_masterLengths;
	vector<SummarizedLink>m_masterLinks;
	int m_summaryIterator;
	bool m_summarySent;
	vector<SummarizedLink>m_summary;
	bool m_summaryPerformed;
	bool m_entrySent;
	map<uint64_t,map<char,map<uint64_t,map<char,vector<ScaffoldingLink> > > > > m_scaffoldingSummary;
	bool m_reverseDirectionsReceived;
	bool m_reverseDirectionLengthReceived;
	uint64_t m_pairedReverseDirectionName;
	int m_pairedReverseDirectionPosition;
	int m_pairedReverseMarkerCoverage;
	bool m_pairedReverseHasDirection;
	bool m_reverseDirectionLengthRequested;
	int m_pairedReverseDirectionLength;
	// these are mostly flags for sends/receives
	bool m_forwardDirectionLengthRequested;
	bool m_forwardDirectionLengthReceived;
	int m_pairedForwardDirectionLength;
	bool m_forwardDirectionsRequested;
	bool m_forwardDirectionsReceived;
	int m_pairedForwardMarkerCoverage;
	bool m_pairedForwardHasDirection;
	uint64_t m_pairedForwardDirectionName;
	int m_pairedForwardDirectionPosition;
	bool m_reverseDirectionsRequested;
	int m_readLength;
	bool m_markersRequested;
	bool m_markersReceived;
	int m_pairedForwardOffset;
	int m_pairedReverseOffset;
	Kmer m_pairedForwardMarker;
	Kmer m_pairedReverseMarker;
	bool m_pairReceived;
	int m_pairedReadRank;
	int m_pairedReadIndex;
	int m_pairedReadLibrary;
	int m_pairedReadLength;
	bool m_pairRequested;
	bool m_hasPairReceived;
	bool m_hasPairRequested;
	bool m_hasPair;
	bool m_initialisedFetcher;
	int m_readAnnotationId;
	ReadFetcher m_readFetcher;
	vector<uint64_t> m_activeWorkers;
	uint64_t m_workerId;
	VirtualCommunicator*m_virtualCommunicator;
	bool m_coverageRequested;
	bool m_coverageReceived;
	int m_receivedCoverage;

	bool m_reverseDone;
	bool m_forwardDone;
	int m_contigId;
	int m_positionOnContig;
	vector<vector<Kmer> >*m_contigs;
	vector<uint64_t>*m_contigNames;

	Parameters*m_parameters;
	StaticVector*m_inbox;
	StaticVector*m_outbox;
	RingAllocator*m_outboxAllocator;
	bool m_ready;

	/**
 *	gets a contig sequence by receiving several MPI messages
 */
	void getContigSequence(uint64_t id);
	void processContig();
	void processContigPosition();
	void processVertex(Kmer*vertex);
	void processAnnotations();
	void processAnnotation();
	void performSummary();
	void sendSummary();
	void sendContigInfo();
	void extractScaffolds(char state,map<uint64_t,int>*colors,uint64_t vertex,
		map<uint64_t,map<char,vector<vector<uint64_t> > > >*parents,
		map<uint64_t,map<char,vector<vector<uint64_t> > > >*children,set<int>*completedColours);


	void computeStatistics(vector<int>*lengths,int minimumLength,ostream*outputStream);
	void printInStream(ostream*outputStream);

public:

	bool m_initialised;
	/**
 *	Number of ranks that have finished scaffolding
 */
	int m_numberOfRanksFinished;

	/**
 *	Constructor of the scaffolder
 */
	void constructor(StaticVector*outbox,StaticVector*inbox,RingAllocator*outboxAllocator,Parameters*parameters,
		VirtualCommunicator*vc,SwitchMan*switchMan);
	void call_RAY_SLAVE_MODE_SCAFFOLDER();
	void setContigPaths(vector<uint64_t>*names,vector<vector<Kmer> >*paths);
	void addMasterLink(SummarizedLink*link);
	void solve();
	void addMasterContig(uint64_t name,int length);
	void call_RAY_MASTER_MODE_WRITE_SCAFFOLDS();
	void printFinalMessage();

	void setTimePrinter(TimePrinter*a);

	void registerPlugin(ComputeCore*core);
	void resolveSymbols(ComputeCore*core);
};

#endif

