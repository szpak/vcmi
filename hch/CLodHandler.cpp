#define VCMI_DLL
#include "../stdafx.h"
#include "zlib.h"
#include "CLodHandler.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>
#include "boost/filesystem/operations.hpp"
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/thread.hpp>
DLL_EXPORT int readNormalNr (int pos, int bytCon, unsigned char * str)
{
	int ret=0;
	int amp=1;
	if (str)
	{
		for (int i=0; i<bytCon; i++)
		{
			ret+=str[pos+i]*amp;
			amp<<=8;
		}
	}
	else return -1;
	return ret;
}
unsigned char * CLodHandler::giveFile(std::string defName, int * length)
{
	std::transform(defName.begin(), defName.end(), defName.begin(), (int(*)(int))toupper);
	Entry * ourEntry = entries.znajdz(Entry(defName));
	if(!ourEntry) //nothing's been found
	{
		tlog1<<"Cannot find file: "<<defName;
		return NULL;
	}
	if(length) *length = ourEntry->realSize;
	mutex->lock();
	fseek(FLOD, ourEntry->offset, 0);
	unsigned char * outp;
	if (ourEntry->offset<0) //file is in the sprites/ folder; no compression
	{
		unsigned char * outp = new unsigned char[ourEntry->realSize];
		char name[30];memset(name,0,30);
		strcat(name, myDir.c_str());
		strcat(name, PATHSEPARATOR);
		strcat(name,(char*)ourEntry->name);
		FILE * f = fopen(name,"rb");
		int result = fread(outp,1,ourEntry->realSize,f);
		mutex->unlock();
		if(result<0) {tlog1<<"Error in file reading: "<<name<<std::endl;delete[] outp; return NULL;}
		else
			return outp;
	}
	else if (ourEntry->size==0) //file is not compressed
	{
		outp = new unsigned char[ourEntry->realSize];
		fread((char*)outp, 1, ourEntry->realSize, FLOD);
		mutex->unlock();
		return outp;
	}
	else //we will decompress file
	{
		outp = new unsigned char[ourEntry->size];
		fread((char*)outp, 1, ourEntry->size, FLOD);
		mutex->unlock();
		unsigned char * decomp = NULL;
		int decRes = infs2(outp, ourEntry->size, ourEntry->realSize, decomp);
		delete[] outp;
		return decomp;
	}
	return NULL;
}
int CLodHandler::infs(unsigned char * in, int size, int realSize, std::ofstream & out, int wBits)
{
	int ret;
	unsigned have;
	z_stream strm;
	unsigned char inx[NLoadHandlerHelp::fCHUNK];
	unsigned char outx[NLoadHandlerHelp::fCHUNK];

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit2(&strm, wBits);
	if (ret != Z_OK)
		return ret;
	int chunkNumber = 0;
	do
	{
		int readBytes = 0;
		for(int i=0; i<NLoadHandlerHelp::fCHUNK && (chunkNumber * NLoadHandlerHelp::fCHUNK + i)<size; ++i)
		{
			inx[i] = in[chunkNumber * NLoadHandlerHelp::fCHUNK + i];
			++readBytes;
		}
		++chunkNumber;
		strm.avail_in = readBytes;
		//strm.avail_in = fread(inx, 1, NLoadHandlerHelp::fCHUNK, source);
		/*if (in.bad())
		{
			(void)inflateEnd(&strm);
			return Z_ERRNO;
		}*/
		if (strm.avail_in == 0)
			break;
		strm.next_in = inx;

		/* run inflate() on input until output buffer not full */
		do
		{
			strm.avail_out = NLoadHandlerHelp::fCHUNK;
			strm.next_out = outx;
			ret = inflate(&strm, Z_NO_FLUSH);
			//assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
			switch (ret)
			{
				case Z_NEED_DICT:
					ret = Z_DATA_ERROR;	 /* and fall through */
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					(void)inflateEnd(&strm);
					return ret;
			}
			have = NLoadHandlerHelp::fCHUNK - strm.avail_out;
			/*if (fwrite(out, 1, have, dest) != have || ferror(dest))
			{
				(void)inflateEnd(&strm);
				return Z_ERRNO;
			}*/
			out.write((char*)outx, have);
			if(out.bad())
			{
				(void)inflateEnd(&strm);
				return Z_ERRNO;
			}
		} while (strm.avail_out == 0);

		/* done when inflate() says it's done */
	} while (ret != Z_STREAM_END);

	/* clean up and return */
	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

DLL_EXPORT int CLodHandler::infs2(unsigned char * in, int size, int realSize, unsigned char *& out, int wBits)
{
	int ret;
	unsigned have;
	z_stream strm;
	unsigned char inx[NLoadHandlerHelp::fCHUNK];
	unsigned char outx[NLoadHandlerHelp::fCHUNK];
	out = new unsigned char [realSize];
	int latPosOut = 0;

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	ret = inflateInit2(&strm, wBits);
	if (ret != Z_OK)
		return ret;
	int chunkNumber = 0;
	do
	{
		int readBytes = 0;
		for(int i=0; i<NLoadHandlerHelp::fCHUNK && (chunkNumber * NLoadHandlerHelp::fCHUNK + i)<size; ++i)
		{
			inx[i] = in[chunkNumber * NLoadHandlerHelp::fCHUNK + i];
			++readBytes;
		}
		++chunkNumber;
		strm.avail_in = readBytes;
		if (strm.avail_in == 0)
			break;
		strm.next_in = inx;

		/* run inflate() on input until output buffer not full */
		do
		{
			strm.avail_out = NLoadHandlerHelp::fCHUNK;
			strm.next_out = outx;
			ret = inflate(&strm, Z_NO_FLUSH);
			//assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
			switch (ret)
			{
				case Z_NEED_DICT:
					ret = Z_DATA_ERROR;	 /* and fall through */
				case Z_DATA_ERROR:
				case Z_MEM_ERROR:
					(void)inflateEnd(&strm);
					return ret;
			}
			have = NLoadHandlerHelp::fCHUNK - strm.avail_out;
			for(int oo=0; oo<have; ++oo)
			{
				out[latPosOut] = outx[oo];
				++latPosOut;
			}
		} while (strm.avail_out == 0);

		/* done when inflate() says it's done */
	} while (ret != Z_STREAM_END);

	/* clean up and return */
	(void)inflateEnd(&strm);
	return ret == Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

void CLodHandler::extract(std::string FName)
{
	std::ofstream FOut;
	for (int i=0;i<totalFiles;i++)
	{
		fseek(FLOD, entries[i].offset, 0);
		std::string bufff = (DATA_DIR + FName.substr(0, FName.size()-4) + PATHSEPARATOR + (char*)entries[i].name);
		unsigned char * outp;
		if (entries[i].size==0) //file is not compressed
		{
			outp = new unsigned char[entries[i].realSize];
			fread((char*)outp, 1, entries[i].realSize, FLOD);
			std::ofstream out;
			out.open(bufff.c_str(), std::ios::binary);
			if(!out.is_open())
			{
				tlog1<<"Unable to create "<<bufff;
			}
			else
			{
				for(int hh=0; hh<entries[i].realSize; ++hh)
				{
					out<<*(outp+hh);
				}
				out.close();
			}
		}
		else
		{
			outp = new unsigned char[entries[i].size];
			fread((char*)outp, 1, entries[i].size, FLOD);
			fseek(FLOD, 0, 0);
			std::ofstream destin;
			destin.open(bufff.c_str(), std::ios::binary);
			//int decRes = decompress(outp, entries[i].size, entries[i].realSize, bufff);
			int decRes = infs(outp, entries[i].size, entries[i].realSize, destin);
			destin.close();
			if(decRes!=0)
			{
				tlog1<<"LOD Extraction error"<<"  "<<decRes<<" while extracting to "<<bufff<<std::endl;
			}
		}
		delete[] outp;
	}
	fclose(FLOD);
}

void CLodHandler::extractFile(std::string FName, std::string name)
{
	std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))toupper);
	for (int i=0;i<totalFiles;i++)
	{
		std::string buf1 = std::string((char*)entries[i].name);
		std::transform(buf1.begin(), buf1.end(), buf1.begin(), (int(*)(int))toupper);
		if(buf1!=name)
			continue;
		fseek(FLOD, entries[i].offset, 0);
		std::string bufff = (FName);
		unsigned char * outp;
		if (entries[i].size==0) //file is not compressed
		{
			outp = new unsigned char[entries[i].realSize];
			fread((char*)outp, 1, entries[i].realSize, FLOD);
			std::ofstream out;
			out.open(bufff.c_str(), std::ios::binary);
			if(!out.is_open())
			{
				tlog1<<"Unable to create "<<bufff;
			}
			else
			{
				for(int hh=0; hh<entries[i].realSize; ++hh)
				{
					out<<*(outp+hh);
				}
				out.close();
			}
		}
		else //we will decompressing file
		{
			outp = new unsigned char[entries[i].size];
			fread((char*)outp, 1, entries[i].size, FLOD);
			fseek(FLOD, 0, 0);
			std::ofstream destin;
			destin.open(bufff.c_str(), std::ios::binary);
			//int decRes = decompress(outp, entries[i].size, entries[i].realSize, bufff);
			int decRes = infs(outp, entries[i].size, entries[i].realSize, destin);
			destin.close();
			if(decRes!=0)
			{
				tlog1<<"LOD Extraction error"<<"  "<<decRes<<" while extracting to "<<bufff<<std::endl;
			}
		}
		delete[] outp;
	}
}

int CLodHandler::readNormalNr (unsigned char* bufor, int bytCon, bool cyclic)
{
	int ret=0;
	int amp=1;
	for (int i=0; i<bytCon; i++)
	{
		ret+=bufor[i]*amp;
		amp*=256;
	}
	if(cyclic && bytCon<4 && ret>=amp/2)
	{
		ret = ret-amp;
	}
	return ret;
}

void CLodHandler::init(std::string lodFile, std::string dirName)
{
	myDir = dirName;
	mutex = new boost::mutex;
	std::string Ts;
	FLOD = fopen(lodFile.c_str(), "rb");
	fseek(FLOD, 8, 0);
	unsigned char temp[4];
	fread((char*)temp, 1, 4, FLOD);
	totalFiles = readNormalNr(temp,4);
	fseek(FLOD, 0x5c, 0);
	for (int i=0; i<totalFiles; i++)
	{
		Entry entry;
		char * bufc = new char;
		bool appending = true;
		for(int kk=0; kk<12; ++kk)
		{
			//FLOD.read(bufc, 1);
			fread(bufc, 1, 1, FLOD);
			if(appending)
			{
				entry.name[kk] = toupper(*bufc);
			}
			else
			{
				entry.name[kk] = 0;
				appending = false;
			}
		}
		delete bufc;
		fread((char*)entry.hlam_1, 1, 4, FLOD);
		fread((char*)temp, 1, 4, FLOD);
		entry.offset=readNormalNr(temp,4);
		fread((char*)temp, 1, 4, FLOD);
		entry.realSize=readNormalNr(temp,4);
		fread((char*)entry.hlam_2, 1, 4, FLOD);
		fread((char*)temp, 1, 4, FLOD);
		entry.size=readNormalNr(temp,4);
		for (int z=0;z<12;z++)
		{
			if (entry.name[z])
				entry.nameStr+=entry.name[z];
			else break;
		}
		entries.push_back(entry);
	}
	boost::filesystem::directory_iterator enddir;
	if(boost::filesystem::exists(dirName))
	{
		for (boost::filesystem::directory_iterator dir(dirName);dir!=enddir;dir++)
		{
			if(boost::filesystem::is_regular(dir->status()))
			{
				std::string name = dir->path().leaf();
				std::transform(name.begin(), name.end(), name.begin(), (int(*)(int))toupper);
				boost::algorithm::replace_all(name,".BMP",".PCX");
				Entry * e = entries.znajdz(name);
				if(e) //file present in .lod - overwrite its entry
				{
					e->offset = -1;
					e->realSize = e->size = boost::filesystem::file_size(dir->path());
				}
				else //file not present in lod - add entry for it
				{
					Entry e;
					e.offset = -1;
					e.nameStr = name;
					e.realSize = e.size = boost::filesystem::file_size(dir->path());
					entries.push_back(e);
				}
			}
		}
	}
	else
		tlog1<<"Warning: No "+dirName+"/ folder!"<<std::endl;
}
std::string CLodHandler::getTextFile(std::string name)
{
	int length=-1;
	unsigned char* data = giveFile(name,&length);
	std::string ret;
	ret.reserve(length);
	for(int i=0;i<length;i++)
		ret+=data[i];
	delete [] data;
	return ret;
}
