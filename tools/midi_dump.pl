# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Copyright (c) 2026 Partho Ghosh.
# Print a Standard MIDI File as text: each track's name and notes on a 16th
# grid, for turning a tune into loop data by hand.
#   perl tools/midi_dump.pl file.mid [bars to print, default 12]
use strict; use warnings;
my ($path, $bars) = @ARGV; $bars ||= 12;
open my $f, "<:raw", $path or die "$path: $!"; local $/; my $d = <$f>;
my ($id, $len, $fmt, $ntrk, $div) = unpack "a4 N n n n", $d;
die "not a MIDI file\n" unless $id eq "MThd";
printf "format %d, %d tracks, %d ticks per quarter\n", $fmt, $ntrk, $div;
my $pos = 8 + $len; my @names = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B");
for my $t (0 .. $ntrk - 1) {
    my ($tid, $tlen) = unpack "a4 N", substr($d, $pos, 8); my $end = $pos + 8 + $tlen; $pos += 8;
    my ($tick, $run, $name, %on, @notes, %chan) = (0, 0, "");
    while ($pos < $end) {
        my $delta = 0; my $b;
        do { $b = ord substr($d, $pos++, 1); $delta = ($delta << 7) | ($b & 127) } while ($b & 128);
        $tick += $delta;
        my $s = ord substr($d, $pos, 1);
        if ($s == 0xff) {
            my $type = ord substr($d, $pos + 1, 1); $pos += 2; my $l = 0;
            do { $b = ord substr($d, $pos++, 1); $l = ($l << 7) | ($b & 127) } while ($b & 128);
            my $data = substr($d, $pos, $l); $pos += $l;
            $name = $data if $type == 3 && !$name;
            printf "  tempo %.1f BPM at tick %d\n", 60e6 / unpack("N", "\0$data"), $tick if $type == 0x51;
            printf "  time %d/%d at tick %d\n", ord($data), 2 ** ord(substr $data, 1, 1), $tick if $type == 0x58;
            next;
        }
        if ($s == 0xf0 || $s == 0xf7) { $pos++; my $l = 0; do { $b = ord substr($d, $pos++, 1); $l = ($l << 7) | ($b & 127) } while ($b & 128); $pos += $l; next }
        if ($s & 128) { $run = $s; $pos++ } else { $s = $run }
        my $kind = $s & 0xf0; my $a = ord substr($d, $pos++, 1);
        my $v = ($kind == 0xc0 || $kind == 0xd0) ? 0 : ord substr($d, $pos++, 1);
        $chan{$s & 15}++ if $kind == 0x90;
        if ($kind == 0x90 && $v) { $on{$a} = [$tick, $v] }
        elsif (($kind == 0x80 || $kind == 0x90) && $on{$a}) { push @notes, [$on{$a}[0], $a, $tick - $on{$a}[0], $on{$a}[1]]; delete $on{$a} }
    }
    $pos = $end;
    next unless @notes;
    @notes = sort { $a->[0] <=> $b->[0] || $a->[1] <=> $b->[1] } @notes;
    my %pitch; $pitch{$_->[1]}++ for @notes;
    printf "\ntrack %d \"%s\": %d notes, channel(s) %s, pitches %s\n", $t, $name, scalar @notes, join(",", map { $_ + 1 } sort keys %chan),
        join(" ", map { $names[$_ % 12] . (int($_ / 12) - 1) . "x$pitch{$_}" } sort { $a <=> $b } keys %pitch);
    my $step = $div / 4; my $bar = -1;
    for my $n (@notes) {
        my $s16 = $n->[0] / $step; my $b = int($s16 / 16);
        last if $b >= $bars;
        if ($b != $bar) { $bar = $b; printf "\n  bar %2d:", $b + 1 }
        printf " %g:%s%d(%g)", $s16 - 16 * $b, $names[$n->[1] % 12], int($n->[1] / 12) - 1, $n->[2] / $step;
    }
    print "\n";
}
