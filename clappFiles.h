/******************************************************************************
 *
 *	File:			clapp.h (Command Line APPlication)
 *
 *					$Id: clappFiles.h,v 1.1 2007/03/10 04:15:37 reid Exp $
 *
 *	Function:		basic parent class for handling command line arguments
 *
 *	Author(s):		Reid Woodbury Jr
 *
 *	Copyright:		Copyright (c) 2006 Reid Woodbury Jr
 *					All Rights Reserved.
 *
 *	Source:			Original.
 *
 *	Notes:			
 *
 *	Change History:
 *			06-11-03	Started source.
 *	
 ******************************************************************************/

#ifndef CLAPPFILES_H
#define CLAPPFILES_H
#pragma once

#include "clapp.h"

class clappFiles : public clapp
{
public:
					clappFiles(void) {}
	virtual			~clappFiles() {}
	
	virtual void	Run ( void );
	
protected:
	virtual void	SetOptionStruct(void);
	virtual void	InitPrefs(void);
	virtual void	HandleArgs(int argc, char * argv[]);
	virtual bool	HandleArg(int);
	
					//	messages
	virtual void	MessageUsage(void);
	virtual void	MessageHelp(void);
	
					//	utilities
	void			BuildPath(char *inPath);
	
	void			HandleItemInPath(std::string *inPath);	//	recursive
	
					//	Returns true if the inputted file path points to a type
					//		that is not to be processed.
	virtual bool	SkipFile(std::string *inFile);
	
					//	initiates forking, if needed (to be programmed later, ALSO look into threads)
					//		Forking handled here becuse these cleanup the
					//		possible temp files used in Process()
	virtual void	PrepFilePair(std::string* inSrc, std::string* inDest);
	
					//	Create a temporary destination file when we know a
					//		destination file already exists.
	virtual void	PrepFilesTempDest(std::string* inSrc, std::string* inDest);
	
					//	Always handles files in pairs. Must be overridden.
					//	Source will be a path to a good, openable file.
					//	Dest will at least have a good full path and dest file name.
	virtual bool	Process(std::string* inSrc, std::string* inDest);
	
	
	//	Explicit user preferences.
	bool		pRecursive, pDelete, pUpdate, pNewer;
	std::string	pSrcPath, pDestPath, pSuffix;
	
	//	Settings derived from input.
	bool		mFile2File;	//	Is this a single file to file xform?
	
private:
				clappFiles(const clappFiles&);
	clappFiles&	operator=(const clappFiles&);
};


#endif /* CLAPPFILES_H */
