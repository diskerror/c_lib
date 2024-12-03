/******************************************************************************
 *
 *	File:			clappFiles.cpp (Command Line APPlication, file handling)
 *
 *					$Id: clappFiles.cp,v 1.1 2007/03/10 04:15:37 reid Exp $
 *
 *	Function:		basic parent class for handling command line arguments
 *
 *	Author(s):		Reid Woodbury Jr
 *
 *	Copyright:		Copyright (c) 2006 Reid Woodbury Jr
 *					All Rights Reserved.
 *
 *	Source:			Started anew.
 *
 *	Notes:			
 *
 *	Change History:
 *			07-03-03	Started source
 *	
 *****************************************************************************/

#include "clappFiles.h"

#include <iostream>
#include <sstream>

#include <sys/param.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/errno.h>

#include <err.h>
#include <fcntl.h>
#include <sysexits.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <getopt.h>

#include <AvailabilityMacros.h>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////
void clappFiles::SetOptionStruct(void)
{
	static option	longOptInit[] =
	{
		/* { name  has_arg  *flag  val } */
		{"suffix", required_argument, 0, 0},	//	suffix to add to new files without explicit file name given
		{"recursive", no_argument, NULL, 'R'},		//	process directories recursively
		{"delete", no_argument, NULL, 'D'},			//	delete/replace destination file if it already exists
		{"update", no_argument, NULL, 'U'},			//	process and update current file
		{"newer", no_argument, NULL, 'N'},			//	replace dest file only if source file is newer
		{"help", no_argument, NULL, 'h'},
		{ NULL, 0, NULL, 0 }
	};
	mLongOpts	= longOptInit;
	
	mShortOpts	= "RDUNh";
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clappFiles::InitPrefs(void)
{
	clapp::InitPrefs();
	
	pRecursive	= false;
	pDelete		= false;
	pUpdate		= false;
	pNewer		= false;
	pSuffix		= "";
	mFile2File	= false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clappFiles::HandleArgs(int argc, char * argv[])
{
	clapp::HandleArgs(argc, argv);
	
	//////////////////////////////////////////////////////////////////////////
	//	Handle the file arguments.
	//
	//	filter conditions for input
	//		1 parameter
	//			if not updating original
	//				error
	//		2 parameters
	//			first is file
	//				second can be different file or different directory
	//			first is directory
	//				second must be a different directory
	//		>2 parameters
	//			last must be a directory different from all other parameters
	//			other parameters (file/directory names) can be anything
	
	//	for zero or one parameter
	if ( mArgc == 0 || (mArgc == 1 && !pUpdate) )
		MessageUsage();
	
	//	for two parameters
	else if ( mArgc == 2 )
	{
		struct stat	statSrc, statDst;
		if(::stat(mArgv[0], &statSrc))
		{
			cerr << "Problem getting stat of first file:\n"
					<< mArgv[0] << "\n";
			cerr << "error " << errno << endl;
			exit(errno);
		}
		
		//	If updating existing files then the second file must exist. Else create path from second arg.
		if(::stat(mArgv[1], &statDst))
		{
			if(pUpdate)	//	needs to exist if updating
			{
				cerr << "Problem getting stat of second file (may not exist and required for update, " << __LINE__ << "):\n"
						<< mArgv[1] << "\n";
				cerr << "error " << errno << endl;
				exit(errno);
			}
			
			if(errno == ENOENT)
			{
				BuildPath( mArgv[mArgc-1] );
				::stat(mArgv[1], &statDst);
			}
			else
			{
				cerr << "Problem getting stat of second file (line " << __LINE__ << "):\n"
						<< mArgv[1] << "\n";
				cerr << "error " << errno << endl;
				exit(errno);
			}
		}
		
		//	if first is directory and second is not...
		if( S_ISDIR(statSrc.st_mode) && !S_ISDIR(statDst.st_mode) )
			MessageUsage();
		
		//	check to see if both are the same directory
		if( S_ISDIR(statSrc.st_mode) && S_ISDIR(statDst.st_mode ) )
		{
			//	make sure path strings don't just differ by a trailing slash
			char	resolvedPath[PATH_MAX];
			string	pathSrc		= ::realpath(mArgv[0], resolvedPath);
			string	pathDest	= ::realpath(mArgv[1], resolvedPath);
			if ( pathSrc[pathSrc.size()-1] != '/' )
				pathSrc += "/";
			if ( pathDest[pathDest.size()-1] != '/' )
				pathDest += "/";
			
			if ( pathSrc == pathDest )
				MessageUsage();
		}
		
		//	check to see if this is a file to file xform
		mFile2File	= S_ISREG(statSrc.st_mode) && S_ISREG(statDst.st_mode) && !pUpdate;
	}
	
	//	if more than 2 files, & last parameter is not a good directory and not updating, then error
	else if ( !pUpdate )
	{
		struct stat	statLast;
		if(::stat(mArgv[mArgc-1], &statLast))
		{
			cerr << "Problem getting stat of last path:\n"
					<< mArgv[mArgc-1] << "\n" << endl;
			cerr << "error " << errno << endl;
			exit(errno);
		}
		
		if(!S_ISDIR(statLast.st_mode))
		{
			cerr << "Can't work with last path as output directory.\n";
			MessageUsage();
		}
		
		//	make sure no more than one source directory has a trailing slash
		long	slashCount	= 0;
		for (long sl=0; sl<mArgc-1; sl++)
		{
			if( mArgv[sl][strlen(mArgv[sl])-1] == '/' )
				slashCount++;
		}
		if( slashCount > 1 )
		{
			cerr << "Can't match directory structure from more than one source directory.\n\n";
			MessageUsage();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//	switch through all options that have a short option
bool clappFiles::HandleArg(int arg)
{
	if(clapp::HandleArg(arg))	
		return true;
	else
	{
		switch (arg)
		{
			case 'R':
			pRecursive	= true;
			break;
			
			case 'D':
			pUpdate	= false;
			pNewer	= false;
			pDelete	= true;
			break;
			
			case 'U':
			pDelete	= false;
			pNewer	= false;
			pUpdate	= true;
			break;
			
			case 'N':
			pDelete	= false;
			pUpdate	= false;
			pNewer	= true;
			break;
			
			case 'S':
			pSuffix = optarg;
			if (pSuffix[0] != '.')
				pSuffix.insert(0, ".");
			return true;
			
			case 'h':
			MessageHelp();
			break;
					
			default:
			return false;
		}
	}
	
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clappFiles::MessageUsage()
{
	cerr << "\n\
usage:  " << mProgName << " [-h]|[[-RDUNS] file ...]\n\
        " << mProgName << " [-RS] -U file ...\n\
        " << mProgName << " [-DN] source_file new_file\n\
        " << mProgName << " [-RDNS] source_file ... target_directory\n\
        " << mProgName << " [-RDNS] source_directory/ ... target_directory\n" << endl;
	
	exit(EX_USAGE);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clappFiles::MessageHelp()
{
	cerr << "clappFiles: Command Line APPlication class extension for files" << endl;
	cerr << "\n    -R\n\
    --recursive   process directories recursively\n\
\n\
    -D\n\
    --delete      replace destination file if it already exists\n\
\n\
    -U\n\
	--update      process and update current file\n\
\n\
	-N\n\
	--newer       replace dest file only if source file is newer\n\
\n\
	-S            Add a suffix to the end of the file name. This my also determine file\n\
	--suffix      type for new files or updated files.\n\
\n\
	-h\n\
	--help        this screen\n\
\n\
    NOTE: The destination path's directories will be created, if necessary." << endl;

	cerr << "Slash or no slash.\n\
  If you want to maintain an directory name as an entity in itself, leave off the\n\
trailing slash. It can be thought of as a request to act on the item itself. If\n\
you want to use the contents of a directory use the trailing slash. The trailing\n\
slash tells the program that you recognize it as a directory and want it to act\n\
on it's contents. This is also similar to putting an asterisk after the slash, but\n\
the asterisk will be expanded by the shell before this command is run. So...\n\
  For a list of files and directories to be copied to a new directory, only one\n\
of the source directory paths may have a trailing slash. The one with the slash\n\
will have it's structure mimicked in the destination directory, with the pivot\n\
point being the last directory name in each path. If there is no trailing slash\n\
then a directory by that name is added to the contents of the destination\n\
directory if one doesn't already exist. The delete (-D) option will cause a\n\
duplicate name to be replaced, and the newer (-N) option will cause a duplicate\n\
name to be replaced if the source is newer than the destination. It doesn't matter\n\
whether the destination directory has a trailing slash or not.\n\
  If -R (recursively traverse directories, depth first) is specified, then the\n\
entire directory structure is acted on. Otherwise only the readable files in the\n\
first level are acted upon." << endl;

	MessageUsage();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//	Run. Do initial examination of the list of file parameters.
void clappFiles::Run(void)
{
	char	resolvedPath[PATH_MAX];
	
	////////////////////////////////////////////////////////////////////////////////////////////
	//	if transforming from one file to another
	if (mFile2File)
	{
		PrepFilePair((string)mArgv[0], (string)mArgv[1]);
		return;
	}
	
	////////////////////////////////////////////////////////////////////////////////////////////
	//	If not updating original file then grab last arg as a destination directory path.
	if( !pUpdate )
	{
		/* Last path is a directory (checked/built earlier), move each file into it. */
		char	resolvedPath[PATH_MAX];
		pDestPath = ::realpath(mArgv[mArgc-1], resolvedPath);
		if( pDestPath == "" )
		{
			cerr << "Couldn't resolve path (clappFiles::Run " << __LINE__ << "):\n"
					<< resolvedPath << "\n";
			cerr << "error " << errno << endl;
			exit(errno);
		}
		
		if ( pDestPath[pDestPath.size()-1] != '/' )
			pDestPath += "/";
		
		mArgc--;
	}
	
	while ( mArgc-- )
	{
		string	thisPath	= ::realpath(*mArgv, resolvedPath);	//	this will remove the trailing slash on directories, if any
		
		struct stat	statNext;
		if(::stat(thisPath.c_str(), &statNext))
		{
			cerr << "Problem getting stat of:\n"
					<< thisPath << "\n";
			cerr << "error " << errno << endl;
			exit(errno);
		}
		
		if( !S_ISDIR(statNext.st_mode) )
		{
			pSrcPath	= ::dirname(thisPath.c_str());
			HandleItemInPath(&thisPath);
		}
		else
		{
			thisPath	+= "/";
			
			//	see if there is a trailing slash indicating usage (must test the original *mArgv)
			size_t	theLen	= strlen(*mArgv);
			if ( *(*mArgv+theLen-1) == '/' )
				pSrcPath	= thisPath;
			else
			{
				pSrcPath	= ::dirname(thisPath.c_str());
				pSrcPath	+= "/";
			}
			
			//	always get contents of first level of directory, recursion handled later
			DIR* dirp = ::opendir(*mArgv);
			struct dirent*	dp;
			while ( (dp = ::readdir(dirp)) != NULL )
				HandleItemInPath( &(thisPath + dp->d_name) );
			::closedir(dirp);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//	BuildPath based on FreeBSD: src/bin/mkdir/mkdir.c,v 1.19.2.2 2001/08/01 04:42:37 obrien
//		Every element of inPath must be or be able to be a directory, or error.
void clappFiles::BuildPath(char *inPath)
{
	struct stat		sb;
	mode_t			numask, oumask = 0;
	int				first, last;
	char			*p	= inPath;
	
	if (p[0] == '/')		// Skip leading '/'.
		++p;
	
	for (first = 1, last = 0; !last ; ++p)
	{
		if (p[0] == '\0')
			last = 1;
		else if (p[0] != '/')
			continue;
		
		*p = '\0';
		if (p[1] == '\0')
			last = 1;
		
		if (first)
		{
			oumask = ::umask(0);
			numask = oumask & ~(S_IWUSR | S_IXUSR);
			(void)::umask(numask);
			first = 0;
		}
		
		if (last)
			(void)::umask(oumask);
		
		if (::stat(inPath, &sb))
		{
			if (errno != ENOENT || mkdir(inPath, S_IRWXU | S_IRWXG | S_IRWXO) < 0)
			{
				cerr << "Problem making path (clappFiles::BuildPath " << __LINE__ << "):\n"
						<< inPath << "\n" << endl;
				cerr << "error " << errno << endl;
				exit(errno);
			}
		}
		else if ((sb.st_mode & S_IFMT) != S_IFDIR)
		{
			errno	= last ? EEXIST : ENOTDIR;
			cerr << "Problem making path (clappFiles::BuildPath " << __LINE__ << "):\n"
					<< inPath << "\n" << endl;
			cerr << "error " << errno << endl;
			exit(errno);
		}
		
		if (!last)
		    *p = '/';
	}
	
	if (!first && !last)
		(void)::umask(oumask);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clappFiles::HandleItemInPath(string *inPath)
{
	//	Override this method to skip your choice of files.
	if( SkipFile(inPath) )
		return;
	
	struct stat	statNext;
	if(::stat(inPath->c_str(), &statNext))
	{
		cerr << "Problem getting stat (clappFiles::HandleItemInPath " << __LINE__ << ") of:\n"
				<< *inPath << "\n";
		cerr << "error " << errno << endl;
		exit(errno);
	}
	
	if( !S_ISDIR(statNext.st_mode) )
	{
		if(pUpdate)			//	If updating existing file:
			PrepFilesTempDest(inPath, inPath);
		else				//	Else we're putting results in new directory structure:
		{
			//	Extract changeable part of the source path and concatenate with destination path.
			string destName	= pDestPath + inPath->substr(pSrcPath.size());
			
			//	Apply new suffix. Remove old.
			//	To qualify has having a suffix there must be a "." within 5 places from the end.
			if(pSuffix.size())
			{
				string::size_type	sufPos;
				if( (sufPos = destName.rfind(".", 5)) != string::npos )
					destName = destName.erase(sufPos,destName.size());
				
				destName += pSuffix;
			}
			PrepFilePair(inPath, &destName);
		}
	}
	else if(pRecursive)
	{
		if ( inPath->c_str()[inPath->size()-1] != '/' )
			*inPath	+= "/";
		
		DIR* dirp = ::opendir(inPath->c_str());
		struct dirent*	dp;
		while ( (dp = ::readdir(dirp)) != NULL )
			HandleItemInPath( &(*inPath + dp->d_name) );
		::closedir(dirp);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//	List rules here to skip file. True means to skip it.
bool clappFiles::SkipFile(string *inFile)
{
	char*	theBaseName	= ::basename(inFile->c_str());
	if ( !theBaseName )
	{
		cerr << "Problem getting basename (clappFiles::SkipFile " << __LINE__ << ") of:\n"
				<< *inFile << "\n";
		cerr << "error " << errno << endl;
		exit(errno);
	}
	
	if(theBaseName[0] == '.')
		return true;	//	using the UNIX convention, skip files/directories that begin with a dot
	
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clappFiles::PrepFilePair(string inSrc, string inDest)
{
	char	pathSrc[PATH_MAX];
	
	if( !::realpath(inSrc.c_str(), pathSrc) )
	{
		cerr << "Couldn't resolve path (clappFiles::PrepFilePair " << __LINE__ << "):\n"
				<< pathSrc << "\n";
		cerr << "error " << errno << endl;
		exit(errno);
	}
	
	//	Create path to destination file. Called if even if not needed.
	BuildPath( ::dirname(inDest.c_str()) );	//	BuildPath throws it's own errors.
	
	struct stat statSrc, statDest;
	
	//	source must exist and be accessable. Otherwise an error is thrown.
	if( ::stat(pathSrc, &statSrc) )
	{
		cerr << "Problem getting stat of source file (clappFiles::PrepFilePair " << __LINE__ << "):\n"
				<< pathSrc << "\n";
		cerr << "error " << errno << endl;
		exit(errno);
	}
	
	::stat(inDest.c_str(), &statDest);
	if(	errno == ENOENT )					//	if dest doesn't exist then just create a new file
	{
		Process( &(string)pathSrc, inDest );
	}
	else if( errno == 0 )	//	if it does exist...
	{
		//	and it's a regular file...
		if( S_ISREG(statDest.st_mode) )		//	SHOULD THIS BE ABLE TO HANDLE A LINK?
		{
			//	and we are to delete it...
			if( pDelete || (pNewer && statSrc.st_mtimespec.tv_sec > statDest.st_mtimespec.tv_sec) )
				PrepFilesTempDest( &(string)pathSrc, inDest );
			else if (!pDelete && !pNewer)
				cerr << "Destination file already exists.\n" << endl;	//	**** need separate overridable methods for these strings
			else
				cerr << "Destination file is newer than source.\n" << endl;
		}
		else if (pDelete || pNewer)
			cerr << "Destination must be a regular file in order to delete it.\n" << endl;
	}
	else
	{
		cerr << "Problem getting stat of dest file (clappFiles::PrepFilePair " << __LINE__ << "):\n"
				<< inDest << "\n";
		cerr << "error " << errno << endl;
		exit(errno);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//	Create a temporary destination file when we know a destination file already exists.
void clappFiles::PrepFilesTempDest(string *inSrc, string *inDest)
{
	//	Create a temporary destinatin name.
	string	tempName = ::dirname((char*)inDest->c_str());
	tempName	+= "/tempXXXXXX";
	tempName	= ::mktemp((char*)tempName.c_str());	//	replace with created temp name
	
	if ( Process( inSrc, &tempName ) )
	{
		//	swap temp and old file
#if TARGET_OS_MAC
		//	Use MacOS calls because new file may now have a resource fork.
		FSRef	destRef, tempRef;
		
		if( errno = ::FSPathMakeRef (inDest->c_str(), &destRef, NULL) )
		{
			cerr << "Can't make reference from path (clappFiles::PrepFilesTempDest " << __LINE__ << "):\n"
					<< inOrig << "\n" << endl;
			cerr << "error " << errno << endl;
			exit(errno);
		}
		if( errno = ::FSPathMakeRef (tempName.c_str(), &tempRef, NULL) )
		{
			cerr << "Can't make reference from path (clappFiles::PrepFilesTempDest " << __LINE__ << "):\n"
					<< inTemp << "\n" << endl;
			cerr << "error " << errno << endl;
			exit(errno);
		}
		
		if( errno = ::FSExchangeObjects (&tempRef, &destRef) )
		{
			cerr << "Can't exchange files (clappFiles::PrepFilesTempDest " << __LINE__ << ")." << endl;
			cerr << "error " << errno << endl;
			exit(errno);
		}
		
		if( errno = ::FSDeleteObject ( &tempRef ) )
		{
			cerr << "Problem deleting temporary file (clappFiles::PrepFilesTempDest " << __LINE__ << ")." << endl;
			cerr << "error " << errno << endl;
			exit(errno);
		}
#else
		if ( ::remove(inDest->c_str()) )
		{
			cerr << "Problem removing original file (clappFiles::PrepFilesTempDest " << __LINE__ << ")." << endl;
			cerr << "error " << errno << endl;
			exit(errno);
		}
		if ( ::rename(tempName.c_str(), inDest->c_str()) )
		{
			cerr << "Problem renaming file (clappFiles::PrepFilesTempDest " << __LINE__ << ")." << endl;
			cerr << "error " << errno << endl;
			exit(errno);
		}
#endif
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
//	Demo function for Process()
bool clappFiles::Process(string* inSrc, string* inDest)
{
	cout << "src:  " << *inSrc << "\n" <<
			"dest: " << *inDest << "\n" << endl;
	return false;	//	returning true means to 
}
