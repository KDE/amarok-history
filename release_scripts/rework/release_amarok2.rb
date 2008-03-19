#!/usr/bin/env ruby
#
# Generates an Amarok release tarball from KDE SVN
#
# Copyright (C) 2007-2008 Harald Sitter <harald@getamarok.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

NAME      = "amarok"
COMPONENT = "extragear"
SECTION   = "multimedia"
BASEPATH  = Dir.getwd()

require 'fileutils'
require './lib/libbase.rb'
require './lib/librelease.rb'
require './lib/libl10n.rb'
require './lib/libtag.rb'

def Amarok()
  # Change version
  Dir.chdir(BASEPATH + "/" + @folder)
  Dir.chdir("src")
  file = File.new( "#{NAME}.h", File::RDWR )
  str = file.read()
  file.rewind()
  file.truncate( 0 )
  str.sub!( /APP_VERSION \".*\"/, "APP_VERSION \"#{@version}\"" )
  file << str
  file.close()
  Dir.chdir("..") #amarok

  # Remove crap
  toberemoved = ["release_scripts","supplementary_scripts","src/history"]
  for object in toberemoved
    FileUtils.rm_rf(object)
  end

  Dir.chdir(BASEPATH)
end

InformationQuery()

# TODO: why is this done here?
@folder = "#{NAME}-#{@version}" #create folder instance var

FetchSource()

FetchTranslations()

FetchDocumentation()

CreateTranslationStats()

Tag()

Amarok()

CreateTar()

