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
#include "frog/enr_ner_tagger_mod.h"
#include "config.h"

using namespace std;
using namespace TiCC;

LogStream mylog(cerr);

static ENERTagger myNer(&mylog);
MbtAPI *MyTagger = 0;

string EOS_MARK = "\n";

// some sane defaults (for Dutch)
const string cgn_mbt_settings = "Frog.mbt.1.0.settings";
const string cgn_tagset  = "http://ilk.uvt.nl/folia/sets/frog-mbpos-cgn";
const string dutch_ner_set  = "http://ilk.uvt.nl/folia/sets/frog-ner";

const string ner_p_pat = "ddwdwfWawawaa";
const string ner_P_pat = "chnppddwdwFawawaasss";
const string ner_TimblOpts = "+vS -G -FColumns K: -a1 U: -a2 -q2 -mM -k19 -dID";
const string ner_n_opt = "10";
const string ner_M_opt = "-M1000";

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


bool fill_gazet( const string& name ){
  string file = TiCC::basename( name );
  string dir = TiCC::dirname( name );
  return myNer.read_gazets( file, dir );
}

void spit_out( ostream& os,
	       const vector<Tagger::TagResult>& tagv,
	       const vector<string>& ner_file_tags ){
  vector<string> words;
  vector<string> tags;
  for( const auto& tr : tagv ){
    words.push_back( tr.word() );
    tags.push_back( tr.assignedTag() );
  }

  vector<string> ner_tags;
  myNer.create_ner_list( words, ner_tags );
  string prevP = "_";
  string prevN = "_";
  for ( size_t i=0; i < words.size(); ++i ){
    string line = words[i] + "\t" + prevP + "\t" + tags[i] + "\t";
    prevP = tags[i];
    if ( i < words.size() - 1 ){
      line += tags[i+1] + "\t";
    }
    else {
      line += "_\t";
    }
    line += prevN + "\t" + ner_tags[i] + "\t";
    prevN = ner_tags[i];
    if ( i < words.size() - 1 ){
      line += ner_tags[i+1] + "\t";
    }
    else {
      line += "_\t";
    }
    line += ner_file_tags[i];
    os << line << endl;
  }
}

void create_train_file( const string& inpname,
			const string& outname ){
  ofstream os( outname );
  ifstream is( inpname );
  string line;
  string blob;
  vector<string> ner_tags;
  size_t HeartBeat=0;
  while ( getline( is, line ) ){
    if ( line == "<utt>" ){
      EOS_MARK = "<utt>";
      line.clear();
    }
    if ( line.empty() ) {
      if ( !blob.empty() ){
	vector<Tagger::TagResult> tagv = MyTagger->TagLine( blob );
	spit_out( os, tagv, ner_tags );
	os << EOS_MARK << endl;
	if ( ++HeartBeat % 8000 == 0 ) {
	  cout << endl;
	}
	if ( HeartBeat % 100 == 0 ) {
	  cout << ".";
	  cout.flush();
	}
	blob.clear();
	ner_tags.clear();
      }
      continue;
    }
    vector<string> parts;
    if ( TiCC::split( line, parts) != 2 ){
      cerr << "DOOD: " << line << endl;
      exit(EXIT_FAILURE);
    }
    blob += parts[0] + "\n";
    ner_tags.push_back( parts[1] );
  }
  if ( !blob.empty() ){
    vector<Tagger::TagResult> tagv = MyTagger->TagLine( blob );
    spit_out( os, tagv, ner_tags );
  }
}

int main(int argc, char * const argv[] ) {
  TiCC::CL_Options opts("b:O:c:hVg:X","version");
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
      base_name = "nergen";
    }
  }
  if ( opts.extract( 'g', gazeteer_name ) ){
    if ( !fill_gazet( gazeteer_name ) ){
      exit( EXIT_FAILURE );
    }
  }
  else {
    cerr << "missing gazeteer option (-g)" << endl;
    exit(EXIT_FAILURE);
  }
  if ( have_config ){
    myNer.init( my_config );
  }
  string mbt_setting = TiCC::trim( my_config.lookUp( "settings", "tagger" ), " \"" );
  if ( mbt_setting.empty() ){
    mbt_setting = cgn_mbt_settings;
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

  cout << "Start converting: " << inpname
       << " (every dot represents 100 sentences)" << endl;
  create_train_file( inpname, outname );
  cout << "Created a trainingfile: " << outname << endl;

  string p_pat = TiCC::trim( my_config.lookUp( "p", "NER" ), " \"" );
  if ( p_pat.empty() ){
    p_pat = ner_p_pat;
  }
  string P_pat = TiCC::trim( my_config.lookUp( "P", "NER" ), " \"" );
  if ( P_pat.empty() ){
    P_pat = ner_P_pat;
  }
  string timblopts = TiCC::trim( my_config.lookUp( "timblOpts", "NER" )
				 , " \"" );
  if ( timblopts.empty() ){
    timblopts = ner_TimblOpts;
  }
  string M_opt = TiCC::trim( my_config.lookUp( "M", "NER" ), " \"" );
  if ( M_opt.empty() ){
    M_opt = ner_M_opt;
  }
  string N_opt = TiCC::trim( my_config.lookUp( "n", "NER" ), " \"" );
  string taggercommand = "-E " + outname
    + " -s " + outname + ".settings"
    + " -p " + p_pat + " -P " + P_pat
    + " -O\""+ timblopts + "\""
    + " -M " + M_opt;
  if ( !N_opt.empty() ){
    taggercommand += " -n " + N_opt;
  }
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
  frog_config.clearatt( "p", "NER" );
  frog_config.clearatt( "P", "NER" );
  frog_config.clearatt( "timblOpts", "NER" );
  frog_config.clearatt( "M", "NER" );
  frog_config.clearatt( "n", "NER" );
  frog_config.clearatt( "baseName" );
  frog_config.clearatt( "configDir", "global" );
  if ( !outputdir.empty() ){
    frog_config.setatt( "configDir", outputdir, "global" );
  }
  string ner_set_name = TiCC::trim( my_config.lookUp( "set", "NER" ) );
  if ( ner_set_name.empty() && !have_config ){
    ner_set_name = dutch_ner_set;
  }
  if ( !ner_set_name.empty() ){
    frog_config.setatt( "set", ner_set_name, "NER" );
  }
  frog_config.setatt( "settings", outname + ".settings", "NER" );
  frog_config.setatt( "known_ners", gazeteer_name, "NER" );
  frog_config.setatt( "version", "2.0", "NER" );

  string frog_cfg = outputdir + "frog-ner.cfg.template";
  frog_config.create_configfile( frog_cfg );
  cout << "stored a frog configfile template: " << frog_cfg << endl;
  return EXIT_SUCCESS;
}
