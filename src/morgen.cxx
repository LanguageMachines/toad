/*
  Copyright (c) 2015 - 2017
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

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <map>
#include <set>
#include <string>
#include "ticcutils/StringOps.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/Configuration.h"
#include "timbl/TimblAPI.h"
#include "mbt/MbtAPI.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"
#include "frog/mbma_mod.h"
#include "config.h"

using namespace std;
using namespace TiCC;

const int HISTORY = 20;
const int LEFT = 6;
const int RIGHT = 6;

int debug = 0;
bool have_config = false;

static Mbma myMbma(new TiCC::LogStream(cerr));

// some defaults (for Dutch)
const string dutch_morph_timbl_opts = "-a1 -w2 +vS";
const string dutch_mbma_set = "http://ilk.uvt.nl/folia/sets/frog-mbma-nl";

static Configuration my_config;
static Configuration frog_config;

UnicodeString UnicodeFromS( const string& s, const string& enc = "UTF8" ){
  return UnicodeString( s.c_str(), s.length(), enc.c_str() );
}

string UnicodeToUTF8( const UnicodeString& s ){
  string result;
  s.toUTF8String(result);
  return result;
}

void usage(){
  cerr << "morgen [-c configfile] [-O outputdir] "
       << endl;
}

void spitOut( ostream& os, const UnicodeString& word,
	      vector<set<string> >& morphemes ){
  for ( int i=0; i < word.length(); ++i ){
    UnicodeString out;
    // left context
    for ( int j=0; j<LEFT; j++){
      if ((i-(LEFT-j))<0)
	out += "_";
      else
	out += word[i-(LEFT-j)];
      out += ",";
    }
    // focus
    out += word[i];
    out += ",";
    // right context
    for ( int j=0; j<RIGHT; j++) {
      if ( (i+j+1) >= word.length() )
	out += "_";
      else
	out += word[i+j+1];
      out += ",";
    }
    // class
    set<string>::const_iterator it = morphemes[i].begin();
    while ( it != morphemes[i].end() ){
      out += UnicodeFromS( *it );
      ++it;
      if ( it != morphemes[i].end() )
	out += "|";
    }
    os << out << endl;
  }
}

void create_instance_file( const string& inpname, const string& outname ){
  ifstream bron( inpname );
  if ( !bron ){
    cerr << "could not open input file '" << inpname << "'" << endl;
    exit(EXIT_FAILURE);
  }

  ofstream os( outname );
  if ( !os ){
    cerr << "could not open output file '" << outname << "'" << endl;
    exit(EXIT_FAILURE);
  }
  cerr << "start converting inputfile: " << inpname << endl;
  string line;
  vector<set<string> > morphemes;
  morphemes.resize(250);
  UnicodeString prevword;
  while ( getline(bron, line ) ){
    if ( line.empty() ){
	continue;
    }
    vector<string> parts;
    int num = TiCC::split( line, parts );
    if ( num < 2 ){
      cerr << "Problem in line '" << line << "' (to short?)" << endl;
      exit(1);
    }
    UnicodeString word = UnicodeFromS( parts[0] );
    if ( word.length() != num-1 ){
      cerr << "Problem in line '" << line << "' (" << word.length()
	   << " letters, but got " << num-1 << " morphemes)" << endl;
      exit(1);
    }
    parts.erase(parts.begin());
    vector<Rule *> r = myMbma.execute( word, parts );
    if ( r.empty() ){
      cerr << "problems with entry: '" << line << "'" << endl;
      continue;
    }
    if ( word != prevword ){
      if ( !prevword.isEmpty() ){
	spitOut( os, prevword, morphemes );
      }
      prevword = word;
      for ( size_t i=0; i < morphemes.size(); ++i ){
	morphemes[i].clear();
      }
    }
    for ( size_t i=0; i < parts.size(); ++i ){
      morphemes[i].insert(parts[i]);
    }
  }
  if ( !prevword.isEmpty() ){
    spitOut( os, prevword, morphemes );
  }
  cerr << "created morphological datafile: " << outname << endl;
}

void create_instance_base( const string& dataname, const string& treename ){
  string timblopts = TiCC::trim( my_config.lookUp( "timblOpts", "mbma" ),
				 " \"" );
  if ( timblopts.empty() ){
    timblopts = dutch_morph_timbl_opts;
  }
  frog_config.setatt( "timblOpts", timblopts, "mbma" );
  cout << "Timbl: Start training " << dataname << " with Options: "
       << timblopts << endl;

  Timbl::TimblAPI timbl( timblopts );
  timbl.Learn( dataname );
  timbl.WriteInstanceBase( treename );
  cout << "Timbl: Done, stored instancebase : " << treename << endl;
}

int main(int argc, char * const argv[] ) {
  TiCC::CL_Options opts("b:O:c:hV","version");
  try {
    opts.parse_args( argc, argv );
  }
  catch ( const exception& e ){
    cerr << e.what() << endl;
    exit(EXIT_FAILURE);
  }

  string outputdir;
  string configfile;
  string base_name;
  if ( opts.extract( 'h' ) ){
    usage();
    exit( EXIT_SUCCESS );
  }

  if ( opts.extract( 'V' ) || opts.extract( "version" ) ){
    cerr << "VERSION: " << VERSION << endl;
    exit( EXIT_SUCCESS );
  }

  if ( opts.extract( 'c', configfile ) ){
    if ( !my_config.fill( configfile ) ) {
      cerr << "unable to open:" << configfile << endl;
      exit( EXIT_FAILURE );
    }
    have_config = true;
    cout << "using configuration: " << configfile << endl;
  }
  opts.extract( 'O', outputdir );
  if ( !outputdir.empty() ){
    if ( outputdir[outputdir.length()-1] != '/' )
      outputdir += "/";
    if ( !isDir( outputdir ) && !createPath( outputdir ) ){
      cerr << "output dir not usable: " << outputdir << endl;
      exit(EXIT_FAILURE);
    }
  }
  if ( !opts.extract( 'b', base_name ) ){
    base_name = TiCC::trim( my_config.lookUp( "baseName" ), " \"");
    if ( base_name.empty() ){
      base_name = "morgen";
    }
  }

  vector<string> names = opts.getMassOpts();
  if ( names.size() == 0 ){
    cerr << "missing inputfile" << endl;
    exit(EXIT_FAILURE);
  }
  else if ( names.size() > 1 ){
    cerr << "only 1 inputfile is allowed" << endl;
    exit(EXIT_FAILURE);
  }
  frog_config = my_config;
  frog_config.clearatt( "configDir", "global" );
  if ( !outputdir.empty() ){
    frog_config.setatt( "configDir", outputdir, "global" );
  }
  string inpname = names[0];
  string outname = outputdir + base_name + ".data";
  string treename = TiCC::trim( my_config.lookUp( "treeFile", "mbma" ) );
  if ( treename.empty() ){
    treename = base_name + ".tree";
  }
  frog_config.setatt( "treeFile", treename, "mbma" );
  string full_treename = outputdir + treename;
  create_instance_file( inpname, outname );
  create_instance_base( outname, full_treename );

  frog_config.clearatt( "baseName" );
  string mbma_set_name = TiCC::trim( my_config.lookUp( "set", "mbma" ) );
  if ( mbma_set_name.empty() && !have_config ){
    mbma_set_name = dutch_mbma_set;
  }
  if ( !mbma_set_name.empty() ){
    frog_config.setatt( "set", mbma_set_name, "mbma" );
  }
  string frog_cfg = outputdir + "frog.cfg.template";
  frog_config.create_configfile( frog_cfg );
  cout << "stored a frog configfile template: " << frog_cfg << endl;
  return EXIT_SUCCESS;
}
