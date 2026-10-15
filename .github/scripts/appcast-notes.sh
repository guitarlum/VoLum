#!/usr/bin/env bash
set -euo pipefail

# The About card draws one clipped line of notes. Keep the appcast value short,
# plain-text, and useful even when a GitHub release starts with generated Markdown.
perl -CSDA -Mutf8 -e '
  use strict;
  use warnings;

  my $in_comment = 0;
  while (defined(my $line = <STDIN>)) {
    chomp $line;
    $line =~ s/\r\z//;
    $line =~ s/^\s+|\s+$//g;

    if ($in_comment) {
      $in_comment = 0 if $line =~ /-->/;
      next;
    }
    if ($line =~ /^<!--/) {
      $in_comment = 1 if $line !~ /-->/;
      next;
    }
    next if $line eq q{};
    next if $line =~ /^#{1,6}(?:\s|$)/;

    (my $compact = $line) =~ s/\s+//g;
    next if $compact =~ /^(?:-{3,}|\*{3,}|_{3,})$/;

    $line =~ s/^(?:[-*+]\s+|\d+[.)]\s+)//;

    my $visible = $line;
    1 while $visible =~ s/!\[[^\]]*\]\((?:[^()]|\([^()]*\))*\)//g;
    $visible =~ s/<img\b[^>]*>//gi;
    1 while $visible =~ s/\[\s*\]\((?:[^()]|\([^()]*\))*\)//g;
    $visible =~ s/\s+//g;
    next if $visible eq q{};

    1 while $line =~ s/\[([^\[\]]+)\]\((?:[^()]|\([^()]*\))*\)/$1/g;
    $line =~ s/(?:\*\*|__|`+)//g;
    $line =~ s/(?<!\w)[*_](?=\S)//g;
    $line =~ s/(?<=\S)[*_](?!\w)//g;
    $line =~ s/\s+/ /g;
    $line =~ s/^\s+|\s+$//g;
    next if $line eq q{};

    $line = substr($line, 0, 117) . q{...} if length($line) > 120;
    print "$line\n";
    last;
  }
  # Drain the rest so a large body does not break the writer'"'"'s pipe.
  1 while <STDIN>;
'
