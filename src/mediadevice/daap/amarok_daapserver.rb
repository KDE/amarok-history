#!/usr/bin/env ruby

#A DAAP Server
# (c) 2006 Ian Monroe <ian@monroe.nu>
# License: GNU General Public License V2

require "#{ARGV[0]}" #codes.rb
require "#{ARGV[1]}" #debug.rb
require 'webrick'
require 'pp'
require 'rubygems'
require 'ruby-prof'

$app_name = "Daap"
$debug_prefix = "Server"

class Element
    attr_accessor :name

    public 
        def initialize(name, value = Array.new)
            @name, @value = name, value
        end
        
        def to_s( codes = nil )
            if @value.nil? then
                log @name + ' is null'
                @name + Element.long_convert( 0 )
            else
                content = valueToString( codes )
                @name + Element.long_convert(content.length) + content
            end
        end
        
        def collection?
            @value.class == Array
        end
        
        def <<( child )
            @value << child
        end
        
        def size
            @value.size
        end

        
        def Element.char_convert( v ) 
            packing( v, 'c' )
        end
        
        def Element.short_convert( v )
            packing( v, 'n' )
        end
        
        def Element.long_convert( v )
            packing( v, 'N' )
        end
        
        def Element.longlong_convert( v )
            v = v.to_i  if( v.is_a?(String) )
            a = Array.new
            a[0] = v >> 32
            b = Array.new
            b[0] = v & 0xffffffff
            a.pack('N') + b.pack('N')
        end
 
    private
        def valueToString( codes )
            case CODE_TYPE[@name]
                when :string then
                    @value
                when :long then 
                    Element.long_convert( @value )
                when :container then
                    values = String.new
                    @value.each do |i|
                        values += i.to_s( codes )
                    end
                    values
                when :mlit then
                    values = String.new
                    @value.each { |i|
                        values += i.to_s( codes ) if codes.member?( i.name )
                    }
                    values
                when :char then
                    Element.char_convert( @value )
                when :short then
                    Element.short_convert( @value )
                when :longlong then
                    Element.longlong_convert( @value )
                when :date then
                    Element.long_convert( @value )
                when :version then
                    Element.short_convert( @value )
                else
                    log "type error! #{@value} #{CODE_TYPE[@name]}"
            end
        end

        def Element.packing( v, packer )
            v = v.to_i  if( v.is_a?(String) )
            a = Array.new
            a[0] = v
            a.pack(packer)
        end

end

class Mlit < Element
	attr_accessor :songformat, :id
end
#{"mlog"=>{"mlid"=>[1842003488], "mstt"=>[200]}}
class LoginServlet < WEBrick::HTTPServlet::AbstractServlet
    @@sessionId = 42
    
    def do_GET( req, resp )
        root =  Element.new( 'mlog' )
        root << Element.new( 'mlid', @@sessionId )
        root << Element.new( 'mstt',  WEBrick::HTTPStatus::OK.code )
        resp.body = root.to_s
        log resp.body.dump
        @@sessionId += 1
    end
end

#{"mupd"=>{"mstt"=>[200], "musr"=>[2]}}
class UpdateServlet < WEBrick::HTTPServlet::AbstractServlet
    include DebugMethods
    
    debugMethod(:do_GET)
    def do_GET( req, resp )
        root = Element.new( 'mupd' )
        root << Element.new( 'mstt', WEBrick::HTTPStatus::OK.code )
        root << Element.new( 'musr', 2 )
        resp.body = root.to_s
        log resp.body.dump
    end

end

class DatabaseServlet < WEBrick::HTTPServlet::AbstractServlet
  include DebugMethods
  public
      @@instance = nil
      def self.get_instance( config, *options )
          @@instance = @@instance || self.new
      end

      def initialize
          puts "hello"
          artists = Array.new
          albums = Array.new
          genre = Array.new
          device_paths = Array.new

          indexes = [  { :dbresult=> query( 'select * from album' ),  :indexed => albums }, 
          { :dbresult=> query( 'select * from artist' ), :indexed => artists },
          { :dbresult=> query( 'select * from genre' )  , :indexed => genre },
          { :dbresult=> query( 'select id, lastmountpoint from devices' ), :indexed => device_paths } ]
          indexes.each { |h|
              0.step( h[ :dbresult ].size, 2 ) { |i|
                  h[ :indexed ][ h[ :dbresult ][i].to_i ] = h[ :dbresult ][ i.to_i+1 ]
              }
          }
          columns =     [ "album, ", "artist, ", "genre, ", "track, ", "title, ", "year, ", "length, ", "samplerate, ", "url, ", "deviceid" ]
          @column_keys = [ :songalbum, :songartist, :songgenre,  :songtracknumber, :itemname, :songyear, :songtime, :songsamplerate, :url,  :deviceid ]
          #TODO composer :songcomposer
          dbitems = query( "SELECT #{columns.to_s} FROM tags LIMIT 500" )
          @music = Array.new
          id = 0
          @items = Element.new( 'mlcl' )
          0.step( dbitems.size - columns.size, columns.size ) { |overallIt|
              track = Mlit.new( 'mlit' )
              0.step( 2, 1 ) { |columnIt|
                    track << Element.new( METAS[ @column_keys[columnIt] ][:code], indexes[columnIt][ :indexed ][ dbitems[ overallIt + columnIt].to_i ] )
                }
              3.step( @column_keys.size-3, 1 ) { |columnIt|
                puts @column_keys[columnIt]
                track << Element.new( METAS[ @column_keys[columnIt] ][ :code ], dbitems[ overallIt + columnIt ] )
              }
              if overallIt > (dbitems.size - 500) then
                  log dbitems[ overallIt, overallIt + @column_keys.size].inspect
              end
              columnIt = @column_keys.size-2
              id += 1
              url = dbitems[ overallIt + columnIt ].reverse.chop.reverse
          #   log "indexes: #{dbitems[ columnIt + overallIt + 1 ]} - #{indexes[3][:indexed][ dbitems[ columnIt + overallIt + 1 ].to_i ]} #{url}"
              device_id = dbitems[ columnIt + overallIt + 1 ].to_i
              if device_id == -1 then
                  @music[id] = url
              else
                  url[0] = ''
                  @music[id] = "#{indexes[3][:indexed][ device_id ]}/#{url}"
              end
              track << Element.new( 'miid', id )
              track << Element.new( 'asfm', File::extname( url ).reverse.chop.reverse )
              @items << track
          }
          @column_keys.push( :itemid )
          @column_keys.push( :songformat )
      end
      debugMethod(:new)

      def do_GET( req, resp )
          if @items.nil? then
              initItems()
          end
      
          command = File.basename( req.path )
          case command
              when "databases" then
              # {"avdb"=>
              #   {"muty"=>[nil],
              #    "mstt"=>[200],
              #    "mrco"=>[1],
              #    "mtco"=>[1],
              #    "mlcl"=>
              #     [{"mlit"=>
              #        [{"miid"=>[1],
              #          "mper"=>[0],
              #          "minm"=>["Banshee Music Share"],
              #          "mctc"=>[1],
              #          "mimc"=>[1360]}]}]}}
                  avdb = Element.new( 'avdb' )
                  avdb << Element.new( 'muty', 0 )
                  avdb << Element.new( 'mstt', WEBrick::HTTPStatus::OK.code )
                  avdb << Element.new( 'mrco', 1 )
                  avdb << Element.new( 'mtco', 1 )
                  mlcl = Element.new( 'mlcl' )
                  avdb << mlcl
                      mlit = Element.new( 'mlit' )
                      mlcl << mlit
                      mlit << Element.new( 'miid', 1 )
                      mlit << Element.new( 'mper', 0 )
                      mlit << Element.new( 'minm', ENV['USER'] + " Amarok" )
                      mlit << Element.new( 'mctc', 1 )
                      mlit << Element.new( 'mimc', @items.size )
                  resp.body = avdb.to_s
              when "items" then
              # {"adbs"=>
              #     {"muty"=>[nil],
              #     "mstt"=>[200],
              #     "mrco"=>[1360],
              #     "mtco"=>[1360],
              #     "mlcl"=>
              #         [{"mlit"=>
              #             {"asal"=>["Be Human: Ghost in the Shell"],
              #             "miid"=>[581],
              #             "astm"=>[86000],
              #             "minm"=>["FAX me"],
              #             "astn"=>[nil],
              #             "asar"=>["Yoko Kanno"],
              #             "ascm"=>[""]},
              #             ...
                  requested = req.query['meta'].split(',')
                  toDisplay =  Array.new
                  requested.each { |str|
                      str[0,5] = ''
                      index = str.to_sym
                      if @column_keys.include?( index ) then
                          if( METAS[ index ] )
                              toDisplay.push( METAS[ index ][:code] )
                          else
                              log "not being displayed #{index.to_s}"
                          end
                      end
                  }
                  adbs = Element.new( 'adbs' )
                  adbs << Element.new( 'muty', nil )
                  adbs << Element.new( 'mstt', WEBrick::HTTPStatus::OK.code )
                  adbs << Element.new( 'mrco', @items.size )
                  adbs << Element.new( 'mtco', @items.size )
                  adbs << @items
                  resp.body = adbs.to_s( toDisplay )
              else if command =~ /([\d]*)\.(.*)$/ #1232.mp3
                      log "sending #{@music[ $1.to_i ]}"
                      resp.body = open( @music[ $1.to_i ] )
                  else
                      log "unimplemented request #{req.path}"
                  end
          end
      end
      debugMethod(:do_GET)

  private
      def query( sql )
         out = Array.new
         # $stdout.flush
         # $stdout.syswrite "SQL QUERY: #{sql}\n"
          puts "SQL QUERY: #{sql}"
         
          while ( line = $stdin.gets) && (line.chop! != '**** END SQL ****' )
            out.push( line )
          end
          out
      end
      debugMethod(:query)

      def querySelect
          @items = Array.new
          @music = Array.new
          id = 0
            
      end	
end

def log( string )
  f = open('/tmp/test.ruby', File::WRONLY | File::APPEND | File::CREAT )
  f.puts( string )
  f.close
end

class Controller

    def initialize
        server = WEBrick::HTTPServer.new( { :Port=>8081 } )
        ['INT', 'TERM'].each { |signal|
            trap(signal) { 
                server.shutdown
            }
        }
        server.mount( '/login', LoginServlet )
        server.mount( '/update', UpdateServlet )
        server.mount( '/databases', DatabaseServlet )
        server.start
    end

end

$stdout.sync = true
$stderr.sync = true

RubyProf.start
#start_profile
Controller.new
#stop_profile
#f = open('/tmp/Ndaapserver.log', File::WRONLY | File::CREAT )
#print_profile(f)
result = RubyProf.stop

printer = RubyProf::GraphHtmlPrinter.new(result)
f = open('/tmp/test.html', File::WRONLY | File::CREAT )
printer.print( f, 3 )