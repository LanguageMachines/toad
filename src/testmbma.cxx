/*
  $Id: mbma_prog.cxx 18699 2015-10-06 12:27:09Z sloot $
  $URL: https://ilk.uvt.nl/svn/sources/Frog/trunk/src/mbma_prog.cxx $

  Copyright (c) 2006 - 2015
  Tilburg University
  Radboud University

  A Morphological-Analyzer for Dutch

  This file is part of frog

  frog is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  frog is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.

  For questions and suggestions, see:
      http://ilk.uvt.nl/software.html
  or send mail to:
      timbl@uvt.nl
*/


#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <map>

#include "config.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/CommandLine.h"
#include "ucto/unicode.h"
#include "ticcutils/LogStream.h"
#include "libfolia/document.h"

#include "frog/cgn_tagger_mod.h"
#include "frog/mbma_mod.h"

using namespace std;
using namespace Timbl;

int debug = 0;
LogStream my_default_log( cerr, "", StampMessage ); // fall-back
LogStream *theErrLog = &my_default_log;  // fill the externals

vector<string> fileNames;

Configuration configuration;
static string configDir = string(SYSCONF_PATH) + "/frog/";
static string configFileName = configDir + "frog.cfg";
static Mbma myMbma(theErrLog);


void usage( ) {
  cout << endl << "Options:\n";
  cout << "\t============= INPUT MODE (mandatory, choose one) ========================\n"
       << "\t -t <testfile>          Run mbma on this file\n"
       << "\t -c <filename>    Set configuration file (default " << configFileName << ")\n"
       << "\t============= OTHER OPTIONS ============================================\n"
       << "\t -h. give some help.\n"
       << "\t -V or --version .   Show version info.\n"
       << "\t -d <debug level>    (for more verbosity)\n";
}

bool parse_args( TiCC::CL_Options& Opts ) {
  if ( Opts.is_present( 'V' ) || Opts.is_present("version") ){
    // we already did show what we wanted.
    exit( EXIT_SUCCESS );
  }
  if ( Opts.is_present('h') ){
    usage();
    exit( EXIT_SUCCESS );
  };
  // is a config file specified?
  Opts.extract( 'c', configFileName );
  if ( configuration.fill( configFileName ) ){
    cerr << "config read from: " << configFileName << endl;
  }
  else {
    cerr << "failed to read configuration from! '" << configFileName << "'" << endl;
    cerr << "did you correctly install the frogdata package?" << endl;
    return false;
  }
  string value;
  // debug opts
  if ( Opts.extract('d', value ) ){
    if ( !TiCC::stringTo<int>( value, debug ) ){
      cerr << "-d value should be an integer" << endl;
      return false;
    }
    if ( debug > 1 )
      configuration.setatt( "debug", value, "mbma" );
  };

  if ( Opts.extract( 't', value ) ){
    ifstream is( value );
    if ( !is ){
      cerr << "input stream " << value << " is not readable" << endl;
      return false;
    }
    fileNames.push_back( value );
  }
  else {
    fileNames = Opts.getMassOpts();
  }
  return true;
}

bool init(){
  // for some modules init can take a long time
  // so first make sure it will not fail on some trivialities
  //
  if ( !myMbma.init( configuration ) ){
    cerr << "MBMA Initialization failed." << endl;
    return false;
  }
  cerr << "Initialization done." << endl;
  return true;
}

void Test( istream& in ){
  string line;
  while ( getline( in, line ) ){
    line = TiCC::trim( line );
    if ( line.empty() )
      continue;
    //    cerr << "processing: " << line << endl;
    vector<string> parts;
    if ( TiCC::split( line, parts ) < 2 )
      continue;
    UnicodeString uWord = folia::UTF8ToUnicode(parts[0]);
    uWord.toLower();
    parts.erase(parts.begin());
    vector<Rule *> rules = myMbma.execute( uWord, parts );
    if ( rules.empty() ){
      cout << "no rule matched: " << line << endl;
    }
    else {
      for ( auto const& r : rules ){
	cout << uWord << "==> " << r->extract_morphemes() << endl;
	delete r;
      }
    }
  }
  return;
}


int main(int argc, char *argv[]) {
  std::ios_base::sync_with_stdio(false);
  cerr << "mbma_tester " << VERSION << " (c) LaMa 1998 - 2015" << endl;
  cerr << "Language Machine Group, Radboud University" << endl;
  TiCC::CL_Options Opts("Vt:d:hc:","version");
  try {
    Opts.parse_args(argc, argv);
  }
  catch ( const exception& e ){
    cerr << "fatal error: " << e.what() << endl;
    return EXIT_FAILURE;
  }
  cerr << "based on [" << Timbl::VersionName() << "]" << endl;
  cerr << "configdir: " << configDir << endl;
  if ( parse_args(Opts) ){
    if (  !init() ){
      cerr << "terminated." << endl;
      return EXIT_FAILURE;
    }
    for ( size_t i=0; i < fileNames.size(); ++i ){
      string TestFileName = fileNames[i];
      ifstream in(TestFileName);
      if ( in.good() ){
	cerr << "Processing: " << TestFileName << endl;
	Test( in );
      }
      else {
	cerr << "unable to open: " << TestFileName << endl;
	return EXIT_FAILURE;
      }
    }
  }
  else {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
