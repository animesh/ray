/*
 	Ray
    Copyright (C) 2010  Sébastien Boisvert

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

#include"Machine.h"
#include"Message.h"
#include"common_functions.h"
#include<iostream>
#include<fstream>
#include"CoverageDistribution.h"
#include<string.h>
#include"SplayTreeIterator.h"
#include<mpi.h>
#include"Read.h"
#include"Loader.h"
#include"MyAllocator.h"
using namespace std;

Machine::Machine(int argc,char**argv){
	m_USE_MPI_Send=0;
	m_USE_MPI_Isend=1;
	m_Sending_Mechanism=m_USE_MPI_Send;

	m_sentMessages=0;
	m_readyToSeed=0;
	m_ticks=0;
	m_receivedMessages=0;
	m_wordSize=21;
	m_reverseComplementVertex=false;
	m_last_value=0;
	m_mode_send_ingoing_edges=false;
	m_mode_send_edge_sequence_id_position=0;
	m_mode_send_vertices=false;
	m_mode_sendDistribution=false;
	m_mode_send_outgoing_edges=false;
	m_mode_send_vertices_sequence_id=0;
	m_mode_send_vertices_sequence_id_position=0;
	m_reverseComplementEdge=false;

	m_numberOfMachinesDoneSendingVertices=0;
	m_numberOfMachinesDoneSendingEdges=0;
	m_numberOfMachinesReadyToSendDistribution=0;
	m_numberOfMachinesDoneSendingCoverage=0;
	m_machineRank=0;
	m_messageSentForVerticesDistribution=false;
	MASTER_RANK=0;
	m_sequence_ready_machines=0;

	m_TAG_WELCOME=0;
	m_TAG_SEND_SEQUENCE=1;
	m_TAG_SEQUENCES_READY=2;
	m_TAG_MASTER_IS_DONE_SENDING_ITS_SEQUENCES_TO_OTHERS=3;
	m_TAG_VERTICES_DATA=4;
	m_TAG_VERTICES_DISTRIBUTED=5;
	m_TAG_VERTEX_PTR_REQUEST=6;
	m_TAG_OUT_EDGE_DATA_WITH_PTR=7;
	m_TAG_OUT_EDGES_DATA=8;
	m_TAG_SHOW_VERTICES=9;
	m_TAG_START_VERTICES_DISTRIBUTION=10;
	m_TAG_EDGES_DISTRIBUTED=11;
	m_TAG_IN_EDGES_DATA=12;
	m_TAG_IN_EDGE_DATA_WITH_PTR=13;
	m_TAG_START_EDGES_DISTRIBUTION=14;
	m_TAG_START_EDGES_DISTRIBUTION_ASK=15;
	m_TAG_START_EDGES_DISTRIBUTION_ANSWER=16;
	m_TAG_PREPARE_COVERAGE_DISTRIBUTION_QUESTION=17;
	m_TAG_PREPARE_COVERAGE_DISTRIBUTION_ANSWER=18;
	m_TAG_PREPARE_COVERAGE_DISTRIBUTION=19;
	m_TAG_COVERAGE_DATA=20;
	m_TAG_COVERAGE_END=21;
	m_TAG_SEND_COVERAGE_VALUES=22;
	m_TAG_READY_TO_SEED=23;
	m_TAG_START_SEEDING=24;
	m_TAG_REQUEST_VERTEX_COVERAGE=25;
	m_TAG_REQUEST_VERTEX_COVERAGE_REPLY=26;
	m_TAG_REQUEST_VERTEX_OUTGOING_EDGES=27;
	m_TAG_REQUEST_VERTEX_KEY_AND_COVERAGE=28;
	m_TAG_REQUEST_VERTEX_OUTGOING_EDGES_REPLY=29;
	m_TAG_REQUEST_VERTEX_KEY_AND_COVERAGE_REPLY=30;
	m_TAG_REQUEST_VERTEX_OUTGOING_EDGES=31;
	m_TAG_REQUEST_VERTEX_OUTGOING_EDGES_REPLY=32;
	m_TAG_REQUEST_VERTEX_KEY_AND_COVERAGE=33;
	m_TAG_REQUEST_VERTEX_KEY_AND_COVERAGE_REPLY=34;
	m_TAG_SEEDING_IS_OVER=35;
	m_TAG_GOOD_JOB_SEE_YOU_SOON=36;
	m_TAG_I_GO_NOW=37;

	m_MODE_START_SEEDING=0;
	m_MODE_DO_NOTHING=1;

	m_mode=m_MODE_DO_NOTHING;

	MPI_Init(&argc,&argv);
	MPI_Comm_rank(MPI_COMM_WORLD,&m_rank);
	MPI_Comm_size(MPI_COMM_WORLD,&m_size);
	MPI_Get_processor_name (m_name, &m_nameLen); 
	m_alive=true;
	m_welcomeStep=true;
	m_loadSequenceStep=false;
	m_inputFile=argv[1];
	m_vertices_sent=0;
	m_totalLetters=0;
	m_distribution_file_id=m_distribution_sequence_id=m_distribution_currentSequenceId=0;
	if(argc!=2){
		if(isMaster()){
			cout<<"You must provide a input file."<<endl;
		}
		return ;
	}
	run();
	MPI_Barrier(MPI_COMM_WORLD);
	cout<<"Rank "<<getRank()<<" finalizes."<<endl;
	MPI_Finalize();
}

void Machine::run(){
	cout<<"Rank "<<getRank()<<" is "<<m_name<<endl;
	while(isAlive()){
		if(m_ticks%100==0){
			MPI_Barrier(MPI_COMM_WORLD);
		}

		m_ticks++;
		receiveMessages(); 
		checkRequests();
		processMessages();
		processData();
		sendMessages();
	}
}

void Machine::sendMessages(){
	for(int i=0;i<(int)m_outbox.size();i++){
		Message*aMessage=&(m_outbox[i]);
		if(aMessage->getDestination()==getRank()){
			int sizeOfElements=8;
			if(aMessage->getTag()==m_TAG_SEND_SEQUENCE){
				sizeOfElements=1;
			}
			void*newBuffer=m_inboxAllocator.allocate(sizeOfElements*aMessage->getCount());
			memcpy(newBuffer,aMessage->getBuffer(),sizeOfElements*aMessage->getCount());
			aMessage->setBuffer(newBuffer);
			m_inbox.push_back(*aMessage);
			m_receivedMessages++;
		}else{
			if(m_Sending_Mechanism==m_USE_MPI_Send){
				MPI_Send(aMessage->getBuffer(), aMessage->getCount(), aMessage->getMPIDatatype(),aMessage->getDestination(),aMessage->getTag(), MPI_COMM_WORLD);
			}else if(m_Sending_Mechanism==m_USE_MPI_Isend){
				MPI_Request request;
				MPI_Status status;
				int flag;
				MPI_Test(&request,&flag,&status);
				if(!flag){
					m_pendingMpiRequest.push_back(request);
				}
			}
		}
		m_sentMessages++;
	}
	m_outbox.clear();
	if(m_outboxAllocator.getNumberOfChunks()>1){
		m_outboxAllocator.clear();
		m_outboxAllocator.constructor();
	}
}

void Machine::checkRequests(){
	if(m_Sending_Mechanism==m_USE_MPI_Send)
		return;
	MPI_Request theRequests[1024];
	MPI_Status theStatus[1024];
	
	for(int i=0;i<(int)m_pendingMpiRequest.size();i++){
		theRequests[i]=m_pendingMpiRequest[i];
	}
	MPI_Waitall(m_pendingMpiRequest.size(),theRequests,theStatus);
	m_pendingMpiRequest.clear();
}

void Machine::receiveMessages(){
	int numberOfMessages=0;
	int flag;
	MPI_Status status;
	MPI_Iprobe(MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&flag,&status);
	while(flag){
		if(numberOfMessages>10)
			break;
		MPI_Datatype datatype=MPI_UNSIGNED_LONG_LONG;
		int sizeOfType=8;
		int tag=status.MPI_TAG;
		if(tag==m_TAG_SEND_SEQUENCE){
			datatype=MPI_BYTE;
			sizeOfType=1;
		}

		int source=status.MPI_SOURCE;
		int length;
		MPI_Get_count(&status,datatype,&length);
		void*incoming=(void*)m_inboxAllocator.allocate(length*sizeOfType);
		MPI_Status status2;
		MPI_Recv(incoming,length,datatype,source,tag,MPI_COMM_WORLD,&status2);
		Message aMessage(incoming,length,datatype,source,tag,source);
		m_inbox.push_back(aMessage);
		m_receivedMessages++;
		numberOfMessages++;
		MPI_Iprobe(MPI_ANY_SOURCE,MPI_ANY_TAG,MPI_COMM_WORLD,&flag,&status);
	}

	/*
	if(numberOfMessages>10){
		cout<<"Rank "<<getRank()<<" received "<<numberOfMessages<<" messages."<<endl;
	}
	*/
}



int Machine::getRank(){
	return m_rank;
}

void Machine::loadSequences(){
	vector<string> allFiles=m_parameters.getAllFiles();
	if(m_distribution_reads.size()>0 and m_distribution_sequence_id>(int)m_distribution_reads.size()-1){
		m_distribution_file_id++;
		m_distribution_sequence_id=0;
		m_distribution_reads.clear();
	}
	if(m_distribution_file_id>(int)allFiles.size()-1){
		m_loadSequenceStep=true;
		for(int i=0;i<getSize();i++){
			char*message=m_name;
			Message aMessage(message, 0, MPI_UNSIGNED_LONG_LONG, i, m_TAG_MASTER_IS_DONE_SENDING_ITS_SEQUENCES_TO_OTHERS,getRank());
			m_outbox.push_back(aMessage);
		}
		m_distribution_reads.clear();
		m_distributionAllocator.clear();
		return;
	}
	if(m_distribution_reads.size()==0){
		Loader loader;
		m_distribution_reads.clear();
		m_distributionAllocator.clear();
		m_distributionAllocator.constructor();
		m_distributionAllocator.clear();
		m_distributionAllocator.constructor();
		loader.load(allFiles[m_distribution_file_id],&m_distribution_reads,&m_distributionAllocator,&m_distributionAllocator);
		cout<<m_distribution_reads.size()<<" sequences to distribute"<<endl;
	}
	for(int i=0;i<1*getSize();i++){
		if(m_distribution_sequence_id>(int)m_distribution_reads.size()-1)
			break;

		int destination=m_distribution_currentSequenceId%getSize();

		if(destination<0 or destination>getSize()-1){
			cout<<destination<<" is bad"<<endl;
		}
		char*sequence=m_distribution_reads[m_distribution_sequence_id]->getSeq();
		if(false and destination==MASTER_RANK){

		}else{
			Message aMessage(sequence, strlen(sequence), MPI_BYTE, destination, m_TAG_SEND_SEQUENCE,getRank());
			m_outbox.push_back(aMessage);
		}
		m_distribution_currentSequenceId++;
		m_distribution_sequence_id++;
	}
}

void Machine::processMessage(Message*message){
	void*buffer=message->getBuffer();
	int count=message->getCount();
	int tag=message->getTag();
	int source=message->getSource();


	if(tag==m_TAG_VERTICES_DATA){
		int length=count;
		uint64_t*incoming=(uint64_t*)buffer;
		//cout<<"Length="<<length<<endl;
		for(int i=0;i<length;i++){
			uint64_t l=incoming[i];
			if(m_last_value!=m_subgraph.size() and m_subgraph.size()%100000==0){
				m_last_value=m_subgraph.size();
				cout<<"Rank "<<getRank()<<" has "<<m_subgraph.size()<<" vertices "<<endl;
			}
			SplayNode<uint64_t,Vertex>*tmp=m_subgraph.insert(l);
		
			if(m_subgraph.inserted()){
				tmp->getValue()->constructor(); 
			}
			tmp->getValue()->setCoverage(tmp->getValue()->getCoverage()+1);
		}
	}else if(tag==m_TAG_START_SEEDING){
		m_mode=m_MODE_START_SEEDING;
		m_SEEDING_iterator=new SplayTreeIterator<uint64_t,Vertex>(&m_subgraph);
		m_SEEDING_NodeInitiated=false;
	}else if(tag==m_TAG_REQUEST_VERTEX_COVERAGE){
		uint64_t*incoming=(uint64_t*)buffer;
		SplayNode<uint64_t,Vertex>*node=(SplayNode<uint64_t,Vertex>*)incoming[0];
		uint64_t coverage=node->getValue()->getCoverage();
		//cout<<"Rank "<<getRank()<<" ptr "<<node<<" resolves. "<<node->getKey()<<" "<<coverage<<endl;
		uint64_t*message=(uint64_t*)m_outboxAllocator.allocate(1*sizeof(uint64_t));
		message[0]=coverage;
		Message aMessage(message,1,MPI_UNSIGNED_LONG_LONG,source,m_TAG_REQUEST_VERTEX_COVERAGE_REPLY,getRank());
		m_outbox.push_back(aMessage);
	}else if(tag==m_TAG_REQUEST_VERTEX_OUTGOING_EDGES){
		uint64_t*incoming=(uint64_t*)buffer;
		SplayNode<uint64_t,Vertex>*node=(SplayNode<uint64_t,Vertex>*)incoming[0];
		//cout<<"Rank "<<getRank()<<" prepares outgoing edges, pointer="<<node<<"."<<endl;
		int c=0;
		if(node==NULL){
			cout<<"Invalid pointer."<<endl;
		}
			
		Edge*e=node->getValue()->getFirstOutgoingEdge();
		while(e!=NULL){
			c++;
			e=e->getNext();
		}
		uint64_t*message=(uint64_t*)m_outboxAllocator.allocate(c*2*sizeof(uint64_t));
		int i=0;
		e=node->getValue()->getFirstOutgoingEdge();
		while(e!=NULL){
			message[i]=e->getRank();
			i++;
			message[i]=(uint64_t)e->getPtr();
			++i;
			e=e->getNext();
		}
		Message aMessage(message,c*2,MPI_UNSIGNED_LONG_LONG,source,m_TAG_REQUEST_VERTEX_OUTGOING_EDGES_REPLY,getRank());
		m_outbox.push_back(aMessage);
		//cout<<"Rank "<<getRank()<<" sends outgoing edges."<<endl;
	}else if(tag==m_TAG_GOOD_JOB_SEE_YOU_SOON){
		m_alive=false;
	}else if(tag==m_TAG_SEEDING_IS_OVER){
		m_numberOfRanksDoneSeeding++;
	}else if(tag==m_TAG_REQUEST_VERTEX_OUTGOING_EDGES_REPLY){
		uint64_t*incoming=(uint64_t*)buffer;
		int i=0;
		m_SEEDING_edge=NULL;
		while(i<count){
			Edge*edge=(Edge*)m_seedingAllocator.allocate(sizeof(Edge));
			int rank=incoming[i];
			i++;
			void*ptr=(void*)incoming[i];
			++i;
			edge->constructor(rank,ptr);
			if(m_SEEDING_edge==NULL){
			}else{
				edge->setNext(m_SEEDING_edge);
			}
			m_SEEDING_edge=edge;
		}
		m_SEEDING_edgesReceived=true;
		m_SEEDING_edge_initiated=true;
	}else if(tag==m_TAG_REQUEST_VERTEX_KEY_AND_COVERAGE){
		uint64_t*incoming=(uint64_t*)buffer;
		SplayNode<uint64_t,Vertex>*node=(SplayNode<uint64_t,Vertex>*)incoming[0];
		uint64_t key=node->getKey();
		uint64_t coverage=node->getValue()->getCoverage();
		//cout<<"Rank "<<getRank()<<" ptr "<<node<<" resolves. "<<node->getKey()<<" "<<coverage<<endl;
		uint64_t*message=(uint64_t*)m_outboxAllocator.allocate(2*sizeof(uint64_t));
		message[0]=key;
		message[1]=coverage;
		Message aMessage(message,2,MPI_UNSIGNED_LONG_LONG,source,m_TAG_REQUEST_VERTEX_KEY_AND_COVERAGE_REPLY,getRank());
		m_outbox.push_back(aMessage);
	}else if(tag==m_TAG_REQUEST_VERTEX_KEY_AND_COVERAGE_REPLY){
		uint64_t*incoming=(uint64_t*)buffer;
		m_SEEDING_receivedKey=incoming[0];
		m_SEEDING_receivedVertexCoverage=incoming[1];
		m_SEEDING_vertexKeyAndCoverageReceived=true;
	}else if(tag==m_TAG_REQUEST_VERTEX_COVERAGE_REPLY){
		uint64_t*incoming=(uint64_t*)buffer;
		m_SEEDING_receivedVertexCoverage=incoming[0];
		m_SEEDING_vertexCoverageReceived=true;
		//cout<<"Coverage is "<<m_SEEDING_receivedVertexCoverage<<endl;
	// receive coverage data, and merge it
	}else if(tag==m_TAG_COVERAGE_DATA){
		int length=count;
		uint64_t*incoming=(uint64_t*)buffer;

		for(int i=0;i<length;i+=2){
			int coverage=incoming[i+0];
			uint64_t count=incoming[i+1];
			//cout<<coverage<<" "<<count<<endl;
			m_coverageDistribution[coverage]+=count;
		}
	// receive an outgoing edge in respect to prefix, along with the pointer for suffix
	}else if(tag==m_TAG_OUT_EDGE_DATA_WITH_PTR){
		uint64_t*incoming=(uint64_t*)buffer;
		void*ptr=(void*)incoming[2];
		uint64_t prefix=incoming[0];
		uint64_t suffix=incoming[1];
		int rank=vertexRank(suffix);
		
		SplayNode<uint64_t,Vertex>*node=m_subgraph.find(prefix);
		if(node==NULL){
			cout<<"NULL "<<prefix<<" Rank="<<getRank()<<endl;
		}else{
			Vertex*vertex=node->getValue();
			vertex->addOutgoingEdge(rank,ptr,&m_persistentAllocator);
		}
	// receive an ingoing edge in respect to prefix, along with the pointer for suffix
	}else if(tag==m_TAG_IN_EDGE_DATA_WITH_PTR){
		uint64_t*incoming=(uint64_t*)buffer;
		uint64_t prefix=incoming[0];
		uint64_t suffix=incoming[1];
		void*ptr=(void*)incoming[2];
		int rank=vertexRank(prefix);


		SplayNode<uint64_t,Vertex>*node=m_subgraph.find(suffix);
		if(node==NULL){
			cout<<"NULL "<<prefix<<endl;
		}else{
			Vertex*vertex=node->getValue();
			vertex->addIngoingEdge(rank,ptr,&m_persistentAllocator);
		}
	}else if(tag==m_TAG_SEND_COVERAGE_VALUES){
		uint64_t*incoming=(uint64_t*)buffer;
		m_minimumCoverage=incoming[0];
		m_seedCoverage=incoming[1];
		m_peakCoverage=incoming[2];
		Message aMessage(m_name,NULL,MPI_UNSIGNED_LONG_LONG,MASTER_RANK,m_TAG_READY_TO_SEED,getRank());
		m_outbox.push_back(aMessage);
	}else if(tag==m_TAG_READY_TO_SEED){
		m_readyToSeed++;
	// receive a out edge, send it back with the pointer
	}else if(tag==m_TAG_OUT_EDGES_DATA){
		int length=count;
		uint64_t*incoming=(uint64_t*)buffer;
		

		for(int i=0;i<(int)length;i+=2){
			int currentLength=3;
			uint64_t*sendBuffer=(uint64_t*)m_outboxAllocator.allocate(currentLength*sizeof(uint64_t));
			sendBuffer[0]=incoming[i+0];
			sendBuffer[1]=incoming[i+1];
			sendBuffer[2]=(uint64_t)m_subgraph.find(incoming[i+1]);
			int destination=vertexRank(incoming[i+0]);


			Message aMessage(sendBuffer,currentLength,MPI_UNSIGNED_LONG_LONG,destination,m_TAG_OUT_EDGE_DATA_WITH_PTR,getRank());
			m_outbox.push_back(aMessage);
		}

	// receive an ingoing edge, send it back with the pointer
	}else if(tag==m_TAG_IN_EDGES_DATA){
		int length=count;
		uint64_t*incoming=(uint64_t*)buffer;

		for(int i=0;i<(int)length;i+=2){
			int currentLength=3;
			uint64_t*sendBuffer=(uint64_t*)m_outboxAllocator.allocate(currentLength*sizeof(uint64_t));
			sendBuffer[0]=incoming[i+0];
			sendBuffer[1]=incoming[i+1];
			sendBuffer[2]=(uint64_t)m_subgraph.find(incoming[i+0]);
			
			int destination=vertexRank(incoming[i+1]);

			Message aMessage(sendBuffer,currentLength,MPI_UNSIGNED_LONG_LONG,destination,m_TAG_IN_EDGE_DATA_WITH_PTR,getRank());
			m_outbox.push_back(aMessage);
		}
	}else if(tag==m_TAG_WELCOME){

	}else if(tag==m_TAG_SEND_SEQUENCE){
		int length=count;
		char incoming[2000];
		for(int i=0;i<(int)length;i++)
			incoming[i]=((char*)buffer)[i];

		incoming[length]='\0';
		Read*myRead=(Read*)m_persistentAllocator.allocate(sizeof(Read));
		myRead->copy(NULL,incoming,&m_persistentAllocator);
		m_myReads.push_back(myRead);
		if(m_myReads.size()%10000==0){
			cout<<"Rank "<<getRank()<<" has "<<m_myReads.size()<<" sequences"<<endl;
		}
	}else if(tag==m_TAG_MASTER_IS_DONE_SENDING_ITS_SEQUENCES_TO_OTHERS){
		cout<<"Rank "<<getRank()<<" has "<<m_myReads.size()<<" sequences"<<endl;
		char*message=m_name;
		Message aMessage(message,0,MPI_UNSIGNED_LONG_LONG,source,m_TAG_SEQUENCES_READY,getRank());
		m_outbox.push_back(aMessage);
	}else if(tag==m_TAG_SHOW_VERTICES){
		cout<<"Rank "<<getRank()<<" has "<<m_subgraph.size()<<" vertices (DONE)"<<endl;
	}else if(tag==m_TAG_SEQUENCES_READY){
		m_sequence_ready_machines++;
	}else if(tag==m_TAG_START_EDGES_DISTRIBUTION_ANSWER){
		m_numberOfMachinesReadyForEdgesDistribution++;
	}else if(tag==m_TAG_PREPARE_COVERAGE_DISTRIBUTION_ANSWER){
		m_numberOfMachinesReadyToSendDistribution++;
	}else if(tag==m_TAG_PREPARE_COVERAGE_DISTRIBUTION){
		//cout<<"Rank "<<getRank()<<" prepares its distribution."<<endl;
		m_mode_send_coverage_iterator=0;
		m_mode_sendDistribution=true;
	}else if(tag==m_TAG_START_EDGES_DISTRIBUTION_ASK){
		char*message=m_name;
		Message aMessage(message, 0, MPI_UNSIGNED_LONG_LONG, source, m_TAG_START_EDGES_DISTRIBUTION_ANSWER,getRank());
		m_outbox.push_back(aMessage);
	}else if(tag==m_TAG_PREPARE_COVERAGE_DISTRIBUTION_QUESTION){
		char*message=m_name;
		Message aMessage(message, 0, MPI_UNSIGNED_LONG_LONG, source, m_TAG_PREPARE_COVERAGE_DISTRIBUTION_ANSWER,getRank());
		m_outbox.push_back(aMessage);
	}else if(tag==m_TAG_START_EDGES_DISTRIBUTION){
		m_mode_send_outgoing_edges=true;
		m_mode_send_edge_sequence_id=0;
	}else if(tag==m_TAG_START_VERTICES_DISTRIBUTION){
		m_mode_send_vertices=true;
		m_mode_send_vertices_sequence_id=0;
	}else if(tag==m_TAG_VERTICES_DISTRIBUTED){
		m_numberOfMachinesDoneSendingVertices++;
	}else if(tag==m_TAG_COVERAGE_END){
		m_numberOfMachinesDoneSendingCoverage++;
		//cout<<"m_numberOfMachinesDoneSendingCoverage="<<m_numberOfMachinesDoneSendingCoverage<<" "<<source<<" is done"<<endl;
	}else if(tag==m_TAG_EDGES_DISTRIBUTED){
		m_numberOfMachinesDoneSendingEdges++;
	}else{
		cout<<"Unknown tag"<<tag<<endl;
	}
}

void Machine::processMessages(){
	for(int i=0;i<(int)m_inbox.size();i++){
		processMessage(&(m_inbox[i]));
	}
	m_inbox.clear();
	if(m_inboxAllocator.getNumberOfChunks()>1){
		m_inboxAllocator.clear();
		m_inboxAllocator.constructor();
	}
}

void Machine::processData(){
	if(!m_parameters.isInitiated()&&isMaster()){
		m_parameters.load(m_inputFile);
	}else if(m_welcomeStep==true && m_loadSequenceStep==false&&isMaster()){
		loadSequences();
	}else if(m_loadSequenceStep==true && m_mode_send_vertices==false&&isMaster() and m_sequence_ready_machines==getSize()&&m_messageSentForVerticesDistribution==false){
		cout<<"Rank "<<getRank()<<" tells others to start vertices distribution"<<endl;
		for(int i=0;i<getSize();i++){
			char*sequence=m_name;
			Message aMessage(sequence, 0, MPI_UNSIGNED_LONG_LONG,i, m_TAG_START_VERTICES_DISTRIBUTION,getRank());
			m_outbox.push_back(aMessage);
		}
		m_messageSentForVerticesDistribution=true;
	}else if(m_numberOfMachinesDoneSendingVertices==getSize()){
		m_numberOfMachinesReadyForEdgesDistribution=0;
		for(int i=0;i<getSize();i++){
			char*sequence=m_name;
			Message aMessage(sequence, 0, MPI_UNSIGNED_LONG_LONG,i, m_TAG_START_EDGES_DISTRIBUTION_ASK,getRank());
			m_outbox.push_back(aMessage);
		}
		m_numberOfMachinesDoneSendingVertices=-1;
	}else if(m_numberOfMachinesReadyForEdgesDistribution==getSize()){
		cout<<"Rank "<<getRank()<<" tells others to start edges distribution"<<endl;
		m_numberOfMachinesReadyForEdgesDistribution=-1;
		for(int i=0;i<getSize();i++){
			char*sequence=m_name;
			Message aMessage(sequence, 0, MPI_UNSIGNED_LONG_LONG,i, m_TAG_START_EDGES_DISTRIBUTION,getRank());
			m_outbox.push_back(aMessage);
		}
	}else if(m_numberOfMachinesDoneSendingCoverage==getSize()){
		m_numberOfMachinesDoneSendingCoverage=-1;
		CoverageDistribution distribution(m_coverageDistribution,m_parameters.getDirectory());
		m_minimumCoverage=distribution.getMinimumCoverage();
		m_peakCoverage=distribution.getPeakCoverage();
		m_seedCoverage=(m_minimumCoverage+m_peakCoverage)/2;
		cout<<"Rank "<<getRank()<<" MinimumCoverage = "<<m_minimumCoverage<<endl;
		cout<<"Rank "<<getRank()<<" PeakCoverage = "<<m_peakCoverage<<endl;
		cout<<"Rank "<<getRank()<<" SeedCoverage = "<<m_seedCoverage<<endl;

		// see these values to everyone.
		uint64_t*buffer=(uint64_t*)m_outboxAllocator.allocate(3*sizeof(uint64_t));
		buffer[0]=m_minimumCoverage;
		buffer[1]=m_seedCoverage;
		buffer[2]=m_peakCoverage;
		for(int i=0;i<getSize();i++){
			Message aMessage(buffer,3,MPI_UNSIGNED_LONG_LONG,i,m_TAG_SEND_COVERAGE_VALUES,getRank());
			m_outbox.push_back(aMessage);
		}
	}

	if(m_mode_send_vertices==true){
		if(m_mode_send_vertices_sequence_id%10000==0 and m_mode_send_vertices_sequence_id_position==0){
			string reverse="";
			if(m_reverseComplementVertex==true)
				reverse="(reverse complement) ";
			cout<<"Rank "<<getRank()<<" is extracting vertices "<<reverse<<"from sequences "<<m_mode_send_vertices_sequence_id<<"/"<<m_myReads.size()<<endl;
		}

		if(m_mode_send_vertices_sequence_id>(int)m_myReads.size()-1){
			if(m_reverseComplementVertex==false){
				cout<<"Rank "<<getRank()<<" is extracting vertices from sequences "<<m_mode_send_vertices_sequence_id<<"/"<<m_myReads.size()<<" (DONE)"<<endl;
				m_mode_send_vertices_sequence_id=0;
				m_mode_send_vertices_sequence_id_position=0;
				m_reverseComplementVertex=true;
			}else{
				char*message=m_name;
				Message aMessage(message, 0, MPI_UNSIGNED_LONG_LONG, MASTER_RANK, m_TAG_VERTICES_DISTRIBUTED,getRank());
				m_outbox.push_back(aMessage);
				m_mode_send_vertices=false;
				cout<<"Rank "<<getRank()<<" is extracting vertices (reverse complement) from sequences "<<m_mode_send_vertices_sequence_id<<"/"<<m_myReads.size()<<" (DONE)"<<endl;
			}
		}else{
			char*readSequence=m_myReads[m_mode_send_vertices_sequence_id]->getSeq();
			int len=strlen(readSequence);
			char memory[100];
			int lll=len-m_wordSize;
	
			map<int,vector<uint64_t> > messagesStock;
			for(int p=m_mode_send_vertices_sequence_id_position;p<=m_mode_send_vertices_sequence_id_position;p++){
				memcpy(memory,readSequence+p,m_wordSize);
				memory[m_wordSize]='\0';
				if(isValidDNA(memory)){
					VertexMer a=wordId(memory);
					if(m_reverseComplementVertex==false){
						messagesStock[vertexRank(a)].push_back(a);
					}else{
						VertexMer b=complementVertex(a,m_wordSize);
						messagesStock[vertexRank(b)].push_back(b);
					}
				}
			}
			m_mode_send_vertices_sequence_id_position++;
			// send messages
			for(map<int,vector<uint64_t> >::iterator i=messagesStock.begin();i!=messagesStock.end();i++){
				int destination=i->first;
				int length=i->second.size();
				uint64_t *data=(uint64_t*)m_outboxAllocator.allocate(sizeof(uint64_t)*length);
				for(int j=0;j<(int)i->second.size();j++){
					data[j]=i->second[j];
				}
				
				
				Message aMessage(data, length, MPI_UNSIGNED_LONG_LONG,destination, m_TAG_VERTICES_DATA,getRank());
				m_outbox.push_back(aMessage);
				m_vertices_sent+=length;
			}

			if(m_mode_send_vertices_sequence_id_position>lll){
				m_mode_send_vertices_sequence_id++;
				m_mode_send_vertices_sequence_id_position=0;
			}
		}


	}else if(m_numberOfMachinesDoneSendingVertices==getSize()){
		cout<<"Now showing everyone."<<endl;
		m_numberOfMachinesDoneSendingVertices=0;
		for(int i=0;i<getSize();i++){
			char*message=m_name;
			Message aMessage(message, 0, MPI_UNSIGNED_LONG_LONG, i, m_TAG_SHOW_VERTICES,getRank());
			m_outbox.push_back(aMessage);
		}
		
	}else if(m_numberOfMachinesDoneSendingEdges==getSize()){
		m_numberOfMachinesDoneSendingEdges=-1;
		cout<<"Rank "<<getRank()<<" says that edges are distributed."<<endl;
		for(int i=0;i<getSize();i++){
			char*message=m_name;
			Message aMessage(message, 0, MPI_UNSIGNED_LONG_LONG, i, m_TAG_PREPARE_COVERAGE_DISTRIBUTION_QUESTION,getRank());
			m_outbox.push_back(aMessage);
		}
	}else if(m_numberOfMachinesReadyToSendDistribution==getSize()){
		//m_numberOfMachinesReadyToSendDistribution=-1;

		if(m_machineRank<=m_numberOfMachinesDoneSendingCoverage){
			//cout<<"Rank "<<getRank()<<" tells "<<m_machineRank<<" to distribute its distribution."<<endl;
			char*message=m_name;
			Message aMessage(message, 0, MPI_UNSIGNED_LONG_LONG, m_machineRank, m_TAG_PREPARE_COVERAGE_DISTRIBUTION,getRank());
			m_outbox.push_back(aMessage);
			m_machineRank++;
		}

		if(m_machineRank==getSize()){
			m_numberOfMachinesReadyToSendDistribution=-1;
		}
	}

	if(m_mode_sendDistribution){
		if(m_distributionOfCoverage.size()==0){
			SplayTreeIterator<uint64_t,Vertex> iterator(&m_subgraph);
			while(iterator.hasNext()){
				int coverage=iterator.next()->getValue()->getCoverage();
				m_distributionOfCoverage[coverage]++;
			}
		}


		int length=2;
		uint64_t*data=(uint64_t*)m_outboxAllocator.allocate(sizeof(uint64_t)*length);
		int j=0;
		for(map<int,uint64_t>::iterator i=m_distributionOfCoverage.begin();i!=m_distributionOfCoverage.end();i++){
			int coverage=i->first;
			uint64_t count=i->second;
			if(m_mode_send_coverage_iterator==j){
				data[0]=coverage;
				data[1]=count;
				break;
			}
			j++;
		}

		//cout<<"Rank "<<getRank()<<" is sending distribution to "<<MASTER_RANK<<""<<endl;
		Message aMessage(data,length, MPI_UNSIGNED_LONG_LONG, MASTER_RANK, m_TAG_COVERAGE_DATA,getRank());
		m_outbox.push_back(aMessage);

		m_mode_send_coverage_iterator++;
		if(m_mode_send_coverage_iterator>=(int)m_distributionOfCoverage.size()){
			m_mode_sendDistribution=false;
			char*message=m_name;
			Message aMessage(message, 0, MPI_UNSIGNED_LONG_LONG,MASTER_RANK, m_TAG_COVERAGE_END,getRank());
			m_outbox.push_back(aMessage);
			m_distributionOfCoverage.clear();
		}

	}else if(m_mode_send_outgoing_edges==true){ 

		if(m_mode_send_edge_sequence_id%10000==0 and m_mode_send_edge_sequence_id_position==0){
			string strand="";
			if(m_reverseComplementEdge)
				strand="(reverse complement)";
			cout<<"Rank "<<getRank()<<" is extracting outgoing edges "<<strand<<" "<<m_mode_send_edge_sequence_id<<"/"<<m_myReads.size()<<endl;
		}

		if(m_mode_send_edge_sequence_id>(int)m_myReads.size()-1){
			if(m_reverseComplementEdge==false){
				m_mode_send_edge_sequence_id_position=0;
				m_reverseComplementEdge=true;
				cout<<"Rank "<<getRank()<<" is extracting outgoing edges "<<m_mode_send_edge_sequence_id<<"/"<<m_myReads.size()<<" (DONE)"<<endl;
				m_mode_send_edge_sequence_id=0;
			}else{
				m_mode_send_outgoing_edges=false;
				m_mode_send_ingoing_edges=true;
				m_mode_send_edge_sequence_id_position=0;
				cout<<"Rank "<<getRank()<<" is extracting outgoing edges (reverse complement) "<<m_mode_send_edge_sequence_id<<"/"<<m_myReads.size()<<" (DONE)"<<endl;
				m_mode_send_edge_sequence_id=0;
				m_reverseComplementEdge=false;
			}
		}else{


			char*readSequence=m_myReads[m_mode_send_edge_sequence_id]->getSeq();
			int len=strlen(readSequence);
			char memory[100];
			int lll=len-m_wordSize-1;
			map<int,vector<uint64_t> > messagesStockOut;
			for(int p=m_mode_send_edge_sequence_id_position;p<=m_mode_send_edge_sequence_id_position;p++){
				memcpy(memory,readSequence+p,m_wordSize+1);
				memory[m_wordSize+1]='\0';
				if(isValidDNA(memory)){
					VertexMer a=wordId(memory);
					if(m_reverseComplementEdge){
						VertexMer b=complementVertex(a,m_wordSize+1);
						uint64_t b_1=getKPrefix(b,m_wordSize);
						uint64_t b_2=getKSuffix(b,m_wordSize);
						int rankB=vertexRank(b_2);
						messagesStockOut[rankB].push_back(b_1);
						messagesStockOut[rankB].push_back(b_2);
					}else{
						uint64_t a_1=getKPrefix(a,m_wordSize);
						uint64_t a_2=getKSuffix(a,m_wordSize);
						int rankA=vertexRank(a_2);
						messagesStockOut[rankA].push_back(a_1);
						messagesStockOut[rankA].push_back(a_2);
					}
					
				}
			}

			m_mode_send_edge_sequence_id_position++;

			for(map<int,vector<uint64_t> >::iterator i=messagesStockOut.begin();i!=messagesStockOut.end();i++){
				int destination=i->first;
				int length=i->second.size();
				uint64_t*data=(uint64_t*)m_outboxAllocator.allocate(sizeof(uint64_t)*(length));
				for(int j=0;j<(int)i->second.size();j++){
					data[j]=i->second[j];
				}
	
				Message aMessage(data, length, MPI_UNSIGNED_LONG_LONG,destination, m_TAG_OUT_EDGES_DATA,getRank());
				m_outbox.push_back(aMessage);
			}


			if(m_mode_send_edge_sequence_id_position>lll){
				m_mode_send_edge_sequence_id++;
				m_mode_send_edge_sequence_id_position=0;
			}
		}
	}else if(m_mode_send_ingoing_edges==true){ 

		if(m_mode_send_edge_sequence_id%10000==0 and m_mode_send_edge_sequence_id_position==0){
			string strand="";
			if(m_reverseComplementEdge)
				strand="(reverse complement)";
			cout<<"Rank "<<getRank()<<" is extracting ingoing edges "<<strand<<" "<<m_mode_send_edge_sequence_id<<"/"<<m_myReads.size()<<endl;
		}

		if(m_mode_send_edge_sequence_id>(int)m_myReads.size()-1){
			if(m_reverseComplementEdge==false){
				m_reverseComplementEdge=true;
				m_mode_send_edge_sequence_id=0;
				m_mode_send_edge_sequence_id_position=0;
				cout<<"Rank "<<getRank()<<" is extracting ingoing edges "<<m_mode_send_edge_sequence_id<<"/"<<m_myReads.size()<<" (DONE)"<<endl;
			}else{
				char*message=m_name;
				Message aMessage(message, strlen(message), MPI_UNSIGNED_LONG_LONG, MASTER_RANK, m_TAG_EDGES_DISTRIBUTED,getRank());
				m_outbox.push_back(aMessage);
				m_mode_send_ingoing_edges=false;
				cout<<"Rank "<<getRank()<<" is extracting ingoing edges (reverse complement) "<<m_mode_send_edge_sequence_id<<"/"<<m_myReads.size()<<" (DONE)"<<endl;
			}
		}else{


			char*readSequence=m_myReads[m_mode_send_edge_sequence_id]->getSeq();
			int len=strlen(readSequence);
			char memory[100];
			int lll=len-m_wordSize-1;
			

			map<int,vector<uint64_t> > messagesStockIn;
			for(int p=m_mode_send_edge_sequence_id_position;p<=m_mode_send_edge_sequence_id_position;p++){
				memcpy(memory,readSequence+p,m_wordSize+1);
				memory[m_wordSize+1]='\0';
				if(isValidDNA(memory)){
					VertexMer a=wordId(memory);

					if(m_reverseComplementEdge){
						VertexMer b=complementVertex(a,m_wordSize+1);
						uint64_t b_1=getKPrefix(b,m_wordSize);
						uint64_t b_2=getKSuffix(b,m_wordSize);
						int rankB=vertexRank(b_1);
						messagesStockIn[rankB].push_back(b_1);
						messagesStockIn[rankB].push_back(b_2);
					}else{
						uint64_t a_1=getKPrefix(a,m_wordSize);
						uint64_t a_2=getKSuffix(a,m_wordSize);
						int rankA=vertexRank(a_1);
						messagesStockIn[rankA].push_back(a_1);
						messagesStockIn[rankA].push_back(a_2);
					}
				}
			}

			m_mode_send_edge_sequence_id_position++;

			// send messages
			for(map<int,vector<uint64_t> >::iterator i=messagesStockIn.begin();i!=messagesStockIn.end();i++){
				int destination=i->first;
				int length=i->second.size();
				uint64_t*data=(uint64_t*)m_outboxAllocator.allocate(sizeof(uint64_t)*(length));
				for(int j=0;j<(int)i->second.size();j++){
					data[j]=i->second[j];
				}
	
				Message aMessage(data, length, MPI_UNSIGNED_LONG_LONG,destination, m_TAG_IN_EDGES_DATA,getRank());
				m_outbox.push_back(aMessage);
			}

			if(m_mode_send_edge_sequence_id_position>lll){
				m_mode_send_edge_sequence_id++;
				m_mode_send_edge_sequence_id_position=0;
			}
		}
	}

	if(m_readyToSeed==getSize()){
		m_readyToSeed=-1;
		m_numberOfRanksDoneSeeding=0;
		// tell everyone to seed now.
		for(int i=0;i<getSize();i++){
			Message aMessage(NULL,0,MPI_UNSIGNED_LONG_LONG,i,m_TAG_START_SEEDING,getRank());
			m_outbox.push_back(aMessage);
		}
	}
	if(m_mode==m_MODE_START_SEEDING){
		// seed.
		//cout<<m_minimumCoverage<<" "<<m_seedCoverage<<" "<<m_peakCoverage<<endl;
	
		// steps.
		// 1) check its coverage.
		// 2) check its parents (don't have exactly 1 parent with >= seedCoverage
		// if this is ok, then it is a source, then extent it to a seed.

		if(!m_SEEDING_NodeInitiated){
			if(!m_SEEDING_iterator->hasNext()){
				m_mode=m_MODE_DO_NOTHING;
				cout<<"Rank "<<getRank()<<": seeding is over."<<endl;
				m_seedingAllocator.clear();
				Message aMessage(NULL,0,MPI_UNSIGNED_LONG_LONG,MASTER_RANK,m_TAG_SEEDING_IS_OVER,getRank());
				m_outbox.push_back(aMessage);
			}else{
				m_SEEDING_node=m_SEEDING_iterator->next();
				m_SEEDING_numberOfIngoingEdgesWithSeedCoverage=0;
				m_SEEDING_passedCoverageTest=false;
				m_SEEDING_NodeInitiated=true;
				if(m_seedingAllocator.getNumberOfChunks()>1){
					m_seedingAllocator.clear();
					m_seedingAllocator.constructor();
				}	
			}
		}else if(!m_SEEDING_passedCoverageTest){
			int coverage=m_SEEDING_node->getValue()->getCoverage();
			//cout<<"ok, coverage="<<coverage<<endl;
			if(false and coverage<m_seedCoverage){
				m_SEEDING_NodeInitiated=false;
			}else{
				m_SEEDING_passedCoverageTest=true;
				m_SEEDING_passedParentsTest=false;
				m_SEEDING_edge_initiated=false;
			}
		}else if(!m_SEEDING_passedParentsTest){
			if(!m_SEEDING_edge_initiated){
				//cout<<"Initiating edge."<<endl;
				m_SEEDING_edge_initiated=true;
				m_SEEDING_edge=m_SEEDING_node->getValue()->getFirstIngoingEdge();
				m_SEEDING_vertexCoverageRequested=false;
			}else if(m_SEEDING_edge!=NULL){
				if(!m_SEEDING_vertexCoverageRequested){
					int rank=m_SEEDING_edge->getRank();
					void*ptr=m_SEEDING_edge->getPtr();
					uint64_t*message=(uint64_t*)m_outboxAllocator.allocate(1*sizeof(uint64_t));
					message[0]=(uint64_t)ptr;
					Message aMessage(message,1,MPI_UNSIGNED_LONG_LONG,rank,m_TAG_REQUEST_VERTEX_COVERAGE,getRank());
					m_outbox.push_back(aMessage);
					m_SEEDING_vertexCoverageRequested=true;
					m_SEEDING_vertexCoverageReceived=false;
				}else if(m_SEEDING_vertexCoverageReceived){
					int coverage=m_SEEDING_receivedVertexCoverage;
					if(coverage>=m_seedCoverage){
						m_SEEDING_numberOfIngoingEdgesWithSeedCoverage++;
					}
					m_SEEDING_edge=m_SEEDING_edge->getNext();
					m_SEEDING_vertexCoverageRequested=false;
				}
			}else if(m_SEEDING_edge==NULL){
				if(m_SEEDING_numberOfIngoingEdgesWithSeedCoverage==1){
					m_SEEDING_NodeInitiated=false;
				}else{
					m_SEEDING_passedParentsTest=true;
					m_SEEDING_Extended=false;
					m_SEEDING_currentVertex=m_SEEDING_node->getKey();
					m_SEEDING_seed.clear();
					m_SEEDING_seed.push_back(m_SEEDING_currentVertex);
					m_SEEDING_edge_initiated=true;
					m_SEEDING_edge=m_SEEDING_node->getValue()->getFirstOutgoingEdge();
					m_SEEDING_outgoingRanks.clear();
					m_SEEDING_outgoingCoverages.clear();
					m_SEEDING_outgoingPointers.clear();
					m_SEEDING_outgoingKeys.clear();
				}
			}
		}else if(!m_SEEDING_Extended){
			//cout<<"Rank "<<getRank()<<" extending."<<endl;
			// here, we have m_SEEDING_node.
			// let us extend it now, we will first extend it as long as there is only one next >=seedCoverage
			if(!m_SEEDING_edge_initiated){
				if(!m_SEEDING_edgesRequested){
					//cout<<"Rank "<<getRank()<<" requests outgoing edges."<<endl;
					m_SEEDING_edgesRequested=true;
					m_SEEDING_edgesReceived=false;
					int rank=m_SEEDING_currentRank;
					void*ptr=m_SEEDING_currentPointer;
					uint64_t*message=(uint64_t*)m_outboxAllocator.allocate(1*sizeof(uint64_t));
					message[0]=(uint64_t)ptr;
					Message aMessage(message,1,MPI_UNSIGNED_LONG_LONG,rank,m_TAG_REQUEST_VERTEX_OUTGOING_EDGES,getRank());
					m_outbox.push_back(aMessage);
				}

			}else if(m_SEEDING_edge!=NULL){
				if(!m_SEEDING_vertexKeyAndCoverageRequested){
					int rank=m_SEEDING_edge->getRank();
					void*ptr=m_SEEDING_edge->getPtr();
					uint64_t*message=(uint64_t*)m_outboxAllocator.allocate(1*sizeof(uint64_t));
					message[0]=(uint64_t)ptr;
					Message aMessage(message,1,MPI_UNSIGNED_LONG_LONG,rank,m_TAG_REQUEST_VERTEX_KEY_AND_COVERAGE,getRank());
					m_outbox.push_back(aMessage);
					m_SEEDING_vertexKeyAndCoverageRequested=true;
					m_SEEDING_vertexKeyAndCoverageReceived=false;
				}else if(m_SEEDING_vertexKeyAndCoverageReceived){
					int rank=m_SEEDING_edge->getRank();
					void*ptr=m_SEEDING_edge->getPtr();
					//cout<<"Rank "<<getRank()<<" received key and coverage for rank="<<rank<<" and pointer="<<ptr<<endl;
					uint64_t key=m_SEEDING_receivedKey;
					int coverage=m_SEEDING_receivedVertexCoverage;
					m_SEEDING_outgoingRanks.push_back(rank);
					m_SEEDING_outgoingPointers.push_back(ptr);
					m_SEEDING_outgoingKeys.push_back(key);
					m_SEEDING_outgoingCoverages.push_back(coverage);
					m_SEEDING_edge=m_SEEDING_edge->getNext();
					m_SEEDING_vertexKeyAndCoverageRequested=false;
					
					//cout<<"Rank "<<getRank()<<" with "<<m_SEEDING_outgoingRanks.size()<<" outgoing edges so far."<<endl;
				}
			}else if(m_SEEDING_edge==NULL){
				// choose the best fit between outgoing edges.
				int index=-1;
				int numberOfSeedCoverageCandidates=0;
				if(false and m_SEEDING_outgoingCoverages.size()==1){
					index=0;
					numberOfSeedCoverageCandidates=1;
				}else{
					for(int i=0;i<(int)m_SEEDING_outgoingCoverages.size();i++){
						if(m_SEEDING_outgoingCoverages[i]>=m_seedCoverage){
							index=i;
							numberOfSeedCoverageCandidates++;
						}else{
							//cout<<"Rank "<<getRank()<<" test: "<<m_SEEDING_outgoingCoverages[i]<<" < "<<m_seedCoverage<<endl;
						}
					}
				}
				if(numberOfSeedCoverageCandidates==1){
					//cout<<"Rank "<<getRank()<<" has exactly 1"<<endl;
					m_SEEDING_seed.push_back(m_SEEDING_currentVertex);
					m_SEEDING_currentVertex=m_SEEDING_outgoingKeys[index];
					m_SEEDING_currentRank=m_SEEDING_outgoingRanks[index];
					m_SEEDING_currentPointer=m_SEEDING_outgoingPointers[index];
					//cout<<"Rank "<<getRank()<<" sets current pointer to "<<m_SEEDING_currentPointer<<endl;
					m_SEEDING_edge_initiated=false;
					m_SEEDING_edgesRequested=false;
					m_SEEDING_outgoingRanks.clear();
					m_SEEDING_outgoingCoverages.clear();
					m_SEEDING_outgoingPointers.clear();
					m_SEEDING_outgoingKeys.clear();
				}else{
					m_SEEDING_NodeInitiated=false;
					cout<<"Rank "<<getRank()<<" has a beautiful seed with "<<m_SEEDING_seed.size()<<" vertices."<<endl;
				}
			}
		}
	}
	if(m_numberOfRanksDoneSeeding==getSize()){
		m_numberOfRanksDoneSeeding=-1;
		cout<<"Rank "<<getRank()<<" All work is done, good job ranks!"<<endl;
		for(int i=0;i<getSize();i++){
			Message aMessage(NULL,0,MPI_UNSIGNED_LONG_LONG,i,m_TAG_GOOD_JOB_SEE_YOU_SOON,getRank());
			m_outbox.push_back(aMessage);
		}
	}
}

bool Machine::isMaster(){
	return getRank()==MASTER_RANK;
}



int Machine::getSize(){
	return m_size;
}



bool Machine::isAlive(){
	return m_alive;
}

void Machine::printStatus(){
	cout<<"********"<<endl;
	cout<<"Rank: "<<getRank()<<endl;
	cout<<"Reads: "<<m_myReads.size()<<endl;
	cout<<"Inbox: "<<m_inbox.size()<<endl;
	cout<<"Outbox: "<<m_outbox.size()<<endl;
	cout<<"ReceivedMessages="<<m_receivedMessages<<endl;
	cout<<"SentMessages="<<m_sentMessages<<endl;
}

int Machine::vertexRank(uint64_t a){
	return hash_uint64_t(a)%getSize();
}
