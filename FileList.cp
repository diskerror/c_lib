/*
 *  FileList.cp
 *
 *  Created by Reid Woodbury Jr on 2012-12-22.
 *  Copyright 2012. All rights reserved.
 *
 */

// #include <iterator>
// #include <fstream>
// #include <string>
// #include <vector>
// #include "boost/filesystem.hpp"
// #include <boost/regex.hpp>

#include "FileList.h"

using namespace std;
namespace fs = boost::filesystem;

FileList::FileList(const string &suf="")
{
	cout << "Initializing File List." << endl;
	
	mSuffix.assign(suf+"$", boost::regex_constants::perl|boost::regex_constants::optimize);
}

FileList::~FileList()
{
}

vector<string> FileList::Get(string& input)
{
	cout << "FileList retreiving list of files." << endl;
	
	if( !mList.empty() )
		mList.clear();		//	does not release space previously used, just the values
	
	fs::path	p(input.c_str());
	DoDirectory(p);
	
	mList.resize(mList.size());
	return mList;
}

vector<string> FileList::Get(fs::path& input)
{
	if( !mList.empty() )
		mList.clear();		//	does not release space previously used, just the values
	
	DoDirectory(input);
	
	mList.resize(mList.size());
	return mList;
}

void FileList::DoDirectory(fs::path& p)
{
	fs::directory_iterator diriter( p );
	fs::directory_iterator dirend;
	
	/* begin loop to move through directories */
	while( diriter != dirend )
	{
		fs::path temp = p / diriter->path().filename();
		
		/* check if iterator is a directory */
		if( fs::is_directory( diriter->path() ) )
		{
			DoDirectory( temp );
		}   
		else
		{
			if( boost::regex_search( temp.string(), mSuffix ) )
			{
				mList.push_back(temp.string());
			}
		}
		
		++diriter;
	}
}

size_t FileList::ListSize()
{
	return mList.size();
}

//	return file name
string FileList::operator[]( size_t pos )
{
	return mList[pos];
}

void FileList::ReadFile(string f, string& out)
{
	out.reserve( fs::file_size(f) );
	ifstream	infile( f.c_str() );
	out.assign( (istreambuf_iterator<char>(infile) ), (istreambuf_iterator<char>()) );
}

void FileList::ReadFile(size_t i, string& out)
{
	out.reserve( fs::file_size(mList[i]) );
	ifstream	infile( mList[i].c_str() );
	out.assign( (istreambuf_iterator<char>(infile) ), (istreambuf_iterator<char>()) );
}

string FileList::ReadFile(string f)
{
	string	chunk;
	ReadFile(f, chunk);
	return chunk;
}

string FileList::ReadFile(size_t i)
{
	string	chunk;
	ReadFile(i, chunk);
	return chunk;
}

//	return file contents
string FileList::operator()( size_t pos )
{
	return ReadFile( mList[pos] );
}
