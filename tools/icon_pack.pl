#!/usr/bin/perl
# SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
# Copyright (c) 2026 Partho Ghosh.
#
# Icon packer: turns the drawings in the Claude Design files into small tables
# of outlines for the FM-1 (PRD "Screen and look"). Each <svg role/aria-label>
# becomes one picture: its curves are cut into short straight lines and stored
# as points in quarter units of the drawing's own grid.
#
#   perl tools/icon_pack.pl > assets/icons/art.inc
#
# Picture bytes: width, height (grid units), shape count, then per shape:
#   fill << 4 | stroke   palette index, 8 = the colour the caller gives, 15 = none
#   stroke width         quarter units
#   subpath count, then per subpath: points | 0x80 when closed, x, y pairs
use strict;
use warnings;
use POSIX qw(ceil floor);

my $root = $0; $root =~ s{[^/\\]*$}{}; $root .= '..';
my $TOLERANCE = 0.1;
my $PI = 4 * atan2(1, 1);
my %palette = ('#2A0A3D' => 0, '#FF5FA2' => 1, '#E0218A' => 2, '#7B2CBF' => 3, '#C9A0FF' => 4,
               '#FFD166' => 5, '#FFF4FB' => 6, '#3D1656' => 7, 'CURRENTCOLOR' => 8, 'NONE' => 15);

# C name => [file, aria-label]
my @pictures = (
    # Page themes: drum pages 1 to 6, melody pages 1 to 6.
    (map { ["theme_$_->[0]", 'docs/design/Icons.dc.html', $_->[1]] }
        [pop => 'Bouncy Pop'], [disco => 'Disco Dance'], [headnod => 'Head-Nod'], [world => 'Around the World'],
        [kitchen => 'Silly Kitchen'], [sleepy => 'Sleepy Time'], [twinkle => 'Twinkle'], [bubblegum => 'Bubblegum'],
        [space => 'Space'], [animals => 'Animal Choir'], [game => 'Game On'], [dreamy => 'Dreamy']),
    (map { ["kit_$_->[0]", 'docs/design/Icons.dc.html', "$_->[1] kit"] }
        [robot => 'Robot'], [spoon => 'Kitchen'], [planet => 'Space'], [jungle => 'Jungle'], [boom => '808'],
        [rock => 'Rock'], [toy => 'Toy'], [magic => 'Magic']),
    (map { ["inst_$_->[0]", 'docs/design/Icons.dc.html', $_->[1]] }
        [piano => 'Piano'], [bells => 'Bells'], [flute => 'Flute'], [guitar => 'Guitar'], [harp => 'Harp'],
        [trumpet => 'Trumpet'], [xylophone => 'Xylophone'], [organ => 'Organ'], [laser => 'Laser'],
        [bloop => 'Bloop'], [choir => 'Choir'], [whistle => 'Whistle'],
        [toypiano => 'Toy piano'], [bass => 'Bass'], [pad => 'Pad'], [pluck => 'Pluck'],
        [chiptune => 'Chiptune'], [glass => 'Glass'], [shimmer => 'Shimmer']),
    (map { [$_, 'assets/icons/extra.html', $_] }
        qw(battery speed1 speed2 speed3 fx_echo fx_sparkle fx_wobble fx_wah fx_crunch fx_stutter fx_swoosh
           fx_balance spark note hello_drum hello_notes heart_big heart_saved heart_now heart_empty star_face
           sun_rays sun_face mood_sun mood_cloud mood_moon mood_balloon mood_lantern mood_map sleepy_battery zed
           word_aj word_box word_tada digit0 digit1 digit2 digit3 digit4 digit5 digit6 digit7 digit8 digit9 gear
           ch_P ch_D ch_S ch_B ch_I ch_N ch_O ch_M ch_a ch_b ch_d ch_e ch_f ch_g ch_h ch_i ch_l ch_m ch_n ch_o ch_p
           ch_r ch_s ch_t ch_u ch_v ch_w ch_x)),
);

my %files;
sub slurp { my $f = shift; local $/; open my $h, '<', "$root/$f" or die "$f: $!"; my $t = <$h>; $t }

sub attrs { my $s = shift; my %a; $a{lc $1} = $2 while $s =~ /([\w-]+)\s*=\s*"([^"]*)"/g; \%a }

sub circle_steps { my ($r, $angle) = @_;
    return 2 if $r <= $TOLERANCE;
    my $n = ceil(abs($angle) / (2 * atan2(sqrt(1 - (1 - $TOLERANCE / $r) ** 2), 1 - $TOLERANCE / $r)));
    $n < 2 ? 2 : $n }

# Points on an ellipse arc, leaving out the start point.
sub arc_points { my ($cx, $cy, $rx, $ry, $phi, $t1, $dt) = @_;
    my $n = circle_steps($rx > $ry ? $rx : $ry, $dt); my @p;
    for my $i (1 .. $n) {
        my $t = $t1 + $dt * $i / $n; my ($x, $y) = ($rx * cos $t, $ry * sin $t);
        push @p, [$cx + $x * cos($phi) - $y * sin($phi), $cy + $x * sin($phi) + $y * cos($phi)];
    }
    @p }

sub angle { my ($ux, $uy, $vx, $vy) = @_; atan2($ux * $vy - $uy * $vx, $ux * $vx + $uy * $vy) }

# SVG arc from (x1,y1) to (x2,y2): the standard change to centre form.
sub svg_arc { my ($x1, $y1, $rx, $ry, $rot, $large, $sweep, $x2, $y2) = @_;
    return ([$x2, $y2]) if !$rx || !$ry || ($x1 == $x2 && $y1 == $y2);
    ($rx, $ry) = (abs $rx, abs $ry); my $phi = $rot * $PI / 180;
    my ($dx, $dy) = (($x1 - $x2) / 2, ($y1 - $y2) / 2);
    my ($xp, $yp) = (cos($phi) * $dx + sin($phi) * $dy, -sin($phi) * $dx + cos($phi) * $dy);
    my $l = $xp ** 2 / $rx ** 2 + $yp ** 2 / $ry ** 2;
    if ($l > 1) { $rx *= sqrt $l; $ry *= sqrt $l }
    my $num = $rx ** 2 * $ry ** 2 - $rx ** 2 * $yp ** 2 - $ry ** 2 * $xp ** 2;
    my $den = $rx ** 2 * $yp ** 2 + $ry ** 2 * $xp ** 2;
    my $c = $den ? sqrt(($num < 0 ? 0 : $num) / $den) : 0; $c = -$c if $large == $sweep;
    my ($cxp, $cyp) = ($c * $rx * $yp / $ry, -$c * $ry * $xp / $rx);
    my ($cx, $cy) = (cos($phi) * $cxp - sin($phi) * $cyp + ($x1 + $x2) / 2,
                     sin($phi) * $cxp + cos($phi) * $cyp + ($y1 + $y2) / 2);
    my $t1 = angle(1, 0, ($xp - $cxp) / $rx, ($yp - $cyp) / $ry);
    my $dt = angle(($xp - $cxp) / $rx, ($yp - $cyp) / $ry, (-$xp - $cxp) / $rx, (-$yp - $cyp) / $ry);
    $dt -= 2 * $PI if !$sweep && $dt > 0; $dt += 2 * $PI if $sweep && $dt < 0;
    my @p = arc_points($cx, $cy, $rx, $ry, $phi, $t1, $dt); $p[-1] = [$x2, $y2]; @p }

sub bezier { my @c = @_;   # 3 or 4 control points, leaving out the start
    my $dd = 0;
    for my $i (0 .. $#c - 2) {
        my $d = sqrt(($c[$i][0] - 2 * $c[$i+1][0] + $c[$i+2][0]) ** 2 + ($c[$i][1] - 2 * $c[$i+1][1] + $c[$i+2][1]) ** 2);
        $dd = $d if $d > $dd;
    }
    $dd *= @c == 4 ? 6 : 2;
    my $n = ceil(sqrt($dd / (8 * $TOLERANCE))); $n = 1 if $n < 1; my @p;
    for my $i (1 .. $n) {
        my $t = $i / $n; my @q = map { [@$_] } @c;
        while (@q > 1) { @q = map { [$q[$_][0] + ($q[$_+1][0] - $q[$_][0]) * $t, $q[$_][1] + ($q[$_+1][1] - $q[$_][1]) * $t] } 0 .. $#q - 1 }
        push @p, $q[0];
    }
    @p }

# Path data to subpaths: each is {points => [[x, y]...], closed => 0 or 1}.
sub path { my $d = shift; my @t = $d =~ /[MmLlHhVvCcSsQqTtAaZz]|-?(?:\d*\.\d+|\d+\.?)(?:[eE][-+]?\d+)?/g;
    my (@subs, $cur, $cmd, @last_control); my ($x, $y, $sx, $sy) = (0, 0, 0, 0);
    my $num = sub { die "path: number expected in '$d'" unless @t && $t[0] !~ /^[A-Za-z]$/; shift @t };
    while (@t) {
        $cmd = shift @t if $t[0] =~ /^[A-Za-z]$/;
        my $rel = $cmd eq lc $cmd; my $c = uc $cmd; my ($ox, $oy) = $rel ? ($x, $y) : (0, 0);
        my $previous = $c;
        if ($c eq 'Z') { $cur->{closed} = 1 if $cur; ($x, $y) = ($sx, $sy); undef $cur; @last_control = (); next }
        if ($c eq 'M') {
            ($x, $y) = ($ox + $num->(), $oy + $num->()); ($sx, $sy) = ($x, $y);
            $cur = {points => [[$x, $y]], closed => 0}; push @subs, $cur; $cmd = $rel ? 'l' : 'L'; @last_control = (); next;
        }
        if (!$cur) { $cur = {points => [[$x, $y]], closed => 0}; push @subs, $cur }
        my @new;
        if ($c eq 'L') { @new = ([$ox + $num->(), $oy + $num->()]) }
        elsif ($c eq 'H') { @new = ([$ox + $num->(), $y]) }
        elsif ($c eq 'V') { @new = ([$x, $oy + $num->()]) }
        elsif ($c eq 'C' || $c eq 'S' || $c eq 'Q' || $c eq 'T') {
            my @c = ([$x, $y]);
            if ($c eq 'S' || $c eq 'T') { push @c, @last_control ? [2 * $x - $last_control[0], 2 * $y - $last_control[1]] : [$x, $y] }
            my $more = {C => 3, S => 2, Q => 2, T => 1}->{$c};
            push @c, [$ox + $num->(), $oy + $num->()] for 1 .. $more;
            @new = bezier(@c); @last_control = ($c[-2][0], $c[-2][1], $c eq 'C' || $c eq 'S' ? 'c' : 'q');
        }
        elsif ($c eq 'A') {
            my ($rx, $ry, $rot, $large, $sweep) = map { $num->() } 1 .. 5;
            @new = svg_arc($x, $y, $rx, $ry, $rot, $large, $sweep, $ox + $num->(), $oy + $num->());
        }
        else { die "path: command $cmd" }
        @last_control = () unless $c =~ /[CSQT]/;
        push @{$cur->{points}}, @new; ($x, $y) = @{$new[-1]};
    }
    @subs }

sub ellipse { my ($cx, $cy, $rx, $ry) = @_;
    ({points => [arc_points($cx, $cy, $rx, $ry, 0, 0, 2 * $PI)], closed => 1}) }

sub rect { my ($x, $y, $w, $h, $r) = @_;
    return ({points => [[$x, $y], [$x + $w, $y], [$x + $w, $y + $h], [$x, $y + $h]], closed => 1}) unless $r;
    $r = $w / 2 if $r > $w / 2; $r = $h / 2 if $r > $h / 2; my @p;
    my @corners = ([$x + $w - $r, $y + $r, -$PI / 2], [$x + $w - $r, $y + $h - $r, 0],
                   [$x + $r, $y + $h - $r, $PI / 2], [$x + $r, $y + $r, $PI]);
    for my $c (@corners) {
        push @p, [$c->[0] + $r * cos($c->[2]), $c->[1] + $r * sin($c->[2])], arc_points($c->[0], $c->[1], $r, $r, 0, $c->[2], $PI / 2);
    }
    ({points => \@p, closed => 1}) }

sub rotate { my ($subs, $transform) = @_;
    return unless $transform;
    $transform =~ /^\s*rotate\(\s*(-?[\d.]+)[\s,]+(-?[\d.]+)[\s,]+(-?[\d.]+)\s*\)\s*$/ or die "transform: $transform";
    my ($a, $cx, $cy) = ($1 * $PI / 180, $2, $3);
    for my $s (@$subs) { for my $p (@{$s->{points}}) {
        my ($dx, $dy) = ($p->[0] - $cx, $p->[1] - $cy);
        @$p = ($cx + $dx * cos($a) - $dy * sin($a), $cy + $dx * sin($a) + $dy * cos($a));
    } } }

sub colour { my ($v, $name) = @_; my $i = $palette{uc $v}; die "$name: colour $v is not in the palette" unless defined $i; $i }

sub pack_picture { my ($name, $file, $label) = @_;
    $files{$file} //= slurp($file);
    $files{$file} =~ /<svg\b([^>]*aria-label="\Q$label\E"[^>]*)>(.*?)<\/svg>/s or die "$name: no drawing labelled '$label' in $file";
    my ($top, $body) = (attrs($1), $2);
    my ($vx, $vy, $vw, $vh) = split ' ', $top->{viewbox}; die "$name: viewBox" if $vx || $vy;
    my @stack = ({fill => '#000000', stroke => 'none', 'stroke-width' => 1, %$top}); my @shapes;
    delete $stack[0]{transform};
    while ($body =~ /<(\/?)(g|path|circle|ellipse|rect)\b([^>]*)>/g) {
        my ($close, $tag, $a) = ($1, $2, attrs($3));
        if ($tag eq 'g') { if ($close) { pop @stack } else { push @stack, {%{$stack[-1]}, %$a, transforms => [@{$stack[-1]{transforms} || []}, $a->{transform} // ()]} } next }
        next if $close;
        my %e = (%{$stack[-1]}, %$a); my @subs;
        if ($tag eq 'path') { @subs = path($e{d}) }
        elsif ($tag eq 'circle') { @subs = ellipse($e{cx}, $e{cy}, $e{r}, $e{r}) }
        elsif ($tag eq 'ellipse') { @subs = ellipse($e{cx}, $e{cy}, $e{rx}, $e{ry}) }
        else { @subs = rect($e{x} // 0, $e{y} // 0, $e{width}, $e{height}, $e{rx} // 0) }
        rotate(\@subs, $a->{transform}); rotate(\@subs, $_) for reverse @{$stack[-1]{transforms} || []};
        my ($fill, $stroke) = (colour($e{fill}, $name), colour($e{stroke}, $name));
        my $width = $stroke == 15 ? 0 : int($e{'stroke-width'} * 4 + 0.5);
        my @packed;
        for my $s (@subs) {
            my @q;
            for my $p (@{$s->{points}}) {
                my ($qx, $qy) = map { floor($_ * 4 + 0.5) } @$p;
                die "$name: point $p->[0],$p->[1] is outside 0..63.75" if $qx < 0 || $qy < 0 || $qx > 255 || $qy > 255;
                push @q, [$qx, $qy] unless @q && $q[-1][0] == $qx && $q[-1][1] == $qy;
            }
            pop @q if $s->{closed} && @q > 1 && $q[0][0] == $q[-1][0] && $q[0][1] == $q[-1][1];
            die "$name: a subpath has " . @q . " points (limit 127)" if @q > 127;
            push @packed, [(scalar @q) | ($s->{closed} ? 0x80 : 0), map { @$_ } @q];
        }
        my $points = 0; $points += $_->[0] & 127 for @packed;
        die "$name: a shape has $points points (limit 192) or " . @packed . " subpaths (limit 16)" if $points > 192 || @packed > 16;
        push @shapes, [$fill << 4 | $stroke, $width, scalar @packed, map { @$_ } @packed];
    }
    die "$name: no shapes" unless @shapes;
    (ceil($vw), ceil($vh), scalar @shapes, map { @$_ } @shapes) }

print "/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception\n",
      " * Copyright (c) 2026 Partho Ghosh.\n",
      " * AJ Box's pictures as outlines. Made by tools/icon_pack.pl from docs/design\n",
      " * and assets/icons/extra.html; do not edit by hand. */\n";
my $total = 0;
for my $p (@pictures) {
    my @bytes = pack_picture(@$p); $total += @bytes;
    print "static const uint8_t art_$p->[0]\[\] = {";
    for my $i (0 .. $#bytes) { print "\n   " unless $i % 24; print " $bytes[$i]," }
    print "\n};\n";
}
print "/* ", scalar @pictures, " pictures, $total bytes. */\n";
print STDERR scalar @pictures, " pictures, $total bytes\n";
