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

#include <core/constants.h>
#include<structures/StaticVector.h>
#include<assert.h>
#include <core/common_functions.h>

void StaticVector::constructor(int size,int type){
	m_type=type;
	#ifdef ASSERT
	assert(size!=0);
	#endif

	m_maxSize=size;
	m_messages=(Message*)__Malloc(sizeof(Message)*m_maxSize,m_type);
	m_size=0;
}

Message*StaticVector::operator[](int i){
	return at(i);
}

Message*StaticVector::at(int i){
	#ifdef ASSERT
	assert(i<m_size);
	#endif
	return m_messages+i;
}

void StaticVector::push_back(Message a){
	m_messages[m_size++]=a;
}

int StaticVector::size(){
	return m_size;
}

void StaticVector::clear(){
	m_size=0;
}


