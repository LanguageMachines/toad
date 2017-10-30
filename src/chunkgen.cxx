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
#include "config.h"

using namespace std;
using namespace TiCC;

LogStream mylog(cerr);

string EOS_MARK = "\n";

static Configuration use_config;
static Configuration default_config;

void set_default_config(){
  default_config.setatt( "configDir", string(SYSCONF_PATH) + "/frog/nld/", "global");
  default_config.setatt( "baseName", "chunkgen", "global" );
  default_config.setatt( "settings", "Frog.mbt.1.0.settings", "tagger" );
  default_config.setatt( "p", "dddwfWawa", "IOB" );
  default_config.setatt( "P", "chnppddwFawasss", "IOB" );
  default_config.setatt( "n", "10", "IOB" );
  default_config.setatt( "M", "200", "IOB" );
  default_config.setatt( "%", "5", "IOB" );
  default_config.setatt( "timblOpts",
		    "+vS -G -FColumns K: -a4 -mM -k5 -dID U: -a0 -mM -k19 -dID",
		    "IOB" );
  default_config.setatt( "set", "http://ilk.uvt.nl/folia/sets/frog-chunker-nl", "IOB" );
}

void merge_cf_val( Configuration& out, const Configuration& in,
		   const string& att, const string& section ) {
  string val = out.lookUp( att, section );
  if ( val.empty() ){
    string in_val = in.lookUp( att, section );
    if ( !in_val.empty() ){
      out.setatt( att, in_val, section );
    }
  }
}

void merge_configs( Configuration& out, const Configuration& in ) {
  // should be a member of Configuration Class that does this smartly
  // for now: we just enrich 'out' with all 'in' stuff that is NOT
  // already present in 'out'

  // first the global stuff
  merge_cf_val( out, in, "configDir", "global" );
  // the default POS tagger
  merge_cf_val( out, in, "settings", "tagger" );
  // and the NER stuff
  merge_cf_val( out, in, "baseName", "IOB" );
  merge_cf_val( out, in, "p", "IOB" );
  merge_cf_val( out, in, "P", "IOB" );
  merge_cf_val( out, in, "n", "IOB" );
  merge_cf_val( out, in, "M", "IOB" );
  merge_cf_val( out, in, "%", "IOB" );
  merge_cf_val( out, in, "timblOpts", "IOB" );
  merge_cf_val( out, in, "set", "IOB" );
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

void usage( const string& name ){
  cerr << name << " [-c configfile] [-O outputdir] inputfile"
       << endl;
  cerr << name << " will convert a 'traditionally' IOB tagged corpus into\n"
       << " a MBT datafile enriched with both POS tag information\n"
       << endl << " After that, a MBT tagger will be trained on that file"
       << endl;
  cerr << "-c 'configfile'\t An existing configfile that will be enriched\n"
       << "\t\t with additional NER specific information." << endl;
  cerr << "-O 'outputdir'\t The directoy where all the outputfiles are stored\n"
       << "\t\t highly recommended to use, because a lot of files are created\n"
       << "\t\t and your working directory will get cluttered." << endl;
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

void create_train_file( MbtAPI *MyTagger,
			const string& inpname,
			const string& outname ){
  ofstream os( outname );
  ifstream is( inpname );
  string line;
  string blob;
  vector<string> chunk_tags;
  size_t HeartBeat = 0;
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
	if ( ++HeartBeat % 8000 == 0 ) {
	  cout << endl;
	}
	if ( HeartBeat % 100 == 0 ) {
	  cout << ".";
	  cout.flush();
	}
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
    use_config.setatt( "baseName", base_name, "IOB" );
  }
  merge_configs( use_config, default_config ); // to be sure to have all we need

  // first check the validity of the configfile.
  // We are picky. ALL parameters are needed!
  string chunk_set_name = use_config.lookUp( "set", "IOB" );
  if ( chunk_set_name.empty() ){
    throw setting_error( "set", "IOB" );
  }
  string p_pat = use_config.lookUp( "p", "IOB" );
  if ( p_pat.empty() ){
   throw setting_error( "p", "IOB" );
  }
  string P_pat = use_config.lookUp( "P", "IOB" );
  if ( P_pat.empty() ){
   throw setting_error( "P", "IOB" );
  }
  string timblopts = use_config.lookUp( "timblOpts", "IOB" );
  if ( timblopts.empty() ){
   throw setting_error( "timblOpts", "IOB" );
  }
  string M_opt = use_config.lookUp( "M", "IOB" );
  if ( M_opt.empty() ){
   throw setting_error( "M", "IOB" );
  }
  string n_opt = use_config.lookUp( "n", "IOB" );
  if ( n_opt.empty() ){
   throw setting_error( "n", "IOB" );
  }
  string perc_opt = use_config.lookUp( "%", "IOB" );
  if ( perc_opt.empty() ){
   throw setting_error( "%", "IOB" );
  }
  base_name = use_config.lookUp( "baseName", "IOB" );
  if ( base_name.empty() ){
   throw setting_error( "baseName", "IOB" );
  }

  string mbt_setting = use_config.lookUp( "settings", "tagger" );
  if ( mbt_setting.empty() ){
    throw setting_error( "settings", "tagger" );
  }

  mbt_setting = "-s " + use_config.configDir() + mbt_setting + " -vcf" ;
  vector<string> names = opts.getMassOpts();
  if ( names.size() == 0 ){
    cerr << "missing inputfile" << endl;
    usage( opts.prog_name() );
    exit(EXIT_FAILURE);
  }
  else if ( names.size() > 1 ){
    cerr << "only 1 inputfile is allowed" << endl;
    usage( opts.prog_name() );
    exit(EXIT_FAILURE);
  }
  MbtAPI *PosTagger = new MbtAPI( mbt_setting, mylog );
  if ( !PosTagger->isInit() ){
    exit( EXIT_FAILURE );
  }
  string inpname = names[0];
  string outname = outputdir + base_name + ".data";

  cout << "Start converting: " << inpname
       << " (every dot represents 100 tagged sentences)" << endl;
  create_train_file( PosTagger, inpname, outname );
  cout << endl << "Created a trainingfile: " << outname << endl;

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
  Configuration frog_config = use_config;
  frog_config.clearatt( "p", "IOB" );
  frog_config.clearatt( "P", "IOB" );
  frog_config.clearatt( "timblOpts", "IOB" );
  frog_config.clearatt( "M", "IOB" );
  frog_config.clearatt( "n", "IOB" );
  frog_config.clearatt( "%", "IOB" );
  frog_config.clearatt( "baseName", "IOB" );
  frog_config.clearatt( "configDir", "global" );
  if ( !outputdir.empty() ){
    frog_config.setatt( "configDir", outputdir, "global" );
  }
  frog_config.setatt( "settings", outname + ".settings", "IOB" );
  frog_config.setatt( "version", "2.0", "IOB" );
  string frog_cfg = outputdir + "frog-chunker.cfg.template";
  frog_config.create_configfile( frog_cfg );
  cout << "stored a frog configfile template: " << frog_cfg << endl;
  return EXIT_SUCCESS;
}
