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
#include "mbt/MbtAPI.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"
#include "frog/ner_tagger_mod.h"
#include "config.h"

using namespace std;
using namespace TiCC;

static NERTagger myMbma(new TiCC::LogStream(cerr));

// some defaults (for Dutch)
const string dutch_morph_timbl_opts = "-a1 -w2 +vS";
const string cgn_tagset  = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
const string dutch_ner_set  = "http://ilk.uvt.nl/folia/sets/frog-ner";

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
  cerr << "nergen [-c configfile] [-O outputdir] [-g gazeteerfile] inputfile"
       << endl;
}


void fill_gazet( const string& name ){
}

void create_train_file( const string& inpname,
			const string& outname ){
}

void create_tagger( const string& outname,
		    const string& full_treename ){
}

int main(int argc, char * const argv[] ) {
  TiCC::CL_Options opts("b:O:c:hVg","version");
  try {
    opts.parse_args( argc, argv );
  }
  catch ( const exception& e ){
    cerr << e.what() << endl;
    exit(EXIT_FAILURE);
  }
  bool have_config=false;
  string outputdir;
  string configfile;
  string base_name;
  string gazeteer_name;
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
      base_name = "nergen";
    }
  }
  if ( !opts.extract( 'g', gazeteer_name ) ){
    fill_gazet( gazeteer_name );
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
  string treename = TiCC::trim( my_config.lookUp( "treeFile", "ner" ) );
  if ( treename.empty() ){
    treename = base_name + ".tree";
  }
  frog_config.setatt( "treeFile", treename, "mbma" );
  string full_treename = outputdir + treename;
  create_train_file( inpname, outname );
  create_tagger( outname, full_treename );

  frog_config.clearatt( "baseName" );
  string ner_set_name = TiCC::trim( my_config.lookUp( "set", "ner" ) );
  if ( ner_set_name.empty() && !have_config ){
    ner_set_name = dutch_ner_set;
  }
  if ( !ner_set_name.empty() ){
    frog_config.setatt( "set", ner_set_name, "ner" );
  }
  string frog_cfg = outputdir + "frog.cfg.template";
  frog_config.create_configfile( frog_cfg );
  cout << "stored a frog configfile template: " << frog_cfg << endl;
  return EXIT_SUCCESS;
}
