/******************************************************************************
 *
 *  File:           clapp.h (Command Line APPlication)
 *
 *                  $Id: clapp.h,v 1.8 2007/03/10 04:13:49 reid Exp $
 *
 *  Function:       basic parent class for handling command line arguments
 *
 *  Author(s):      Reid Woodbury Jr
 *
 *  Copyright:      Copyright (c) 2006 Reid Woodbury Jr
 *                  All Rights Reserved.
 *
 *  Source:         Original.
 *
 *  Notes:
 *
 *  Change History:
 *          06-11-03    Started source.
 *
 ******************************************************************************/

#ifndef LIB_CLAPP_H_
#define LIB_CLAPP_H_
#pragma once

#include <string>

struct  option;  //  forward declaration

class clapp {
public:
                    clapp() : mArgc(0), mArgv(NULL), mProgName("") {}
    virtual         ~clapp() {}

    virtual void    Setup(int argc, char * argv[]);
    virtual void    Run(void) = 0;

protected:
    virtual void    SetOptionStruct(void);
    virtual void    InitPrefs(void) {}
    virtual void    HandleArgs(int argc, char * argv[]);
    virtual bool    HandleArg(int);
    virtual bool    HandleLongIndex(int);

                    //  messages
    virtual void    MessageUsage(void);
    virtual void    MessageHelp(void);
    virtual void    MessageMissingOption(void);
    virtual void    MessageAmbiguousOption(void);
    virtual void    MessageBadLongOptIndex(int longindex);

    //  Data passed in to program.
    int         mArgc;
    char**      mArgv;
    std::string mProgName;

    //  Hard programmed options.
    option      *mLongOpts;
    std::string mShortOpts;

private:
                clapp(const clapp&);
    clapp&      operator=(const clapp&);
};


#endif  // LIB_CLAPP_H_
