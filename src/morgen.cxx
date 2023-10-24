/*
  Copyright (c) 2015 - 2023
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
#include "ticcutils/Unicode.h"
#include "timbl/TimblAPI.h"
#include "mbt/MbtAPI.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"
#include "frog/mbma_mod.h"
#include "config.h"

using namespace std;
using namespace	icu;

const int LEFT = 6;
const int RIGHT = 6;

int debug = 0;
bool have_config = false;
string temp_dir = "/tmp/froggen";
string base_name = "morgen";
string cgn_dir = string(SYSCONF_PATH) + "/frog/nld/";

static Mbma myMbma(new TiCC::LogStream(cerr));

static TiCC::Configuration default_config;
static TiCC::Configuration use_config;

void set_default_config(){
  default_config.setatt( "baseName", base_name, "mbma" );
  default_config.setatt( "cgn_clex_main", "cgntags.main", "mbma" );
  default_config.setatt( "cgn_clex_sub", "cgntags.sub", "mbma" );
  default_config.setatt( "timblOpts", "-a1 -w2 +vS", "mbma" );
  default_config.setatt( "set", "http://ilk.uvt.nl/folia/sets/frog-mbma-nl", "mbma" );
  default_config.setatt( "clex_set", "http://ilk.uvt.nl/folia/sets/frog-mbpos-clex", "mbma" );
  default_config.setatt( "cgnDir", cgn_dir, "mbma" );
}

void usage( const string& name ){
  cerr << name <<" [-c configfile] [-O outputdir] inputfile"
       << endl;
  cerr << "-c 'config' an optional configfile. Use only to override the system defaults" << endl;
  cerr << "\t O 'outputdir' Store all files in 'outputdir'"
       << " (Higly recommended)" << endl;
  cerr << "--temp-dir 'dirname' The directory to store teporary files. "
       << "(default: " << temp_dir << " )" << endl;
  cerr << "--cgn 'cgndir' The location of the (required) CGN datafiles."
       << " (default=" << cgn_dir << ")" << endl;
  cerr << "-b 'basename' Set a basename for the outputfiles (default="
       << base_name << ")" << endl;
}

void copy_cgn_files( const string& output_dir, const string& cgn_path ){
  // copy the cgn files to the output_dir
  try {
    string infile = cgn_path + "cgntags.main";
    if ( !TiCC::isFile( infile ) ){
      throw( "opening: " + infile + " failed: " );
    }
    string outfile = output_dir + "cgntags.main";
    ifstream is( infile );
    ofstream os( outfile );
    os << is.rdbuf();
  }
  catch ( const exception& e ){
    cerr << "adding 'cgntags.main' failed: " << e.what() << endl;
    exit( EXIT_FAILURE );
  }
  try {
    string infile = cgn_path + "cgntags.sub";
    if ( !TiCC::isFile( infile ) ){
      throw( "opening: " + infile + " failed: " );
    }
    string outfile = output_dir + "cgntags.sub";
    ifstream is( infile );
    ofstream os( outfile );
    os << is.rdbuf();
  }
  catch ( const exception& e ){
    cerr << "adding 'cgntags.sub' failed: " << e.what() << endl;
    exit( EXIT_FAILURE );
  }
}

void spitOut( ostream& os, const UnicodeString& word,
	      vector<set<UnicodeString> >& morphemes ){
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
    auto it = morphemes[i].begin();
    while ( it != morphemes[i].end() ){
      out += *it;
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
  vector<set<UnicodeString> > morphemes;
  morphemes.resize(250);
  UnicodeString prevword;
  UnicodeString line;
  while ( TiCC::getline( bron, line ) ){
    if ( line.isEmpty() ){
	continue;
    }
    vector<UnicodeString> parts = TiCC::split( line );
    int num = parts.size();
    if ( num < 2 ){
      cerr << "Problem in line '" << line << "' (to short?)" << endl;
      exit(1);
    }
    UnicodeString word = parts[0];
    if ( word.length() != num-1 ){
      cerr << "Problem in line '" << line << "' (" << word.length()
	   << " letters, but got " << num-1 << " morphemes)" << endl;
      exit(1);
    }
    parts.erase(parts.begin());
    vector<Rule *> r = myMbma.execute( word, "", parts );
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
  string timblopts = use_config.lookUp( "timblOpts", "mbma" );
  cout << "Timbl: Start training " << dataname << " with Options: "
       << timblopts << endl;

  Timbl::TimblAPI timbl( timblopts );
  timbl.Learn( dataname );
  timbl.WriteInstanceBase( treename );
  cout << "Timbl: Done, stored instancebase : " << treename << endl;
}

int main(int argc, char * const argv[] ) {
  TiCC::CL_Options opts("b:O:c:hV","version,help,cgn:,temp-dir");
  try {
    opts.parse_args( argc, argv );
  }
  catch ( const exception& e ){
    cerr << e.what() << endl;
    exit(EXIT_FAILURE);
  }

  string outputdir;
  string configfile;
  if ( opts.extract( 'h' ) || opts.extract( "help" ) ){
    usage( opts.prog_name() );
    exit( EXIT_SUCCESS );
  }

  if ( opts.extract( 'V' ) || opts.extract( "version" ) ){
    cerr << "VERSION: " << VERSION << endl;
    exit( EXIT_SUCCESS );
  }
  set_default_config();
  if ( opts.extract( 'c', configfile ) ){
    if ( !use_config.fill( configfile ) ) {
      cerr << "unable to open:" << configfile << endl;
      exit( EXIT_FAILURE );
    }
    cout << "using configuration: " << configfile << endl;
  }
  opts.extract( 'O', outputdir );
  if ( !outputdir.empty() ){
    if ( outputdir[outputdir.length()-1] != '/' )
      outputdir += "/";
    if ( !TiCC::isDir( outputdir ) && !TiCC::createPath( outputdir ) ){
      cerr << "output dir not usable: " << outputdir << endl;
      exit(EXIT_FAILURE);
    }
  }
  if ( opts.extract( 'b', base_name ) ){
    use_config.setatt( "baseName", base_name, "mbma" );
  }
  use_config.merge( default_config ); // to be sure to have all we need
  opts.extract( "temp-dir", temp_dir );
  cerr << "TEMP_DIR =" << temp_dir << endl;
  if ( !temp_dir.empty() ){
    if ( temp_dir.back() != '/' ){
      temp_dir += "/";
    }
    if ( !TiCC::isWritableDir( temp_dir )
	 && !TiCC::createPath( temp_dir ) ) {
      cerr << "temporary dir '" << temp_dir << "' not usable" << endl;
      return EXIT_FAILURE;
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

  base_name = use_config.getatt( "baseName", "mbma" );

  TiCC::Configuration frog_config = use_config;
  //  frog_config.clearatt( "configDir", "global" );
  string inpname = names[0];
  string data_out_name = temp_dir + base_name + ".data";
  string treename = use_config.lookUp( "treeFile", "mbma" );
  if ( treename.empty() ){
    treename = base_name + ".tree";
  }

  bool cgn_opt = opts.extract( "cgn", cgn_dir );
  if ( cgn_opt ){
    use_config.setatt( "cgnDir", cgn_dir, "mbma" );
  }
  cgn_dir = use_config.getatt( "cgnDir", "mbma" );
  if ( !cgn_dir.empty() && (cgn_dir.back() != '/' ) ){
    use_config.clearatt( "cgnDir", "mbma" );
    cgn_dir += "/";
  }
  if ( !TiCC::isDir( cgn_dir ) ){
    cerr << "unable to find CGN dir: " << cgn_dir << endl;
    exit(EXIT_FAILURE);
  }

  copy_cgn_files( outputdir, cgn_dir );
  frog_config.setatt( "treeFile", treename, "mbma" );
  string full_treename = outputdir + treename;
  create_instance_file( inpname, data_out_name );
  create_instance_base( data_out_name, full_treename );

  frog_config.clearatt( "baseName", "mbma" );

  string cfg_out;
  if ( configfile.empty() ){
    cfg_out = outputdir + "frog-morgen.cfg.template";
  }
  else {
    configfile = TiCC::basename( configfile );
    const auto ppos = configfile.find( "." );
    if ( ppos == string::npos ){
      cfg_out = outputdir + configfile + "-morgen.cfg.template";
    }
    else {
      cfg_out = outputdir + configfile.substr(0,ppos)
	+ "-morgen" + configfile.substr( ppos );
    }
  }
  frog_config.create_configfile( cfg_out );
  cout << "stored a frog configfile template: " << cfg_out << endl;
  return EXIT_SUCCESS;
}
