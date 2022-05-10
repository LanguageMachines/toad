/*
  Copyright (c) 2015 - 2020
  CLST Radboud University

  This file is part of toad

  toad is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 3 of the License, or
  (at your option) any later version.

  toad is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.


  For questions and suggestions, see:
      https://github.com/LanguageMachines/toad/issues
  or send mail to:
      lamasoftware (at ) science.ru.nl
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
#include "ticcutils/PrettyPrint.h"
#include "ticcutils/LogStream.h"
#include "ticcutils/Unicode.h"
#include "libfolia/folia.h"

#include "frog/cgn_tagger_mod.h"
#include "frog/mbma_mod.h"

using namespace std;
using namespace	icu;
using namespace Timbl;
using namespace TiCC;

int debug = 0;
LogStream my_default_log( cerr, "", StampMessage ); // fall-back
LogStream *theErrLog = &my_default_log;  // fill the externals

vector<string> fileNames;

Configuration configuration;
static string configDir = string(SYSCONF_PATH) + "/frog/nld/";
static string configFileName = configDir + "frog.cfg";
static Mbma myMbma(theErrLog);


void usage( ) {
  cout << endl << "Options:\n";
  cout << "\t============= INPUT MODE (mandatory, choose one) ========================\n"
       << "\t -t <testfile>          Run mbma on this file\n"
       << "\t -c <filename>    Set configuration file (default " << configFileName << ")\n"
       << "\t --deep-morph     Do deep morphe anlysis\n"
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
  if ( Opts.extract( "deep-morph" ) ){
    configuration.setatt( "deep-morph", "1", "mbma" );
  };
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

void Test( istream& in, bool deep ){
  UnicodeString line;
  while ( TiCC::getline( in, line ) ){
    line.trim();
    if ( line.isEmpty() )
      continue;
    //    cerr << "processing: " << line << endl;
    vector<UnicodeString> parts = TiCC::split( line );
    if ( parts.size() < 2 ){
      continue;
    }
    UnicodeString uWord = parts[0];
    uWord.toLower();
    parts.erase(parts.begin());
    vector<Rule *> rules = myMbma.execute( uWord, parts );
    if ( rules.empty() ){
      cout << "no rule matched: " << line << endl;
    }
    else {
      for ( auto const& r : rules ){
	cout << uWord << "==> " << r->pretty_string( deep )
	     << " " << r->tag << endl;
	delete r;
      }
    }
  }
  return;
}


int main(int argc, char *argv[]) {
  std::ios_base::sync_with_stdio(false);
  cerr << "mbma_tester " << VERSION << " (c) LaMa 1998 - 2020" << endl;
  cerr << "Language Machine Group, Radboud University" << endl;
  TiCC::CL_Options Opts("Vt:d:hc:","version,deep-morph");
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
    bool deep = !configuration.getatt( "deep-morph", "mbma" ).empty();
    for ( size_t i=0; i < fileNames.size(); ++i ){
      string TestFileName = fileNames[i];
      ifstream in(TestFileName);
      if ( in.good() ){
	cerr << "Processing: " << TestFileName << endl;
	Test( in, deep );
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
