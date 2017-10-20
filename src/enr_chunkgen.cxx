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
#include "frog/iob_tagger_mod.h"
#include "config.h"

using namespace std;
using namespace TiCC;

LogStream mylog(cerr);

MbtAPI *MyTagger = 0;

string EOS_MARK = "\n";

static Configuration my_config;
static Configuration frog_config;

void set_default_config(){
  my_config.setatt( "configDir", string(SYSCONF_PATH) + "/frog/nld/", "global");
  my_config.setatt( "baseName", "chunkgen", "global" );
  my_config.setatt( "settings", "Frog.mbt.1.0.settings", "tagger" );
  my_config.setatt( "p", "dddwfWawa", "IOB" );
  my_config.setatt( "P", "chnppddwFawasss", "IOB" );
  my_config.setatt( "n", "10", "IOB" );
  my_config.setatt( "M", "200", "IOB" );
  my_config.setatt( "%", "5", "IOB" );
  my_config.setatt( "timblOpts",
		    "+vS -FColumns K: -a4 -mM -k5 -dID U: -a0 -mM -k19 -dID",
		    "IOB" );
  my_config.setatt( "set", "http://ilk.uvt.nl/folia/sets/frog-chunker-nl", "IOB" );
}

class setting_error: public std::runtime_error {
public:
  setting_error( const string& key, const string& mod ):
    std::runtime_error( "missing key: '" + key + "' for module: '" + mod + "'" )
  {};
};


UnicodeString UnicodeFromS( const string& s, const string& enc = "UTF8" ){
  return UnicodeString( s.c_str(), s.length(), enc.c_str() );
}

string UnicodeToUTF8( const UnicodeString& s ){
  string result;
  s.toUTF8String(result);
  return result;
}

void usage(){
  cerr << "chunkgen [-c configfile] [-O outputdir] inputfile"
       << endl;
}


void spit_out( ostream& os,
	       const vector<Tagger::TagResult>& tagv,
	       const vector<string>& chunk_file_tags ){
  vector<string> words;
  vector<string> tags;
  for( const auto& tr : tagv ){
    words.push_back( tr.word() );
    tags.push_back( tr.assignedTag() );
  }
  string prevP = "_";
  for ( size_t i=0; i < words.size(); ++i ){
    string line = words[i] + "\t" + prevP + "\t" + tags[i] + "\t";
    prevP = tags[i];
    if ( i < words.size() - 1 ){
      line += tags[i+1] + "\t";
    }
    else {
      line += "_\t";
    }
    line += chunk_file_tags[i];
    os << line << endl;
  }
}

void create_train_file( const string& inpname,
			const string& outname ){
  ofstream os( outname );
  ifstream is( inpname );
  string line;
  string blob;
  vector<string> chunk_tags;
  while ( getline( is, line ) ){
    if ( line == "<utt>" ){
      EOS_MARK = "<utt>";
      line.clear();
    }
    if ( line.empty() ) {
      if ( !blob.empty() ){
	vector<Tagger::TagResult> tagv = MyTagger->TagLine( blob );
	spit_out( os, tagv, chunk_tags );
	os << EOS_MARK << endl;
	blob.clear();
	chunk_tags.clear();
      }
      continue;
    }
    vector<string> parts;
    if ( TiCC::split( line, parts) != 2 ){
      cerr << "DOOD: " << line << endl;
      exit(EXIT_FAILURE);
    }
    blob += parts[0] + "\n";
    chunk_tags.push_back( parts[1] );
  }
  if ( !blob.empty() ){
    vector<Tagger::TagResult> tagv = MyTagger->TagLine( blob );
    spit_out( os, tagv, chunk_tags );
  }
}

int main(int argc, char * const argv[] ) {
  TiCC::CL_Options opts("b:O:c:hVX","version");
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
    cout << "using configuration: " << configfile << endl;
  }
  else {
    set_default_config();
  }
  bool keepX = opts.extract( 'X' );
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
      base_name = "chunkgen";
    }
  }

  string mbt_setting = TiCC::trim( my_config.lookUp( "settings", "tagger" ), " \"" );
  if ( mbt_setting.empty() ){
    throw setting_error( "settings", "tagger" );
  }
  mbt_setting = "-s " + my_config.configDir() + mbt_setting + " -vcf" ;
  MyTagger = new MbtAPI( mbt_setting, mylog );
  if ( !MyTagger->isInit() ){
    exit( EXIT_FAILURE );
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
  string inpname = names[0];
  string outname = outputdir + base_name + ".data";

  cout << "Start converting: " << inpname << endl;
  create_train_file( inpname, outname );
  cout << "Created a trainingfile: " << outname << endl;

  string p_pat = TiCC::trim( my_config.lookUp( "p", "IOB" ), " \"" );
  if ( p_pat.empty() ){
   throw setting_error( "p", "IOB" );
  }
  string P_pat = TiCC::trim( my_config.lookUp( "P", "IOB" ), " \"" );
  if ( P_pat.empty() ){
   throw setting_error( "P", "IOB" );
  }
  string timblopts = TiCC::trim( my_config.lookUp( "timblOpts", "IOB" )
				 , " \"" );
  if ( timblopts.empty() ){
   throw setting_error( "timblOpts", "IOB" );
  }
  string M_opt = TiCC::trim( my_config.lookUp( "M", "IOB" ), " \"" );
  if ( M_opt.empty() ){
   throw setting_error( "M", "IOB" );
  }
  string n_opt = TiCC::trim( my_config.lookUp( "n", "IOB" ), " \"" );
  if ( n_opt.empty() ){
   throw setting_error( "n", "IOB" );
  }
  string perc_opt = TiCC::trim( my_config.lookUp( "%", "IOB" ), " \"" );
  if ( perc_opt.empty() ){
   throw setting_error( "%", "IOB" );
  }
  string taggercommand = "-E " + outname
    + " -s " + outname + ".settings"
    + " -p " + p_pat + " -P " + P_pat
    + " -O\""+ timblopts + "\""
    + " -M " + M_opt
    + " -n " + n_opt
    + " -% " + perc_opt;
  if ( EOS_MARK != "<utt>" ){
    taggercommand += " -eEL";
  }
  if ( keepX ){
    taggercommand += " -X";
  }
  taggercommand += " -DLogSilent"; // shut up
  cout << "start tagger: " << taggercommand << endl;
  cout << "this may take several minutes, depending on the corpus size."
       << endl;
  MbtAPI::GenerateTagger( taggercommand );
  cout << "finished tagger" << endl;
  frog_config = my_config;
  frog_config.clearatt( "p", "IOB" );
  frog_config.clearatt( "P", "IOB" );
  frog_config.clearatt( "timblOpts", "IOB" );
  frog_config.clearatt( "M", "IOB" );
  frog_config.clearatt( "n", "IOB" );
  frog_config.clearatt( "%", "IOB" );
  frog_config.clearatt( "baseName" );
  frog_config.clearatt( "configDir", "global" );
  if ( !outputdir.empty() ){
    frog_config.setatt( "configDir", outputdir, "global" );
  }
  string chunk_set_name = TiCC::trim( my_config.lookUp( "set", "IOB" ) );
  if ( chunk_set_name.empty() ){
    throw setting_error( "set", "IOB" );
  }
  frog_config.setatt( "settings", outname + ".settings", "IOB" );
  frog_config.setatt( "version", "2.0", "IOB" );
  string frog_cfg = outputdir + "frog-chunker.cfg.template";
  frog_config.create_configfile( frog_cfg );
  cout << "stored a frog configfile template: " << frog_cfg << endl;
  return EXIT_SUCCESS;
}
