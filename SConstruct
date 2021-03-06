
#
#  Jan. 31, 2014  David Lawrence
#
# This SConstruct file can be copied into a directory containing
# the source for a plugin and used to compile it. It will use and
# install into the directory specified by the HALLD_MY environment
# variable if defined. Otherwise, it will install in the HALLD_HOME
# directory.
#
# This file should not need modification. It will be copied in by
# the mkplugin or mkfactory_plugin scripts or you can just copy it
# by hand.
#
# To use this, just type "scons install" to build and install.
# Just type "scons" if you want to build, but not install it.
#
# > scons install
#
# Note that unlike the rest of the SBMS system, this does not
# use a separate build directory and builds everything in the
# source directory. This is due to technical details and will hopefully
# be fixed in the future. For now it means, that you must be
# diligent of building for multiple platforms using the same source.
#

import os
import sys
import subprocess
import glob

# Get JANA_HOME environment variable, verifying it is set
jana_home = os.getenv('JANA_HOME')
if(jana_home == None):
	print 'JANA_HOME environment variable not set!'
	exit(-1)

# Get JANA_MY if it exists. Otherwise use HALLD_HOME
jana_my = os.getenv('JANA_MY', jana_home)

# Add SBMS directory to PYTHONPATH
sbmsdir = "%s/../SBMS" % jana_home
sys.path.append(sbmsdir)

import sbms

# Get command-line options
SHOWBUILD = ARGUMENTS.get('SHOWBUILD', 0)

# Get platform-specific name
osname = os.getenv('BMS_OSNAME', 'build')

# Get architecture name
arch = subprocess.Popen(["uname"], stdout=subprocess.PIPE).communicate()[0].strip()

# Setup initial environment
installdir = "%s" %(jana_my)
include = "%s/include" % (installdir)
bin = "%s/bin" % (installdir)
lib = "%s/lib" % (installdir)
plugins = "%s/plugins" % (installdir)
env = Environment(    CPPPATH = ["%s/include" % jana_home, "%s/include/JANA" % jana_home],
                      LIBPATH = ["%s/lib" % jana_home],  # n.b. add JANA_HOME here and prepend JANA_MY below
                  variant_dir = ".%s" % (osname))

# Only add JANA_MY search paths if they already exist
# since we'll get a warning otherwise
if (os.path.exists(lib)): env.PrependUnique(LIBPATH=[lib])

# These are SBMS-specific variables (i.e. not default scons ones)
env.Replace(INSTALLDIR    = installdir,
				OSNAME        = osname,
				INCDIR        = include,
				BINDIR        = bin,
				LIBDIR        = lib,
				PLUGINSDIR    = plugins,
				ALL_SOURCES   = [],        # used so we can add generated sources
				SHOWBUILD     = SHOWBUILD,
		 COMMAND_LINE_TARGETS = COMMAND_LINE_TARGETS)

# Use terse output unless otherwise specified
if SHOWBUILD==0:
	env.Replace(   CCCOMSTR       = "Compiling  [$SOURCE]",
				  CXXCOMSTR       = "Compiling  [$SOURCE]",
				  FORTRANPPCOMSTR = "Compiling  [$SOURCE]",
				  FORTRANCOMSTR   = "Compiling  [$SOURCE]",
				  SHCCCOMSTR      = "Compiling  [$SOURCE]",
				  SHCXXCOMSTR     = "Compiling  [$SOURCE]",
				  LINKCOMSTR      = "Linking    [$TARGET]",
				  SHLINKCOMSTR    = "Linking    [$TARGET]",
				  INSTALLSTR      = "Installing [$TARGET]",
				  ARCOMSTR        = "Archiving  [$TARGET]",
				  RANLIBCOMSTR    = "Ranlib     [$TARGET]")


# Get compiler from environment variables (if set)
env.Replace( CXX = os.getenv('CXX', 'g++'),
             CC  = os.getenv('CC' , 'gcc'),
             FC  = os.getenv('FC' , 'gfortran') )

# Add local directory, directories from HALLD_MY and HALLD_HOME to include search path
env.PrependUnique(CPPPATH = ['%s/../src' % jana_home, '%s/../src/JANA' % jana_home])
env.PrependUnique(CPPPATH = ['%s/../src' % jana_my, '%s/../src/JANA' % jana_my])

# Turn on debug symbols and warnings
env.PrependUnique(      CFLAGS = ['-g', '-fPIC', '-Wall'])
env.PrependUnique(    CXXFLAGS = ['-g', '-fPIC', '-Wall', '-std=c++11'])
env.PrependUnique(FORTRANFLAGS = ['-g', '-fPIC', '-Wall'])

# Apply any platform/architecture specific settings
sbms.ApplyPlatformSpecificSettings(env, arch)
sbms.ApplyPlatformSpecificSettings(env, osname)

# Make plugin from source in this directory
sbms.AddJANAInstalled(env)
sbms.AddROOT(env)  # should do nothing unless ROOTSYS is defined
sbms.plugin(env)

# Make install target
env.Alias('install', installdir)


