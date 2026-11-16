#!/usr/bin/perl
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Copyright (c) 2026 Partho Ghosh.
#
# Builds the AJ Box manual: one HTML page with everything inside it.
#
#   bash dsp/stock/build.sh
#   dsp/build/ajstock.exe screens renders/s2/screens
#   dsp/build/ajstock.exe oneshots renders/s2/oneshots
#   perl docs/manual/build.pl
#
# It writes three pages:
#   docs/manual/aj-box-manual.html            the manual, a whole page that opens by itself (kept in git)
#   docs/manual/out/aj-box-manual.html        the same without the page wrapper, for publishing as an Artifact
#   docs/manual/out/aj-box-manual-print.html  everything laid out for paper; the PDF is printed from it:
#     chrome --headless --no-pdf-header-footer --virtual-time-budget=8000
#       --print-to-pdf=docs/manual/aj-box-manual.pdf docs/manual/out/aj-box-manual-print.html
#
# The FM-1 photo is M-VAVE's, fetched once from Synth Anatomy into
# docs/manual/cache, which is not kept in git.
use strict;
use warnings;
use MIME::Base64 qw(encode_base64);
use Compress::Zlib qw(compress uncompress crc32);
use JSON::PP;

my $here = $0; $here =~ s{[^/\\]*$}{}; my $root = "$here../..";
my $PHOTO_URL = 'https://synthanatomy.com/wp-content/uploads/2026/06/M-vave-FM-1-synth.jpg';
sub slurp { my ($f) = @_; open my $h, '<:raw', $f or die "$f: $!\n"; local $/; my $d = <$h>; $d }
sub uri { my ($type, $data) = @_; "data:$type;base64," . encode_base64($data, '') }

# The renderer writes PNGs without compression; pack them properly here.
sub repack_png { my ($data) = @_;
    my ($at, $idat, $ihdr) = (8, '', '');
    while ($at < length $data) {
        my ($n, $type) = unpack 'N a4', substr($data, $at, 8);
        $ihdr = substr($data, $at + 8, $n) if $type eq 'IHDR';
        $idat .= substr($data, $at + 8, $n) if $type eq 'IDAT';
        $at += 12 + $n;
    }
    my $chunk = sub { my ($type, $body) = @_; pack('N', length $body) . $type . $body . pack('N', crc32($type . $body)) };
    substr($data, 0, 8) . $chunk->('IHDR', $ihdr) . $chunk->('IDAT', compress(uncompress($idat), 9)) . $chunk->('IEND', '') }

my $cache = "${here}cache"; mkdir $cache unless -d $cache;
my $photo = "$cache/fm1-top.jpg";
system('curl', '-sL', '-A', 'Mozilla/5.0', '-o', $photo, $PHOTO_URL) == 0 or die "could not fetch the photo\n" unless -s $photo;

my %shot_file = (hello => '01-hello', home => '02-home-nothing-playing', play => '03-play', effects => '04-effects-and-knob',
    recording => '05-recording', heart => '06-heart-3-saved', mood => '08-mood-happy', muted => '10-page4-drums-muted-fast',
    kit => '11-kit-picture-slow', dice => '12-swoosh-knob-and-dice', swoosh => '12-swoosh-knob-and-dice',
    instrument => '13-instrument-picture', balance => '14-balance-knob', finish => '15-finish', battery => '16-battery-low',
    parent => '17-parent-mode', tempo => '19-tempo-pop-up', nosave => '07-heart-not-saved');
my (%shots, %packed);
for my $key (sort keys %shot_file) {
    my $f = $shot_file{$key};
    $packed{$f} //= uri('image/png', repack_png(slurp("$root/renders/s2/screens/$f.png")));
    $shots{$key} = $packed{$f};
}
my @sounds = map { uri('audio/wav', slurp($_)) } sort glob("$root/renders/s2/oneshots/pad*.wav");
die "expected 8 one-shot WAVs, found " . @sounds . "\n" unless @sounds == 8;

my $sheet = slurp("$root/docs/design/Icons.dc.html");
my %icons;
for my $label ('Bouncy Pop', 'Disco Dance', 'Head-Nod', 'Around the World', 'Silly Kitchen', 'Sleepy Time',
               'Twinkle', 'Bubblegum', 'Space', 'Animal Choir', 'Game On', 'Dreamy') {
    $sheet =~ /<svg\b([^>]*aria-label="\Q$label\E"[^>]*)>(.*?)<\/svg>/s or die "no picture for $label\n";
    my ($attrs, $body) = ($1, $2);
    $attrs =~ s/\s(?:role|aria-label|style)="[^"]*"//g;
    $icons{$label} = "<svg aria-hidden=\"true\"$attrs>$body</svg>";
}

my $json = JSON::PP->new->canonical;
my $page = slurp("${here}template.html");
my %fill = (PHOTO => uri('image/jpeg', slurp($photo)), SHOTS => $json->encode(\%shots), SOUNDS => $json->encode(\@sounds),
            ICONS => $json->encode(\%icons), PRINT => 0);
my $whole = "<!doctype html>\n<html lang=\"en\">\n<meta charset=\"utf-8\">\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
mkdir "${here}out" unless -d "${here}out";
for my $make (["out/aj-box-manual.html", 0, ''], ["aj-box-manual.html", 0, $whole], ["out/aj-box-manual-print.html", 1, $whole]) {
    my ($name, $print, $head) = @$make;
    $fill{PRINT} = $print;
    (my $made = $page) =~ s/%%(\w+)%%/exists $fill{$1} ? $fill{$1} : die "unknown slot $1\n"/ge;
    open my $out, '>:raw', "$here$name" or die "$name: $!\n";
    print $out $head, $made; close $out;
    printf "docs/manual/%s  %d KiB\n", $name, length($head . $made) / 1024;
}
