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
    along with this program (LICENSE).  
	see <http://www.gnu.org/licenses/>

*/


#include<FastqLoader.h>
#include<fstream>

void FastqGzLoader::load(string file,vector<Read*>*reads,MyAllocator*seqMyAllocator,MyAllocator*readMyAllocator){
	gzFile*f=fopen(file.c_str(),"r");
	char buffer[4096];

	int rotatingVariable=0;
	while(Z_NULL!=gzgets(f,buffer,4096)){
		if(rotatingVariable==1){
			Read*t=(Read*)readMyAllocator->allocate(sizeof(Read));
			t->copy(NULL,buffer,readMyAllocator);
			reads->push_back(t);
		}
		rotatingVariable++;
		if(rotatingVariable==2){
			rotatingVariable=0;
		}
	}
	gzclose(f);
}
