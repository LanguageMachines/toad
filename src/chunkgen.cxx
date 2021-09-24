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

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include "ticcutils/StringOps.h"
#include "ticcutils/CommandLine.h"
#include "ticcutils/FileUtils.h"
#include "ticcutils/Configuration.h"
#include "ticcutils/Unicode.h"
#include "mbt/MbtAPI.h"
#include "libfolia/folia.h"
#include "ucto/tokenize.h"
#include "unicode/ustream.h"
#include "unicode/unistr.h"
#include "config.h"

using namespace std;
using namespace icu;
using namespace TiCC;

LogStream mylog(cerr);

string EOS_MARK = "\n";

static Configuration use_config;
static Configuration default_config;

void set_default_config(){
  default_config.setatt( "baseName", "chunkgen", "IOB" );
  default_config.setatt( "settings", "froggen.settings", "tagger" );
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

class setting_error: public std::runtime_error {
public:
  setting_error( const string& key, const string& mod ):
    std::runtime_error( "missing key: '" + key + "' for module: '" + mod + "'" )
  {};
};

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
  cerr << "-b 'name' use 'name' as the label in the configfile." << endl;
  cerr << "-X keep intermediate files." << endl;
  cerr << "-V or --version Show version information" << endl;
  cerr << "-h or --help Display this information." << endl;
}


void spit_out( ostream& os,
	       const vector<Tagger::TagResult>& tagv,
	       const vector<UnicodeString>& chunk_file_tags ){
  vector<UnicodeString> words;
  vector<UnicodeString> tags;
  for( const auto& tr : tagv ){
    words.push_back( tr.word() );
    tags.push_back( tr.assigned_tag() );
  }
  UnicodeString prevP = "_";
  for ( size_t i=0; i < words.size(); ++i ){
    UnicodeString line = words[i] + "\t" + prevP + "\t" + tags[i] + "\t";
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
  UnicodeString blob;
  vector<UnicodeString> chunk_tags;
  size_t HeartBeat = 0;
  while ( getline( is, line ) ){
    if ( line == "<utt>" ){
      EOS_MARK = "<utt>";
      line.clear();
    }
    if ( line.empty() ) {
      if ( !blob.isEmpty() ){
	vector<Tagger::TagResult> tagv = MyTagger->TagLine( blob );
	spit_out( os, tagv, chunk_tags );
	os << EOS_MARK << endl;
	blob.remove();
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
    vector<UnicodeString> parts = TiCC::split( TiCC::UnicodeFromUTF8(line) );
    if ( parts.size() != 2 ){
      cerr << "DOOD: " << line << endl;
      exit(EXIT_FAILURE);
    }
    blob += parts[0] + "\n";
    chunk_tags.push_back( parts[1] );
  }
  if ( !blob.isEmpty() ){
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
  else if ( !configfile.empty() ){
    outputdir = TiCC::dirname( configfile );
  }

  if ( opts.extract( 'b', base_name ) ){
    use_config.setatt( "baseName", base_name, "IOB" );
  }
  use_config.merge( default_config ); // to be sure to have all we need

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
  string use_dir = use_config.configDir();
  if ( use_dir.empty() ){
    mbt_setting = "-s " + outputdir + mbt_setting + " -vcf" ;
  }
  else {
    mbt_setting = "-s " + use_dir + mbt_setting + " -vcf" ;
  }
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
  else if ( !TiCC::isFile( names[0] ) ){
    cerr << "unable to open inputfile '" << names[0] << "'" << endl;
    exit(EXIT_FAILURE);
  }
  MbtAPI *PosTagger = new MbtAPI( mbt_setting, mylog );
  if ( !PosTagger->isInit() ){
    exit( EXIT_FAILURE );
  }
  string inpname = names[0];
  string outname = outputdir + base_name + ".data";
  string setting_name = outputdir + base_name + ".settings";

  cout << "Start converting: " << inpname
       << " (every dot represents 100 tagged sentences)" << endl;
  create_train_file( PosTagger, inpname, outname );
  cout << endl << "Created a trainingfile: " << outname << endl;

  string taggercommand = "-E " + outname
    + " -s " + setting_name
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
  frog_config.setatt( "settings", base_name + ".settings", "IOB" );
  frog_config.setatt( "version", "2.0", "IOB" );
  string cfg_out;
  if ( configfile.empty() ){
    cfg_out = outputdir + "frog-chunkgen.cfg.template";
  }
  else {
    configfile = TiCC::basename( configfile );
    const auto ppos = configfile.find( "." );
    if ( ppos == string::npos ){
      cfg_out = outputdir + configfile + "-chunkgen.cfg.template";
    }
    else {
      cfg_out = outputdir + configfile.substr(0,ppos)
	+ "-chunkgen" + configfile.substr( ppos );
    }
  }
  frog_config.create_configfile( cfg_out );
  cout << "stored a frog configfile template: " << cfg_out << endl;
  return EXIT_SUCCESS;
}
