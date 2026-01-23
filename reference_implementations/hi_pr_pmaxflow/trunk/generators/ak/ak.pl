#!/usr/bin/perl

print "usage: program numnodes\noutputs dimacs format digraph to stdout\n" and exit if($#ARGV < 0);

$N = $ARGV[0];
$N = 4 if $$ < 4;
$N = $N + 1 if $N % 2 != 0;

$K = ($N - 2) / 2;

$M = 3 * $K + 2;

print "p max $N $M\n";
print "n 1 s\n";
print "n $N t\n";

foreach $v (1 .. $K) # upper path
{
    printf "a %d %d %d\n", $v, $v + 1, $K + 2 -$v;
}

printf "a %d %d 1\n", $K + 1, $N; # last arc of upper path

printf "a 1 %d 1\n", $K + 2; # first arc of lower path

foreach $v ($K + 2 .. $N - 1) # lower path
{
    printf "a %d %d %d\n", $v, $v + 1, $K + 1;
}

foreach $v (2 .. $K + 1) # inner arcs
{
    printf "a %d %d 1\n", $v, $K + 2;
}

