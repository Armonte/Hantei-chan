# JSON.pm shim for the native build (lane LINUX deliverable 5).
#
# vkd3d's configure.ac insists on the CPAN `JSON` module
# (m4/check-perl-module.m4) even though the only thing the build uses it for is
# decode_json() in libs/vkd3d-shader/make_spirv and demos/make_objc. There is no
# sudo on this machine, so instead of installing libjson-perl this file provides
# the same two functions on top of JSON::PP, which is part of core Perl.
#
# build_linux.sh puts this directory on PERL5LIB for the vkd3d configure and
# make. Nothing in the vkd3d submodule is modified.
package JSON;

use strict;
use warnings;

use JSON::PP ();

our $VERSION = '4.99';

use Exporter 'import';
our @EXPORT = qw(decode_json encode_json);

sub decode_json { return JSON::PP::decode_json(@_); }
sub encode_json { return JSON::PP::encode_json(@_); }

1;

