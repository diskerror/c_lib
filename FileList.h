
#ifndef RAW_FILELIST_H
#define RAW_FILELIST_H
#pragma once

// #include <string>
// #include <vector>
// #include <boost/filesystem.hpp>
// #include <boost/regex.hpp>

using namespace std;
using namespace boost;

class FileList
{
	vector<string>	mList;
	boost::regex	mSuffix;
	
public:
			FileList(const string &suf);
			~FileList();
	
	vector<string>	Get(string& input);
	vector<string>	Get(filesystem::path& input);
	
	size_t		ListSize();
	
	//	return file name at index
	string		operator[]( size_t pos );
	
	static	void	ReadFile(string f, string &out);
			void	ReadFile(size_t i, string &out);
	static	string	ReadFile(string f);
			string	ReadFile(size_t i);
	
	//	return contents of file name at index
	string		operator()( size_t pos );
	
protected:
	void	DoDirectory(filesystem::path& p);

};

#endif	//	RAW_FILELIST_H
